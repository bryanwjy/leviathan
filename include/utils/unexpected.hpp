// Copyright 2025 Bryan Wong

#pragma once

#include "utils/common.hpp"
#include "utils/type_traits.hpp"

#include <concepts>
#include <initializer_list>
#include <type_traits>
#include <utility>

LEV_STD_NAMESPACE_BEGIN
struct unexpect_t;
template <typename>
class unexpected;
LEV_STD_NAMESPACE_END

namespace ltl {

struct LEV_API unexpect_t {
    LEV_HIDE_INSTANTIATION explicit inline constexpr unexpect_t() noexcept = default;
    template <same_as<::std::unexpect_t> U = ::std::unexpect_t>
    LEV_HIDE_INSTANTIATION inline constexpr unexpect_t(U) noexcept {}

    template <same_as<::std::unexpect_t> U = ::std::unexpect_t>
    LEV_HIDE_INSTANTIATION inline constexpr operator U() const noexcept {
        return U{};
    }
};

inline constexpr unexpect_t unexpect{};

namespace details::unexpect {
template <typename U>
concept tag_type = same_as<U, ::std::unexpect_t> || same_as<U, unexpect_t>;

} // namespace details::unexpect

template <typename E>
class LEV_API unexpected {
    static_assert(std::is_destructible_v<E>, "Invalid type");
    static_assert(!std::is_array_v<E>, "Invalid type");
    static_assert(!std::is_reference_v<E>, "Invalid type");

public:
    LEV_HIDE_INSTANTIATION inline constexpr unexpected(unexpected const&) noexcept(
        std::is_nothrow_copy_constructible_v<E>) = default;
    LEV_HIDE_INSTANTIATION inline constexpr unexpected(unexpected&&) noexcept(
        std::is_nothrow_move_constructible_v<E>) = default;

    LEV_HIDE_INSTANTIATION inline constexpr unexpected& operator=(unexpected const&) noexcept(
        std::is_nothrow_copy_assignable_v<E>) = default;
    LEV_HIDE_INSTANTIATION inline constexpr unexpected& operator=(unexpected&&) noexcept(
        std::is_nothrow_move_assignable_v<E>) = default;

    LEV_HIDE_INSTANTIATION inline constexpr ~unexpected() noexcept = default;

    template <typename E1 = E>
    requires __LTL different_from<std::remove_cvref_t<E1>, __LTL unexpected> &&
        __LTL different_from<std::remove_cvref_t<E1>, std::unexpected> &&
        __LTL different_from<std::remove_cvref_t<E1>, std::in_place_t> &&
        std::is_constructible_v<E, E1>
    LEV_HIDE_INSTANTIATION inline constexpr unexpected(E1&& arg) noexcept(
        std::is_nothrow_constructible<E, E1>)
        : error_{std::forward<E1>(arg)} {}

    template <typename... Args>
    requires std::is_constructible_v<E, Args...>
    LEV_HIDE_INSTANTIATION inline constexpr unexpected(in_place_t, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<E, Args...>)
        : error_{std::forward<Args>(args)...} {}

    template <typename U, typename... Args>
    requires std::is_constructible_v<E, ::std::initializer_list<U>&, Args...>
    LEV_HIDE_INSTANTIATION inline constexpr unexpected(in_place_t, ::std::initializer_list<U> il,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<E,
        ::std::initializer_list<U>&, Args...>)
        : error_{il, std::forward<Args>(args)...} {}

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E const&
    error() const& noexcept {
        return error_;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E& error() & noexcept {
        return error_;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E const&&
    error() const&& noexcept {
        return std::move(error_);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr E&& error() && noexcept {
        return std::move(error_);
    }

    LEV_HIDE_INSTANTIATION inline constexpr void swap(unexpected& other) noexcept(
        std::is_nothrow_swappable<E>)
    requires std::is_swappable_v<E>
    {
        using std::swap;
        swap(error_, other.error_);
    }

    LEV_HIDE_INSTANTIATION friend inline constexpr void swap(unexpected& left,
        unexpected& right) noexcept(noexcept(left.swap(right)))
    requires requires { left.swap(right); }
    {
        left.swap(right);
    }

    template <std::equality_comparable_with<E> E1>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr bool
    operator==(unexpected const& left, unexpected<E1> const& right) noexcept(
        noexcept(left.error() == right.error())) {
        return left.error() == right.error();
    }

    template <std::equality_comparable_with<E> E1>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr bool
    operator!=(unexpected const& left, unexpected<E1> const& right) noexcept(
        noexcept(!(left.error() == right.error()))) {
        return !(left.error() == right.error());
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr bool operator==(
        unexpected const& other) const noexcept = default;
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr bool operator!=(
        unexpected const& other) const noexcept = default;
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto operator<=>(
        unexpected const& other) const noexcept = default;

private:
    E error_;
};

template <typename E>
unexpected(E) -> unexpected<E>;

namespace details {
template <typename T>
LEV_HIDDEN inline constexpr bool is_unexpected_type_v = false;
template <typename T>
LEV_HIDDEN inline constexpr bool is_unexpected_type_v<T const> =
    is_unexpected_type_v<T>;
template <typename T>
LEV_HIDDEN inline constexpr bool is_unexpected_type_v<T volatile> =
    is_unexpected_type_v<T>;
template <typename T>
LEV_HIDDEN inline constexpr bool is_unexpected_type_v<unexpected<T>> = true;
template <typename T>
concept unexpected_type = is_unexpected_type_v<T>;
} // namespace details

} // namespace ltl
