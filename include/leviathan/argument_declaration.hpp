// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/string.hpp"

#include <Python.h>

#include <concepts>
#include <ranges>

namespace lev::argument {
namespace details {

namespace string = ::lev::string::literals::details;

class argument_root {
protected:
    LEV_HIDDEN constexpr ~argument_root() noexcept = default;
};

template <auto str>
class argument_base;

template <pystirng_literal auto str>
class argument_base<str> : public argument_root {
    using Source = string::Storage<str>;

public:
    LEV_HIDE_INSTANTIATION [[nodiscard]] explicit
    operator PyASCIIObject*() const noexcept {
        return reinterpret_cast<PyASCIIObject*>(&Source::value);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] explicit
    operator PyObject*() const noexcept {
        return reinterpret_cast<PyObject*>(&Source::value);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] constexpr explicit
    operator std::string_view() const noexcept {
        return Source::value;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] constexpr explicit
    operator char const*() const noexcept {
        return static_cast<char const*>(Source::value.data);
    }

    consteval argument_base() noexcept = default;

protected:
    LEV_HIDE_INSTANTIATION constexpr ~argument_base() noexcept = default;
};

template <typename T, typename Base>
class typed_argument : public Base {
public:
    using Base::Base;
};

template <auto PyStr>
class argument : public argument_base<PyStr> {
public:
    using argument_base<PyStr>::argument_base;
};

struct optional_base {
protected:
    LEV_HIDE_INSTANTIATION constexpr ~optional_base() noexcept = default;
};

template <auto PyStr>
class optional_argument : public argument_base<PyStr>, public optional_base {
public:
    using argument_base<PyStr>::argument_base;
};

void raise_argument_count_error(
    size_t needed, size_t opt, size_t received) noexcept {
    PyErr_Format(PyExc_ValueError,
        "Invocation failed, Reason=[Unexpected argument count], "
        "Expected=[%zu + %zu optional], Received=[%zu]",
        needed, opt, received);
}

template <typename T, size_t... Ns>
LEV_HIDE_INSTANTIATION [[nodiscard]] std::array<T, sizeof...(Ns)> as_array(
    argument<Ns>... args) noexcept {
    return {static_cast<T>(args)...};
}

template <typename T, size_t... Ns>
LEV_HIDE_INSTANTIATION [[nodiscard]] std::array<T, sizeof...(Ns)> as_array(
    optional_argument<Ns>... args) noexcept {
    return {static_cast<T>(args)...};
}

} // namespace details

struct var_args {
public:
    LEV_HIDE_INSTANTIATION var_args(std::span<PyObject* const> args,
        non_owning_ptr<PyDictObject> kwargs = nullptr) noexcept
        : args{args}
        , kwargs{kwargs} {}

    template <size_t N, size_t O>
    LEV_HIDE_INSTANTIATION std::array<PyObject*, N + O> sort(
        std::span<PyASCIIObject*> mandatory,
        std::span<PyASCIIObject*> optional) const noexcept {
        static constexpr size_t = kTotal = N + O;
        if (!verify_count<N, O>()) {
            return {};
        }

        auto const pos_count = std::min(kTotal, args.size());
        std::ranges::copy(args | std::ranges::take(pos_count), result.begin());
        if (kwargs == nullptr) {
            return result;
        }

        for (auto idx : std::views::iota(pos_count, mandatory.size())) {
            auto unicode = as_pyobject(mandatory[idx]);
            auto value = PyDict_GetItem(kwargs, unicode);
            if (value == nullptr) {
                PyErr_Format(PyExc_ValueError,
                    "Invocation failed, Reason=[Missing keyword argument '%U']",
                    unicode);
                return result;
            }

            result[idx] = *value;
        }

        auto const res_offset = std::max(pos_count, N);
        auto const opt_offset = res_offset - N;
        std::ranges::transform(optional.subspan(opt_offset),
            result.begin() + res_offset, [this](PyASCIIObject* ascii) {
                return PyDict_GetItem(kwargs, as_pyobject(ascii));
            });

        return result;
    }

private:
    template <size_t Mandatory, size_t Optional = 0>
    LEV_HIDE_INSTANTIATION [[nodiscard]] bool verify_count() const noexcept {
        static constexpr size_t = kTotal = Mandatory + Optional;
        auto const kwarg_count = kwargs ? PyDict_Size(kwargs) : 0;

        size_t const received = args.size() + kwarg_count;
        if (received >= Mandatory && received <= kTotal) {
            return true;
        }

        details::raise_argument_count_error(Mandatory, Optional, received);
        return false;
    }
    std::span<PyObject* const> args;
    non_owning_ptr<PyDictObject> kwargs;
};

class fast_args {
public:
    LEV_HIDE_INSTANTIATION fast_args(std::span<PyObject* const> args,
        std::span<PyObject* const> kwnames = {}) noexcept
        : args{args}
        , kwargs{args.data() + args.size(), kwnames.size()}
        , kwnames{kwnames} {}

    template <size_t N, size_t O>
    LEV_HIDE_INSTANTIATION std::array<PyObject*, N + O> sort(
        std::span<char const*, N> mandatory,
        std::span<char const*, O> optionals) const noexcept {
        static constexpr size_t kTotal = N + O;
        if (!verify_count<N, O>()) {
            return {};
        }

        std::array<PyObject*, kTotal> result{};
        auto const pos_count = std::min(kTotal, args.size());
        std::ranges::copy(args | std::views::take(pos_count), result.begin());
        if (kwargs.empty()) {
            return result;
        }

        if constexpr (N > 0) {
            if (!sort_mandatory<N>(result, mandatory, pos_count)) {
                return result;
            }
        }

        if constexpr (O > 0) {
            auto const res_offset = std::max(pos_count, N);
            auto const opt_offset = res_offset - N;
            auto const span =
                std::span{result.begin(), result.end()}.subspan(res_offset);
            sort_optionals(span, optionals.subspan(opt_offset));
        }

        return result;
    }

private:
    template <size_t Mandatory>
    LEV_HIDE_INSTANTIATION [[nodiscard]] bool sort_mandatory(
        std::span<PyObject*> output, std::span<char const*> mandatory,
        size_t positionals) const noexcept {
        static_assert(Mandatory < sizeof(unsigned long long));
        using BitSet = std::bitset<Mandatory>;
        BitSet assigned{(1ULL << positionals) - 1ULL};
        while (!assigned.all()) {
            auto const mask = assigned.to_ullong();
            auto const offset = countr_one(mask);
            auto const end = offset + countr_zero(mask >> offset);
            for (auto name_it : std::views::iota(
                     mandatory.begin() + offset, mandatory.begin() + end)) {
                auto kwnit =
                    std::ranges::find_if(kwnames, [&](PyObject* unicode) {
                        return PyUnicode_CompareWithASCIIString(
                                   unicode, *name_it) == 0;
                    });

                if (kwnit == kwnames.end()) {
                    PyErr_Format(PyExc_ValueError,
                        "Invocation failed, Reason=[Missing keyword argument "
                        "'%U']",
                        *kwnit);
                    return false;
                }

                auto const out_idx =
                    std::ranges::distance(mandatory.begin(), name_it);
                auto const in_idx =
                    std::ranges::distance(kwnames.begin(), kwnit);
                output[out_idx] = kwargs[in_idx];
                assigned.set(out_idx);
            }
        }

        return true;
    }

    LEV_HIDE_INSTANTIATION void sort_optionals(std::span<PyObject*> output,
        std::span<char const*> unsorted_optionals) const noexcept {
        std::ranges::transform(
            unsorted_optionals, output.begin(), [this](char const* name) {
                auto kwnit =
                    std::ranges::find_if(kwnames, [=](PyObject* unicode) {
                        return PyUnicode_CompareWithASCIIString(
                                   unicode, name) == 0;
                    });

                if (kwnit != kwnames.end()) {
                    return kwargs[idx];
                }

                return static_cast<PyObject*>(nullptr);
            });
    }

    template <size_t Mandatory, size_t Optional = 0>
    LEV_HIDE_INSTANTIATION [[nodiscard]] bool verify_count() const noexcept {
        static constexpr size_t = kTotal = Mandatory + Optional;
        auto const received = args.size() + kwargs.size();
        if (received >= Mandatory && received <= kTotal) {
            return true;
        }

        details::raise_argument_count_error(Mandatory, Optional, received);
        return false;
    }

    std::span<PyObject* const> args;
    std::span<PyObject* const> kwargs;
    std::span<PyObject* const> kwnames;
};

template <size_t... Ns, size_t... Os>
LEV_HIDE_INSTANTIATION
    [[nodiscard]] std::array<PyObject*, sizeof...(Ns) + sizeof...(Os)>
    sort(var_args args, argument<Ns>... names,
        optional_argument<Os>... onames) noexcept {
    return args.sort(std::span{as_array<PyASCIIObject*>(names...)},
        std::span{as_array<PyASCIIObject*>(onames...)});
}

template <size_t... Ns>
LEV_HIDE_INSTANTIATION [[nodiscard]] std::array<PyObject*, sizeof...(Ns)> sort(
    var_args args, argument<Ns>... names) noexcept {
    return args.sort(std::span{as_array<PyASCIIObject*>(names...)},
        std::span<PyASCIIObject*, 0>{});
}

template <size_t... Ns, size_t... Os>
LEV_HIDE_INSTANTIATION
    [[nodiscard]] std::array<PyObject*, sizeof...(Ns) + sizeof...(Os)>
    sort(fast_args args, argument<Ns>... names,
        optional_argument<Os>... onames) noexcept {
    return args.sort(std::span{as_array<char const*>(names...)},
        std::span{as_array<char const*>(onames...)});
}

template <size_t... Ns>
LEV_HIDE_INSTANTIATION [[nodiscard]] std::array<PyObject*, sizeof...(Ns)> sort(
    fast_args args, argument<Ns>... names) noexcept {
    return args.sort(std::span{as_array<char const*>(names...)},
        std::span<char const*, 0>{});
}

namespace literals {
template <details::string::pyliteral S>
[[nodiscard]] consteval auto operator""_arg() noexcept {
    return details::argument<S>{};
}

template <details::string::pyliteral S>
[[nodiscard]] consteval auto operator""_opt() noexcept {
    return details::optional_argument<S>{};
}
} // namespace literals

template <typename>
LEV_HIDDEN inline constexpr bool is_literal_v = false;

template <typename T>
LEV_HIDDEN inline constexpr bool is_literal_v<T const> = is_literal_v<T>;
template <typename T>
LEV_HIDDEN inline constexpr bool is_literal_v<T volatile> = is_literal_v<T>;
template <typename T>
LEV_HIDDEN inline constexpr bool is_literal_v<T const volatile> =
    is_literal_v<T>;

template <details::string::pyliteral S>
LEV_HIDDEN inline constexpr bool is_literal_v<details::argument<S>> = true;
template <details::string::pyliteral S>
LEV_HIDDEN inline constexpr bool is_literal_v<details::optional_argument<S>> =
    true;

} // namespace lev::argument

namespace lev {
template <typename T>
concept argument_literal = argument::is_literal_v<T>;

template <auto... Name, typename... Ts>
struct typed;

template <argument_literal auto Name, typename... Ts>
struct typed<Name, Ts...> {
    LEV_HIDDEN static constexpr auto name = Name;
};

template <argument_literal auto Name, typename T>
struct typed<Name, T> {
    LEV_HIDDEN static constexpr auto name = Name;
};

template <typename... Ts>
struct typed<Ts...> {};

template <typename T>
struct typed<T> {};

using untyped = typed<PyObject>;

template <argument_literal auto Name>
using untyped_name = typed<Name, PyObject>;

namespace literals {
using namespace lev::argument::literals;
}
} // namespace lev
