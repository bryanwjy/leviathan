// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/argument_declaration.hpp"
#include "leviathan/string.hpp"

#include <Python.h>

#include <concepts>
#include <ranges>

namespace lev::argument {
namespace details {
struct basic_args {
public:
    LEV_HIDE_INSTANTIATION basic_args(
        std::span<PyObject* const> args, py::views::kwargs kwargs = {}) noexcept
        : args{args}
        , kwargs{kwargs} {}

    template <size_t N, size_t O>
    LEV_HIDE_INSTANTIATION std::array<PyObject*, N + O> sort(
        std::span<PyASCIIObject* const> mandatory,
        std::span<PyASCIIObject* const> optional) const noexcept {
        static constexpr size_t = kTotal = N + O;
        if (!verify_count<N, O>()) {
            return {};
        }

        std::array<PyObject*, kTotal> result{};
        auto const pos_count = std::min(kTotal, args.size());
        std::ranges::copy(args | std::ranges::take(pos_count), result.begin());
        if (kwargs.empty()) {
            return result;
        }

        for (auto idx : std::views::iota(pos_count, mandatory.size())) {
            auto value = kwargs[mandatory[idx]];
            if (value == nullptr) {
                PyErr_Format(PyExc_ValueError,
                    "Invocation failed, Reason=[Missing keyword argument '%U']",
                    mandatory[idx]);
                return result;
            }

            result[idx] = *value;
        }

        auto const res_offset = std::max(pos_count, N);
        auto const opt_offset = res_offset - N;
        std::ranges::transform(optional.subspan(opt_offset),
            result.begin() + res_offset,
            [this](PyASCIIObject* ascii) { return kwargs[ascii]; });

        return result;
    }

    template <size_t N, size_t O>
    LEV_HIDE_INSTANTIATION std::array<PyObject*, N + O> sort() const noexcept {
        static constexpr size_t kTotal = N + O;
        if (!verify_count<N, O>()) {
            return {};
        }

        std::array<PyObject*, kTotal> result{};
        auto const pos_count = std::min(kTotal, args.size());
        std::ranges::copy(args | std::views::take(pos_count), result.begin());
        return result;
    }

private:
    template <size_t Mandatory, size_t Optional = 0>
    LEV_HIDE_INSTANTIATION [[nodiscard]] bool verify_count() const noexcept {
        static constexpr size_t = kTotal = Mandatory + Optional;
        size_t const received = args.size() + kwargs.size();
        if (received >= Mandatory && received <= kTotal) {
            return true;
        }

        details::raise_count_error(Mandatory, Optional, received);
        return false;
    }
    std::span<PyObject* const> args;
    py::views::kwargs kwargs;
};

class vector_args {
public:
    LEV_HIDE_INSTANTIATION vector_args(std::span<PyObject* const> args,
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

    template <size_t N, size_t O>
    LEV_HIDE_INSTANTIATION std::array<PyObject*, N + O> sort() const noexcept {
        static constexpr size_t kTotal = N + O;
        if (!verify_count<N, O>()) {
            return {};
        }

        std::array<PyObject*, kTotal> result{};
        auto const pos_count = std::min(kTotal, args.size());
        std::ranges::copy(args | std::views::take(pos_count), result.begin());
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

template <named_declaration... Ns, optional_declaration... Os>
requires (... && !optional_declaration<Ns>) && (... && named_declaration<Ns>)
LEV_HIDE_INSTANTIATION
    [[nodiscard]] std::array<PyObject*, sizeof...(Ns) + sizeof...(Os)> sort(
        basic_args args, Ns..., Os...) noexcept {
    return args.sort(
        std::span{details::as_array<PyASCIIObject*>(name_of_v<Ns>...)},
        std::span{details::as_array<PyASCIIObject*>(name_of_v<Os>...)});
}

template <named_declaration... Ns>
requires (... && !optional_declaration<Ns>)
LEV_HIDE_INSTANTIATION [[nodiscard]] std::array<PyObject*, sizeof...(Ns)> sort(
    basic_args args, Ns...) noexcept {
    return args.sort(
        std::span{details::as_array<PyASCIIObject*>(name_of_v<Ns>...)},
        std::span<PyASCIIObject*, 0>{});
}

template <named_declaration... Ns, optional_declaration... Os>
requires (... && !optional_declaration<Ns>) && (... && named_declaration<Ns>)
LEV_HIDE_INSTANTIATION
    [[nodiscard]] std::array<PyObject*, sizeof...(Ns) + sizeof...(Os)> sort(
        vector_args args, Ns... names, Os... onames) noexcept {
    return args.sort(
        std::span{adetails::as_array<char const*>(name_of_v<Ns>...)},
        std::span{details::as_array<char const*>(name_of_v<Os>...)});
}

template <named_declaration... Ns>
requires (... && !optional_declaration<Ns>)
LEV_HIDE_INSTANTIATION [[nodiscard]] std::array<PyObject*, sizeof...(Ns)> sort(
    vector_args args, Ns... names) noexcept {
    return args.sort(
        std::span{details::as_array<char const*>(name_of_v<Ns>...)},
        std::span<char const*, 0>{});
}

} // namespace details
} // namespace lev::argument
