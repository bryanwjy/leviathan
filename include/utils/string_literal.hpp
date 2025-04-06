// Copyright 2025, Bryan Wong
#pragma once

#include "utils/type_traits.hpp"

#include <concepts>
#include <string_view>
#include <utility>

namespace ltl {

template <size_t N>
struct string_literal {
    char const data[N];

    consteval string_literal(char const (&str)[N]) noexcept
        : string_literal{str, std::make_index_sequence<N>{}} {}

    consteval operator std::string_view() const noexcept {
        return std::string_view{static_cast<char const*>(data), N - 1};
    }

    consteval explicit operator char const*() const noexcept {
        return static_cast<char const*>(data);
    }

    consteval string_literal(string_literal const&) noexcept = default;
    consteval string_literal& operator=(
        string_literal const&) noexcept = default;

private:
    template <size_t... Is>
    requires (sizeof...(Is) == N)
    consteval string_literal(
        char const (&str)[N], std::index_sequence<Is...>) noexcept
        : data{str[Is]...} {}
};

inline namespace literals {
inline namespace string_literals {
template <string_literal S>
inline consteval auto operator""_str() noexcept {
    return S;
}
} // namespace string_literals
} // namespace literals

template <typename T>
LEV_HIDDEN inline constexpr bool is_string_literal_v = false;
template <size_t N>
LEV_HIDDEN inline constexpr bool is_string_literal_v<string_literal<N>> = true;

template <typename T>
concept string_literal_type = is_string_literal_v<T>;

} // namespace ltl
