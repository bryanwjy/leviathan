// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/string.hpp"

#include <Python.h>

#include <concepts>
#include <ranges>

namespace lev::argument {
namespace details {

class argument_root {
protected:
    LEV_HIDDEN constexpr ~argument_root() noexcept = default;
};

template <auto str>
class argument_base;

template <pystring_literal auto str>
class argument_base<str> : public argument_root {
    using source_type = static_storage<str>;

public:
    LEV_HIDE_INSTANTIATION [[nodiscard]] explicit
    operator PyASCIIObject*() const noexcept {
        return reinterpret_cast<PyASCIIObject*>(&source_type::value);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] explicit
    operator PyObject*() const noexcept {
        return reinterpret_cast<PyObject*>(&source_type::value);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] constexpr explicit
    operator std::string_view() const noexcept {
        return source_type::value;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] constexpr explicit
    operator char const*() const noexcept {
        return static_cast<char const*>(source_type::value.data);
    }

    consteval argument_base() noexcept = default;

protected:
    LEV_HIDE_INSTANTIATION constexpr ~argument_base() noexcept = default;
};

template <auto PyStr>
class argument : public argument_base<PyStr> {
public:
    using argument_base<PyStr>::argument_base;
};

template <auto PyStr>
class optional_argument : public argument_base<PyStr> {
public:
    using argument_base<PyStr>::argument_base;
};

void raise_count_error(size_t needed, size_t opt, size_t received) noexcept {
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

struct basic_call {
public:
    LEV_HIDE_INSTANTIATION basic_call(std::span<PyObject* const> args,
        unmanaged_ptr<PyDictObject> kwargs = nullptr) noexcept
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

        details::raise_count_error(Mandatory, Optional, received);
        return false;
    }
    std::span<PyObject* const> args;
    unmanaged_ptr<PyDictObject> kwargs;
};

class vector_call {
public:
    LEV_HIDE_INSTANTIATION vector_call(std::span<PyObject* const> args,
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

        details::raise_count_error(Mandatory, Optional, received);
        return false;
    }

    std::span<PyObject* const> args;
    std::span<PyObject* const> kwargs;
    std::span<PyObject* const> kwnames;
};

template <size_t... Ns, size_t... Os>
LEV_HIDE_INSTANTIATION
    [[nodiscard]] std::array<PyObject*, sizeof...(Ns) + sizeof...(Os)>
    sort(basic_call args, details::argument<Ns>... names,
        details::optional_argument<Os>... onames) noexcept {
    return args.sort(std::span{as_array<PyASCIIObject*>(names...)},
        std::span{as_array<PyASCIIObject*>(onames...)});
}

template <size_t... Ns>
LEV_HIDE_INSTANTIATION [[nodiscard]] std::array<PyObject*, sizeof...(Ns)> sort(
    basic_call args, details::argument<Ns>... names) noexcept {
    return args.sort(std::span{as_array<PyASCIIObject*>(names...)},
        std::span<PyASCIIObject*, 0>{});
}

template <size_t... Ns, size_t... Os>
LEV_HIDE_INSTANTIATION
    [[nodiscard]] std::array<PyObject*, sizeof...(Ns) + sizeof...(Os)>
    sort(vector_call args, details::argument<Ns>... names,
        optional_argument<Os>... onames) noexcept {
    return args.sort(std::span{as_array<char const*>(names...)},
        std::span{as_array<char const*>(onames...)});
}

template <size_t... Ns>
LEV_HIDE_INSTANTIATION [[nodiscard]] std::array<PyObject*, sizeof...(Ns)> sort(
    vector_call args, details::argument<Ns>... names) noexcept {
    return args.sort(std::span{as_array<char const*>(names...)},
        std::span<char const*, 0>{});
}
} // namespace lev::argument

namespace lev {
template <typename>
LEV_HIDDEN inline constexpr bool is_argument_literal_v = false;

template <typename T>
LEV_HIDDEN inline constexpr bool is_argument_literal_v<T const> =
    is_argument_literal_v<T>;
template <typename T>
LEV_HIDDEN inline constexpr bool is_argument_literal_v<T volatile> =
    is_argument_literal_v<T>;
template <typename T>
LEV_HIDDEN inline constexpr bool is_argument_literal_v<T const volatile> =
    is_argument_literal_v<T>;

template <string::details::pyliteral S>
LEV_HIDDEN inline constexpr bool is_argument_literal_v<details::argument<S>> =
    true;

template <string::details::pyliteral S>
LEV_HIDDEN inline constexpr bool
    is_argument_literal_v<details::optional_argument<S>> = true;

template <typename T>
concept argument_literal = is_argument_literal_v<T>;

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

inline namespace literals {
inline namespace argument_literals {
template <string::details::pyliteral S>
[[nodiscard]] consteval auto operator""_arg() noexcept {
    return argument::details::argument<S>{};
}

template <string::details::pyliteral S>
[[nodiscard]] consteval auto operator""_opt() noexcept {
    return argument::details::optional_argument<S>{};
}
} // namespace argument_literals
} // namespace literals

namespace argument {

template <typename>
LEV_HIDDEN inline constexpr bool is_declaration_v = false;

template <argument_literal auto Name, typename T, typename... Ts>
LEV_HIDDEN inline constexpr bool is_declaration_v<typed<Name, T, Ts...>> = true;

template <typename T, typename... Ts>
LEV_HIDDEN inline constexpr bool is_declaration_v<typed<T, Ts...>> = true;

template <typename>
LEV_HIDDEN inline constexpr bool is_named_declaration_v = false;

template <argument_literal auto Name, typename... Ts>
LEV_HIDDEN inline constexpr bool is_named_declaration_v<typed<Name, Ts...>> =
    true;

template <typename>
LEV_HIDDEN inline constexpr bool is_optional_declaration_v = false;

template <details::string::details::pyliteral S, typename... Ts>
LEV_HIDDEN inline constexpr bool
    is_optional_declaration_v<typed<optional_argument<S>, Ts...>> = true;

template <typename>
LEV_HIDDEN inline constexpr bool is_variant_declaration_v = false;

template <auto Name, typename T0, typename T1, typename... Ts>
LEV_HIDDEN inline constexpr bool
    is_variant_declaration_v<typed<Name, T0, T1, Ts...>> = true;

template <typename T0, typename T1, typename... Ts>
LEV_HIDDEN inline constexpr bool
    is_variant_declaration_v<typed<T0, T1, Ts...>> = true;

template <typename T>
concept declaration = is_declaration_v<T>;

template <typename T>
concept named_declaration = declaration<T> && is_named_declaration_v<T>;

template <typename T>
concept optional_declaration = declaration<T> && is_optional_declaration_v<T>;

template <typename T>
concept variant_declaration = declaration<T> && is_variant_declaration_v<T>;

template <typename>
struct name_of {};

template <auto Name, typename... Ts>
struct name_of<typed<Name, Ts...>> {
    static constexpr auto value = Name;
};

template <declaration T>
LEV_HIDDEN inline constexpr auto name_of_v = name_of<T>::value;

template <typename>
struct type_of {};

template <typename T>
using type_of_t = typename type_of<T>::type;

template <typename T>
struct type_of<typed<T>> {
    using type = T;
};

template <auto Name, typename T>
struct type_of<typed<Name, T>> : type_of<typed<T>> {};

template <typename...>
class tuple;
template <typename...>
class variant;

template <typename T0, typename... Ts>
struct type_of<typed<T0, Ts...>> {
    using type = variant<T0, Ts...>;
};
} // namespace argument
} // namespace lev
