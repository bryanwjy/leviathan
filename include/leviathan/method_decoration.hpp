// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/string.hpp"

#include <Python.h>

#include <concepts>
#include <ranges>

namespace lev::method {

template <typename... T>
struct decorator_t;

struct option_t {
protected:
    LEV_HIDDEN ~option_t() = default;
};

consteval int option_base(...) noexcept = delete;

template <typename D>
using option_type_t = decltype(option_base(std::declval<D>()));

template <typename D, typename B>
struct option_base_t : B {
    friend inline consteval B option_base(D) noexcept { return {}; }

protected:
    LEV_HIDE_INSTANTIATION ~option_base_t() = default;
};

struct throw_option_t : option_t {
protected:
    LEV_HIDDEN ~throw_option_t() = default;
};

struct nothrow_t : option_base_t<nothrow_t, throw_option_t> {
    explicit inline consteval nothrow_t() = default;
};

struct default_throw_t : option_base_t<default_throw_t, throw_option_t> {
    explicit inline consteval default_throw_t() = default;
};

struct call_option_t : option_t {
protected:
    LEV_HIDDEN ~call_option_t() = default;
};

struct vector_call_t : option_base_t<vector_call_t, call_option_t> {
    explicit inline consteval vector_call_t() = default;
};

struct baisc_call_t : option_base_t<baisc_call_t, call_option_t> {
    explicit inline consteval baisc_call_t() = default;
};

struct scope_option_t : option_t {
protected:
    LEV_HIDDEN ~scope_option_t() = default;
};

struct static_t : option_base_t<static_t, scope_option_t> {
    explicit inline consteval static_t() = default;
};

struct instance_t : option_base_t<instance_t, scope_option_t> {
    explicit inline consteval instance_t() = default;
};

template <typename... Os>
struct decorator_t<Os...> : public Os... {
    static_assert(
        (... && std::is_base_of_v<option_t, Os>), "Invalid option type");

public:
    explicit inline consteval decorator_t() = default;

    friend consteval Os decoration(decorator_t) noexcept
    requires (sizeof...(Os) == 1)
    {
        return Os{};
    }

    template <typename U>
    requires requires(U u) { requires (... || std::same_as<U, Os>); }
    inline consteval decorator_t<Os...> operator|(
        decorator_t<U>) const noexcept {
        return *this;
    }

    template <typename U>
    requires requires(U u) {
        typename option_type_t<U>;
        requires !(... || std::same_as<U, Os>);
        requires !(... || std::same_as<option_type_t<U>, option_type_t<Os>>);
        decorator_t<Os..., U>{};
    }
    inline consteval decorator_t<Os..., U> operator|(
        decorator_t<U>) const noexcept {
        return decorator_t<Os..., U>{};
    }
};

template <typename T>
LEV_HIDDEN inline constexpr bool is_decorator_v = false;

template <typename T>
LEV_HIDDEN inline constexpr bool is_decorator_v<T const> = is_decorator_v<T>;

template <typename... Ts>
LEV_HIDDEN inline constexpr bool is_decorator_v<decorator_t<Ts...>> = true;

template <typename T>
concept decorator = is_decorator_v<T>;

template <auto T, auto... Os>
concept decorated_with = decorator<decltype(T)> &&
    (... && decorator<decltype(Os)>)&&(
        ... && std::derived_from<T, decltype(decoration(Os))>);

} // namespace lev::method

namespace lev {

LEV_HIDDEN inline constexpr auto nothrow =
    method::decorator_t<method::nothrow_t>{};
LEV_HIDDEN inline constexpr auto default_throw =
    method::decorator_t<method::default_throw_t>{};
LEV_HIDDEN inline constexpr auto vector_call =
    method::decorator_t<method::vector_call_t>{};
LEV_HIDDEN inline constexpr auto baisc_call =
    method::decorator_t<method::baisc_call_t>{};
LEV_HIDDEN inline constexpr auto static_method =
    method::decorator_t<method::static_t>{};
LEV_HIDDEN inline constexpr auto instance_method =
    method::decorator_t<method::instance_t>{};

} // namespace lev
