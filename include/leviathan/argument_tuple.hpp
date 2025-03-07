// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/argument_declaration.hpp"
#include "leviathan/bitset.hpp"
#include "leviathan/type_traits.hpp"

#include <array>
#include <variant>

namespace lev {
namespace argument {
template <typename>
LEV_HIDDEN inline constexpr bool is_declaration_v = false;

template <argument_literal auto Name, typename T, typename... Ts>
LEV_HIDDEN inline constexpr bool is_declaration_v<typed<Name, T, Ts...>> = true;

template <typename T, typename... Ts>
LEV_HIDDEN inline constexpr bool is_declaration_v<typed<T, Ts...>> = true;

template <typename>
LEV_HIDDEN inline constexpr bool is_named_v = false;

template <argument_literal auto Name, typename... Ts>
LEV_HIDDEN inline constexpr bool is_named_v<typed<Name, Ts...>> = true;

template <typename>
LEV_HIDDEN inline constexpr bool is_optional_v = false;

template <details::string::pyliteral S, typename... Ts>
LEV_HIDDEN inline constexpr bool
    is_optional_v<typed<details::optional_argument<S>, Ts...>> = true;

template <typename>
LEV_HIDDEN inline constexpr bool is_variant_v = false;

template <auto Name, typename T0, typename T1, typename... Ts>
LEV_HIDDEN inline constexpr bool is_variant_v<typed<Name, T0, T1, Ts...>> =
    true;

template <typename T0, typename T1, typename... Ts>
LEV_HIDDEN inline constexpr bool is_variant_v<typed<T0, T1, Ts...>> = true;

template <typename T>
concept declaration = is_declaration_v<T>;

template <typename T>
concept named_declaration = declaration<T> && is_named_v<T>;

template <typename>
struct type_of {};

template <typename T>
using type_of_t = typename type_of<T>::type;

template <auto Name, typename T>
struct type_of<typed<Name, T>> {
    using type = T;
};

template <typename T>
struct type_of<typed<T>> {
    using type = T;
};

template <auto... Name, typename T0, typename... Ts>
struct type_of<typed<Name..., T0, Ts...>> {
    using type = std::conditional_t<
        std::is_nothrow_default_constructible_v<std::variant<T0, Ts...>>,
        std::variant<T0, Ts...>, std::variant<std::monostate, T0, Ts...>>;
};

template <typename T0, typename... Ts>
struct type_of<typed<T0, Ts...>> {
    using type = std::conditional_t<
        std::is_nothrow_default_constructible_v<std::variant<T0, Ts...>>,
        std::variant<T0, Ts...>, std::variant<std::monostate, T0, Ts...>>;
};

template <typename>
struct name_of {};

template <auto Name, typename... Ts>
struct name_of<typed<Name, Ts...>> {
    static constexpr auto value = Name;
};

template <declaration T>
LEV_HIDDEN inline constexpr auto name_of_v = name_of<T>::value;

template <typename T>
struct stored_type;

template <typename T>
using stored_type_t = typename stored_type<T>::type;

template <declaration T>
requires (!pyobj_type<type_of_t<T>>)
struct stored_type {
    static_assert(
        !std::is_reference_v<type_of_t<T>>, "Reference types cannot be stored");
    static_assert(sizeof(type_of_t<T>), "Incomplete types cannot be stored");
    using type = type_of_t<T>;
};

template <declaration T>
requires (pyobj_type<type_of_t<T>>)
struct stored_type {
    static_assert(!std::is_reference_v<type_of_t<T>>,
        "References to PyObject types cannot be stored");
    using type = std::add_pointer_t<type_of_t<T>>;
};

template <typename>
struct access_type;

template <typename T>
using access_type_t = typename access_type<T>::type;

template <declaration T>
requires (is_optional_v<T> || pyobj_type<T>)
struct access_type : std::add_pointer<T> {};

template <declaration T>
struct access_type<T> {
    using type = T;
};

template <typename...>
class tuple;

namespace details {

using ::lev::details::conditionally_overlapable;
using ::lev::details::fits_in_tail_padding_v;

struct empty_t {
    LEV_HIDDEN inline constexpr operator decltype(nullptr)() const noexcept {
        return nullptr;
    }

    template <typename T>
    LEV_HIDDEN inline constexpr operator T*() const noexcept {
        return nullptr;
    }
};

struct converting_t {
    LEV_HIDDEN explicit inline constexpr converting_t() noexcept = default;
};
LEV_HIDDEN inline constexpr converting_t converting{};

template <typename T>
union data_union {
    LEV_HIDE_INSTANTIATION inline constexpr data_union(
        data_union const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr data_union(
        data_union const&) noexcept(std::is_nothrow_copy_constructible_v<T>)
    requires (std::is_copy_constructible_v<T> &&
                 std::is_trivially_copy_constructible_v<T>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr data_union(data_union&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr data_union(data_union&&) noexcept(
        std::is_nothrow_move_constructible_v<T>)
    requires (std::is_move_constructible_v<T> &&
                 std::is_trivially_move_constructible_v<T>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr data_union& operator=(
        data_union const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr data_union& operator=(
        data_union const&) noexcept(std::is_nothrow_copy_assignable_v<T>)
    requires (std::is_copy_assignable_v<T> &&
                 std::is_trivially_copy_assignable_v<T>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr data_union& operator=(
        data_union&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr data_union& operator=(
        data_union&&) noexcept(std::is_nothrow_move_assignable_v<T>)
    requires (std::is_move_assignable_v<T> &&
                 std::is_trivially_move_assignable_v<T>)
    = default;

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit data_union(std::in_place_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : value{std::forward<Args>(args)...} {}

    LEV_HIDE_INSTANTIATION inline constexpr explicit data_union(
        empty_t) noexcept
        : empty{} {}

    template <typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit data_union(converting_t,
        F&& func,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : value{std::invoke(
              std::forward<F>(func), std::forward<Args>(args)...)} {}

    LEV_HIDE_INSTANTIATION inline constexpr data_union() noexcept = default;

    LEV_HIDE_INSTANTIATION inline constexpr ~data_union() noexcept
    requires (std::is_trivially_destructible_v<T>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr ~data_union() noexcept {}

    [[LEV_MSVC no_unique_address]] empty_t empty;
    [[LEV_MSVC no_unique_address]] T value;
};

template <typename T, typename U>
inline constexpr T make_from_union(bool has_value, U&& arg) noexcept(
    std::is_nothrow_constructible_v<T, std::in_place_t,
        decltype(std::declval<U>().value)>) {
    return has_value ? T{std::in_place, forward_like<U>(arg.value)} : T{};
}

template <size_t N, declaration Head, declaration... Tail>
class storage_base {
    static_assert(N < 64, "Too many arguments");
    using value_type = stored_type_t<Head>;
    using head_type = data_union<value_type>;
    using tail_type =
        conditional_t<sizeof...(Tail), storage_base<N, Tail...>, bitset<N>>;
    static constexpr bool place_remainder_in_tail =
        fits_in_tail_padding_v<head_type, tail_type>;
    static constexpr bool allow_external_overlap = !place_remainder_in_tail;

private:
    struct container {
        template <typename H, typename T>
        LEV_HIDE_INSTANTIATION inline constexpr container(std::in_place_t tag,
            H&& h,
            T&& t) noexcept(std::is_nothrow_constructible_v<value_type, H> &&
            std::is_nothrow_constructible_v<tail_type, T>)
        requires (std::is_constructible_v<value_type, H> &&
                     std::is_constructible_v<tail_type, T>)
            : head_{tag, std::forward<H>(h)}
            , tail_{std::forward<T>(t)} {}

        template <typename T>
        LEV_HIDE_INSTANTIATION inline constexpr container(empty_t tag,
            T&& t) noexcept(std::is_nothrow_constructible_v<tail_type, T>)
        requires (std::is_constructible_v<tail_type, T>)
            : head_{tag}
            , tail_{std::forward<T>(t)} {}

        LEV_HIDE_INSTANTIATION inline constexpr container() noexcept = default;

        LEV_HIDE_INSTANTIATION inline constexpr container(
            container const&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container(
            container const&) noexcept
        requires (std::is_trivially_copy_constructible_v<value_type>)
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr container(container&&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container(container&&) noexcept
        requires (std::is_trivially_move_constructible_v<value_type>)
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(
            container const&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(
            container const&) noexcept
        requires (std::is_trivially_copy_assignable_v<value_type>)
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(
            container&&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(
            container&&) noexcept
        requires (std::is_trivially_move_assignable_v<value_type>)
        = default;

        template <typename H, typename T>
        requires (std::is_constructible_v<value_type, H> &&
                     std::is_constructible_v<tail_type, T> &&
                     allow_external_overlap)
        LEV_HIDE_INSTANTIATION inline constexpr container(converting_t tag,
            bool has_value, H&& h,
            T&& t) noexcept(noexcept(make_from_union<head_type>(has_value,
                                std::declval<H>())) &&
            std::is_nothrow_constructible_v<tail_type, T>)
            : head_{tag,
                  [&]() {
                      return make_from_union<head_type>(
                          has_value, std::forward<H>(h));
                  }}
            , tail_{std::forward<T>(t)} {}

        LEV_HIDE_INSTANTIATION inline constexpr ~container() noexcept
        requires (std::is_trivially_destructible_v<value_type>)
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr ~container() noexcept {
            destroy_member();
        }

        LEV_HIDE_INSTANTIATION inline constexpr void destroy_union() noexcept
        requires (allow_external_overlap)
        {
            if constexpr (!std::is_trivially_destructible_v<value_type>) {
                destroy_member();
            }
            std::destroy_at(std::addressof(head_.data));
        }

        template <typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr void
        construct_union(std::in_place_t tag, Args&&... args) noexcept(
            std::is_nothrow_constructible_v<value_type, Args...>)
        requires (allow_external_overlap)
        {
            std::construct_at(
                std::addressof(head_.data), tag, std::forward<Args>(args)...);
        }

        template <typename F, typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr void
        construct_union(converting_t tag, F&& func, Args&&... args) noexcept(
            std::is_nothrow_constructible_v<value_type, Args...>)
        requires (allow_external_overlap)
        {
            std::construct_at(std::addressof(head_.data), tag,
                std::forward<F>(func), std::forward<Args>(args)...);
        }

        LEV_HIDE_INSTANTIATION inline constexpr void construct_union(
            empty_t tag) noexcept
        requires (allow_external_overlap)
        {
            std::construct_at(std::addressof(head_.data), tag);
        }

        template <size_t I = 0>
        LEV_HIDE_INSTANTIATION [[gnu::flatten, nodiscard]] inline constexpr bool
        has_value() const noexcept {
            static_assert(I <= sizeof...(Tail), "Index out of range");
            if constexpr (sizeof...(Tail) > 0) {
                return tail_.template has_value<I + 1>();
            } else {
                return tail_.test(N - I - 1);
            }
        }

        template <size_t I = 0>
        LEV_HIDE_INSTANTIATION [[gnu::flatten, nodiscard]] inline constexpr void
        set_value() const noexcept {
            static_assert(I <= sizeof...(Tail), "Index out of range");
            if constexpr (sizeof...(Tail) > 0) {
                tail_.template set_value<I + 1>();
            } else {
                tail_.set(N - I - 1);
            }
        }

        template <size_t I = 0>
        LEV_HIDE_INSTANTIATION [[gnu::flatten, nodiscard]] inline constexpr void
        clear_value() const noexcept {
            static_assert(I <= sizeof...(Tail), "Index out of range");
            if constexpr (sizeof...(Tail) > 0) {
                tail_.template clear_value<I + 1>();
            } else {
                tail_.clear(N - I - 1);
            }
        }

        template <size_t Offset, size_t Count = static_cast<size_t>(-1)>
        LEV_HIDE_INSTANTIATION [[gnu::flatten, nodiscard]] inline constexpr auto
        has_values() const noexcept {
            static_assert(Offset < N, "Offset out of range");
            if constexpr (sizeof...(Tail) > 0) {
                return tail_.template has_values<Offset + 1, Count>();
            } else {
                return tail_.subset<Offset, Count>();
            }
        }

        [[LEV_MSVC no_unique_address]] conditionally_overlapable<
            place_remainder_in_tail, head_type>
            head_;
        [[LEV_MSVC no_unique_address]] tail_type tail_;

    private:
        LEV_HIDE_INSTANTIATION inline constexpr void destroy_member() noexcept {
            if (has_value()) {
                std::destroy_at(std::addressof(head_.data.value));
            }
        }
    };

    template <typename H, typename T>
    LEV_HIDE_INSTANTIATION static inline constexpr container make_container(
        bool has_value, H&& h,
        T&& t) noexcept(std::is_nothrow_constructible_v<value_type,
                            decltype(std::declval<H>().value)> &&
        std::is_nothrow_constructible_v<tail_type, T>)
    requires (place_remainder_in_tail)
    {
        return has_value ? container{std::in_place, forward_like<H>(h.value),
                               std::forward<T>(t)}
                         : container{empty_t{}, std::forward<T>(t)};
    }

public:
    LEV_HIDE_INSTANTIATION inline constexpr ~storage_base() noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(
        storage_base const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(
        storage_base const&) noexcept
    requires (std::is_copy_constructible_v<value_type> &&
                 std::is_copy_constructible_v<tail_type> &&
                 std::is_trivially_copy_constructible_v<value_type> &&
                 std::is_trivially_copy_constructible_v<tail_type>)
    = default;

    LEV_HIDE_INSTANTIATION inline constexpr storage_base(storage_base const&
            other) noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
        std::is_nothrow_copy_constructible_v<tail_type>)
    requires (std::is_copy_constructible_v<value_type> &&
        std::is_copy_constructible_v<tail_type> &&
        !(std::is_trivially_copy_constructible_v<value_type> &&
            std::is_trivially_copy_constructible_v<tail_type>))
        : storage_base(converting, other.has_value(), other.head_ref(),
              other.tail_ref()) {}

    LEV_HIDE_INSTANTIATION inline constexpr storage_base(
        storage_base&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(
        storage_base&&) noexcept
    requires (std::is_move_constructible_v<value_type> &&
                 std::is_move_constructible_v<tail_type> &&
                 std::is_trivially_move_constructible_v<value_type> &&
                 std::is_trivially_move_constructible_v<tail_type>)
    = default;

    LEV_HIDE_INSTANTIATION inline constexpr storage_base(storage_base&&
            other) noexcept(std::is_nothrow_move_constructible_v<value_type> &&
        std::is_nothrow_move_constructible_v<tail_type>)
    requires (std::is_move_constructible_v<value_type> &&
        std::is_move_constructible_v<tail_type> &&
        !(std::is_trivially_move_constructible_v<value_type> &&
            std::is_trivially_move_constructible_v<tail_type>))
        : storage_base(converting, other.has_value(),
              std::move(other.head_ref()), std::move(other.tail_ref())) {}

    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(
        storage_base const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(
        storage_base const&) noexcept
    requires (std::is_copy_assignable_v<value_type> &&
                 std::is_copy_assignable_v<tail_type> &&
                 std::is_trivially_copy_assignable_v<value_type> &&
                 std::is_trivially_copy_assignable_v<tail_type>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base&
    operator=(storage_base const& other) noexcept(
        std::is_nothrow_copy_assignable_v<value_type> &&
        std::is_nothrow_copy_assignable_v<tail_type>)
    requires (std::is_copy_assignable_v<value_type> &&
        std::is_copy_assignable_v<tail_type> &&
        !(std::is_trivially_copy_assignable_v<value_type> &&
            std::is_trivially_copy_assignable_v<tail_type>))
    {
        if (this->has_value() != other.has_value()) {
            if (other.has_value()) {
                this->reinitialize_as_value(other.value_ref());
            } else {
                this->reinitialize_as_empty();
            }
        } else if (other.has_value()) {
            this->value_ref() = other.value_ref();
        }
        this->tail_ref() = other.tail_ref();

        return *this;
    }

    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(
        storage_base&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(
        storage_base&&) noexcept
    requires (std::is_move_assignable_v<value_type> &&
                 std::is_move_assignable_v<tail_type> &&
                 std::is_trivially_move_assignable_v<value_type> &&
                 std::is_trivially_move_assignable_v<tail_type>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base&
    operator=(storage_base&& other) noexcept(
        is_nothrow_move_assignable_v<value_type> &&
        is_nothrow_move_assignable_v<tail_type>)
    requires (std::is_move_assignable_v<value_type> &&
        std::is_move_assignable_v<tail_type> &&
        !(std::is_trivially_move_assignable_v<value_type> &&
            std::is_trivially_move_assignable_v<tail_type>))
    {
        if (this->has_value() != other.has_value()) {
            if (other.has_value()) {
                this->reinitialize_as_value(std::move(other.value_ref()));
            } else {
                this->reinitialize_as_empty();
            }
        } else if (other.has_value()) {
            this->value_ref() = std::move(other.value_ref());
        }
        this->tail_ref() = std::move(other.tail_ref());

        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr head_type const&
    head_ref() const& noexcept {
        return container_.data.head_.data;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr head_type&
    head_ref() & noexcept {
        return container_.data.head_.data;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr head_type const&&
    head_ref() const&& noexcept {
        return std::move(container_.data.head_.data);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr head_type&&
    head_ref() && noexcept {
        return std::move(container_.data.head_.data);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr value_type const*
    value_ptr() const noexcept {
        return std::addressof(container_.data.head_.data.value);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr value_type*
    value_ptr() noexcept {
        return std::addressof(container_.data.head_.data.value);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr value_type const&
    value_ref() const& noexcept {
        return container_.data.head_.data.value;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr value_type&
    value_ref() & noexcept {
        return container_.data.head_.data.value;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr value_type const&&
    value_ref() const&& noexcept {
        return std::move(container_.data.head_.data.value);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr value_type&&
    value_ref() && noexcept {
        return std::move(container_.data.head_.data.value);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr tail_type const&
    tail_ref() const& noexcept {
        return container_.data.tail_;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr tail_type&
    tail_ref() & noexcept {
        return container_.data.tail_;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr tail_type const&&
    tail_ref() const&& noexcept {
        return std::move(container_.data.tail_);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr tail_type&&
    tail_ref() && noexcept {
        return std::move(container_.data.tail_);
    }

    template <size_t I = 0>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr bool
    has_value() const noexcept {
        return container_.data.template has_value<I>();
    }

    template <size_t Offset, size_t Count = static_cast<size_t>(-1)>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    has_value() const noexcept {
        return container_.data.template has_values<Offset, Count>();
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        [[nodiscard, gnu::flatten]] inline constexpr decltype(auto)
        get() const& noexcept {
        if constexpr (I == 0) {
            if constexpr (is_optional_v<Head>) {
                return has_value() ? head_ref().value : head_ref().empty;
            } else {
                return value_ref();
            }
        } else {
            return tail_ref().template get<I - 1>();
        }
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        [[nodiscard, gnu::flatten]] inline constexpr decltype(auto)
        get() & noexcept {
        if constexpr (I == 0) {
            if constexpr (is_optional_v<Head>) {
                return has_value() ? head_ref().value : head_ref().empty;
            } else {
                return head_ref().value;
            }
        } else {
            return tail_ref().template get<I - 1>();
        }
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        [[nodiscard, gnu::flatten]] inline constexpr decltype(auto)
        get() const&& noexcept {
        if constexpr (I == 0) {
            if constexpr (is_optional_v<Head>) {
                return has_value() ? head_ref().value : head_ref().empty;
            } else {
                return std::move(value_ref());
            }
        } else {
            return tail_ref().template get<I - 1>();
        }
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        [[nodiscard, gnu::flatten]] inline constexpr decltype(auto)
        get() && noexcept {
        if constexpr (I == 0) {
            if constexpr (is_optional_v<Head>) {
                return has_value() ? head_ref().value : head_ref().empty;
            } else {
                return std::move(value_ref());
            }
        } else {
            return tail_ref().template get<I - 1>();
        }
    }

    template <size_t I, typename F, typename... Args>
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::flatten]] inline constexpr void
    generate_at(F&& func, Args&&... args) noexcept(
        std::is_nothrow_invocable_v<F, Args...>) {
        if constexpr (I == 0) {
            LEV_ASSERT(!this->has_value());
            container_.data.construct_union(
                converting, std::forward<F>(func), std::forward<Args>(args)...);
        } else {
            tail_ref().template generate_at<I - 1>(
                std::forward<F>(func), std::forward<Args>(args)...);
        }
    }

    template <size_t I, typename... Args>
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::flatten]] inline constexpr void
    emplace_at(Args&&... args) noexcept(std::is_nothrow_constructible_v<
        stored_type_t<template_element_t<I, typelist<Head, Tail...>>>,
        Args...>) {
        if constexpr (I == 0) {
            LEV_ASSERT(!this->has_value());
            container_.data.construct_union(
                std::in_place, std::forward<Args>(args)...);
        } else {
            tail_ref().template emplace_at<I - 1>(std::forward<Args>(args)...);
        }
    }

private:
    template <typename H, typename T>
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(converting_t tag,
        bool val, H&& h,
        T&& t) noexcept(std::is_nothrow_constructible_v<container, converting_t,
        bool, H, T>)
    requires (allow_external_overlap)
        : container_{std::in_place, tag, val, std::forward<H>(h),
              std::forward<T>(t)} {}

    template <typename H, typename T>
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(converting_t tag,
        bool val, H&& h, T&& t) noexcept(noexcept(make_container(val,
        std::declval<H>(), std::declval<T>())))
    requires (place_remainder_in_tail)
        : container_{tag, [&]() {
                         return make_container(
                             val, std::forward<H>(h), std::forward<T>(t));
                     }} {}

    LEV_HIDE_INSTANTIATION inline constexpr void destroy() noexcept {
        container_.data.destroy_union();
    }

    template <size_t I = 0>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr void
    set_value() const noexcept {
        container_.data.template set_value<I>();
    }

    template <size_t I = 0>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr void
    clear_value() const noexcept {
        container_.data.template clear_value<I>();
    }

    LEV_HIDE_INSTANTIATION inline constexpr empty_t*
    construct_empty() noexcept {
        clear_value();
        container_.data.construct_union(empty_t{});
        return std::addressof(head_ref().empty);
    }

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr value_type*
    construct_head(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<value_type, Args...>) {
        container_.data.construct_union(
            std::in_place, std::forward<Args>(args)...);
        set_value();
        return value_ptr();
    }

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr T*
    reinitialize_as_value(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<value_type, Args...>) {
        LEV_ASSERT(!has_value());
        destroy();
        if constexpr (std::is_nothrow_constructible_v<value_type, Args...>) {
            return construct_head(std::forward<Args>(args)...);
        } else LEV_TRY {
            return construct_head(std::forward<Args>(args)...);
        } LEV_CATCH(...) {
            construct_empty();
            LEV_RETHROW();
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr E*
    reinitialize_as_empty() noexcept {
        LEV_ASSERT(has_value());
        destroy();
        return construct_empty();
    }

    [[LEV_MSVC no_unique_address]] conditionally_overlapable<
        allow_external_overlap, container>
        container_;
};

} // namespace details

template <declaration... Ts>
class tuple<Ts...> : private details::storage_base<sizeof...(Ts), Ts...> {
    static_assert(
        (... && named_declaration<Ts>) || (... && !named_declaration<Ts>),
        "All arguments must be named, or all must be unnamed");
    static_assert((... && named_declaration<Ts>)&&sizeof...(Ts) < 2,
        "Keywords are unnecessary when there is less than 2 arguments");
    static_assert(
        []() {
            std::array result{is_optional_v<Ts>...};
            return std::ranges::all_of(
                std::ranges::find(result, true), result.end());
        }(),
        "All optional arguments must be at the tail end of the argument "
        "list");

    using base_type = details::storage_base<sizeof...(Ts), Ts...>;

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr decltype(auto)
    get(tuple& t) noexcept LEV_LIFETIMEBOUND {
        static_assert(I < sizeof...(Ts), "Index out of range");
        return t.template get<I>();
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr decltype(auto)
    get(tuple const& t) noexcept LEV_LIFETIMEBOUND {
        static_assert(I < sizeof...(Ts), "Index out of range");
        return t.template get<I>();
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr decltype(auto)
    get(tuple&& t) noexcept LEV_LIFETIMEBOUND {
        static_assert(I < sizeof...(Ts), "Index out of range");
        return std::move(t.template get<I>());
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr decltype(auto)
    get(tuple const&& t) noexcept LEV_LIFETIMEBOUND {
        static_assert(I < sizeof...(Ts), "Index out of range");
        return std::move(t.template get<I>());
    }

    static consteval size_t name_to_index(string_literal auto name) noexcept {
        std::array str{static_cast<std::string_view>(name_of_v<Ts>)...};
        return std::ranges::distance(str.begin(),
            std::ranges::find(str, static_cast<std::string_view>(name)));
    }

    template <string_literal auto Name>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr decltype(auto)
    get(tuple const& t) noexcept LEV_LIFETIMEBOUND
    requires ((... && named_declaration<Ts>))
    {
        static_assert(
            name_to_index(Name) < sizeof...(Ts), "Invalid argument name");
        return get<name_to_index(Name)>(t);
    }

    template <string_literal auto Name>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr decltype(auto)
    get(tuple& t) noexcept LEV_LIFETIMEBOUND
    requires ((... && named_declaration<Ts>))
    {
        static_assert(
            name_to_index(Name) < sizeof...(Ts), "Invalid argument name");
        return get<name_to_index(Name)>(t);
    }

    template <string_literal auto Name>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr decltype(auto)
    get(tuple&& t) noexcept LEV_LIFETIMEBOUND
    requires ((... && named_declaration<Ts>))
    {
        static_assert(
            name_to_index(Name) < sizeof...(Ts), "Invalid argument name");
        return get<name_to_index(Name)>(std::move(t));
    }

    template <string_literal auto Name>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr decltype(auto)
    get(tuple const&& t) noexcept LEV_LIFETIMEBOUND
    requires ((... && named_declaration<Ts>))
    {
        static_assert(
            name_to_index(Name) < sizeof...(Ts), "Invalid argument name");
        return get<name_to_index(Name)>(std::move(t));
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr auto has_value(
        tuple const& t) noexcept {
        static_assert(I < sizeof...(Ts), "Index out of range");
        return t.template has_value<I>();
    }

    template <size_t O, size_t Count = static_cast<size_t>(-1)>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend inline constexpr auto
    has_values(tuple const& t) noexcept {
        static_assert(I < sizeof...(Ts), "Index out of range");
        return t.template has_values<O, Count>();
    }

    template <size_t I, typename... Args>
    LEV_HIDE_INSTANTIATION friend inline constexpr void emplace_at(
        tuple& t, Args&&... args) noexcept {
        t.template emplace_at<I>(std::forward<Args>(args)...);
    }

    static consteval size_t optional_offset() noexcept {
        std::array opt{is_optional_v<Ts>};
        return std::ranges::distance(opt.begin(), std::ranges::find(opt, true));
    }

public:
    LEV_HIDE_INSTANTIATION inline constexpr tuple() noexcept = default;

    LEV_HIDE_INSTANTIATION inline constexpr tuple(tuple const&) noexcept(
        std::is_nothrow_copy_constructible_v<base_type>) = default;
    LEV_HIDE_INSTANTIATION inline constexpr tuple(tuple&&) noexcept(
        std::is_nothrow_move_constructible_v<base_type>) = default;
    LEV_HIDE_INSTANTIATION inline constexpr tuple&
    operator=(tuple const&) noexcept(
        std::is_nothrow_copy_assignable_v<base_type>) = default;
    LEV_HIDE_INSTANTIATION inline constexpr tuple& operator=(tuple&&) noexcept(
        std::is_nothrow_move_assignable_v<base_type>) = default;

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    mandatory() const& noexcept {
        return []<size_t... Is>(std::index_sequence<Is...>) {
            using return_type = std::tuple<decltype(get<Is>(*this))...>;
            return return_type{get<Is>(*this)...};
        }(std::make_index_sequence<optional_offset()>{});
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    optional() const& noexcept LEV_LIFETIMEBOUND {
        return []<size_t... Is>(std::index_sequence<Is...>) {
            using return_type =
                std::tuple<decltype(get<Is + optional_offset()>(*this))...>;
            return return_type{get<Is + optional_offset()>(*this)...};
        }(std::make_index_sequence<sizeof...(Ts) - optional_offset()>{});
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    all() const& noexcept LEV_LIFETIMEBOUND {
        return []<size_t... Is>(std::index_sequence<Is...>) {
            using return_type = std::tuple<decltype(get<Is>(*this))...>;
            return return_type{get<Is>(*this)...};
        }(std::make_index_sequence<sizeof...(Ts)>{});
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    mandatory() & noexcept LEV_LIFETIMEBOUND {
        return []<size_t... Is>(std::index_sequence<Is...>) {
            using return_type = std::tuple<decltype(get<Is>(*this))...>;
            return return_type{get<Is>(*this)...};
        }(std::make_index_sequence<optional_offset()>{});
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    optional() & noexcept LEV_LIFETIMEBOUND {
        return []<size_t... Is>(std::index_sequence<Is...>) {
            using return_type =
                std::tuple<decltype(get<Is + optional_offset()>(*this))...>;
            return return_type{get<Is + optional_offset()>(*this)...};
        }(std::make_index_sequence<sizeof...(Ts) - optional_offset()>{});
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    all() & noexcept LEV_LIFETIMEBOUND {
        return []<size_t... Is>(std::index_sequence<Is...>) {
            using return_type = std::tuple<decltype(get<Is>(*this))...>;
            return return_type{get<Is>(*this)...};
        }(std::make_index_sequence<sizeof...(Ts)>{});
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    mandatory() && noexcept LEV_LIFETIMEBOUND {
        return []<size_t... Is>(std::index_sequence<Is...>) {
            using return_type =
                std::tuple<decltype(get<Is>(std::move(*this)))...>;
            return return_type{get<Is>(std::move(*this))...};
        }(std::make_index_sequence<optional_offset()>{});
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    all() && noexcept LEV_LIFETIMEBOUND {
        return []<size_t... Is>(std::index_sequence<Is...>) {
            using return_type =
                std::tuple<decltype(get<Is>(std::move(*this)))...>;
            return return_type{get<Is>(std::move(*this))...};
        }(std::make_index_sequence<sizeof...(Ts)>{});
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    mandatory() const&& noexcept LEV_LIFETIMEBOUND {
        return []<size_t... Is>(std::index_sequence<Is...>) {
            using return_type =
                std::tuple<decltype(get<Is>(std::move(*this)))...>;
            return return_type{get<Is>(std::move(*this))...};
        }(std::make_index_sequence<optional_offset()>{});
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    all() const&& noexcept LEV_LIFETIMEBOUND {
        return []<size_t... Is>(std::index_sequence<Is...>) {
            using return_type =
                std::tuple<decltype(get<Is>(std::move(*this)))...>;
            return return_type{get<Is>(std::move(*this))...};
        }(std::make_index_sequence<sizeof...(Ts)>{});
    }
};

namespace details {
template <declaration Arg>
using source_type_t = PyObject;

template <declaration Arg>
using target_type_t = stored_type_t<Arg>;

template <size_t Idx, typename To, argument::declaration... Ts,
    typename... Args>
requires requires {
    typename template_element_t<Idx, typelist<Ts...>>;
    requires is_variant_v<template_element_t<Idx, typelist<Ts...>>>;
    requires std::is_constructible_v<template_element_t<Idx, typelist<Ts...>>,
        std::in_place_type<To>, Args...>;
}
LEV_HIDE_INSTANTIATION void
invoke_emplace_at(argument::tuple<Ts...>& tuple, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<template_element_t<Idx, typelist<Ts...>>,
        std::in_place_type_t<To>, Args...>) {
    emplace_at<Idx>(tuple, std::in_place_type<To>, std::forward<Args>(args)...);
}

template <size_t Idx, typename To, argument::declaration... Ts,
    typename... Args>
requires requires {
    typename template_element_t<Idx, typelist<Ts...>>;
    requires !is_variant_v<template_element_t<Idx, typelist<Ts...>>>;
    requires std::is_constructible_v<template_element_t<Idx, typelist<Ts...>>,
        Args...>;
}
LEV_HIDE_INSTANTIATION void
invoke_emplace_at(argument::tuple<Ts...>& tuple, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<template_element_t<Idx, typelist<Ts...>>,
        Args...>) {
    emplace_at<Idx>(tuple, std::forward<Args>(args)...);
}

} // namespace details

template <typename From, typename To>
class converter;

template <named_declaration... Args>
result_code populate(std::span<PyObject* const> args,
    non_owning_ptr<PyDictObject> kwargs, tuple<Args...>& tuple) noexcept {

    auto array = sort(var_args{args, kwargs}, Args::name...);

    if (PyErr_Occurred()) {
        return result_code::failed;
    }

    using sequence_for_args = std::index_sequence_for<Args...>;
    using source_typelist = typelist<details::source_type_t<Args>...>;
    using target_typelist = typelist<details::target_type_t<Args>...>;
    LEV_TRY {
        std::apply(
            [&](auto... obj) {
                [&]<size_t... Is, typename... From, typename... To> {
                    (..., converter<From, To>::try_emplace<Is>(obj, tuple));
                }(sequence_for_args{}, source_typelist{}, target_typelist{});
            },
            array);
    } LEV_CATCH(std::exception const& error) {
        //
    } LEV_CATCH(...) {
        //
    }

    return PyErr_Occurred() ? result_code::failed : result_code::success;
}

template <named_declaration... Args>
result_code populate(std::span<PyObject* const> args,
    std::span<PyObject* const> kwnames, tuple<Args...>& tuple) noexcept {
    auto array = sort(fast_args{args, kwnames}, Args::name...);

    if (PyErr_Occurred()) {
        return result_code::failed;
    }

    using sequence_for_args = std::index_sequence_for<Args...>;
    using source_typelist = typelist<details::source_type_t<Args>...>;
    using target_typelist = typelist<details::target_type_t<Args>...>;
    LEV_TRY {
        std::apply(
            [&](auto... obj) {
                [&]<size_t... Is, typename... From, typename... To> {
                    (..., Converter<From, To>::try_emplace<Is>(obj, tuple));
                }(sequence_for_args{}, source_typelist{}, target_typelist{});
            },
            array);
    } LEV_CATCH(std::exception const& error) {
        //
    } LEV_CATCH(...) {
        //
    }

    return PyErr_Occurred() ? result_code::failed : result_code::success;
}

template <declaration... Args>
result_code populate(
    std::span<PyObject* const> args, tuple<Args...>& tuple) noexcept {
    if (args.size() != sizeof...(Args)) {
        PyErr_Format(PyExc_ValueError,
            "Invocation failed, Reason=[Unexpected argument count], "
            "Expected=[%zu], Received=[%zu]",
            sizeof...(Args), args.size());
        return result_code::failed;
    }

    using sequence_for_args = std::index_sequence_for<Args...>;
    using source_typelist = typelist<details::source_type_t<Args>...>;
    using target_typelist = typelist<details::target_type_t<Args>...>;
    LEV_TRY {
        [&]<size_t... Is, typename... From, typename... To> {
            (..., Converter<From, To>::try_emplace<Is>(args[Is], tuple));
        }(sequence_for_args{}, source_typelist{}, target_typelist{});
    } LEV_CATCH(std::exception const& error) {
        //
    } LEV_CATCH(...) {
        //
    }

    return PyErr_Occurred() ? result_code::failed : result_code::success;
}

template <pyobj_type T>
class converter<T, T> {
public:
    template <size_t Idx, argument::declaration... Ts>
    requires std::same_as<template_element_t<Idx, typelist<Ts...>>, T*>
    static bool try_emplace(T* src, argument::tuple<Ts...>& dst) noexcept {
        details::invoke_emplace_at<Idx, T*>(dst, src);
        return true;
    }
};

template <pyobj_type From, pyobj_type To>
class converter<From, To> {
public:
    template <size_t Idx, argument::declaration... Ts>
    static bool try_emplace(From* src, argument::tuple<Ts...>& dst) noexcept {
        if (auto ptr = dynamic_ptr_cast<To>(src)) {
            details::invoke_emplace_at<Idx, To*>(dst, ptr);
            return true;
        }

        PyErr_Format(PyExc_TypeError,
            "Argument conversion failed, Reason=[Unexpected argument type], "
            "Type=[%s]",
            src ? Py_TYPE(src)->tp_name : "nullptr");
        return false;
    }
};

template <>
class converter<PyObject, std::string_view> {
public:
    template <size_t Idx, argument::declaration... Ts>
    static bool try_emplace(
        argument::tuple<Ts...>& dst, PyObject* src) noexcept {
        if (dynamic_ptr_cast<PyUnicodeObject>(src) == nullptr) {
            PyErr_Format(PyExc_TypeError,
                "Argument conversion failed, Reason=[Unexpected argument "
                "type], "
                "Type=[%s]",
                src ? Py_TYPE(src)->tp_name : "nullptr");
            return false;
        }

        // TODO: How to add more types?

        return converter<PyUnicodeObject, std::string_view>::try_emplace<Idx>(
            dst, src);
    }
};

template <>
class converter<PyObject, char> {
public:
    template <size_t Idx, argument::declaration... Ts>
    static bool try_emplace(
        argument::tuple<Ts...>& dst, PyObject* src) noexcept {
        if (auto ptr = dynamic_ptr_cast<PyLongObject>(src)) {
            return converter<PyLongObject, char>::try_emplace<Idx>(dst, ptr);
        }

        if (auto ptr = dynamic_ptr_cast<PyUnicodeObject>(src)) {
            return converter<PyUnicodeObject, char>::try_emplace<Idx>(dst, ptr);
        }

        PyErr_Format(PyExc_TypeError,
            "Argument conversion failed, Reason=[Unexpected argument type], "
            "Type=[%s]",
            src ? Py_TYPE(src)->tp_name : "nullptr");

        return false;
    }
};

template <>
class converter<PyUnicodeObject, std::string_view> {
public:
    template <size_t Idx, argument::declaration... Ts>
    static bool try_emplace(
        argument::tuple<Ts...>& dst, PyUnicodeObject* src) noexcept {
        Py_ssize_t size = 0;
        auto ptr = PyUnicode_AsUTF8AndSize(as_pyobject(src), &size);
        if (!ptr) {
            return false;
        }

        details::invoke_emplace_at<Idx, std::string_view>(dst, ptr, size);
        return true;
    }
};

template <>
class converter<PyUnicodeObject, char> {
public:
    template <size_t Idx, argument::declaration... Ts>
    static bool try_emplace(
        argument::tuple<Ts...>& dst, PyUnicodeObject* src) noexcept {
        if (auto ptr = PyUnicode_AsUTF8AndSize(as_pyobject(src))) {
            details::invoke_emplace_at<Idx, char>(dst, *ptr, size);
            return true;
        }

        return false;
    }
};

template <>
class converter<PyObject, bool> {
public:
    template <size_t Idx, argument::declaration... Ts>
    static bool try_emplace(
        argument::tuple<Ts...>& dst, PyObject* src) noexcept {
        details::invoke_emplace_at<Idx, bool>(dst, Py_IsTrue(src));
        return true;
    }
};

template <std::integral To>
class converter<PyObject, To> {
public:
    template <size_t Idx, argument::declaration... Ts>
    static bool try_emplace(
        argument::tuple<Ts...>& dst, PyObject* src) noexcept {
        if (auto ptr = dynamic_ptr_cast<PyLongObject>(src)) {
            return converter<PyLongObject, To>::try_emplace<Idx>(dst, ptr);
        }

        PyErr_Format(PyExc_TypeError,
            "Argument conversion failed, Reason=[Unexpected argument type], "
            "Type=[%s]",
            src ? Py_TYPE(src)->tp_name : "nullptr");
        return false;
    }
};

template <std::unsigned_integral To>
class converter<PyLongObject, To> {
public:
    template <size_t Idx, argument::declaration... Ts>
    static bool try_emplace(
        argument::tuple<Ts...>& dst, PyLongObject* src) noexcept {
        static constexpr auto kInvalid = static_cast<unsigned long long>(-1);
        auto value = PyLong_AsUnsignedLongLong(as_pyobject(src));
        if (value == kInvalid && PyErr_Occurred()) [[unlikely]] {
            return false;
        }

        details::invoke_emplace_at<Idx, To>(dst, static_cast<To>(value));
        return true;
    }
};

template <>
class converter<PyLongObject, char> {
public:
    template <size_t Idx, argument::declaration... Ts>
    static bool try_emplace(
        argument::tuple<Ts...>& dst, PyLongObject* src) noexcept {
        static constexpr auto kInvalid = -1L;
        auto value = PyLong_AsLong(as_pyobject(src));
        if (value == kInvalid && PyErr_Occurred()) [[unlikely]] {
            return false;
        }

        details::invoke_emplace_at<Idx, char>(dst, static_cast<To>(value));
        return true;
    }
};

template <std::signed_integral To>
class converter<PyLongObject, To> {
public:
    template <size_t Idx, argument::declaration... Ts>
    static bool try_emplace(
        argument::tuple<Ts...>& dst, PyLongObject* src) noexcept {
        static constexpr auto kInvalid = static_cast<unsigned long long>(-1);
        auto value = PyLong_AsLongLong(as_pyobject(src));
        if (value == kInvalid && PyErr_Occurred()) [[unlikely]] {
            return false;
        }

        details::invoke_emplace_at<Idx, To>(dst, static_cast<To>(value));
        return true;
    }
};

template <std::floating_point To>
class converter<PyObject, To> {
public:
    template <size_t Idx, argument::declaration... Ts>
    static bool try_emplace(
        argument::tuple<Ts...>& dst, PyObject* src) noexcept {
        if (auto ptr = dynamic_ptr_cast<PyFloatObject>(src)) {
            return converter<PyFloatObject, To>::try_emplace<Idx>(dst, ptr);
        }

        // Numpy?

        PyErr_Format(PyExc_TypeError,
            "Argument conversion failed, Reason=[Unexpected argument type], "
            "Type=[%s]",
            src ? Py_TYPE(src)->tp_name : "nullptr");
        return false;
    }
};

template <std::floating_point To>
class converter<PyFloatObject, To> {
public:
    template <size_t Idx, argument::declaration... Ts>
    static bool try_emplace(
        argument::tuple<Ts...>& dst, PyFloatObject* src) noexcept {
        auto value = PyFloat_AsDouble(src);
        if (value == -1.0 && PyErr_Occurred()) [[unlikely]] {
            return false;
        }

        details::invoke_emplace_at<Idx, To>(dst, static_cast<To>(value));
        return true;
    }
};

template <typename... Vs>
class converter<PyObject, std::variant<Vs...>> {
public:
    template <size_t Idx, argument::declaration... Ts>
    requires std::same_as<template_element_t<Idx, typelist<Ts...>>,
        std::variant<Vs...>>
    static bool try_emplace(
        argument::tuple<Ts...>& dst, PyObject* src) noexcept {
        return (... || converter<PyObject, Vs>::try_emplace<Idx>(dst, src));
    }
};

} // namespace argument
} // namespace lev

namespace std {
template <typename... Ts>
struct tuple_size<lev::argument::tuple<Ts...>> :
    size_constant<sizeof...(Ts)> {};
template <size_t I, typename H, typename... Ts>
struct tuple_element<I, lev::argument::tuple<H, Ts...>> :
    tuple_element<I - 1, lev::argument::tuple<H, Ts...>> {};
template <typename H, typename... Ts>
struct tuple_element<0, lev::argument::tuple<H, Ts...>> :
    lev::argument::access_type<H> {};
} // namespace std
