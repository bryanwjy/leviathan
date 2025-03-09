// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/conditionally_overlapable.hpp"
#include "leviathan/jump_table.hpp"
#include "leviathan/type_traits.hpp"

#include <array>
#include <compare>
#include <exception>
#include <initializer_list>
#include <memory>
#include <ranges>

namespace lev {

template <typename...>
class LEV_PUBLIC variant;

class LEV_PUBLIC bad_variant_access : public std::exception {
public:
    char const* what() const noexcept override { return "bad_variant_access"; }
};

template <typename T>
inline constexpr size_t variant_size_v = 0;
template <typename T>
inline constexpr size_t variant_size_v<T const> = variant_size_v<T>;
template <typename T>
inline constexpr size_t variant_size_v<T volatile> = variant_size_v<T>;
template <typename T>
inline constexpr size_t variant_size_v<T const volatile> = variant_size_v<T>;
template <typename... Ts>
inline constexpr size_t variant_size_v<variant<Ts...>> = sizeof...(Ts);
inline constexpr size_t variant_npos = static_cast<size_t>(-1);

template <size_t, typename>
struct LEV_API variant_alternative;

template <size_t I, typename T>
using variant_alternative_t = typename variant_alternative<I, T>::type;

template <size_t I, typename... Ts>
struct LEV_API variant_alternative<I, variant<Ts...>> :
    template_element<I, typelist<Ts...>> {};

struct LEV_API monostate {
    LEV_HIDE_INSTANTIATION
    [[nodiscard, gnu::always_inline]] friend inline constexpr bool operator==(
        monostate, monostate) noexcept {
        return true;
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard, gnu::always_inline]] friend inline constexpr auto operator<=>(
        monostate, monostate) noexcept {
        return std::strong_ordering::equal;
    }
};

namespace details {
using details::conditionally_overlapable;
using details::fits_in_tail_padding_v;

template <typename...>
union multi_union;

template <typename Head, typename... Tail>
union multi_union<Head, Tail...> {
    using first_type = Head;
    using second_type = std::conditional_t<(sizeof...(Tail) > 1),
        multi_union<Tail...>, template_element_t<1, multi_union>>;

    LEV_HIDE_INSTANTIATION static constexpr bool is_last_union =
        (sizeof...(Tail) == 1);

    LEV_HIDE_INSTANTIATION inline constexpr multi_union(
        multi_union const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(
        multi_union const&) noexcept(std::
                                         is_nothrow_copy_constructible_v<
                                             first_type> &&
        std::is_nothrow_copy_constructible_v<second_type>)
    requires (std::is_copy_constructible_v<first_type> &&
                 std::is_copy_constructible_v<second_type> &&
                 std::is_trivially_copy_constructible_v<first_type> &&
                 std::is_trivially_copy_constructible_v<second_type>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(multi_union&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(multi_union&&) noexcept(
        std::is_nothrow_move_constructible_v<first_type> &&
        std::is_nothrow_move_constructible_v<second_type>)
    requires (std::is_move_constructible_v<first_type> &&
                 std::is_move_constructible_v<second_type> &&
                 std::is_trivially_move_constructible_v<first_type> &&
                 std::is_trivially_move_constructible_v<second_type>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr multi_union& operator=(
        multi_union const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr multi_union&
    operator=(multi_union const&) noexcept(
        std::is_nothrow_copy_assignable_v<first_type> &&
        std::is_nothrow_copy_assignable_v<second_type>)
    requires (std::is_copy_assignable_v<first_type> &&
                 std::is_copy_assignable_v<second_type> &&
                 std::is_trivially_copy_assignable_v<first_type> &&
                 std::is_trivially_copy_assignable_v<second_type>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr multi_union& operator=(
        multi_union&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr multi_union& operator=(
        multi_union&&) noexcept(std::is_nothrow_move_assignable_v<first_type> &&
        std::is_nothrow_move_assignable_v<second_type>)
    requires (std::is_move_assignable_v<first_type> &&
                 std::is_move_assignable_v<second_type> &&
                 std::is_trivially_move_assignable_v<first_type> &&
                 std::is_trivially_move_assignable_v<second_type>)
    = default;

    template <typename... Args>
    requires (std::is_constructible_v<first_type, Args...>)
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(
        std::in_place_type_t<first_type>,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<first_type,
        Args...>)
        : first{std::forward<Args>(args)...} {}

    template <typename... Args>
    requires (std::is_constructible_v<first_type, Args...>)
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(
        std::in_place_index_t<0>,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<first_type,
        Args...>)
        : first{std::forward<Args>(args)...} {}

    template <typename F, typename... Args>
    requires (std::is_nothrow_invocable_r_v<first_type, F, Args...>)
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(generator_index_t<0>,
        F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>)
        : first{std::invoke_r<first_type>(
              std::forward<F>(f), std::forward<Args>(args)...)} {}

    template <typename F, typename... Args>
    requires (std::is_nothrow_invocable_r_v<first_type, F, Args...>)
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(
        generator_type_t<first_type>, F&& f,
        Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>)
        : first{std::invoke_r<first_type>(
              std::forward<F>(f), std::forward<Args>(args)...)} {}

    template <typename U, typename... Args>
    requires (!is_last_union &&
        std::is_constructible_v<second_type, std::in_place_type_t<U>, Args...>)
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(
        std::in_place_type_t<U> tag,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<second_type,
        in_place_type_t<U>, Args...>)
        : second{tag, std::forward<Args>(args)...} {}

    template <size_t N, typename... Args>
    requires (!is_last_union &&
        std::is_constructible_v<second_type, std::in_place_index_t<N - 1>,
            Args...>)
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(
        std::in_place_index_t<N>,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<second_type,
        std::in_place_index_t<N>, Args...>)
        : second{std::in_place_index<N - 1>, std::forward<Args>(args)...} {}

    template <size_t N, typename F, typename... Args>
    requires (!is_last_union &&
        std::is_constructible_v<second_type, generator_index_t<N - 1>, F,
            Args...>)
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(generator_index_t<N>,
        F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>)
        : second{generator_index<N - 1>, std::forward<F>(f),
              std::forward<Args>(args)...} {}

    template <typename U, typename F, typename... Args>
    requires (!is_last_union &&
        std::is_constructible_v<second_type, generator_type_t<U>, F, Args...>)
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(generator_type_t<U> tag,
        F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>)
        : second{tag, std::forward<F>(f), std::forward<Args>(args)...} {}

    template <typename... Args>
    requires (is_last_union && std::is_constructible_v<second_type, Args...>)
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(
        std::in_place_type_t<second_type>,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<second_type,
        Args...>)
        : second{std::forward<Args>(args)...} {}

    template <typename... Args>
    requires (is_last_union && std::is_constructible_v<second_type, Args...>)
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(
        std::in_place_index_t<1>,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<second_type,
        Args...>)
        : second{std::forward<Args>(args)...} {}

    template <typename F, typename... Args>
    requires (
        is_last_union && std::is_nothrow_invocable_r_v<second_type, F, Args...>)
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(generator_index_t<1>,
        F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>)
        : second{std::invoke_r<second_type>(
              std::forward<F>(f), std::forward<Args>(args)...)} {}

    template <typename F, typename... Args>
    requires (
        is_last_union && std::is_nothrow_invocable_r_v<second_type, F, Args...>)
    LEV_HIDE_INSTANTIATION inline constexpr multi_union(
        generator_type_t<second_type>, F&& f,
        Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>)
        : second{std::invoke_r<second_type>(
              std::forward<F>(f), std::forward<Args>(args)...)} {}

    LEV_HIDE_INSTANTIATION inline constexpr ~multi_union() noexcept
    requires (std::is_trivially_destructible_v<first_type> &&
                 std::is_trivially_destructible_v<second_type>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr ~multi_union() noexcept {}

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        [[nodiscard, gnu::flatten]] inline constexpr decltype(auto)
        get() const& noexcept {
        static_assert(I <= sizeof...(Tail));
        if constexpr (I == 0) {
            return first;
        } else if constexpr (is_last_union) {
            return second;
        } else {
            return second.template get<I - 1>();
        }
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        [[nodiscard, gnu::flatten]] inline constexpr decltype(auto)
        get() & noexcept {
        static_assert(I <= sizeof...(Tail));
        if constexpr (I == 0) {
            return first;
        } else if constexpr (is_last_union) {
            return second;
        } else {
            return second.template get<I - 1>();
        }
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        [[nodiscard, gnu::flatten]] inline constexpr decltype(auto)
        get() const&& noexcept {
        static_assert(I <= sizeof...(Tail));
        if constexpr (I == 0) {
            return std::move(first);
        } else if constexpr (is_last_union) {
            return std::move(second);
        } else {
            return std::move(second).template get<I - 1>();
        }
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        [[nodiscard, gnu::flatten]] inline constexpr decltype(auto)
        get() && noexcept {
        static_assert(I <= sizeof...(Tail));
        if constexpr (I == 0) {
            return std::move(first);
        } else if constexpr (is_last_union) {
            return std::move(second);
        } else {
            return std::move(second).template get<I - 1>();
        }
    }

    [[LEV_MSVC no_unique_address]] first_type first;
    [[LEV_MSVC no_unique_address]] second_type second;
};

template <typename T>
LEV_HIDE_INSTANTIATION inline constexpr auto jump_table_for =
    []<size_t... Is>(std::index_sequence<Is...>) {
        return jump_table<Is...>{};
    }(std::make_index_sequence<template_size_v<T>>{});

template <typename T, typename U>
LEV_HIDE_INSTANTIATION inline constexpr T make_from_multi_union(size_t index,
    U&& arg) noexcept([]<size_t... Is>(std::index_sequence<Is...>) {
    return (... &&
        std::is_nothrow_constructible_v<T, std::in_place_index_t<Is>,
            copy_cvref_t<U, template_element_t<Is, std::remove_cvref_t<U>>>>);
}(std::make_index_sequence<template_size_v<T>>{})) {
    static_assert(template_size_v<T> == template_size_v<U>);
    return jump_table_for<U>(
        [&]<size_t I>(size_constant<I>) {
            return T{
                std::in_place_index<I>, std::forward<U>(arg).template get<I>()};
        },
        index);
}

struct valueless_t {
    LEV_HIDDEN explicit inline constexpr valueless_t() noexcept = default;
};

struct variant_dispatch_t {
    LEV_HIDDEN explicit inline constexpr variant_dispatch_t() noexcept =
        default;
};

LEV_HIDDEN inline constexpr variant_dispatch_t variant_dispatch{};

template <typename... Ts>
class variant_base {
    using union_type = multi_union<Ts..., valueless_t>;
    using index_type = unsigned char;
    static_assert(
        sizeof...(Ts) < static_cast<index_type>(-1), "limit exceeded");
    LEV_HIDDEN static constexpr bool place_index_in_tail =
        fits_in_tail_padding_v<union_type, index_type>;
    LEV_HIDDEN static constexpr bool allow_external_overlap =
        !place_index_in_tail;

    struct container {
        template <typename T, typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr container(
            std::in_place_type_t<T> tag, Args&&... args)
            : union_{std::in_place, tag, std::forward<Args>(args)...}
            , index_{template_index_v<T, union_type>} {}

        template <size_t I, typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr container(
            std::in_place_index_t<I> tag, Args&&... args)
            : union_{std::in_place, tag, std::forward<Args>(args)...}
            , index_{I} {}

        template <typename T, typename F, typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr container(
            generator_type_t<T> tag, F&& callable, Args&&... args)
            : union_{std::in_place, tag, std::forward<F>(callable),
                  std::forward<Args>(args)...}
            , index_{template_index_v<T, union_type>} {}

        template <size_t I, typename F, typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr container(
            generator_index_t<I> tag, F&& callable, Args&&... args)
            : union_{std::in_place, tag, std::forward<F>(callable),
                  std::forward<Args>(args)...}
            , index_{I} {}

        template <typename U>
        LEV_HIDE_INSTANTIATION inline constexpr container(variant_dispatch_t,
            size_t index,
            U&& arg) noexcept(noexcept(make_from_multi_union<union_type>(index,
            std::declval<U>())))
        requires (allow_external_overlap)
            : union_{generator,
                  [&]() {
                      return make_from_multi_union<union_type>(
                          index, std::forward<U>(arg));
                  }}
            , index_(index) {}

        LEV_HIDE_INSTANTIATION inline constexpr container(
            container const&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container(
            container const&) noexcept
        requires ((... && std::is_copy_constructible_v<Ts>) &&
                     (... && std::is_trivially_copy_constructible_v<Ts>))
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr container(container&&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container(container&&) noexcept
        requires ((... && std::is_move_constructible_v<Ts>) &&
                     (... && std::is_trivially_move_constructible_v<Ts>))
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(
            container const&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(
            container const&) noexcept
        requires ((... && std::is_copy_assignable_v<Ts>) &&
                     (... && std::is_trivially_copy_assignable_v<Ts>))
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(
            container&&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(
            container&&) noexcept
        requires ((... && std::is_move_assignable_v<Ts>) &&
                     (... && std::is_trivially_move_assignable_v<Ts>))
        = default;

        LEV_HIDE_INSTANTIATION inline constexpr ~container() noexcept
        requires ((... && std::is_trivially_destructible_v<Ts>))
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr ~container() noexcept
        requires ((... || !std::is_trivially_destructible_v<Ts>))
        {
            destroy_member();
        }

        LEV_HIDE_INSTANTIATION inline constexpr void destroy_union() noexcept
        requires (allow_external_overlap &&
            (... && std::is_trivially_destructible_v<Ts>))
        {
            std::destroy_at(std::addressof(union_.data));
        }

        LEV_HIDE_INSTANTIATION inline constexpr void destroy_union() noexcept
        requires (allow_external_overlap &&
            (... || !std::is_trivially_destructible_v<Ts>))
        {
            destroy_member();
            std::destroy_at(std::addressof(union_.data));
        }

        template <typename U, typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr template_element_t<I,
            union_type>*
        construct_union(std::in_place_type_t<U> tag, Args&&... args) noexcept(
            std::is_nothrow_constructible_v<U, Args...>)
        requires (allow_external_overlap)
        {
            static_assert(
                template_index_v<U, union_type> < template_size_v<union_type>);
            auto ptr = std::construct_at(
                std::addressof(union_.data), tag, std::forward<Args>(args)...);
            index_ = template_index_v<U, union_type>;
            return ptr;
        }

        template <size_t I, typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr template_element_t<I,
            union_type>*
        construct_union(std::in_place_index_t<I> tag, Args&&... args) noexcept(
            std::is_nothrow_constructible_v<template_element_t<I, union_type>,
                Args...>)
        requires (allow_external_overlap)
        {
            static_assert(I < template_size_v<union_type>);
            auto ptr = std::construct_at(
                std::addressof(union_.data), tag, std::forward<Args>(args)...);
            index_ = I;
            return ptr;
        }

        [[LEV_MSVC no_unique_address]] conditionally_overlapable<
            place_index_in_tail, union_type>
            union_;
        [[LEV_MSVC no_unique_address]] index_type index_;

    private:
        LEV_HIDE_INSTANTIATION inline constexpr void destroy_member() noexcept {
            jump_table_for<union_type>(
                []<size_t I>(size_constant<I>) {
                    std::destroy_at(
                        std::addressof(union_.data.template get<I>()));
                },
                static_cast<size_t>(index_));
        }
    };

    template <typename U>
    LEV_HIDE_INSTANTIATION static inline constexpr container make_container(
        size_t index,
        U&& arg) noexcept([]<size_t... Is>(std::index_sequence<Is...>) {
        return (... &&
            std::is_nothrow_constructible_v<container,
                std::in_place_index_t<Is>,
                copy_cvref_t<U,
                    template_element_t<Is, std::remove_cvref_t<U>>>>);
    }(std::make_index_sequence<template_size_v<T>>{}))
    requires (place_index_in_tail)
    {
        static_assert(template_size_v<union_type> == template_size_v<U>);
        return jump_table_for<union_type>(
            [&]<size_t I>(size_constant<I>) {
                return container{std::in_place_index<I>,
                    std::forward<U>(arg).template get<I>()};
            },
            index);
    }

public:
    LEV_HIDE_INSTANTIATION inline constexpr ~variant_base() noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr variant_base(
        variant_base const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr variant_base(
        variant_base const&) noexcept((... &&
        std::is_nothrow_copy_constructible_v<Ts>))
    requires ((... && std::is_copy_constructible_v<Ts>) &&
                 (... && std::is_trivially_copy_constructible_v<Ts>))
    = default;

    LEV_HIDE_INSTANTIATION inline constexpr variant_base(
        variant_base const& other) noexcept((... &&
        std::is_nothrow_copy_constructible_v<Ts>))
    requires ((... && std::is_copy_constructible_v<Ts>) &&
        !(... && std::is_trivially_copy_constructible_v<Ts>))
        : variant_base(variant_dispatch, other.index(), other.union_ref()) {}

    LEV_HIDE_INSTANTIATION inline constexpr variant_base(
        variant_base&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr variant_base(
        variant_base&&) noexcept
    requires ((... && std::is_move_constructible_v<Ts>) &&
                 (... && std::is_trivially_move_constructible_v<Ts>))
    = default;

    LEV_HIDE_INSTANTIATION inline constexpr variant_base(
        variant_base&& other) noexcept((... &&
        std::is_nothrow_move_constructible_v<Ts>))
    requires ((... && std::is_move_constructible_v<Ts>) &&
        !(... && std::is_trivially_move_constructible_v<Ts>))
        : variant_base(
              variant_dispatch, other.index(), std::move(other.union_ref())) {}

    LEV_HIDE_INSTANTIATION inline constexpr variant_base& operator=(
        variant_base const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr variant_base& operator=(
        variant_base const&) noexcept
    requires ((... && std::is_copy_assignable_v<Ts>) &&
                 (... && std::is_trivially_copy_assignable_v<Ts>))
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr variant_base&
    operator=(variant_base const& other) noexcept(
        (... && std::is_nothrow_copy_assignable_v<Ts>)&&(
            ... && std::is_nothrow_copy_constructible_v<Ts>))
    requires ((... && std::is_copy_assignable_v<Ts>) &&
        !(... && std::is_trivially_copy_assignable_v<Ts>))
    {
        if (this->index() != other.index()) {
            jump_table_for<union_type>(
                [&]<size_t I>(size_constant<I>) {
                    this->reinitialize_value<I>(other.template value_ref<I>());
                },
                other.index());
        } else {
            jump_table_for<union_type>(
                [&]<size_t I>(size_constant<I>) {
                    this->template value_ref<I>() =
                        other.template value_ref<I>();
                },
                other.index());
        }

        return *this;
    }

    LEV_HIDE_INSTANTIATION inline constexpr variant_base& operator=(
        variant_base&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr variant_base& operator=(
        variant_base&&) noexcept
    requires ((... && std::is_move_assignable_v<Ts>) &&
                 (... && std::is_trivially_move_assignable_v<Ts>))
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr variant_base&
    operator=(variant_base&& other) noexcept(
        (... && std::is_nothrow_move_assignable_v<Ts>)&&(
            ... && std::is_nothrow_move_constructible_v<Ts>))
    requires ((... && std::is_move_assignable_v<Ts>) &&
        !(... && std::is_trivially_move_assignable_v<Ts>))
    {
        if (this->index() != other.index()) {
            jump_table_for<union_type>(
                [&]<size_t I>(size_constant<I>) {
                    this->reinitialize_value<I>(
                        std::move(other).template value_ref<I>());
                },
                other.index());
        } else {
            jump_table_for<union_type>(
                [&]<size_t I>(size_constant<I>) {
                    this->template value_ref<I>() =
                        std::move(other).template value_ref<I>();
                },
                other.index());
        }

        return *this;
    }

protected:
    template <typename U, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit variant_base(
        std::in_place_type_t<U> tag,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<container,
        std::in_place_type_t<U>, Args...>)
        : container_{std::in_place, tag, std::forward<Args>(args)...} {}

    template <size_t I, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit variant_base(
        std::in_place_index_t<I> tag,
        Args&&... args) noexcept(is_nothrow_constructible_v<container,
        std::in_place_index_t<I>, Args...>)
        : container_{std::in_place, tag, std::forward<Args>(args)...} {}

    template <typename U, typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit variant_base(
        generator_type_t<U> tag, F&& callable,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<container,
        generator_type_t<U>, Args...>)
        : container_{std::in_place, tag, std::forward<F>(callable),
              std::forward<Args>(args)...} {}

    template <size_t I, typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit variant_base(
        generator_index_t<I> tag, F&& callable,
        Args&&... args) noexcept(is_nothrow_constructible_v<container,
        generator_index_t<I>, F, Args...>)
        : container_{std::in_place, tag, std::forward<F>(callable),
              std::forward<Args>(args)...} {}

    LEV_HIDE_INSTANTIATION inline constexpr size_t index() const noexcept {
        return container_.data.index_;
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard, gnu::always_inline]] inline constexpr decltype(auto)
    union_ref() const& noexcept {
        return container_.data;
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard, gnu::always_inline]] inline constexpr decltype(auto)
    union_ref() & noexcept {
        return container_.data;
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard, gnu::always_inline]] inline constexpr decltype(auto)
    union_ref() const&& noexcept {
        return std::move(container_.data);
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard, gnu::always_inline]] inline constexpr decltype(auto)
    union_ref() && noexcept {
        return std::move(container_.data);
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        [[nodiscard, gnu::always_inline]] inline constexpr decltype(auto)
        value_ref() const& noexcept {
        static_assert(I < template_size_v<union_type>);
        return union_ref().template get<I>();
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        [[nodiscard, gnu::always_inline]] inline constexpr decltype(auto)
        value_ref() & noexcept {
        static_assert(I < template_size_v<union_type>);
        return union_ref().template get<I>();
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        [[nodiscard, gnu::always_inline]] inline constexpr decltype(auto)
        value_ref() const&& noexcept {
        static_assert(I < template_size_v<union_type>);
        return std::move(union_ref().template get<I>());
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        [[nodiscard, gnu::always_inline]] inline constexpr decltype(auto)
        value_ref() && noexcept {
        static_assert(I < template_size_v<union_type>);
        return std::move(union_ref().template get<I>());
    }

    template <size_t I, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr auto
    reinitialize_value(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<template_element_t<I, union_type>,
            Args...>) {
        using target_type = template_element_t<I, union_type>;

        destroy();
        if constexpr (std::is_nothrow_constructible_v<target_type, Args...>) {
            return construct_value<I>(std::forward<Args>(args)...);
        } else LEV_TRY {
            return construct_value<I>(std::forward<Args>(args)...);
        } LEV_CATCH(...) {
            construct_value<sizeof...(Ts)>();
            UTL_RETHROW();
        }
    }

    template <size_t I, typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr auto
    regenerate_value(F&& callable, Args&&... args) noexcept(
        std::is_nothrow_invocable_r_v<template_element_t<I, union_type>, F,
            Args...>) {
        using target_type = template_element_t<I, union_type>;

        destroy();
        if constexpr (std::is_nothrow_invocable_r_v<
                          template_element_t<I, union_type>, F, Args...>) {
            return generate_value<I>(
                std::forward<F>(callable), std::forward<Args>(args)...);
        } else LEV_TRY {
            return generate_value<I>(
                std::forward<F>(callable), std::forward<Args>(args)...);
        } LEV_CATCH(...) {
            construct_value<sizeof...(Ts)>();
            UTL_RETHROW();
        }
    }

private:
    template <typename U>
    LEV_HIDE_INSTANTIATION inline constexpr variant_base(variant_dispatch_t tag,
        size_t index,
        U&& arg) noexcept(std::is_nothrow_constructible_v<container,
        variant_dispatch_t, size_t, U>)
    requires (allow_external_overlap)
        : container_{std::in_place, tag, index, std::forward<U>(arg)} {}

    template <typename U>
    LEV_HIDE_INSTANTIATION inline constexpr variant_base(variant_dispatch_t,
        size_t index,
        U&& arg) noexcept(noexcept(make_container(index, std::declval<U>())))
    requires (place_index_in_tail)
        : container_{generator,
              [&]() { return make_container(index, std::forward<U>(arg)); }} {}

    template <size_t I, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr auto
    construct_value(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<template_element_t<I, union_type>,
            Args...>) {
        if constexpr (place_index_in_tail) {
            std::construct_at(std::addressof(container_.data),
                std::in_place_index<I>, std::forward<Args>(args)...);
            return std::addressof(value_ref<I>());
        } else {
            return container_.data.construct_union(
                std::in_place_index<I>, std::forward<Args>(args)...);
        }
    }

    template <size_t I, typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr auto
    generate_value(F&& callable, Args&&... args) noexcept(
        std::is_nothrow_invocable_r_v<template_element_t<I, union_type>, F,
            Args...>) {
        if constexpr (place_index_in_tail) {
            std::construct_at(std::addressof(container_.data),
                generator_index<I>, std::forward<F>(callable),
                std::forward<Args>(args)...);
            return std::addressof(value_ref<I>());
        } else {
            return container_.data.construct_union(generator_index<I>,
                std::forward<F>(callable), std::forward<Args>(args)...);
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr void destroy() noexcept {
        if constexpr (place_index_in_tail) {
            std::destroy_at(std::addressof(container_.data));
        } else {
            container_.data.destroy_union();
        }
    }

    [[LEV_MSVC no_unique_address]] conditionally_overlapable<
        allow_external_overlap, container>
        container_;
};

template <typename T>
LEV_HIDDEN inline constexpr bool is_tag_type_v = false;
template <typename T>
LEV_HIDDEN inline constexpr bool is_tag_type_v<T const> = is_tag_type_v<T>;
template <typename T>
LEV_HIDDEN inline constexpr bool is_tag_type_v<T const volatile> =
    is_tag_type_v<T>;
template <typename T>
LEV_HIDDEN inline constexpr bool is_tag_type_v<T volatile> = is_tag_type_v<T>;

template <typename T>
LEV_HIDDEN inline constexpr bool is_tag_type_v<std::in_place_t> = true;
template <typename T>
LEV_HIDDEN inline constexpr bool is_tag_type_v<std::in_place_type_t<T>> = true;
template <size_t I>
LEV_HIDDEN inline constexpr bool is_tag_type_v<std::in_place_index_t<I>> = true;
template <typename T>
LEV_HIDDEN inline constexpr bool is_tag_type_v<generator_t> = true;
template <typename T>
LEV_HIDDEN inline constexpr bool is_tag_type_v<generator_type_t<T>> = true;
template <size_t I>
LEV_HIDDEN inline constexpr bool is_tag_type_v<generator_index_t<I>> = true;

template <typename Arg, typename List>
struct resolve_constructor;

template <typename Arg, typename T>
struct resolve_identity {
    using array_type = T[1];
    LEV_HIDE_INSTANTIATION T operator()(T) noexcept = delete;
    LEV_HIDE_INSTANTIATION T operator()(T) noexcept
    requires requires(Arg&& arg) { array_type{std::forward<Arg>(arg)}; };
};

template <typename Arg, template <typename...> List, typename... Ts>
struct resolve_constructor<Arg, List<Ts...>> : resolve_identity<Arg, Ts>... {
    using resolve_identity<Arg, Ts>::operator()...;
};

template <typename T, typename List>
using resolve_conversion_t =
    std::invoke_result_t<resolve_constructor<T, List>, T>;

[[gnu::noinline, noreturn]] void throw_bad_variant_access() {
    throw bad_variant_access();
}

template <typename... Ts>
[[nodiscard, gnu::always_inline]] auto&& as_variant(
    variant<Ts...>& value) noexcept {
    return value;
}

template <typename... Ts>
[[nodiscard, gnu::always_inline]] auto&& as_variant(
    variant<Ts...> const& value) noexcept {
    return value;
}

template <typename... Ts>
[[nodiscard, gnu::always_inline]] auto&& as_variant(
    variant<Ts...>&& value) noexcept {
    return std::move(value);
}

template <typename... Ts>
[[nodiscard, gnu::always_inline]] auto&& as_variant(
    variant<Ts...> const&& value) noexcept {
    return std::move(value);
}

template <typename V, typename F>
concept visitable_by = requires(F&& callable, V&& value) {
    as_variant(value);
    requires[]<size_t... Is>(std::index_sequence<Is...>) {
        return (... && std::is_invocable_v<F, variant_alternative_t<Is, V>>);
    }
    (std::make_index_sequence<template_size_v<std::remove_reference_t<V>>>);
};

template <typename V, typename R, typename F>
concept visitable_by_r = requires(F&& callable, V&& value) {
    as_variant(value);
    requires[]<size_t... Is>(std::index_sequence<Is...>) {
        return (
            ... && std::is_invocable_r_v<R, F, variant_alternative_t<Is, V>>);
    }
    (std::make_index_sequence<template_size_v<std::remove_reference_t<V>>>);
};

template <typename T>
LEV_HIDE_INSTANTIATION inline constexpr auto visit_table_for =
    []<size_t... Is>(std::index_sequence<Is...>) {
        return jump_table<Is..., variant_npos>{};
    }(std::make_index_sequence<template_size_v<T>>{});

} // namespace details

template <typename... Ts>
class variant : private details::variant_base<Ts...> {
    static_assert(sizeof...(Ts) > 0, "Empty variant");
    static_assert((... && std::is_destructible_v<Ts>), "Invalid type");
    static_assert((... && !std::is_reference_v<Ts>), "Invalid type");
    static_assert((... && !std::is_function_v<Ts>), "Invalid type");
    static_assert((... && !details::is_tag_type_v<Ts>), "Invalid type");
    using first_type = template_element_t<0, variant>;
    using base_type = details::variant_base<Ts...>;
    template <typename... Us>
    friend class variant;

public:
    LEV_HIDE_INSTANTIATION explicit(is_explicit_constructible_v<
        first_type>) inline constexpr variant() noexcept(std::
            is_nothrow_default_constructible<first_type>)
    requires (std::is_default_constructible<first_type>)
        : base_type{std::in_place_index<0>} {}

    LEV_HIDE_INSTANTIATION inline constexpr variant(variant const&
            other) noexcept(std::is_nothrow_copy_constructible_v<base_type>) =
        default;
    LEV_HIDE_INSTANTIATION inline constexpr variant(variant&& other) noexcept(
        std::is_nothrow_move_constructible_v<base_type>) = default;

    template <typename U>
    requires requires { typename details::resolve_conversion_t<U, variant>; }
    LEV_HIDE_INSTANTIATION inline constexpr variant(U&& arg) noexcept(
        std::is_nothrow_constructible_v<
            details::resolve_conversion_t<U, variant>, U>)
        : base_type{
              std::in_place_type<details::resolve_conversion_t<U, variant>>,
              std::forward<U>(arg)} {}

    template <typename U, typename... Args>
    LEV_HIDE_INSTANTIATION explicit inline constexpr variant(
        std::in_place_type_t<U> tag,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<U, Args...>)
        : base_type{tag, std::forward<Args>(args)...} {}

    template <typename U, typename V, typename... Args>
    LEV_HIDE_INSTANTIATION explicit inline constexpr variant(
        std::in_place_type_t<U> tag, std::initializer_list<V> il,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<U,
        std::initializer_list<V>, Args...>)
        : base_type{tag, il, std::forward<Args>(args)...} {}

    template <size_t I, typename... Args>
    requires requires { typename template_element_t<I, variant>; }
    LEV_HIDE_INSTANTIATION explicit inline constexpr variant(
        std::in_place_index_t<I> tag, Args&&... args) noexcept(std::
            is_nothrow_constructible_v<template_element_t<I, variant>, Args...>)
        : base_type{tag, std::forward<Args>(args)...} {}

    template <size_t I, typename U, typename... Args>
    requires requires { typename template_element_t<I, variant>; }
    LEV_HIDE_INSTANTIATION explicit inline constexpr variant(
        std::in_place_index_t<I> tag, std::initializer_list<U> il,
        Args&&... args) noexcept(std::
            is_nothrow_constructible_v<template_element_t<I, variant>,
                std::initializer_list<U>, Args...>)
        : base_type{tag, il, std::forward<Args>(args)...} {}

    template <typename U, typename F, typename... Args>
    requires requires(template_count_v<U, variant> == 1 &&
        std::is_invocable_r_v<U, F, Args...>)
    LEV_HIDE_INSTANTIATION explicit inline constexpr variant(
        generator_type_t<U> tag, F&& callable,
        Args&&... args) noexcept(std::is_nothrow_invocable_r_v<U, F, Args...>)
        : base_type{
              tag, std::forward<F>(callable), std::forward<Args>(args)...} {}

    template <size_t I, typename F, typename... Args>
    requires requires(sizeof...(Ts) > I &&
        std::is_invocable_r_v<template_element_t<I, variant>, F, Args...>)
    LEV_HIDE_INSTANTIATION explicit inline constexpr variant(
        generator_index_t<I> tag, F&& callable, Args&&... args) noexcept(std::
            is_nothrow_invocable_r_v<template_element_t<I, variant>, F,
                Args...>)
        : base_type{
              tag, std::forward<F>(callable), std::forward<Args>(args)...} {}

    template <typename F, typename... Args>
    requires requires {
        typename details::resolve_conversion_t<std::invoke_result_t<F, Args...>,
            variant>;
        requires std::is_invocable_r_v<
            details::resolve_conversion_t<std::invoke_result_t<F, Args...>,
                variant>,
            F, Args...>;
    }
    LEV_HIDE_INSTANTIATION explicit inline constexpr variant(
        generator_t, F&& callable, Args&&... args) noexcept(std::
            is_nothrow_invocable_r_v<
                details::resolve_conversion_t<std::invoke_result_t<F, Args...>,
                    variant>,
                F, Args...>)
        : base_type{generator_type<details::resolve_conversion_t<
                        std::invoke_result_t<F, Args...>, variant>>,
              std::forward<F>(callable), std::forward<Args>(args)...} {}

    LEV_HIDE_INSTANTIATION inline constexpr ~variant() = default;

    LEV_HIDE_INSTANTIATION inline constexpr variant&
    operator=(variant const& other) noexcept(
        std::is_nothrow_copy_assignable_v<base_type>) = default;
    LEV_HIDE_INSTANTIATION inline constexpr variant&
    operator=(variant&& other) noexcept(
        std::is_nothrow_move_assignable_v<base_type>) = default;

    template <typename U>
    requires requires { typename details::resolve_conversion_t<U, variant>; }
    LEV_HIDE_INSTANTIATION inline constexpr variant&
    operator=(U&& arg) noexcept(
        std::is_nothrow_assignable_v<details::resolve_conversion_t<U, variant>&,
            U> &&
        std::is_nothrow_constructible_v<
            details::resolve_conversion_t<U, variant>, U>) {
        using target_type = details::resolve_conversion_t<U, variant>;
        if (index() != template_index_v<target_type, variant>) {
            this->reinitialize_value<template_index_v<target_type, variant>>(
                std::forward<U>(arg));
        } else {
            this->value_ref<template_index_v<target_type, variant>>() =
                std::forward<U>(arg);
        }

        return *this;
    }

    LEV_HIDE_INSTANTIATION inline size_t index() const noexcept {
        auto const idx = base_type::index();
        return idx == sizeof...(Ts) ? variant_npos : idx;
    }

    LEV_HIDE_INSTANTIATION inline bool valueless_by_exception() const noexcept {
        return base_type::index() == sizeof...(Ts);
    }

    template <typename U, typename F, typename... Args>
    requires (template_count_v<U, variant> == 1 &&
        std::is_invocable_r_v<U, F, Args...>)
    LEV_HIDE_INSTANTIATION inline U& generate(F&& callable,
        Args&&... args) noexcept(std::is_nothrow_invocable_r_v<U, F, Args...>) {
        return generate<template_index_v<U, variant>>(
            std::forward<F>(callable), std::forward<Args...>(args));
    }

    template <size_t I, typename F, typename... Args>
    requires requires {
        typename template_element_t<I, variant>;
        requires std::is_invocable_r_v<template_element_t<I, variant>, F,
            Args...>;
    }
    LEV_HIDE_INSTANTIATION inline template_element_t<I, variant>&
    generate(F&& callable, Args&&... args) noexcept(
        std::is_nothrow_invocable_r_v<template_element_t<I, variant>, F,
            Args...>) {
        return base_type::template regenerate_value<I>(
            std::forward<F>(callable), std::forward<Args>(args)...);
    }

    template <typename U, typename... Args>
    requires (template_count_v<U, variant> == 1 &&
        std::is_constructible_v<U, Args...>)
    LEV_HIDE_INSTANTIATION inline U& emplace(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<U, Args...>) {
        return emplace<template_index_v<U, variant>>(
            std::forward<Args...>(args));
    }

    template <size_t I, typename... Args>
    requires requires {
        typename template_element_t<I, variant>;
        requires std::is_constructible_v<template_element_t<I, variant>,
            Args...>;
    }
    LEV_HIDE_INSTANTIATION inline template_element_t<I, variant>&
    emplace(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<template_element_t<I, variant>,
            Args...>) {
        return base_type::template reinitialize_value<I>(
            std::forward<Args>(args)...);
    }

    template <typename U, typename V, typename... Args>
    requires (template_count_v<U, variant> == 1 &&
        std::is_constructible_v<U, std::initializer_list<V>, Args...>)
    LEV_HIDE_INSTANTIATION inline U&
    emplace(std::initializer_list<V> il, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<U, std::initializer_list<V>, Args...>) {
        return emplace<template_index_v<U, variant>>(
            il, std::forward<Args...>(args));
    }

    template <size_t I, typename V, typename... Args>
    requires requires {
        typename template_element_t<I, variant>;
        requires std::is_constructible_v<template_element_t<I, variant>,
            std::initializer_list<V>, Args...>;
    }
    LEV_HIDE_INSTANTIATION inline template_element_t<I, variant>&
    emplace(std::initializer_list<V> il, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<template_element_t<I, variant>,
            std::initializer_list<V>, Args...>) {
        return base_type::template reinitialize_value<I>(
            il, std::forward<Args>(args)...);
    }

    LEV_HIDE_INSTANTIATION inline constexpr void swap(variant& other) noexcept(
        (noexcept(std::declval<variant&>() = exchange(
                      std::declval<variant&>(), std::declval<variant>())) &&
            ... && std::is_nothrow_swappable_v<Ts>)) {
        if (base_type::index() == static_cast<base_type&>(other).index()) {
            jump_table_for<base_type>(
                [&]<size_t I>(size_constant<I>) {
                    std::ranges::swap(
                        this->value_ref<I>(), other.value_ref<I>());
                },
                base_type::index());
        } else {
            other = exchange(*this, std::move(other));
        }
    }

    LEV_HIDE_INSTANTIATION friend inline constexpr void swap(
        variant& left, variant& right) noexcept(noexcept(left.swap(right))) {
        return left.swap(right);
    }

    template <typename T>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr bool
    holds_alternative(variant const&) noexcept {
        static_assert(template_count_v<T, variant> == 1, "Invalid alternative");
        return index() == template_index_v<T, variant>;
    }

    template <size_t I>
    requires requires { typename template_element_t<I, variant>; }
    LEV_HIDE_INSTANTIATION
        [[nodiscard]] friend inline constexpr template_element_t<I,
            variant> const&
        get(variant const& val) {
        if (val.index() != I) {
            details::throw_bad_variant_access();
        }
        return val.template value_ref<I>();
    }

    template <size_t I>
    requires requires { typename template_element_t<I, variant>; }
    LEV_HIDE_INSTANTIATION
        [[nodiscard]] friend inline constexpr template_element_t<I, variant>&
        get(variant& val) {
        if (val.index() != I) {
            details::throw_bad_variant_access();
        }
        return val.template value_ref<I>();
    }

    template <size_t I>
    requires requires { typename template_element_t<I, variant>; }
    LEV_HIDE_INSTANTIATION
        [[nodiscard]] friend inline constexpr template_element_t<I,
            variant> const&&
        get(variant const&& val) {
        if (val.index() != I) {
            details::throw_bad_variant_access();
        }

        return std::move(val.template value_ref<I>());
    }

    template <size_t I>
    requires requires { typename template_element_t<I, variant>; }
    LEV_HIDE_INSTANTIATION
        [[nodiscard]] friend inline constexpr template_element_t<I, variant>&&
        get(variant&& val) {
        if (val.index() != I) {
            details::throw_bad_variant_access();
        }

        return std::move(val.template value_ref<I>());
    }

    template <typename T>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr T const& get(
        variant const& val) {
        static_assert(template_count_v<T, variant> == 1, "Invalid alternative");
        return get<template_index_v<T, variant>>(val);
    }

    template <typename T>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr T& get(
        variant& val) {
        static_assert(template_count_v<T, variant> == 1, "Invalid alternative");
        return get<template_index_v<T, variant>>(val);
    }

    template <typename T>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr T&& get(
        variant&& val) {
        static_assert(template_count_v<T, variant> == 1, "Invalid alternative");
        return get<template_index_v<T, variant>>(std::move(val));
    }

    template <typename T>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr T&& get(
        variant const&& val) {
        static_assert(template_count_v<T, variant> == 1, "Invalid alternative");
        return get<template_index_v<T, variant>>(std::move(val));
    }

    template <size_t I>
    requires requires { typename template_element_t<I, variant>; }
    LEV_HIDE_INSTANTIATION
        [[nodiscard]] friend inline constexpr std::add_pointer_t<
            template_element_t<I, variant> const>
        get_if(variant const* val) noexcept {
        if (val && val->index() == I) {
            return std::addressof(val.template value_ref<I>());
        }

        return nullptr;
    }

    template <size_t I>
    requires requires { typename template_element_t<I, variant>; }
    LEV_HIDE_INSTANTIATION
        [[nodiscard]] friend inline constexpr std::add_pointer_t<
            template_element_t<I, variant>>
        get_if(variant* val) noexcept {
        if (val && val->index() == I) {
            return std::addressof(val.template value_ref<I>());
        }
        return nullptr;
    }

    template <typename T>
    LEV_HIDE_INSTANTIATION
        [[nodiscard]] friend inline constexpr std::add_pointer_t<T const>
        get_if(variant const* val) noexcept {
        static_assert(template_count_v<T, variant> == 1, "Invalid alternative");
        return get_if<template_index_v<T, variant>>(val);
    }

    template <typename T>
    LEV_HIDE_INSTANTIATION
        [[nodiscard]] friend inline constexpr std::add_pointer_t<T>
        get_if(variant* val) noexcept {
        static_assert(template_count_v<T, variant> == 1, "Invalid alternative");
        return get_if<template_index_v<T, variant>>(val);
    }

    template <typename F>
    requires details::visitable_by<variant const&, F>
    LEV_HIDE_INSTANTIATION decltype(auto) visit(F&& callable) const& {
        return details::visit_table_for<variant>(
            [&]<size_t I>(size_constant<I>) -> decltype(auto) {
                if constexpr (I == variant_npos) {
                    return std::invoke(
                        std::forward<F>(callable), get<0>(*this));
                } else {
                    return std::invoke(
                        std::forward<F>(callable), get<I>(*this));
                }
            },
            this->index());
    }

    template <typename F>
    requires details::visitable_by<variant&, F>
    LEV_HIDE_INSTANTIATION decltype(auto) visit(F&& callable) & {
        return details::visit_table_for<variant>(
            [&]<size_t I>(size_constant<I>) -> decltype(auto) {
                if constexpr (I == variant_npos) {
                    return std::invoke(
                        std::forward<F>(callable), get<0>(*this));
                } else {
                    return std::invoke(
                        std::forward<F>(callable), get<I>(*this));
                }
            },
            this->index());
    }

    template <typename F>
    requires details::visitable_by<variant const&&, F>
    LEV_HIDE_INSTANTIATION decltype(auto) visit(F&& callable) const&& {
        return details::visit_table_for<variant>(
            [&]<size_t I>(size_constant<I>) -> decltype(auto) {
                if constexpr (I == variant_npos) {
                    return std::invoke(
                        std::forward<F>(callable), std::move(get<0>(*this)));
                } else {
                    return std::invoke(
                        std::forward<F>(callable), std::move(get<I>(*this)));
                }
            },
            this->index());
    }

    template <typename F>
    requires details::visitable_by<variant&&, F>
    LEV_HIDE_INSTANTIATION decltype(auto) visit(F&& callable) && {
        return details::visit_table_for<variant>(
            [&]<size_t I>(size_constant<I>) -> decltype(auto) {
                if constexpr (I == variant_npos) {
                    return std::invoke(
                        std::forward<F>(callable), std::move(get<0>(*this)));
                } else {
                    return std::invoke(
                        std::forward<F>(callable), std::move(get<I>(*this)));
                }
            },
            this->index());
    }

    template <typename R, typename F>
    requires details::visitable_by_r<variant const&, R, F>
    LEV_HIDE_INSTANTIATION R visit(F&& callable) const& {
        return details::visit_table_for<variant>(
            [&]<size_t I>(size_constant<I>) -> R {
                if constexpr (I == variant_npos) {
                    return std::invoke_r<R>(
                        std::forward<F>(callable), get<0>(*this));
                } else {
                    return std::invoke_r<R>(
                        std::forward<F>(callable), get<I>(*this));
                }
            },
            this->index());
    }

    template <typename R, typename F>
    requires details::visitable_by_r<variant&, R, F>
    LEV_HIDE_INSTANTIATION R visit(F&& callable) & {
        return details::visit_table_for<variant>(
            [&]<size_t I>(size_constant<I>) -> R {
                if constexpr (I == variant_npos) {
                    return std::invoke_r<R>(
                        std::forward<F>(callable), get<0>(*this));
                } else {
                    return std::invoke_r<R>(
                        std::forward<F>(callable), get<I>(*this));
                }
            },
            this->index());
    }

    template <typename R, typename F>
    requires details::visitable_by_r<variant const&&, R, F>
    LEV_HIDE_INSTANTIATION R visit(F&& callable) const&& {
        return details::visit_table_for<variant>(
            [&]<size_t I>(size_constant<I>) -> R {
                if constexpr (I == variant_npos) {
                    return std::invoke_r<R>(
                        std::forward<F>(callable), std::move(get<0>(*this)));
                } else {
                    return std::invoke_r<R>(
                        std::forward<F>(callable), std::move(get<I>(*this)));
                }
            },
            this->index());
    }

    template <typename R, typename F>
    requires details::visitable_by_r<variant&&, R, F>
    LEV_HIDE_INSTANTIATION R visit(F&& callable) && {
        return details::visit_table_for<variant>(
            [&]<size_t I>(size_constant<I>) -> R {
                if constexpr (I == variant_npos) {
                    return std::invoke_r<R>(
                        std::forward<F>(callable), std::move(get<0>(*this)));
                } else {
                    return std::invoke_r<R>(
                        std::forward<F>(callable), std::move(get<I>(*this)));
                }
            },
            this->index());
    }
};

namespace details {
template <typename F, typename V, typename... Vs>
LEV_HIDE_INSTANTIATION inline constexpr decltype(auto) variant_visitor(
    F&& callable, V&& value, Vs&&... values) {
    if constexpr (sizeof...(Vs) == 0) {
        return std::forward<V>(value).visit(std::forward<F>(callable));
    } else {
        using variant_type = std::remove_cvref_t<V>;
        return visit_table_for<variant_type>(
            [&]<size_t I>(size_constant<I>) -> decltype(auto) {
                return variant_visitor(
                    [&](auto&&... others) {
                        return std::invoke(std::forward<F>(callable),
                            get<I>(std::forward<V>(value)),
                            std::forward<decltype(others)>(others)...);
                    },
                    std::forward<Vs>(values)...);
            },
            value.index());
    }
}

template <typename R, typename F, typename V, typename... Vs>
LEV_HIDE_INSTANTIATION inline constexpr R variant_visitor(
    F&& callable, V&& value, Vs&&... values) {

    using variant_type = std::remove_cvref_t<V>;
    if constexpr (sizeof...(Vs) == 0) {
        return std::forward<V>(value).template visit<R>(
            std::forward<F>(callable));
    } else {
        using variant_type = std::remove_cvref_t<V>;
        return visit_table_for<variant_type>(
            [&]<size_t I>(size_constant<I>) -> R {
                return variant_visitor<R>(
                    [](auto&&... others) {
                        return std::invoke(std::forward<F>(callable),
                            get<I>(std::forward<V>(value)),
                            std::forward<decltype(others)>(others)...);
                    },
                    std::forward<Vs>(values)...);
            },
            value.index());
    }
}

} // namespace details

template <typename F, typename... Vs>
requires requires { details::as_variant(std::declval<Vs>()); }
LEV_HIDE_INSTANTIATION inline constexpr decltype(auto) visit(
    F&& callable, Vs&&... values) {
    return details::variant_visitor(
        std::forward<F>(callable), std::forward<Vs>(values)...);
}

template <typename R, typename F, typename... Vs>
requires requires { details::as_variant(std::declval<Vs>()); }
LEV_HIDE_INSTANTIATION inline constexpr R visit(F&& callable, Vs&&... values) {
    return details::variant_visitor<R>(
        std::forward<F>(callable), std::forward<Vs>(values)...);
}
} // namespace lev
