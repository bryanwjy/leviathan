// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/_common.hpp"
#include "leviathan/type_traits.hpp"

namespace lev::details {
template <typename T0, typename T1>
LEV_HIDDEN inline constexpr bool fits_in_tail_padding_v = []() {
    struct test_struct {
        [[LEV_MSVC no_unique_address]] T0 first;
        [[LEV_MSVC no_unique_address]] T1 second;
    };
    return sizeof(test_struct) == sizeof(T0);
}();

template <bool NoUniqueAdress, typename T>
struct conditionally_overlapable {
    LEV_HIDE_INSTANTIATION inline constexpr conditionally_overlapable() =
        delete;
    LEV_HIDE_INSTANTIATION explicit(is_explicit_constructible_v<
        T>) inline constexpr conditionally_overlapable() noexcept(std::
            is_nothrow_default_constructible_v<T>)
    requires (std::is_default_constructible_v<T>)
    = default;

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit conditionally_overlapable(
        std::in_place_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : data{std::forward<Args>(args)...} {}

    template <typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit conditionally_overlapable(
        converting_t, F&& f,
        Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...> &&
        std::is_nothrow_constructible_v<T, std::invoke_result_t<F, Args...>>)
        : data{std::invoke(std::forward<F>(f), std::forward<Args>(args)...)} {}

    [[LEV_MSVC no_unique_address]] T data;
};

template <typename T>
struct conditionally_overlapable<false, T> {
    LEV_HIDE_INSTANTIATION inline constexpr conditionally_overlapable() =
        delete;
    LEV_HIDE_INSTANTIATION explicit(is_explicit_constructible_v<
        T>) inline constexpr conditionally_overlapable() noexcept(std::
            is_nothrow_default_constructible_v<T>)
    requires (std::is_default_constructible_v<T>)
    = default;

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit conditionally_overlapable(
        std::in_place_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : data{std::forward<Args>(args)...} {}

    template <typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit conditionally_overlapable(
        converting_t, F&& f,
        Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...> &&
        std::is_nothrow_constructible_v<T, std::invoke_result_t<F, Args...>>)
        : data{std::invoke(std::forward<F>(f), std::forward<Args>(args)...)} {}

    T data;
};
} // namespace lev::details
