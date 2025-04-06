// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/argument_declaration.hpp"
#include "utils/bitset.hpp"
#include "utils/type_traits.hpp"

#include <array>

namespace lev {
namespace argument {

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

template <optional_declaration T>
struct access_type : std::add_pointer<type_of_t<T>> {};

template <declaration T>
struct access_type : stored_type<T> {};

template <typename...>
class tuple;

namespace details {

using ::lev::details::conditionally_overlapable;
using ::lev::details::fits_in_tail_padding_v;

struct empty_t {
    template <typename T>
    LEV_HIDDEN inline constexpr operator T*() const noexcept {
        return nullptr;
    }
};

template <typename T>
union optional_union {
    static_assert(!std::is_same_v<empty_t>, "Invalid type");
    LEV_HIDE_INSTANTIATION inline constexpr optional_union(
        optional_union const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr optional_union(
        optional_union const&) noexcept(std::is_nothrow_copy_constructible_v<T>)
    requires (std::is_copy_constructible_v<T> &&
                 std::is_trivially_copy_constructible_v<T>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr optional_union(
        optional_union&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr optional_union(
        optional_union&&) noexcept(std::is_nothrow_move_constructible_v<T>)
    requires (std::is_move_constructible_v<T> &&
                 std::is_trivially_move_constructible_v<T>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr optional_union& operator=(
        optional_union const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr optional_union& operator=(
        optional_union const&) noexcept(std::is_nothrow_copy_assignable_v<T>)
    requires (std::is_copy_assignable_v<T> &&
                 std::is_trivially_copy_assignable_v<T>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr optional_union& operator=(
        optional_union&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr optional_union& operator=(
        optional_union&&) noexcept(std::is_nothrow_move_assignable_v<T>)
    requires (std::is_move_assignable_v<T> &&
                 std::is_trivially_move_assignable_v<T>)
    = default;

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit optional_union(
        std::in_place_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : value{std::forward<Args>(args)...} {}

    LEV_HIDE_INSTANTIATION inline constexpr explicit optional_union(
        empty_t) noexcept
        : empty{} {}

    template <typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit optional_union(generator_t,
        F&& func,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : value{std::invoke(
              std::forward<F>(func), std::forward<Args>(args)...)} {}

    LEV_HIDE_INSTANTIATION inline constexpr optional_union() noexcept = default;

    LEV_HIDE_INSTANTIATION inline constexpr ~optional_union() noexcept
    requires (std::is_trivially_destructible_v<T>)
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr ~optional_union() noexcept {}

    [[LEV_MSVC no_unique_address]] empty_t empty;
    [[LEV_MSVC no_unique_address]] T value;
};

template <typename T, typename U>
inline constexpr T make_from_union(bool has_value, U&& arg) noexcept(
    std::is_nothrow_constructible_v<T, std::in_place_t,
        decltype(std::declval<U>().value)>) {
    return has_value ? T{std::in_place, forward_like<U>(arg.value)} : T{};
}

template <declaration... Ts>
class storage_base {
    using storage_type =
        std::tuple<optional_union<stored_type_t<Ts>>..., bitset<sizeof...(Ts)>>;

public:
    LEV_HIDE_INSTANTIATION inline constexpr ~storage_base() noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(
        storage_base const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(
        storage_base const&) noexcept
    requires ((... && std::is_copy_constructible_v<Ts>) &&
                 (... && std::is_trivially_copy_constructible_v<Ts>))
    = default;

    LEV_HIDE_INSTANTIATION inline constexpr storage_base(
        storage_base const& other) noexcept((... &&
        std::is_nothrow_copy_constructible_v<Ts>))
    requires ((... && std::is_copy_constructible_v<Ts>) &&
        !(... && std::is_trivially_copy_constructible_v<Ts>))
        : storage_base(
              generator, [&]<size_t... Is>(std::index_sequence<Is...>) {
                  return std::tuple{
                      make_from_union(other.template has_value<Is>(),
                          other.template value_ref<Is>())...};
              }(std::index_sequence_for<Ts...>{})) {}

    LEV_HIDE_INSTANTIATION inline constexpr storage_base(
        storage_base&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(
        storage_base&&) noexcept
    requires ((... && std::is_move_constructible_v<Ts>) &&
                 (... && std::is_trivially_move_constructible_v<Ts>))
    = default;

    LEV_HIDE_INSTANTIATION inline constexpr storage_base(
        storage_base&& other) noexcept((... &&
        std::is_nothrow_move_constructible_v<Ts>))
    requires ((... && std::is_move_constructible_v<Ts>) &&
        !(... && std::is_trivially_move_constructible_v<Ts>))
        : storage_base(
              generator, [&]<size_t... Is>(std::index_sequence<Is...>) {
                  return std::tuple{
                      make_from_union(other.template has_value<Is>(),
                          std::move(other.template value_ref<Is>()))...};
              }(std::index_sequence_for<Ts...>{})) {}

    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(
        storage_base const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(
        storage_base const&) noexcept
    requires ((... && std::is_copy_assignable_v<Ts>) &&
                 (... && std::is_trivially_copy_assignable_v<Ts>))
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(
        storage_base const& other) noexcept((... &&
        std::is_nothrow_copy_assignable_v<Ts>))
    requires ((... && std::is_copy_assignable_v<Ts>) &&
        !(... && std::is_trivially_copy_assignable_v<Ts>))
    {
        if (this == &other) {
            return *this;
        }

        [&]<size_t... Is>(std::index_sequence<Is...>) {
            (..., []<size_t I>(std::integral_constant<size_t, I>) {
                if (this->template has_value<I>() != other.has_value<I>()) {
                    if (other.template has_value<I>()) {
                        this->template emplace_at<I>(other.template get<I>());
                    } else {
                        this->template reset<I>();
                    }
                } else if (other.has_value<I>()) {
                    this->template get<I>() = other.template get<I>();
                }
            }(std::integral_constant<Is>{}));
        }(std::index_sequence_for<Ts...>{});
        return *this;
    }

    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(
        storage_base&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(
        storage_base&&) noexcept
    requires ((... && std::is_move_assignable_v<Ts>) &&
                 (... && std::is_trivially_move_assignable_v<Ts>))
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(
        storage_base&& other) noexcept((... &&
        std::is_nothrow_move_assignable_v<Ts>))
    requires ((... && std::is_move_assignable_v<Ts>) &&
        !(... && std::is_trivially_move_assignable_v<Ts>))
    {
        [&]<size_t... Is>(std::index_sequence<Is...>) {
            (..., []<size_t I>(std::integral_constant<size_t, I>) {
                if (this->template has_value<I>() != other.has_value<I>()) {
                    if (other.template has_value<I>()) {
                        this->template emplace_at<I>(
                            std::move(other.template get<I>()));
                    } else {
                        this->template reset<I>();
                    }
                } else if (other.has_value<I>()) {
                    this->template get<I>() =
                        std::move(other.template get<I>());
                }
            }(std::integral_constant<Is>{}));
        }(std::index_sequence_for<Ts...>{});
        return *this;
    }

    template <size_t I = 0>
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline constexpr bool
    has_value() const noexcept {
        static_assert(I < sizeof...(Ts) - 1);
        return std::get<sizeof...(Ts) - 1>(storage_).test(I);
    }

    template <size_t Offset, size_t Count = static_cast<size_t>(-1)>
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline constexpr auto
    has_values() const noexcept {
        static_assert(Offset < sizeof...(Ts) - 1);
        return std::get<sizeof...(Ts) - 1>(storage_).subset<Offset, Count>();
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr decltype(auto)
    value_ref() const& noexcept {
        static_assert(I < sizeof...(Ts) - 1);
        return std::get<I>(storage_);
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr decltype(auto)
    value_ref() & noexcept {
        static_assert(I < sizeof...(Ts) - 1);
        return std::get<I>(storage_);
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr decltype(auto)
    value_ref() const&& noexcept {
        static_assert(I < sizeof...(Ts) - 1);
        return std::move(std::get<I>(storage_));
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr decltype(auto)
    value_ref() && noexcept {
        static_assert(I < sizeof...(Ts) - 1);
        return std::move(std::get<I>(storage_));
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr decltype(auto)
    get() const& noexcept {
        static_assert(I < sizeof...(Ts) - 1);
        if constexpr (is_optional_v<template_element_t<I, storage_base>>) {
            return has_value<I>() ? std::addressof(std::get<I>(storage_).value)
                                  : std::get<I>(storage_).empty;
        } else {
            LEV_ASSERT(has_value<I>());
            return std::get<I>(storage_).value;
        }
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr decltype(auto)
    get() & noexcept {
        static_assert(I < sizeof...(Ts) - 1);
        if constexpr (is_optional_v<template_element_t<I, storage_base>>) {
            return has_value<I>() ? std::addressof(std::get<I>(storage_).value)
                                  : std::get<I>(storage_).empty;
        } else {
            LEV_ASSERT(has_value<I>());
            return std::get<I>(storage_).value;
        }
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr decltype(auto)
    get() const&& noexcept {
        static_assert(I < sizeof...(Ts) - 1);
        if constexpr (is_optional_v<template_element_t<I, storage_base>>) {
            return has_value<I>() ? std::addressof(std::get<I>(storage_).value)
                                  : std::get<I>(storage_).empty;
        } else {
            LEV_ASSERT(has_value<I>());
            return std::move(std::get<I>(storage_).value);
        }
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr decltype(auto)
    get() && noexcept {
        static_assert(I < sizeof...(Ts) - 1);
        if constexpr (is_optional_v<template_element_t<I, storage_base>>) {
            return has_value<I>() ? std::addressof(std::get<I>(storage_).value)
                                  : std::get<I>(storage_).empty;
        } else {
            LEV_ASSERT(has_value<I>());
            return std::move(std::get<I>(storage_).value);
        }
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr void
    reset() noexcept {
        using target_type = std::tuple_element_t<I, storage_type>;
        if constexpr (!std::is_trivially_destructible_v<target_type>) {
            if (has_value<I>()) {
                std::destroy_at(std::addressof(value_ref<I>().value));
            }
        }

        std::construct_at(std::addressof(value_ref<I>().empty));
        clear_value<I>();
    }

    template <size_t I, typename F, typename... Args>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr void generate_at(
        F&& func,
        Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...> &&
        std::is_nothrow_constructible_v<std::tuple_element_t<I, storage_type>,
            std::invoke_result_t<F, Args...>>) {
        using target_type = std::tuple_element_t<I, storage_type>;

        LEV_ASSERT(!this->template has_value<I>());
        LEV_TRY {
            std::construct_at(std::addressof(value_ref<I>()), lev::generator,
                std::forward<F>(func), std::forward<Args>(args));
            set_value<I>();
        } LEV_CATCH(...) {
            reset<I>();
            LEV_RETHROW();
        }
    }

    template <size_t I, typename... Args>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr void
    emplace_at(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<std::tuple_element_t<I, storage_type>,
            Args...>) {
        using target_type = std::tuple_element_t<I, storage_type>;

        LEV_ASSERT(!this->template has_value<I>());
        LEV_TRY {
            std::construct_at(std::addressof(value_ref<I>().value),
                std::forward<Args>(args)...);
            set_value<I>();
        } LEV_CATCH(...) {
            reset<I>();
            LEV_RETHROW();
        }
    }

private:
    template <size_t I>
    LEV_HIDE_INSTANTIATION inline constexpr void set_value() noexcept {
        return std::get<sizeof...(Ts) - 1>(storage_).set(I);
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION inline constexpr void clear_value() noexcept {
        return std::get<sizeof...(Ts) - 1>(storage_).clear(I);
    }

    template <typename F, typename... Args>
    requires (std::is_invocable_v<F, Args...> &&
        std::is_constructible_v<storage_type, std::invoke_result_t<F, Args...>>)
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(generator_t tag,
        F&& func,
        Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...> &&
        std::is_nothrow_constructible_v<storage_type,
            std::invoke_result_t<F, Args...>>)
        : storage_{std::invoke(
              std::forward<F>(func), std::forward<Args>(args)...)} {}

    storage_type storage_;
};

} // namespace details

template <declaration... Ts>
class tuple<Ts...> : private details::storage_base<Ts...> {
    static_assert(
        (... && named_declaration<Ts>) || (... && !named_declaration<Ts>),
        "All arguments must be named, or all must be unnamed");
    static_assert(!(... && named_declaration<Ts>) || sizeof...(Ts) < 2,
        "Keywords are unnecessary when there is less than 2 arguments");
    static_assert(
        []() {
            std::array result{is_optional_v<Ts>...};
            return std::ranges::all_of(
                std::ranges::find(result, true), result.end());
        }(),
        "All optional arguments must be at the tail end of the argument "
        "list");

    using base_type = details::storage_base<Ts...>;

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

    template <size_t I, typename F, typename... Args>
    LEV_HIDE_INSTANTIATION friend inline constexpr void generate_at(
        tuple& t, F&& func, Args&&... args) noexcept {
        t.template generate_at<I>(
            std::forward<F>(func), std::forward<Args>(args)...);
    }

    static constexpr size_t optional_offset_v = []() {
        std::array opt{is_optional_v<Ts>};
        return std::ranges::distance(opt.begin(), std::ranges::find(opt, true));
    }();

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
        }(std::make_index_sequence<optional_offset_v>{});
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    optional() const& noexcept LEV_LIFETIMEBOUND {
        return []<size_t... Is>(std::index_sequence<Is...>) {
            using return_type =
                std::tuple<decltype(get<Is + optional_offset_v>(*this))...>;
            return return_type{get<Is + optional_offset_v>(*this)...};
        }(std::make_index_sequence<sizeof...(Ts) - optional_offset_v>{});
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
        }(std::make_index_sequence<optional_offset_v>{});
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    optional() & noexcept LEV_LIFETIMEBOUND {
        return []<size_t... Is>(std::index_sequence<Is...>) {
            using return_type =
                std::tuple<decltype(get<Is + optional_offset_v>(*this))...>;
            return return_type{get<Is + optional_offset_v>(*this)...};
        }(std::make_index_sequence<sizeof...(Ts) - optional_offset_v>{});
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
        }(std::make_index_sequence<optional_offset_v>{});
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
        }(std::make_index_sequence<optional_offset_v>{});
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
    python::kwargs_view kwargs, tuple<Args...>& tuple) noexcept {

    auto array = sort(basic_args{args, kwargs}, Args::name...);

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
    auto array = sort(vector_args{args, kwnames}, Args::name...);

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
