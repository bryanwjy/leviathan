// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/string.hpp"

#include <Python.h>

#include <concepts>
#include <ranges>

namespace lev::method {

template <typename... T>
struct decorations_t;

struct decoration_t {
protected:
    LEV_HIDDEN ~decoration_t() = default;
};

consteval int decoration_base(...) noexcept = delete;

template <typename D>
using decoration_type_t = decltype(decoration_base(std::declval<D>()));

template <typename D, typename B>
struct decoration_base_t : B {
    friend inline consteval B decoration_base(D) noexcept { return {}; }

protected:
    LEV_HIDE_INSTANTIATION ~decoration_base_t() = default;
};

struct throw_decoration_t : decoration_t {
protected:
    LEV_HIDDEN constexpr ~throw_decoration_t() = default;
};

struct nothrow_t : decoration_base_t<nothrow_t, throw_decoration_t> {
    explicit inline consteval nothrow_t() = default;
};

struct default_throw_t :
    decoration_base_t<default_throw_t, throw_decoration_t> {
    explicit inline consteval default_throw_t() = default;
};

struct call_decoration_t : decoration_t {
protected:
    LEV_HIDDEN constexpr ~call_decoration_t() = default;
};

struct vector_call_t : decoration_base_t<vector_call_t, call_decoration_t> {
    explicit inline consteval vector_call_t() = default;
};

struct baisc_call_t : decoration_base_t<baisc_call_t, call_decoration_t> {
    explicit inline consteval baisc_call_t() = default;
};

struct scope_decoration_t : decoration_t {
protected:
    LEV_HIDDEN ~scope_decoration_t() = default;
};

struct static_t : decoration_base_t<static_t, scope_decoration_t> {
    explicit inline consteval static_t() = default;
};

struct instance_t : decoration_base_t<instance_t, scope_decoration_t> {
    explicit inline consteval instance_t() = default;
};

template <typename... Os>
struct decorations_t<Os...> : public Os... {
    static_assert(
        (... && std::is_base_of_v<decoration_t, Os>), "Invalid specifier type");

public:
    explicit inline consteval decorations_t() = default;

    friend consteval Os specifier(decorations_t) noexcept
    requires (sizeof...(Os) == 1)
    {
        return Os{};
    }

    template <typename U>
    requires requires(U u) { requires (... || std::same_as<U, Os>); }
    inline consteval decorations_t<Os...> operator|(
        decorations_t<U>) const noexcept {
        return *this;
    }

    template <typename U>
    requires requires(U u) {
        typename decoration_type_t<U>;
        requires !(... || std::same_as<U, Os>);
        requires !(
            ... || std::same_as<decoration_type_t<U>, decoration_type_t<Os>>);
        decorations_t<Os..., U>{};
    }
    inline consteval decorations_t<Os..., U> operator|(
        decorations_t<U>) const noexcept {
        return decorations_t<Os..., U>{};
    }
};

template <typename T>
LEV_HIDDEN inline constexpr bool is_decorations_type_v = false;

template <typename T>
LEV_HIDDEN inline constexpr bool is_decorations_type_v<T const> =
    is_decorations_type_v<T>;

template <typename... Ts>
LEV_HIDDEN inline constexpr bool is_decorations_type_v<decorations_t<Ts...>> =
    true;

template <typename T>
concept decorations = is_decorations_type_v<T>;

template <typename T, auto... Os>
concept decorated_with = decorations<T> &&
    (... && decorations<decltype(Os)>)&&(
        ... && std::derived_from<T, decltype(decoration(Os))>);

} // namespace lev::method

namespace lev {

LEV_HIDDEN inline constexpr auto nothrow =
    method::decorations_t<method::nothrow_t>{};
LEV_HIDDEN inline constexpr auto default_throw =
    method::decorations_t<method::default_throw_t>{};
LEV_HIDDEN inline constexpr auto vector_call =
    method::decorations_t<method::vector_call_t>{};
LEV_HIDDEN inline constexpr auto baisc_call =
    method::decorations_t<method::baisc_call_t>{};
LEV_HIDDEN inline constexpr auto static_method =
    method::decorations_t<method::static_t>{};
LEV_HIDDEN inline constexpr auto instance_method =
    method::decorations_t<method::instance_t>{};

} // namespace lev
