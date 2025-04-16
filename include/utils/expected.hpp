// Copyright 2023-2024 Bryan Wong

#pragma once

#include "utils/type_traits.hpp"
#include "utils/unexpected.hpp"
#include "utils/utility.hpp"

#include <concepts>
#include <new>
#include <type_traits>
#include <utility>

namespace ltl {

template <typename T, typename E>
class LEV_API expected;

template <typename E>
class LEV_API bad_expected_access;

namespace details {
template <typename T>
LEV_HIDDEN inline constexpr bool is_expected_type_v = false;
template <typename T>
LEV_HIDDEN inline constexpr bool is_expected_type_v<T const> =
    is_expected_type_v<T>;
template <typename T>
LEV_HIDDEN inline constexpr bool is_expected_type_v<T volatile> =
    is_expected_type_v<T>;
template <typename T, typename U>
LEV_HIDDEN inline constexpr bool is_expected_type_v<std::expected<T, U>> = true;

template <typename T>
concept expected_type = is_expected_type_v<T>;

namespace expected {

template <typename E, typename U>
requires std::is_constructible_v<E, U>
LEV_HIDDEN [[noreturn]] void throw_bad_access(U&& arg) {
    throw bad_expected_access<E>(std::forward<U>(arg));
}

struct transforming_t {
    LEV_HIDDEN explicit inline constexpr transforming_t() noexcept = default;
};
LEV_HIDDEN inline constexpr transforming_t transforming{};
struct transforming_error_t {
    LEV_HIDDEN explicit inline constexpr transforming_error_t() noexcept =
        default;
};
LEV_HIDDEN inline constexpr transforming_error_t transforming_error{};

struct converting_t {
    LEV_HIDDEN explicit constexpr converting_t() noexcept = default;
};
LEV_HIDDEN inline constexpr converting_t converting{};
struct empty_t {
    LEV_HIDDEN inline constexpr empty_t() noexcept = default;
    template <typename... Ts>
    LEV_HIDDEN inline constexpr empty_t(Ts&&...) noexcept {}
};
LEV_HIDDEN inline constexpr empty_t empty{};

template <typename T, typename U>
LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr T make_from_union(bool has_value,
    U&& arg) noexcept(std::is_nothrow_constructible_v<T, in_place_t,
                          decltype(std::declval<U>().value)> &&
    std::is_nothrow_constructible<T, unexpect_t,
        decltype(std::declval<U>().error)>) {
    return has_value ? T{std::in_place, __LTL forward_like<U>(arg.value)}
                     : T{__LTL unexpect, __LTL forward_like<U>(arg.error)};
}

template <typename T, typename E>
union data_union {
    LEV_HIDE_INSTANTIATION inline constexpr data_union(data_union const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr data_union(data_union const&) noexcept(
        std::is_nothrow_copy_constructible_v<T> &&
        std::is_nothrow_copy_constructible_v<E>)
    requires std::is_copy_constructible_v<T> &&
        std::is_copy_constructible_v<E> &&
        std::is_trivially_copy_constructible_v<T> &&
        std::is_trivially_copy_constructible_v<E>
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr data_union(data_union&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr data_union(data_union&&) noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E>)
    requires std::is_move_constructible_v<T> &&
        std::is_move_constructible_v<E> &&
        std::is_trivially_move_constructible_v<T> &&
        std::is_trivially_move_constructible_v<E>
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr data_union& operator=(data_union const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr data_union& operator=(data_union const&) noexcept(
        std::is_nothrow_copy_assignable_v<T> &&
        std::is_nothrow_copy_assignable_v<E>)
    requires std::is_copy_assignable_v<T> && std::is_copy_assignable_v<E> &&
        std::is_trivially_copy_assignable_v<T> &&
        std::is_trivially_copy_assignable_v<E>
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr data_union& operator=(data_union&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr data_union& operator=(data_union&&) noexcept(
        std::is_nothrow_move_assignable_v<T> &&
        std::is_nothrow_move_assignable_v<E>)
    requires std::is_move_assignable_v<T> && std::is_move_assignable_v<E> &&
        std::is_trivially_move_assignable_v<T> &&
        std::is_trivially_move_assignable_v<E>
    = default;
    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit data_union(in_place_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : value{std::forward<Args>(args)...} {}
    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit data_union(unexpect_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : error{std::forward<Args>(args)...} {}

    template <typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit data_union(transforming_t, F&& f,
        Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>)
        : value{std::invoke(std::forward<F>(f), std::forward<Args>(args)...)} {}

    template <typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit data_union(transforming_error_t, F&& f,
        Args&&... args) noexcept(is_nothrow_invocable_v<F, Args...>)
        : error{std::invoke(std::forward<F>(f), std::forward<Args>(args)...)} {}

    LEV_HIDE_INSTANTIATION inline constexpr ~data_union() noexcept
    requires std::is_trivially_destructible_v<T> &&
        std::is_trivially_destructible_v<E>
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr ~data_union() noexcept {}

    LEV_NO_UNIQUE_ADDRESS T value;
    LEV_NO_UNIQUE_ADDRESS E error;
};

/**
 * The implemenation essentially creates 2 layouts for storage:
 * 1. The boolean flag is placed in the tail padding (if any) of the data_union
 * and the tail padding (if any) of storage_base is not usable by external
 * objects
 * 2. The boolean flag is not placed in the tail padding (if any) of the
 * data_union and the tail padding (if any) of storage_base is usable by
 * external objects
 *
 * In case 1, we can transparently replace the whole of the container that
 * contains both the union and flag when calling swap or emplace, they are under
 * the purview of storage_base
 *
 * In case 2, when calling swap or emplace, we cannot replace the whole
 * container since, some other object may be occupying the tail padding of the
 * container (and in turn storage_base); doing so could overwrite the external
 * data when constructing/destroying the whole container during replacement. As
 * such, we only replace the union itself and manually set the flag.
 */
template <typename T, typename E>
class storage_base {
    static_assert(
        !std::is_same_v<std::remove_cvref_t<T>, empty_t>, "Invalid value type");
    using data_type = data_union<T, E>;
    static constexpr bool place_flag_in_tail =
        fits_in_tail_padding_v<data_type, bool>;
    static constexpr bool allow_external_overlap = !place_flag_in_tail;

public:
    struct container {
        template <typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr explicit container(in_place_t, Args&&... args)
            : union_{std::in_place, std::in_place, std::forward<Args>(args)...}
            , has_value_{true} {}

        template <typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr explicit container(unexpect_t, Args&&... args)
            : union_{std::in_place, __LTL unexpect,
                  std::forward<Args>(args)...}
            , has_value_{false} {}

        template <typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr explicit container(
            transforming_t, Args&&... args)
        requires allow_external_overlap
            : union_{std::in_place, __LTL details::expected::transforming,
                  std::forward<Args>(args)...}
            , has_value_{true} {}

        template <typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr explicit container(
            transforming_error_t, Args&&... args)
        requires allow_external_overlap
            : union_{std::in_place, __LTL details::expected::transforming_error,
                  std::forward<Args>(args)...}
            , has_value_{false} {}

        template <typename U>
        LEV_HIDE_INSTANTIATION inline constexpr explicit container(converting_t, bool has_value,
            U&& u) noexcept(noexcept(make_from_union<data_type>(has_value,
            std::declval<U>())))
        requires allow_external_overlap
            : union_{__LTL details::expected::converting,
                  [&]() {
                      return make_from_union<data_type>(
                          has_value, std::forward<U>(u));
                  }}
            , has_value_{has_value} {}

        LEV_HIDE_INSTANTIATION inline constexpr container(container const&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container(container const&) noexcept
        requires std::is_copy_constructible_v<T> &&
            std::is_trivially_copy_constructible_v<T> &&
            std::is_copy_constructible_v<E> &&
            std::is_trivially_copy_constructible_v<E>
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr container(container&&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container(container&&) noexcept
        requires std::is_move_constructible_v<T> &&
            std::is_trivially_move_constructible_v<T> &&
            std::is_move_constructible_v<E> &&
            std::is_trivially_move_constructible_v<E>
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(container const&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(container const&) noexcept
        requires std::is_copy_assignable_v<T> &&
            std::is_trivially_copy_assignable_v<T> &&
            std::is_copy_assignable_v<E> &&
            std::is_trivially_copy_assignable_v<E>
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(container&&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(container&&) noexcept
        requires std::is_move_assignable_v<T> &&
            std::is_trivially_move_assignable_v<T> &&
            std::is_move_assignable_v<E> &&
            std::is_trivially_move_assignable_v<E>
        = default;

        LEV_HIDE_INSTANTIATION inline constexpr ~container() noexcept
        requires std::is_trivially_destructible_v<T> &&
            std::is_trivially_destructible_v<E>
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr ~container() noexcept
        requires (
            !is_trivially_destructible_v<T> || !is_trivially_destructible_v<E>)
        {
            destroy_member();
        }

        LEV_HIDE_INSTANTIATION inline constexpr void destroy_union() noexcept
        requires allow_external_overlap
            && std::is_trivially_destructible_v<T> &&
            std::is_trivially_destructible_v<E>
        {
            __LTL destroy_at(__LTL addressof(union_.data));
        }

        LEV_HIDE_INSTANTIATION inline constexpr void destroy_union() noexcept
        requires (allow_external_overlap &&
            (!is_trivially_destructible_v<T> ||
                !is_trivially_destructible_v<E>))
        {
            destroy_member();
            __LTL destroy_at(__LTL addressof(union_.data));
        }

        template <typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr void construct_union(std::in_place_t,
            Args&&... args) noexcept(is_nothrow_constructible_v<T, Args...>)
        requires (allow_external_overlap)
        {
            __LTL construct_at(__LTL addressof(union_.data), std::in_place,
                std::forward<Args>(args)...);
            has_value_ = true;
        }

        template <typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr void construct_union(
            __LTL unexpect_t,
            Args&&... args) noexcept(is_nothrow_constructible_v<E, Args...>)
        requires (allow_external_overlap)
        {
            __LTL construct_at(__LTL addressof(union_.data),
                __LTL unexpect, std::forward<Args>(args)...);
            has_value_ = false;
        }

        LEV_NO_UNIQUE_ADDRESS
        conditionally_overlapable<place_flag_in_tail, data_type> union_;
        LEV_NO_UNIQUE_ADDRESS bool has_value_;

    private:
        LEV_HIDE_INSTANTIATION inline constexpr void destroy_member() noexcept {
            if (has_value_) {
                __LTL destroy_at(__LTL addressof(union_.data.value));
            } else {
                __LTL destroy_at(__LTL addressof(union_.data.error));
            }
        }
    };

    template <typename U>
    LEV_HIDE_INSTANTIATION static inline constexpr container make_container(bool has_value,
        U&& u) noexcept(std::is_nothrow_constructible_v<container, in_place_t,
                            decltype(std::declval<U>().value)> &&
        std::is_nothrow_constructible_v<container, unexpect_t,
            decltype(std::declval<U>().error)>)
    requires place_flag_in_tail
    {
        return has_value
            ? container{std::in_place, __LTL forward_like<U>(u.value)}
            : container{__LTL unexpect, __LTL forward_like<U>(u.error)};
    }

public:
    LEV_HIDE_INSTANTIATION inline constexpr ~storage_base() noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(storage_base const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(storage_base const&) noexcept
    requires std::is_copy_constructible_v<T> &&
        std::is_copy_constructible_v<E> &&
        std::is_trivially_copy_constructible_v<T> &&
        std::is_trivially_copy_constructible_v<E>
    = default;

    LEV_HIDE_INSTANTIATION inline constexpr storage_base(storage_base const& other) noexcept(
        std::is_nothrow_copy_constructible_v<T> &&
        std::is_nothrow_copy_constructible_v<E>)
    requires std::is_copy_constructible_v<T> &&
        std::is_copy_constructible_v<E> &&
        (!std::is_trivially_copy_constructible_v<T> ||
            !std::is_trivially_copy_constructible_v<E>)
        : storage_base(__LTL details::expected::converting, other.has_value(),
              other.data_ref()) {}

    LEV_HIDE_INSTANTIATION inline constexpr storage_base(storage_base&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(storage_base&&) noexcept
    requires std::is_move_constructible_v<T> &&
        std::is_move_constructible_v<E> &&
        std::is_trivially_move_constructible_v<T> &&
        std::is_trivially_move_constructible_v<E>
    = default;

    LEV_HIDE_INSTANTIATION inline constexpr storage_base(storage_base&& other) noexcept(
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E>)
    requires std::is_move_constructible_v<T> &&
        std::is_move_constructible_v<E> &&
        (!is_trivially_move_constructible_v<T> ||
            !is_trivially_move_constructible_v<E>)
        : storage_base(__LTL details::expected::converting, other.has_value(),
              std::move(other.data_ref())) {}

protected:
    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit storage_base(std::in_place_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<container,
        std::in_place_t, Args...>)
        : container_{
              std::in_place, std::in_place, std::forward<Args>(args)...} {}

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit storage_base(
        __LTL unexpect_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<container,
        __LTL unexpect_t, Args...>)
        : container_{
              std::in_place, __LTL unexpect, std::forward<Args>(args)...} {}

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit storage_base(transforming_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<container,
        transforming_t, Args...>)
    requires (allow_external_overlap)
        : container_{std::in_place, __LTL details::expected::transforming,
              std::forward<Args>(args)...} {}
    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit storage_base(transforming_error_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<container,
        transforming_error_t, Args...>)
    requires allow_external_overlap
        : container_{std::in_place, __LTL details::expected::transforming_error,
              std::forward<Args>(args)...} {}

    template <typename U>
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(converting_t, bool has_value,
        U&& u) noexcept(std::is_nothrow_constructible_v<container, converting_t,
        bool, U>)
    requires allow_external_overlap
        : container_{std::in_place, __LTL details::expected::converting,
              has_value, std::forward<U>(u)} {}

    template <typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit storage_base(transforming_t, F&& f,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<container,
                                     std::in_place_t, std::in_place_t,
                                     std::invoke_result_t<F, Args...>> &&
        std::is_nothrow_invocable_v<F, Args...>)
    requires place_flag_in_tail
        : container_{__LTL details::expected::converting, [&]() {
                         return container{std::in_place,
                             std::invoke(std::forward<F>(f),
                                 std::forward<Args>(args)...)};
                     }} {}

    template <typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit storage_base(transforming_error_t, F&& f,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<container,
                                     std::in_place_t, __LTL unexpect_t,
                                     std::invoke_result_t<F, Args...>> &&
        std::is_nothrow_invocable_v<F, Args...>)
    requires place_flag_in_tail
        : container_{__LTL details::expected::converting, [&]() {
                         return container{__LTL unexpect,
                             std::invoke(std::forward<F>(f),
                                 std::forward<Args>(args)...)};
                     }} {}

    template <typename U>
    LEV_HIDE_INSTANTIATION inline constexpr storage_base(converting_t, bool has_value,
        U&& u) noexcept(noexcept(make_container(has_value, std::declval<U>())))
    requires place_flag_in_tail
        : container_{__LTL details::expected::converting,
              [&]() { return make_container(has_value, std::forward<U>(u)); }} {
    }

    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(
        storage_base const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(storage_base const&) noexcept
    requires std::is_copy_assignable_v<T> && std::is_copy_assignable_v<E> &&
        std::is_trivially_copy_assignable_v<T> &&
        std::is_trivially_copy_assignable_v<E>
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base&
    operator=(storage_base const& other) noexcept(
        std::is_nothrow_copy_assignable_v<T> &&
        std::is_nothrow_copy_assignable_v<E> &&
        std::is_nothrow_copy_constructible_v<T> &&
        std::is_nothrow_copy_constructible_v<E>)
    requires std::is_copy_assignable_v<T> && std::is_copy_assignable_v<E> &&
        (!std::is_trivially_copy_assignable_v<T> ||
            !std::is_trivially_copy_assignable_v<E>)
    {
        if (this->has_value() != other.has_value()) {
            if (other.has_value()) {
                this->reinitialize_as_value(other.value_ref());
            } else {
                this->reinitialize_as_error(other.error_ref());
            }
        } else if (other.has_value()) {
            this->value_ref() = other.value_ref();
        } else {
            this->error_ref() = other.error_ref();
        }

        return *this;
    }

    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(storage_base&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(storage_base&&) noexcept
    requires std::is_move_assignable_v<T> && std::is_move_assignable_v<E> &&
        std::is_trivially_move_assignable_v<T> &&
        std::is_trivially_move_assignable_v<E>
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr storage_base& operator=(
        storage_base&& other) noexcept(std::is_nothrow_move_assignable_v<T> &&
        std::is_nothrow_move_assignable_v<E> &&
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E>)
    requires std::is_move_assignable_v<T> && std::is_move_assignable_v<E> &&
        (!std::is_trivially_move_assignable_v<T> ||
            !std::is_trivially_move_assignable_v<E>)
    {
        if (this->has_value() != other.has_value()) {
            if (other.has_value()) {
                this->reinitialize_as_value(std::move(other.value_ref()));
            } else {
                this->reinitialize_as_error(std::move(other.error_ref()));
            }
        } else if (other.has_value()) {
            this->value_ref() = std::move(other.value_ref());
        } else {
            this->error_ref() = std::move(other.error_ref());
        }

        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr data_type const&
    data_ref() const& noexcept {
        return container_.data.union_.data;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr data_type&
    data_ref() & noexcept {
        return container_.data.union_.data;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr data_type const&&
    data_ref() const&& noexcept {
        return std::move(container_.data.union_.data);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr data_type&&
    data_ref() && noexcept {
        return std::move(container_.data.union_.data);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr T const*
    value_ptr() const noexcept {
        return __LTL addressof(container_.data.union_.data.value);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr T* value_ptr() noexcept {
        return __LTL addressof(container_.data.union_.data.value);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E const*
    error_ptr() const noexcept {
        return __LTL addressof(container_.data.union_.data.error);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E* error_ptr() noexcept {
        return __LTL addressof(container_.data.union_.data.error);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr T const&
    value_ref() const& noexcept {
        return container_.data.union_.data.value;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr T& value_ref() & noexcept {
        return container_.data.union_.data.value;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr T const&&
    value_ref() const&& noexcept {
        return std::move(container_.data.union_.data.value);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr T&& value_ref() && noexcept {
        return std::move(container_.data.union_.data.value);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E const&
    error_ref() const& noexcept {
        return container_.data.union_.data.error;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E& error_ref() & noexcept {
        return container_.data.union_.data.error;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E const&&
    error_ref() const&& noexcept {
        return std::move(container_.data.union_.data.error);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E&& error_ref() && noexcept {
        return std::move(container_.data.union_.data.error);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr bool has_value() const noexcept {
        return container_.data.has_value_;
    }

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr T& emplace_value(Args&&... args) noexcept {
        static_assert(std::is_nothrow_constructible_v<T, Args...>);
        destroy();
        return *construct_value(std::forward<Args>(args)...);
    }

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr T* reinitialize_as_value(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, Args...>)
        LEV_CONTRACT_PRE(!has_value()) {
        LEV_ASSERT(!has_value());
        if constexpr (std::is_nothrow_constructible_v<T, Args...>) {
            destroy();
            auto ptr = construct_value(std::forward<Args>(args)...);
            return ptr;
        } else {
            E backup(std::move(error_ref()));
            destroy();
            LEV_TRY {
                auto ptr = construct_value(std::forward<Args>(args)...);
                return ptr;
            } LEV_CATCH(...) {
                construct_error(std::move(backup));
                LEV_RETHROW();
            }
        }
    }

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr E* reinitialize_as_error(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<E, Args...>)
        LEV_CONTRACT_PRE(has_value()) {
        LEV_ASSERT(has_value());
        if constexpr (std::is_nothrow_constructible_v<E, Args...>) {
            destroy();
            auto ptr = construct_error(std::forward<Args>(args)...);
            return ptr;
        } else {
            T backup(std::move(value_ref()));
            destroy();
            LEV_TRY {
                auto ptr = construct_error(std::forward<Args>(args)...);
                return ptr;
            } LEV_CATCH(...) {
                construct_value(std::move(backup));
                LEV_RETHROW();
            }
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr void cross_swap(storage_base& other) noexcept(
        std::is_nothrow_move_constructible_v<T, Args...> &&
        std::is_nothrow_move_constructible_v<E, Args...>)
    requires std::swappable<T> && std::swappable<E> &&
        std::is_move_constructible_v<T> && std::is_move_constructible_v<E> &&
        (std::is_nothrow_move_constructible_v<T> ||
            std::is_nothrow_move_constructible_v<E>)
    LEV_CONTRACT_PRE(has_value()) LEV_CONTRACT_PRE(!other.has_value()) {
        LEV_ASSERT(has_value() && !other.has_value());

        if constexpr (std::is_nothrow_move_constructible_v<E>) {
            E tmp(std::move(other.error_ref()));
            other.destroy();
            LEV_TRY {
                other.construct_value(std::move(this->value_ref()));
                this->destroy();
                this->construct_error(std::move(tmp));
            } LEV_CATCH(...) {
                other.construct_error(std::move(tmp));
                LEV_RETHROW();
            }
        } else {
            T tmp(std::move(this->value_ref()));
            this->destroy();
            LEV_TRY {
                this->construct_error(std::move(other.error_ref()));
                other.destroy();
                other.construct_value(std::move(tmp));
            } LEV_CATCH(...) {
                this->construct_value(std::move(tmp));
                LEV_RETHROW();
            }
        }
    }

private:
    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr E* construct_error(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<E, Args...>) {
        if constexpr (place_flag_in_tail) {
            return __LTL construct_at(__LTL addressof(container_.data),
                __LTL unexpect, std::forward<Args>(args)...);
        } else {
            container_.data.construct_union(
                __LTL unexpect, std::forward<Args>(args)...);
            return error_ptr();
        }
    }

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr T* construct_value(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, Args...>) {
        if constexpr (place_flag_in_tail) {
            return __LTL construct_at(__LTL addressof(container_.data),
                std::in_place, std::forward<Args>(args)...);
        } else {
            container_.data.construct_union(
                std::in_place, std::forward<Args>(args)...);
            return value_ptr();
        }
    }
    LEV_HIDE_INSTANTIATION inline constexpr void destroy() noexcept {
        if constexpr (place_flag_in_tail) {
            __LTL destroy_at(__LTL addressof(container_.data));
        } else {
            container_.data.destroy_union();
        }
    }

    LEV_NO_UNIQUE_ADDRESS
    conditionally_overlapable<allow_external_overlap, container> container_;
};

template <typename E>
class void_storage_base {
    using data_type = data_union<empty_t, E>;
    static constexpr bool place_flag_in_tail =
        fits_in_tail_padding_v<data_type, bool>;
    static constexpr bool allow_external_overlap = !place_flag_in_tail;

    struct container {
        LEV_HIDE_INSTANTIATION inline constexpr container() noexcept
            : union_{std::in_place, std::in_place}
            , has_value_{true} {};

        LEV_HIDE_INSTANTIATION inline constexpr explicit container(std::in_place_t tag)
            : union_{std::in_place, tag}
            , has_value_{true} {}

        template <typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr explicit container(unexpect_t, Args&&... args)
            : union_{std::in_place, __LTL unexpect,
                  std::forward<Args>(args)...}
            , has_value_{false} {}

        template <typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr explicit container(
            transforming_error_t, Args&&... args)
            : union_{std::in_place, __LTL details::expected::transforming_error,
                  std::forward<Args>(args)...}
            , has_value_{false} {}

        template <typename U>
        LEV_HIDE_INSTANTIATION inline constexpr explicit container(bool has_value,
            U&& u) noexcept(noexcept(make_from_union<data_type>(has_value,
            std::declval<U>())))
        requires allow_external_overlap
            : union_{__LTL details::expected::converting,
                  [&]() {
                      return make_from_union<data_type>(
                          has_value, std::forward<U>(u));
                  }}
            , has_value_(has_value) {}

        LEV_HIDE_INSTANTIATION inline constexpr container(container const&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container(container const&) noexcept
        requires std::is_copy_constructible_v<E> &&
            std::is_trivially_copy_constructible_v<E>
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr container(container&&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container(container&&) noexcept
        requires std::is_move_constructible_v<E> &&
            std::is_trivially_move_constructible_v<E>
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(container const&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(container const&) noexcept
        requires std::is_copy_assignable_v<E> &&
            std::is_trivially_copy_assignable_v<E>
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(container&&) = delete;
        LEV_HIDE_INSTANTIATION inline constexpr container& operator=(container&&) noexcept
        requires std::is_move_assignable_v<E> &&
            std::is_trivially_move_assignable_v<E>
        = default;

        LEV_HIDE_INSTANTIATION inline constexpr ~container() noexcept
        requires std::is_trivially_destructible_v<E>
        = default;
        LEV_HIDE_INSTANTIATION inline constexpr ~container() noexcept
        requires (!std::is_trivially_destructible_v<E>)
        {
            destroy_member();
        }

        LEV_HIDE_INSTANTIATION inline constexpr void destroy_union() noexcept
        requires allow_external_overlap && std::is_trivially_destructible_v<E>
        {
            __LTL destroy_at(__LTL addressof(union_.data));
        }

        LEV_HIDE_INSTANTIATION inline constexpr void destroy_union() noexcept
        requires allow_external_overlap &&
            (!std::is_trivially_destructible_v<E>)
        {
            destroy_member();
            __LTL destroy_at(__LTL addressof(union_.data));
        }

        template <typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr void construct_union(std::in_place_t) noexcept
        requires allow_external_overlap
        {
            __LTL construct_at(__LTL addressof(union_.data), std::in_place);
            has_value_ = true;
        }

        template <typename... Args>
        LEV_HIDE_INSTANTIATION inline constexpr void construct_union(
            __LTL unexpect_t,
            Args&&... args) noexcept(is_nothrow_constructible_v<E, Args...>)
        requires allow_external_overlap
        {
            __LTL construct_at(__LTL addressof(union_.data),
                __LTL unexpect, std::forward<Args>(args)...);
            has_value_ = false;
        }

    private:
        LEV_HIDE_INSTANTIATION inline constexpr void destroy_member() noexcept {
            if (has_value_) {
                __LTL destroy_at(__LTL addressof(union_.data.value));
            } else {
                __LTL destroy_at(__LTL addressof(union_.data.error));
            }
        }

        LEV_NO_UNIQUE_ADDRESS
        conditionally_overlapable<place_flag_in_tail, data_type> union_;
        LEV_NO_UNIQUE_ADDRESS bool has_value_;
    };

    template <typename U>
    LEV_HIDE_INSTANTIATION static inline constexpr container make_container(bool has_value,
        U&& u) noexcept(std::is_nothrow_constructible_v<container, unexpect_t,
        decltype(std::declval<U>().error)>)
    requires place_flag_in_tail
    {
        return has_value
            ? container{std::in_place}
            : container{__LTL unexpect, __LTL forward_like<U>(u.error)};
    }

protected:
    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base() noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base(void_storage_base const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base(void_storage_base const&) noexcept
    requires std::is_copy_constructible_v<E> &&
        std::is_trivially_copy_constructible_v<E>
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base(void_storage_base const&
            other) noexcept(std::is_nothrow_copy_constructible_v<E>)
    requires std::is_copy_constructible_v<E> &&
        (!std::is_trivially_copy_constructible_v<E>)
        : void_storage_base(__LTL details::expected::converting,
              other.has_value(), other.data_ref()) {}

    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base(void_storage_base&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base(void_storage_base&&) noexcept
    requires std::is_move_constructible_v<E> &&
        std::is_trivially_move_constructible_v<E>
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base(void_storage_base&&
            other) noexcept(std::is_nothrow_move_constructible_v<E>)
    requires (std::is_move_constructible_v<E> &&
        !std::is_trivially_move_constructible_v<E>)
        : void_storage_base(__LTL details::expected::converting,
              other.has_value(), std::move(other.data_ref())) {}

    LEV_HIDE_INSTANTIATION inline constexpr explicit void_storage_base(std::in_place_t) noexcept
        : container_{} {}

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit void_storage_base(unexpect_t,
        Args&&... args) noexcept(istd::s_nothrow_constructible_v<container,
        unexpect_t, Args...>)
        : container_{
              std::in_place, __LTL unexpect, std::forward<Args>(args)...} {}

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit void_storage_base(transforming_error_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<container,
        transforming_error_t, Args...>)
    requires allow_external_overlap
        : container_{std::in_place, __LTL details::expected::transforming_error,
              std::forward<Args>(args)...} {}

    template <typename U>
    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base(converting_t, bool has_value,
        U&& u) noexcept(std::is_nothrow_constructible_v<container, converting_t,
        bool, U>)
    requires allow_external_overlap
        : container_{std::in_place, __LTL details::expected::converting,
              has_value, std::forward<U>(u)} {}

    template <typename F, typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr explicit void_storage_base(transforming_error_t,
        F&& f,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<container,
                                     std::in_place_t, __LTL unexpect_t,
                                     std::invoke_result_t<F, Args...>> &&
        std::is_nothrow_invocable_v<F, Args...>)
    requires place_flag_in_tail
        : container_{__LTL details::expected::converting, [&]() {
                         return container{__LTL unexpect,
                             std::invoke(std::forward<F>(f),
                                 std::forward<Args>(args)...)};
                     }} {}

    template <typename U>
    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base(converting_t, bool has_value,
        U&& u) noexcept(noexcept(make_container(has_value, std::declval<U>())))
    requires place_flag_in_tail
        : container_{__LTL details::expected::converting,
              [&]() { return make_container(has_value, std::forward<U>(u)); }} {
    }

    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base& operator=(
        void_storage_base const&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base& operator=(
        void_storage_base const&) noexcept
    requires std::is_copy_assignable_v<E> &&
        std::is_trivially_copy_assignable_v<E>
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base&
    operator=(void_storage_base const& other) noexcept(
        std::is_nothrow_copy_assignable_v<E> &&
        std::is_nothrow_copy_constructible_v<E>)
    requires std::is_copy_assignable_v<E> &&
        (!std::is_trivially_copy_assignable_v<E>)
    {
        if (this->has_value() != other.has_value()) {
            if (other.has_value()) {
                this->reinitialize_as_value();
            } else {
                this->reinitialize_as_error(other.error_ref());
            }
        } else if (other.has_value()) {
            this->value_ref() = other.value_ref();
        } else {
            this->error_ref() = other.error_ref();
        }

        return *this;
    }

    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base& operator=(
        void_storage_base&&) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base& operator=(
        void_storage_base&&) noexcept
    requires std::is_move_assignable_v<E> &&
        std::is_trivially_move_assignable_v<E>
    = default;
    LEV_HIDE_INSTANTIATION inline constexpr void_storage_base&
    operator=(void_storage_base&& other) noexcept(
        std::is_nothrow_move_assignable_v<E> &&
        std::is_nothrow_move_constructible_v<E>)
    requires std::is_move_assignable_v<E> &&
        (!std::is_trivially_move_assignable_v<E>)
    {
        if (this->has_value() != other.has_value()) {
            if (other.has_value()) {
                this->reinitialize_as_value();
            } else {
                this->reinitialize_as_error(std::move(other.error_ref()));
            }
        } else if (other.has_value()) {
            this->value_ref() = std::move(other.value_ref());
        } else {
            this->error_ref() = std::move(other.error_ref());
        }

        return *this;
    }

    LEV_HIDE_INSTANTIATION inline constexpr ~void_storage_base() noexcept = default;

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr data_type const&
    data_ref() const& noexcept {
        return container_.data.union_.data;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr data_type&
    data_ref() & noexcept {
        return container_.data.union_.data;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr data_type const&&
    data_ref() const&& noexcept {
        return std::move(container_.data.union_.data);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr data_type&&
    data_ref() && noexcept {
        return std::move(container_.data.union_.data);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E const*
    error_ptr() const noexcept {
        return __LTL addressof(container_.data.union_.data.error);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E* error_ptr() noexcept {
        return __LTL addressof(container_.data.union_.data.error);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E const&
    error_ref() const& noexcept {
        return container_.data.union_.data.error;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E& error_ref() & noexcept {
        return container_.data.union_.data.error;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E const&&
    error_ref() const&& noexcept {
        return std::move(container_.data.union_.data.error);
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E&& error_ref() && noexcept {
        return std::move(container_.data.union_.data.error);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr bool has_value() const noexcept {
        return container_.data.has_value_;
    }

    LEV_HIDE_INSTANTIATION inline constexpr void emplace_value() noexcept {
        destroy();
        construct_value();
    }

    LEV_HIDE_INSTANTIATION inline constexpr void
    reinitialize_as_value() noexcept LEV_CONTRACT_PRE(!has_value()) {
        LEV_ASSERT(!has_value());
        destroy();
        construct_value();
    }

    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr E* reinitialize_as_error(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<E, Args...>)
        LEV_CONTRACT_PRE(has_value()) {
        LEV_ASSERT(has_value());

        if constexpr (std::is_nothrow_constructible<E, Args...>) {
            destroy();
            auto ptr = construct_error(std::forward<Args>(args)...);
            return ptr;
        } else {
            destroy();
            LEV_TRY {
                auto ptr = construct_error(std::forward<Args>(args)...);
                return ptr;
            } LEV_CATCH(...) {
                construct_value();
                LEV_RETHROW();
            }
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr void cross_swap(void_storage_base& other) noexcept(
        std::is_nothrow_move_constructible_v<E>)
    requires std::is_move_constructible_v<E>
    LEV_CONTRACT_PRE(has_value()) LEV_CONTRACT_PRE(!other.has_value()) {
        LEV_ASSERT(has_value() && !other.has_value());
        this->reinitialize_as_error(std::move(other.error_ref()));
        other.reinitialize_as_value();
    }

private:
    template <typename... Args>
    LEV_HIDE_INSTANTIATION inline constexpr E* construct_error(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<E, Args...>) {
        if constexpr (place_flag_in_tail) {
            return __LTL construct_at(__LTL addressof(container_.data),
                __LTL unexpect, std::forward<Args>(args)...);
        } else {
            container_.data.construct_union(
                __LTL unexpect, std::forward<Args>(args)...);
            return error_ptr();
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr void construct_value() noexcept {
        if constexpr (place_flag_in_tail) {
            __LTL construct_at(__LTL addressof(container_.data), std::in_place);
        } else {
            container_.data.construct_union(std::in_place);
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr void destroy() noexcept {
        if constexpr (place_flag_in_tail) {
            __LTL destroy_at(__LTL addressof(container_.data));
        } else {
            container_.data.destroy_union();
        }
    }

    LEV_NO_UNIQUE_ADDRESS
    conditionally_overlapable<allow_external_overlap, container> container_;
};

} // namespace expected
} // namespace details

template <typename E>
class LEV_API bad_expected_access : std::exception {

public:
    template <typename U>
    requires std::is_constructible_v<E, U>
    LEV_HIDE_INSTANTIATION bad_expected_access(U&& e)
        : exception()
        , error_(std::forward<U>(e)) {}
    LEV_HIDE_INSTANTIATION bad_expected_access(bad_expected_access const&) = default;
    LEV_HIDE_INSTANTIATION bad_expected_access(bad_expected_access&&) = default;
    LEV_HIDE_INSTANTIATION bad_expected_access& operator=(bad_expected_access const&) = default;
    LEV_HIDE_INSTANTIATION bad_expected_access& operator=(bad_expected_access&&) = default;

    char const* what() const noexcept { return "bad expected access"; }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline E const& error() const& noexcept LEV_LIFETIMEBOUND {
        return error_;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline E const&& error() const&& noexcept LEV_LIFETIMEBOUND {
        return error_;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline E& error() & noexcept LEV_LIFETIMEBOUND {
        return error_;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline E&& error() && noexcept LEV_LIFETIMEBOUND {
        return error_;
    }

private:
    E error_;
};

template <typename T, typename E>
class LEV_API expected : private details::expected::storage_base<T, E> {
    static_assert(std::is_destructible_v<T>, "Invalid value type");
    static_assert(!std::is_reference_v<T>, "Invalid value type");
    static_assert(!std::is_function_v<T>, "Invalid value type");
    static_assert(!__LTL details::unexpect::tag_type<std::remove_cvref_t<T>>,
        "Invalid value type");
    static_assert(!std::same_as<std::remove_cvref_t<T>, std::in_place_t>,
        "Invalid value type");
    static_assert(!__LTL details::unexpected_type<std::remove_cvref_t<T>>,
        "Invalid value type");
    static_assert(__LTL is_complete_v<unexpected<E>>, "Invalid error type");
    using base_type = details::expected::storage_base<T, E>;

    template <typename T1, typename E1, typename T1Qual, typename E1Qual>
    using can_convert = std::conjunction<std::is_constructible<T, T1Qual>,
        std::is_constructible<E, E1Qual>,
        std::conditional_t<!std::same_as<T, bool>,
            std::negation<std::disjunction<
                std::bool_constant<std::same_as<T, T1> && std::same_as<E, E1>>,
                std::is_constructible<T, expected<T1, E1>&>,
                std::is_convertible<expected<T1, E1>&, T>,
                std::is_constructible<T, expected<T1, E1>>,
                std::is_convertible<expected<T1, E1>, T>,
                std::is_constructible<T, expected<T1, E1> const&>,
                std::is_convertible<expected<T1, E1> const&, T>,
                std::is_constructible<T, expected<T1, E1> const>,
                std::is_convertible<expected<T1, E1> const, T>>>,
            std::true_type>,
        std::negation<std::disjunction<
            std::is_constructible<unexpected<E>, expected<T1, E1>&>,
            std::is_constructible<unexpected<E>, expected<T1, E1>>,
            std::is_constructible<unexpected<E>, expected<T1, E1> const&>,
            std::is_constructible<unexpected<E>, expected<T1, E1> const>>>>;

    template <typename U> /* std::details::is_unexpected_type_v<U> */
    using unexpected_error_type = decltype(std::declval<U>().error());

    using base_type::base_type;

    template <typename T1, typename E1>
    friend class expected;

public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;
    template <typename U>
    using rebind = expected<U, error_type>;

    LEV_HIDE_INSTANTIATION explicit(__LTL is_explicit_constructible_v<
        T>) inline constexpr expected() noexcept(std::
            is_nothrow_default_constructible_v<T>)
    requires std::is_default_constructible_v<T>
        : base_type{std::in_place} {}
    LEV_HIDE_INSTANTIATION inline constexpr expected(expected const& other) noexcept(
        std::is_nothrow_copy_constructible_v<base_type>) = default;
    LEV_HIDE_INSTANTIATION inline constexpr expected(expected&& other) noexcept(
        std::is_nothrow_move_constructible_v<base_type>) = default;

    template <typename T1, typename E1>
    requires can_convert<T1, E1, T1 const&, E1 const&>::value
    LEV_HIDE_INSTANTIATION explicit(!std::is_convertible_v<T1 const&, T> ||
        !std::is_convertible_v<E1 const&,
            E>) inline constexpr expected(expected<T1, E1> const&
            other) noexcept(std::is_nothrow_constructible_v<T, T1 const&> &&
        std::is_nothrow_constructible_v<E, E1 const&>)
        : base_type(__LTL details::expected::converting, other.has_value(),
              other.data_ref()) {}

    template <typename T1, typename E1>
    requires can_convert<T1, E1, T1, E1>::value
    LEV_HIDE_INSTANTIATION explicit(!std::is_convertible_v<T1, T> ||
        !std::is_convertible_v<E1, E>) inline constexpr expected(expected<T1,
        E1>&& other) noexcept(std::is_nothrow_constructible_v<T, T1> &&
        std::is_nothrow_constructible_v<E, E1>)
        : base_type(__LTL details::expected::converting, other.has_value(),
              std::move(other.data_ref())) {}

    template <typename U = T>
    requires (std::constructible_from<T, U> &&
        different_from<std::in_place_t, std::remove_cvref_t<U>> &&
        different_from<expected, std::remove_cvref_t<U>> &&
        !__LTL details::is_unexpected_type_v<std::remove_cvref_t<U>> &&
        (different_from<bool, std::remove_cvref_t<T>> ||
            !__LTL details::expected_type<std::remove_cvref_t<U>>))
    LEV_HIDE_INSTANTIATION explicit(!std::is_convertible_v<U, T>) inline constexpr expected(
        U&& value) noexcept(std::is_nothrow_constructible_v<T, U>)
        : base_type(std::in_place, std::forward<U>(value)) {}

    template <typename U>
    requires __LTL details::is_unexpected_type_v<remove_cvref_t<U>> &&
        std::constructible_from<E, unexpected_error_type<U>>
    LEV_HIDE_INSTANTIATION explicit(!std::is_convertible_v<unexpected_error_type<U>,
                   E>) inline constexpr expected(U&&
            e) noexcept(std::is_nothrow_constructible_v<E,
        unexpected_error_type<U>>)
        : base_type(__LTL unexpect, __LTL forward_like<U>(e.error())) {}

    template <typename... Args>
    requires std::constructible_from<T, Args...>
    LEV_HIDE_INSTANTIATION explicit inline constexpr expected(in_place_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : base_type(std::in_place, std::forward<Args>(args)...) {}

    template <typename U, typename... Args>
    requires std::constructible_from<T, ::std::initializer_list<U>&, Args...>
    LEV_HIDE_INSTANTIATION explicit inline constexpr expected(in_place_t,
        ::std::initializer_list<U> il,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<T,
        ::std::initializer_list<U>&, Args...>)
        : base_type(std::in_place, il, std::forward<Args>(args)...) {}

    template <typename... Args>
    requires std::constructible_from<E, Args...>
    LEV_HIDE_INSTANTIATION explicit inline constexpr expected(unexpect_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : base_type(__LTL unexpect, std::forward<Args>(args)...) {}

    template <typename U, typename... Args>
    requires std::constructible_from<E, ::std::initializer_list<U>&, Args...>
    LEV_HIDE_INSTANTIATION explicit inline constexpr expected(unexpect_t,
        ::std::initializer_list<U> il,
        Args&&... args) noexcept(is_nothrow_constructible_v<E,
        ::std::initializer_list<U>&, Args...>)
        : base_type(__LTL unexpect, il, std::forward<Args>(args)...) {}

    LEV_HIDE_INSTANTIATION inline ~expected() noexcept = default;

    LEV_HIDE_INSTANTIATION inline constexpr expected& operator=(expected const&) noexcept(
        std::is_nothrow_copy_assignable_v<T> &&
        std::is_nothrow_copy_assignable_v<E>) = default;
    LEV_HIDE_INSTANTIATION inline constexpr expected& operator=(expected&&) noexcept(
        std::is_nothrow_move_assignable_v<T> &&
        std::is_nothrow_move_assignable_v<E>) = default;

    template <typename U = T>
    requires (!std::same_as<expected, std::remove_cvref_t<U>> &&
        !details::is_unexpected_type_v<std::remove_cvref_t<U>> &&
        std::constructible_from<T, U> && std::assignable_from<T&, U> &&
        (std::is_nothrow_constructible_v<T, U> ||
            std::is_nothrow_move_constructible_v<T> ||
            std::is_nothrow_move_constructible_v<E>))
    LEV_HIDE_INSTANTIATION inline constexpr expected& operator=(U&& u) noexcept(
        std::is_nothrow_assignable_v<T&, U> &&
        std::is_nothrow_constructible_v<T, U>) {
        if (has_value()) {
            this->value_ref() = std::forward<U>(u);
        } else {
            this->reinitialize_as_value(std::forward<U>(u));
        }

        return *this;
    }

    template <typename U>
    requires details::is_unexpected_type<remove_cvref_t<U>>::value &&
        std::constructible_from<E, unexpected_error_type<U>> &&
        std::assignable_from<E&, unexpected_error_type<U>> &&
        (std::is_nothrow_constructible_v<E, unexpected_error_type<U>> ||
            std::is_nothrow_move_constructible_v<T> ||
            std::is_nothrow_move_constructible_v<E>)
    LEV_HIDE_INSTANTIATION inline constexpr expected& operator=(U&& u) noexcept(
        std::is_nothrow_assignable_v<E&, unexpected_error_type<U>> &&
        std::is_nothrow_constructible_v<E, unexpected_error_type<U>>) {
        if (!has_value()) {
            this->error_ref() = __LTL forward_like<U>(u.error());
        } else {
            this->reinitialize_as_error(__LTL forward_like<U>(u.error()));
        }

        return *this;
    }

    using base_type::has_value;

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr T const& value() const& {
        static_assert(std::is_copy_constructible_v<E>, "Invalid error type");
        if (!has_value()) {
            details::expected::throw_bad_access<E>(this->error_ref());
        }

        return this->value_ref();
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr T& value() & {
        static_assert(std::is_copy_constructible_v<E>, "Invalid error type");
        if (!has_value()) {
            details::expected::throw_bad_access<E>(this->error_ref());
        }

        return this->value_ref();
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr T const&& value() const&& {
        static_assert(std::is_copy_constructible_v<E> ||
                std::is_constructible_v<E,
                    decltype(std::move(this->error_ref()))>,
            "Invalid error type");
        if (!has_value()) {
            details::expected::throw_bad_access<E>(this->error_ref());
        }
        return std::move(this->value_ref());
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr T&& value() && {
        static_assert(std::is_copy_constructible_v<E> ||
                std::is_constructible_v<E,
                    decltype(std::move(this->error_ref()))>,
            "Invalid error type");
        if (!has_value()) {
            details::expected::throw_bad_access<E>(this->error_ref());
        }

        return std::move(this->value_ref());
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E const&
    error() const& noexcept LEV_CONTRACT_PRE(!has_value()) {
        LEV_ASSERT(!has_value());
        return this->error_ref();
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E&
    error() & noexcept LEV_CONTRACT_PRE(!has_value()) {
        LEV_ASSERT(!has_value());
        return this->error_ref();
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E const&&
    error() const&& noexcept LEV_CONTRACT_PRE(!has_value()) {
        LEV_ASSERT(!has_value());
        return std::move(this->error_ref());
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E&&
    error() && noexcept LEV_CONTRACT_PRE(!has_value()) {
        LEV_ASSERT(!has_value());
        return std::move(this->error_ref());
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr T* operator->() noexcept LEV_CONTRACT_PRE(has_value()) {
        LEV_ASSERT(has_value());
        return this->value_ptr();
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr T const*
    operator->() const noexcept LEV_CONTRACT_PRE(has_value()) {
        LEV_ASSERT(has_value());
        return this->value_ptr();
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr T& operator*() & noexcept LEV_CONTRACT_PRE(
        has_value()) {
        LEV_ASSERT(has_value());
        return this->value_ref();
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr T&& operator*() && noexcept LEV_CONTRACT_PRE(
        has_value()) {
        LEV_ASSERT(has_value());
        return std::move(this->value_ref());
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr T const&
    operator*() const& noexcept LEV_CONTRACT_PRE(has_value()) {
        LEV_ASSERT(has_value());
        return this->value_ref();
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr T const&&
    operator*() const&& noexcept LEV_CONTRACT_PRE(has_value()) {
        LEV_ASSERT(has_value());
        return std::move(this->value_ref());
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr explicit
    operator bool() const noexcept {
        return has_value();
    }

    template <std::convertible_to<T> U>
    requires std::is_copy_constructible_v<T>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr T value_or(
        U&& default_val) const& noexcept(std::is_nothrow_convertible_v<U, T>) {
        return has_value() ? **this
                           : static_cast<T>(std::forward<U>(default_val));
    }

    template <std::convertible_to<T> U>
    requires std::is_move_constructible_v<T>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr T value_or(
        U&& default_val) && noexcept(std::is_nothrow_convertible_v<U, T>) {
        return has_value() ? std::move(**this)
                           : static_cast<T>(std::forward<U>(default_val));
    }

    template <std::convertible_to<E> U>
    requires std::is_copy_constructible_v<E>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr E error_or(
        U&& default_val) const& noexcept(std::is_nothrow_convertible_v<U, E>) {
        return has_value() ? static_cast<E>(std::forward<U>(default_val))
                           : this->error_ref();
    }

    template <std::convertible_to<E> U>
    requires std::is_move_constructible_v<E>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr E error_or(
        U&& default_val) && noexcept(std::is_nothrow_convertible_v<U, E>) {
        return has_value() ? static_cast<E>(std::forward<U>(default_val))
                           : std::move(this->error_ref());
    }

    template <std::invocable<T&> F>
    requires std::constructible_from<E, E&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto and_then(F&& f) & noexcept(
        std::is_nothrow_invocable_v<F, T&> &&
        std::is_nothrow_constructible_v<E, E&>) -> std::invoke_result_t<F, T&> {
        using return_type = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::same_as<typename return_type::error_type, E>,
            "Invalid return type");
        if (has_value()) {
            return std::invoke(std::forward<F>(f), this->value_ref());
        }

        return return_type{__LTL unexpect, this->error_ref()};
    }

    template <std::invocable<T const&> F>
    requires std::constructible_from<E, E const&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto and_then(F&& f) const& noexcept(
        std::is_nothrow_invocable_v<F, T const&> &&
        std::is_nothrow_constructible_v<E, E const&>)
        -> std::invoke_result_t<F, T const&> {
        using return_type =
            std::remove_cvref_t<std::invoke_result_t<F, T const&>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::same_as<typename return_type::error_type, E>,
            "Invalid return type");

        return has_value() ? std::invoke(std::forward<F>(f), this->value_ref())
                           : return_type{__LTL unexpect, this->error_ref()};
    }

    template <std::invocable<T> F>
    requires std::constructible_from<E, E>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto and_then(F&& f) && noexcept(
        std::is_nothrow_invocable_v<F, T> &&
        std::is_nothrow_constructible_v<E, E>) -> std::invoke_result_t<F, T> {
        using return_type = std::remove_cvref_t<std::invoke_result_t<F, T>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::same_as<typename return_type::error_type, E>,
            "Invalid return type");

        if (has_value()) {
            return std::invoke(
                std::forward<F>(f), std::move(this->value_ref()));
        }

        return return_type{__LTL unexpect, std::move(this->error_ref())};
    }

    template <std::invocable<T const> F>
    requires std::constructible_from<E, E const>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto and_then(F&& f) const&& noexcept(
        std::is_nothrow_invocable_v<F, T const> &&
        std::is_nothrow_constructible_v<E, E const>)
        -> std::invoke_result_t<F, T const> {
        using return_type =
            std::remove_cvref_t<std::invoke_result_t<F, T const>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::same_as<typename return_type::error_type, E>,
            "Invalid return type");

        return has_value()
            ? std::invoke(std::forward<F>(f), std::move(this->value_ref()))
            : return_type{__LTL unexpect, std::move(this->error_ref())};
    }

    template <std::invocable<T&> F>
    requires std::constructible_from<E, E&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform(
        F&& f) & -> expected<std::remove_cv_t<std::invoke_result_t<F, T&>>, E> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F, T&>>;
        static_assert(__LTL is_complete_v<expected<return_type, E>> &&
                (std::is_void_v<return_type> ||
                    std::is_constructible_v<return_type,
                        std::invoke_result_t<F, T&>>),
            "Invalid return type");
        if (!has_value()) {
            return expected<return_type, E>{__LTL unexpect, this->error_ref()};
        }

        if constexpr (std::is_void_v<return_type>) {
            std::invoke(std::forward<F>(f), this->value_ref());
            return expected<return_type, E>{};
        } else {
            return expected<return_type, E>{
                __LTL details::expected::transforming, std::forward<F>(f),
                this->value_ref()};
        }
    }

    template <std::invocable<T const&> F>
    requires std::constructible_from<E, E const&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform(F&& f)
        const& -> expected<std::remove_cv_t<std::invoke_result_t<F, T const&>>,
            E> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F, T const&>>;
        static_assert(__LTL is_complete_v<expected<return_type, E>> &&
                (std::is_void_v<return_type> ||
                    std::is_constructible_v<return_type,
                        std::invoke_result_t<F, T const&>>),
            "Invalid return type");

        if (!has_value()) {
            return expected<return_type, E>{__LTL unexpect, this->error_ref()};
        }

        if constexpr (std::is_void_v<return_type>) {
            std::invoke(std::forward<F>(f), this->value_ref());
            return expected<return_type, E>{};
        } else {
            return expected<return_type, E>{
                __LTL details::expected::transforming, std::forward<F>(f),
                this->value_ref()};
        }
    }

    template <std::invocable<T> F>
    requires std::constructible_from<E, E>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform(
        F&& f) && -> expected<std::remove_cv_t<std::invoke_result_t<F, T>>, E> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F, T>>;
        static_assert(__LTL is_complete_v<expected<return_type, E>> &&
                (is_void_v<return_type> ||
                    is_constructible_v<return_type,
                        std::invoke_result_t<F, T>>),
            "Invalid return type");

        if (!has_value()) {
            return expected<return_type, E>{
                __LTL unexpect, std::move(this->error_ref())};
        }

        if constexpr (is_void_v<return_type>) {
            std::invoke(std::forward<F>(f), std::move(this->value_ref()));
            return expected<return_type, E>{};
        } else {
            return expected<return_type, E>{
                __LTL details::expected::transforming, std::forward<F>(f),
                std::move(this->value_ref())};
        }
    }

    template <std::invocable<T const> F>
    requires std::constructible_from<E, E const>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform(F&& f)
        const&& -> expected<std::remove_cv_t<std::invoke_result_t<F, T const>>,
            E> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F, T const>>;
        static_assert(__LTL is_complete_v<expected<return_type, E>> &&
                (std::is_void_v<return_type> ||
                    std::is_constructible_v<return_type,
                        std::invoke_result_t<F, T const>>),
            "Invalid return type");

        if (!has_value()) {
            return expected<return_type, E>{
                __LTL unexpect, std::move(this->error_ref())};
        }

        if constexpr (std::is_void_v<return_type>) {
            std::invoke(std::forward<F>(f), std::move(this->value_ref()));
            return expected<return_type, E>{};
        } else {
            return expected<return_type, E>{
                __LTL details::expected::transforming, std::forward<F>(f),
                std::move(this->value_ref())};
        }
    }

    template <std::invocable<E&> F>
    requires std::constructible_from<T, T&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto or_else(F&& f) & noexcept(
        std::is_nothrow_invocable_v<F, E&> &&
        std::is_nothrow_constructible_v<T, T&>) -> std::invoke_result_t<F, E&> {
        using return_type = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::same_as<typename return_type::value_type, T>,
            "Invalid return type");
        if (has_value()) {
            return return_type{std::in_place, this->value_ref()};
        }

        return std::invoke(std::forward<F>(f), this->error_ref());
    }

    template <std::invocable<E const&> F>
    requires std::constructible_from<T, T const&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto or_else(F&& f) const& noexcept(
        std::is_nothrow_invocable_v<F, E const&> &&
        std::is_nothrow_constructible_v<T, T const&>)
        -> std::invoke_result_t<F, E const&> {
        using return_type =
            std::remove_cvref_t<std::invoke_result_t<F, E const&>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::same_as<typename return_type::value_type, T>,
            "Invalid return type");

        if (has_value()) {
            return return_type{std::in_place, this->value_ref()};
        }

        return std::invoke(std::forward<F>(f), this->error_ref());
    }

    template <std::invocable<E> F>
    requires std::constructible_from<T, T>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto or_else(F&& f) && noexcept(
        std::is_nothrow_invocable_v<F, E> &&
        std::is_nothrow_constructible_v<T, T>) -> std::invoke_result_t<F, E> {
        using return_type = std::remove_cvref_t<std::invoke_result_t<F, E>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::same_as<typename return_type::value_type, T>,
            "Invalid return type");

        if (has_value()) {
            return return_type{std::in_place, std::move(this->value_ref())};
        }

        return std::invoke(std::forward<F>(f), std::move(this->error_ref()));
    }

    template <std::invocable<E const> F>
    requires std::constructible_from<T, T const>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto or_else(F&& f) const&& noexcept(
        std::is_nothrow_invocable_v<F, E const> &&
        std::is_nothrow_constructible_v<T, T const>)
        -> std::invoke_result_t<F, E const> {
        using return_type =
            std::remove_cvref_t<std::invoke_result_t<F, E const>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::same_as<typename return_type::value_type, T>,
            "Invalid return type");

        if (has_value()) {
            return return_type{std::in_place, std::move(this->value_ref())};
        }

        return std::invoke(std::forward<F>(f), std::move(this->error_ref()));
    }

    template <std::invocable<E&> F>
    requires std::constructible_from<T, T&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform_error(
        F&& f) & -> expected<T, std::remove_cv_t<std::invoke_result_t<F, E&>>> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F, E&>>;
        static_assert(__LTL is_complete_v<expected<T, return_type>> &&
                std::is_constructible_v<return_type,
                    std::invoke_result_t<F, E&>>,
            "Invalid return type");
        if (has_value()) {
            return expected<T, return_type>{std::in_place, this->value_ref()};
        }

        return expected<return_type, E>{
            __LTL details::expected::transforming_error, std::forward<F>(f),
            this->error_ref()};
    }

    template <std::invocable<E const&> F>
    requires std::constructible_from<T, T const&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform_error(
        F&& f) const& -> expected<T,
        std::remove_cv_t<std::invoke_result_t<F, E const&>>> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F, E const&>>;
        static_assert(__LTL is_complete_v<expected<T, return_type>> &&
                std::is_constructible_v<return_type,
                    std::invoke_result_t<F, E const&>>,
            "Invalid return type");

        if (has_value()) {
            return expected<T, return_type>{std::in_place, this->value_ref()};
        }

        return expected<return_type, E>{
            __LTL details::expected::transforming_error, std::forward<F>(f),
            this->error_ref()};
    }

    template <std::invocable<E> F>
    requires std::constructible_from<T, T>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform_error(
        F&& f) && -> expected<T, std::remove_cv_t<std::invoke_result_t<F, E>>> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F, E>>;
        static_assert(__LTL is_complete_v<expected<T, return_type>> &&
                std::is_constructible_v<return_type,
                    std::invoke_result_t<F, E>>,
            "Invalid return type");

        if (has_value()) {
            return expected<T, return_type>{
                std::in_place, std::move(this->value_ref())};
        }

        return expected<T, return_type>{
            __LTL details::expected::transforming_error, std::forward<F>(f),
            std::move(this->error_ref())};
    }

    template <std::invocable<E const> F>
    requires std::constructible_from<T, T const>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform_error(
        F&& f) const&& -> expected<T,
        std::remove_cv_t<std::invoke_result_t<F, E const>>> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F, E const>>;
        static_assert(__LTL is_complete_v<expected<T, return_type>> &&
                std::is_constructible_v<return_type,
                    std::invoke_result_t<F, E const>>,
            "Invalid return type");

        if (has_value()) {
            return expected<T, return_type>{
                std::in_place, std::move(this->value_ref())};
        }

        return expected<T, return_type>{
            __LTL details::expected::transforming_error, std::forward<F>(f),
            std::move(this->error_ref())};
    }

    template <typename... Args>
    requires std::is_nothrow_constructible_v<T, Args...>
    LEV_HIDE_INSTANTIATION inline constexpr T& emplace(Args&&... args) noexcept {
        return this->emplace_value(std::forward<Args>(args)...);
    }

    template <typename U, typename... Args>
    requires std::is_nothrow_constructible_v<T, ::std::initializer_list<U>&,
        Args...>
    LEV_HIDE_INSTANTIATION inline constexpr T& emplace(
        ::std::initializer_list<U> il, Args&&... args) noexcept {
        return this->emplace_value(il, std::forward<Args>(args)...);
    }

    LEV_HIDE_INSTANTIATION inline constexpr void swap(expected& other) noexcept(
        std::is_nothrow_swappable_v<T> && std::is_nothrow_swappable_v<E> &&
        std::is_nothrow_move_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<E>)
    requires std::swappable<T> && std::swappable<E> &&
        std::is_move_constructible_v<T> && std::is_move_constructible_v<E> &&
        (std::is_nothrow_move_constructible_v<T> ||
            std::is_nothrow_move_constructible_v<E>)
    {
        if (this->has_value() == other.has_value()) {
            if (this->has_value()) {
                std::ranges::swap(this->value_ref(), other.value_ref());
            } else {
                std::ranges::swap(this->error_ref(), other.error_ref());
            }
        } else if (this->has_value()) {
            this->cross_swap(other);
        } else {
            other.cross_swap(*this);
        }
    }

    LEV_HIDE_INSTANTIATION friend inline constexpr void swap(
        expected& left, expected& right) noexcept(noexcept(left.swap(right)))
    requires requires { left.swap(right); }
    {
        left.swap(right);
    }

    template <typename T2, typename E2>
    requires (!std::is_void_v<T2>)
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool
    operator==(expected const& left, expected<T2, E2> const& right) noexcept(
        noexcept(std::declval<T const&>() ==
            std::declval<T2 const&>()) && noexcept(std::declval<E const&>() ==
            std::declval<E2 const&>())) {
        static_assert(requires(T left, T2 right) { true && (left == right); });
        static_assert(requires(E left, E2 right) { true && (left == right); });
        return left.has_value() == right.has_value() &&
            (left.has_value() ? left.value_ref() == right.value()
                              : left.error_ref() == right.error());
    }

    template <typename T2>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool
    operator==(expected const& left, T2 const& right) noexcept(
        noexcept(std::declval<T const&>() == std::declval<T2 const&>())) {
        static_assert(requires(T left, T2 right) { true && (left == right); });
        return left.has_value() && left.value_ref() == right;
    }

    template <typename E2>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool
    operator==(expected const& left, unexpected<E2> const& right) noexcept(
        noexcept(std::declval<E const&>() == std::declval<E2 const&>())) {
        static_assert(requires(E left, E2 right) { true && (left == right); });
        return !left.has_value() && left.error_ref() == right.error();
    }

    template <typename T2, typename E2>
    requires (!std::is_void_v<T2>)
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool
    operator!=(expected const& left, expected<T2, E2> const& right) noexcept(
        noexcept(std::declval<T const&>() ==
            std::declval<T2 const&>()) && noexcept(std::declval<E const&>() ==
            std::declval<E2 const&>())) {
        return !(left == right);
    }

    template <typename T2>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool
    operator!=(expected const& left, T2 const& right) noexcept(
        noexcept(std::declval<T const&>() == std::declval<T2 const&>())) {
        return !(left == right);
    }

    template <typename E2>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool
    operator!=(expected const& left, unexpected<E2> const& right) noexcept(
        noexcept(std::declval<E const&>() == std::declval<E2 const&>())) {
        return !(left == right);
    }
};

template <typename Void, typename E>
requires std::is_void_v<Void>
class __UTL_PUBLIC_TEMPLATE expected<Void, E> :
    __LTL details::expected::void_storage_base<E> {
    static_assert(__LTL is_complete_v<unexpected<E>>, "Invalid error type");
    using base_type = details::expected::void_storage_base<E>;

    template <typename T1, typename E1, typename E1Qual>
    using can_convert =
        std::conjunction<std::is_void<T1>, std::is_constructible<E, E1Qual>,
            std::negation<std::disjunction<
                std::is_constructible<unexpected<E>, expected<T1, E1>&>,
                std::is_constructible<unexpected<E>, expected<T1, E1>>,
                std::is_constructible<unexpected<E>, expected<T1, E1> const&>,
                std::is_constructible<unexpected<E>, expected<T1, E1> const>>>>;

    template <typename U> /* std::details::is_unexpected_type_v<U> */
    using unexpected_error_type = decltype(std::declval<U>().error());

    template <typename T1, typename E1>
    friend class expected;

public:
    using value_type = Void;
    using error_type = E;
    using unexpected_type = unexpected<E>;
    template <typename U>
    using rebind = expected<U, error_type>;

    LEV_HIDE_INSTANTIATION inline constexpr expected() noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr expected(expected const& other) noexcept(
        std::is_nothrow_copy_constructible_v<base_type>) = default;
    LEV_HIDE_INSTANTIATION inline constexpr expected(expected&& other) noexcept(
        std::is_nothrow_move_constructible_v<base_type>) = default;

    template <typename T1, typename E1>
    requires can_convert<T1, E1, E1 const&>::value
    LEV_HIDE_INSTANTIATION explicit(!std::is_convertible_v<E1 const&,
                   E>) inline constexpr expected(expected<T1, E1> const&
            other) noexcept(std::is_nothrow_constructible_v<E, E1 const&>)
        : base_type(__LTL details::expected::converting, other.has_value(),
              other.data_ref()) {}

    template <typename T1, typename E1>
    requires can_convert<T1, E1, E1>::value
    LEV_HIDE_INSTANTIATION explicit(!std::is_convertible_v<E1, E>) inline constexpr expected(
        expected<T1, E1>&&
            other) noexcept(std::is_nothrow_constructible_v<E, E1>)
        : base_type(__LTL details::expected::converting, other.has_value(),
              std::move(other.data_ref())) {}

    template <typename U>
    requires details::is_unexpected_type_v<remove_cvref_t<U>> &&
        std::constructible_from<E, unexpected_error_type<U>>
    LEV_HIDE_INSTANTIATION explicit(!std::is_convertible_v<unexpected_error_type<U>,
                   E>) inline constexpr expected(U&&
            e) noexcept(std::is_nothrow_constructible_v<E,
        unexpected_error_type<U>>)
        : base_type(__LTL unexpect, __LTL forward_like<U>(e.error())) {}

    LEV_HIDE_INSTANTIATION explicit inline constexpr expected(std::in_place_t) noexcept
        : base_type(std::in_place) {}

    template <typename... Args>
    requires std::constructible_from<E, Args...>
    LEV_HIDE_INSTANTIATION explicit inline constexpr expected(unexpect_t,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<E, Args...>)
        : base_type(__LTL unexpect, std::forward<Args>(args)...) {}

    template <typename U, typename... Args>
    requires std::constructible_from<E, ::std::initializer_list<U>&, Args...>
    LEV_HIDE_INSTANTIATION explicit inline constexpr expected(unexpect_t,
        ::std::initializer_list<U> il,
        Args&&... args) noexcept(std::is_nothrow_constructible_v<E,
        ::std::initializer_list<U>&, Args...>)
        : base_type(__LTL unexpect, il, std::forward<Args>(args)...) {}

    LEV_HIDE_INSTANTIATION inline ~expected() noexcept = default;

    LEV_HIDE_INSTANTIATION inline constexpr expected& operator=(expected const&) noexcept(
        std::is_nothrow_copy_assignable_v<E>) = default;
    LEV_HIDE_INSTANTIATION inline constexpr expected& operator=(expected&&) noexcept(
        std::is_nothrow_move_assignable_v<E>) = default;

    template <typename U>
    requires details::is_unexpected_type_v<std::remove_cvref_t<U>> &&
        std::constructible_from<E, unexpected_error_type<U>> &&
        std::assignable_from<E&, unexpected_error_type<U>> &&
        (std::is_nothrow_constructible_v<E, unexpected_error_type<U>> ||
            std::is_nothrow_move_constructible_v<E>)
    LEV_HIDE_INSTANTIATION inline constexpr expected& operator=(U&& u) noexcept(
        std::is_nothrow_assignable_v<E&, unexpected_error_type<U>> &&
        std::is_nothrow_constructible_v<E, unexpected_error_type<U>>) {
        if (!has_value()) {
            this->error_ref() = __LTL forward_like<U>(u.error());
        } else {
            this->reinitialize_as_error(__LTL forward_like<U>(u.error()));
        }

        return *this;
    }

    using base_type::has_value;

    LEV_HIDE_INSTANTIATION inline constexpr void value() const& {
        static_assert(std::is_copy_constructible_v<E>, "Invalid error type");
        if (!has_value()) {
            details::expected::throw_bad_access<E>(this->error_ref());
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr void value() && {
        static_assert(std::is_copy_constructible_v<E> ||
                std::is_constructible_v<E,
                    decltype(std::move(this->error_ref()))>,
            "Invalid error type");
        if (!has_value()) {
            details::expected::throw_bad_access<E>(this->error_ref());
        }
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E const&
    error() const& noexcept LEV_CONTRACT_PRE(!has_value()) {
        LEV_ASSERT(!has_value());
        return this->error_ref();
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E&
    error() & noexcept LEV_CONTRACT_PRE(!has_value()) {
        LEV_ASSERT(!has_value());
        return this->error_ref();
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E const&&
    error() const&& noexcept LEV_CONTRACT_PRE(!has_value()) {
        LEV_ASSERT(!has_value());
        return std::move(this->error_ref());
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr E&&
    error() && noexcept LEV_CONTRACT_PRE(!has_value()) {
        LEV_ASSERT(!has_value());
        return std::move(this->error_ref());
    }

    LEV_HIDE_INSTANTIATION inline constexpr void operator*() const noexcept {}

    LEV_HIDE_INSTANTIATION [[nodiscard]] LEV_ALWAYS_INLINE inline constexpr explicit
    operator bool() const noexcept {
        return has_value();
    }

    template <std::convertible_to<E> U>
    requires std::is_copy_constructible_v<E>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr E error_or(
        U&& default_val) const& noexcept(std::is_nothrow_convertible_v<U, E>) {
        return has_value() ? static_cast<E>(std::forward<U>(default_val))
                           : this->error_ref();
    }

    template <std::convertible_to<E> U>
    requires std::is_move_constructible_v<E>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr E error_or(
        U&& default_val) && noexcept(std::is_nothrow_convertible_v<U, E>) {
        return has_value() ? static_cast<E>(std::forward<U>(default_val))
                           : std::move(this->error_ref());
    }

    template <std::invocable F>
    requires std::constructible_from<E, E&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto and_then(F&& f) & noexcept(
        std::is_nothrow_invocable_v<F> &&
        std::is_nothrow_constructible_v<E, E&>) -> std::invoke_result_t<F> {
        using return_type = std::remove_cvref_t<std::invoke_result_t<F>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::same_as<typename return_type::error_type, E>,
            "Invalid return type");
        if (has_value()) {
            return std::invoke(std::forward<F>(f));
        }

        return return_type{__LTL unexpect, this->error_ref()};
    }

    template <std::invocable F>
    requires std::constructible_from<E, E const&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto and_then(F&& f) const& noexcept(
        std::is_nothrow_invocable_v<F> &&
        std::is_nothrow_constructible_v<E, E const&>)
        -> std::invoke_result_t<F> {
        using return_type = std::remove_cvref_t<std::invoke_result_t<F>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::same_as<typename return_type::error_type, E>,
            "Invalid return type");

        return has_value() ? std::invoke(std::forward<F>(f))
                           : return_type{__LTL unexpect, this->error_ref()};
    }

    template <std::invocable F>
    requires std::constructible_from<E, E>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto and_then(F&& f) && noexcept(
        std::is_nothrow_invocable_v<F> && std::is_nothrow_constructible_v<E, E>)
        -> std::invoke_result_t<F> {
        using return_type = std::remove_cvref_t<std::invoke_result_t<F>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::same_as<typename return_type::error_type, E>,
            "Invalid return type");

        if (has_value()) {
            return std::invoke(std::forward<F>(f));
        }

        return return_type{__LTL unexpect, std::move(this->error_ref())};
    }

    template <std::invocable F>
    requires std::constructible_from<E, E const>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto and_then(F&& f) const&& noexcept(
        std::is_nothrow_invocable_v<F> &&
        std::is_nothrow_constructible_v<E, E const>)
        -> std::invoke_result_t<F> {
        using return_type = std::remove_cvref_t<std::invoke_result_t<F>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::same_as<typename return_type::error_type, E>,
            "Invalid return type");

        return has_value()
            ? std::invoke(std::forward<F>(f))
            : return_type{__LTL unexpect, std::move(this->error_ref())};
    }

    template <std::invocable F>
    requires std::constructible_from<E, E&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform(
        F&& f) & -> expected<std::remove_cv_t<std::invoke_result_t<F>>, E> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F>>;
        static_assert(__LTL is_complete_v<expected<return_type, E>> &&
                (std::is_void_v<return_type> ||
                    std::is_constructible_v<return_type,
                        std::invoke_result_t<F>>),
            "Invalid return type");
        if (!has_value()) {
            return expected<return_type, E>{__LTL unexpect, this->error_ref()};
        }

        if constexpr (std::is_void_v<return_type>) {
            std::invoke(std::forward<F>(f));
            return expected<return_type, E>{};
        } else {
            return expected<return_type, E>{
                __LTL details::expected::transforming, std::forward<F>(f)};
        }
    }

    template <std::invocable F>
    requires std::constructible_from<E, E const&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform(F&& f)
        const& -> expected<std::remove_cv_t<std::invoke_result_t<F>>, E> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F>>;
        static_assert(__LTL is_complete_v<expected<return_type, E>> &&
                (std::is_void_v<return_type> ||
                    std::is_constructible_v<return_type,
                        std::invoke_result_t<F>>),
            "Invalid return type");

        if (!has_value()) {
            return expected<return_type, E>{__LTL unexpect, this->error_ref()};
        }

        if constexpr (std::is_void_v<return_type>) {
            std::invoke(std::forward<F>(f));
            return expected<return_type, E>{};
        } else {
            return expected<return_type, E>{
                __LTL details::expected::transforming, std::forward<F>(f)};
        }
    }

    template <std::invocable F>
    requires std::constructible_from<E, E>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform(
        F&& f) && -> expected<std::remove_cv_t<std::invoke_result_t<F>>, E> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F>>;
        static_assert(__LTL is_complete_v<expected<return_type, E>> &&
                (std::is_void_v<return_type> ||
                    std::is_constructible_v<return_type,
                        std::invoke_result_t<F>>),
            "Invalid return type");

        if (!has_value()) {
            return expected<return_type, E>{
                __LTL unexpect, std::move(this->error_ref())};
        }

        if constexpr (std::is_void_v<return_type>) {
            std::invoke(std::forward<F>(f));
            return expected<return_type, E>{};
        } else {
            return expected<return_type, E>{
                __LTL details::expected::transforming, std::forward<F>(f)};
        }
    }

    template <std::invocable F>
    requires std::constructible_from<E, E const>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform(F&& f)
        const&& -> expected<std::remove_cv_t<std::invoke_result_t<F>>, E> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F>>;
        static_assert(__LTL is_complete_v<expected<return_type, E>> &&
                (std::is_void_v<return_type> ||
                    std::is_constructible_v<return_type,
                        std::invoke_result_t<F>>),
            "Invalid return type");

        if (!has_value()) {
            return expected<return_type, E>{
                __LTL unexpect, std::move(this->error_ref())};
        }

        if constexpr (std::is_void_v<return_type>) {
            std::invoke(std::forward<F>(f));
            return expected<return_type, E>{};
        } else {
            return expected<return_type, E>{
                __LTL details::expected::transforming, std::forward<F>(f)};
        }
    }

    template <std::invocable<E&> F>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto or_else(F&& f) & noexcept(
        std::is_nothrow_invocable_v<F, E&>) -> std::invoke_result_t<F, E&> {
        using return_type = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::is_void_v<typename return_type::value_type>,
            "Invalid return type");
        if (has_value()) {
            return return_type{std::in_place};
        }

        return std::invoke(std::forward<F>(f), this->error_ref());
    }

    template <std::invocable<E const&> F>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto or_else(F&& f) const& noexcept(
        std::is_nothrow_invocable_v<F, E const&>)
        -> std::invoke_result_t<F, E const&> {
        using return_type =
            std::remove_cvref_t<std::invoke_result_t<F, E const&>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::is_void_v<typename return_type::value_type>,
            "Invalid return type");

        if (has_value()) {
            return return_type{std::in_place};
        }

        return std::invoke(std::forward<F>(f), this->error_ref());
    }

    template <std::invocable<E> F>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto or_else(F&& f) && noexcept(
        std::is_nothrow_invocable_v<F, E>) -> std::invoke_result_t<F, E> {
        using return_type = std::remove_cvref_t<std::invoke_result_t<F, E>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::is_void_v<typename return_type::value_type>,
            "Invalid return type");

        if (has_value()) {
            return return_type{std::in_place};
        }

        return std::invoke(std::forward<F>(f), std::move(this->error_ref()));
    }

    template <std::invocable<E const> F>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto or_else(F&& f) const&& noexcept(
        std::is_nothrow_invocable_v<F, E const>)
        -> std::invoke_result_t<F, E const> {
        using return_type =
            std::remove_cvref_t<std::invoke_result_t<F, E const>>;
        static_assert(details::is_expected_type_v<return_type> &&
                std::is_void_v<typename return_type::value_type>,
            "Invalid return type");

        if (has_value()) {
            return return_type{std::in_place};
        }

        return std::invoke(std::forward<F>(f), std::move(this->error_ref()));
    }

    template <std::invocable<E&> F>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform_error(
        F&& f) & -> expected<Void,
        std::remove_cv_t<std::invoke_result_t<F, E&>>> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F, E&>>;
        static_assert(__LTL is_complete_v<expected<Void, return_type>> &&
                std::is_constructible_v<return_type,
                    std::invoke_result_t<F, E&>>,
            "Invalid return type");
        if (has_value()) {
            return expected<Void, return_type>{};
        }

        return expected<return_type, E>{
            __LTL details::expected::transforming_error, std::forward<F>(f),
            this->error_ref()};
    }

    template <std::invocable<E const&> F>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform_error(
        F&& f) const& -> expected<Void,
        std::remove_cv_t<std::invoke_result_t<F, E const&>>> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F, E const&>>;
        static_assert(__LTL is_complete_v<expected<Void, return_type>> &&
                std::is_constructible_v<return_type,
                    std::invoke_result_t<F, E const&>>,
            "Invalid return type");

        if (has_value()) {
            return expected<Void, return_type>{};
        }

        return expected<return_type, E>{
            __LTL details::expected::transforming_error, std::forward<F>(f),
            this->error_ref()};
    }

    template <std::invocable<E> F>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform_error(
        F&& f) && -> expected<Void,
        std::remove_cv_t<std::invoke_result_t<F, E>>> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F, E>>;
        static_assert(__LTL is_complete_v<expected<Void, return_type>> &&
                std::is_constructible_v<return_type,
                    std::invoke_result_t<F, E>>,
            "Invalid return type");

        if (has_value()) {
            return expected<Void, return_type>{};
        }

        return expected<Void, return_type>{
            __LTL details::expected::transforming_error, std::forward<F>(f),
            std::move(this->error_ref())};
    }

    template <std::invocable<E const> F>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto transform_error(
        F&& f) const&& -> expected<Void,
        std::remove_cv_t<std::invoke_result_t<F, E const>>> {
        using return_type = std::remove_cv_t<std::invoke_result_t<F, E const>>;
        static_assert(__LTL is_complete_v<expected<Void, return_type>> &&
                std::is_constructible_v<return_type,
                    std::invoke_result_t<F, E const>>,
            "Invalid return type");

        if (has_value()) {
            return expected<Void, return_type>{};
        }

        return expected<Void, return_type>{
            __LTL details::expected::transforming_error, std::forward<F>(f),
            std::move(this->error_ref())};
    }

    LEV_HIDE_INSTANTIATION inline constexpr void emplace() noexcept { this->emplace_value(); }

    LEV_HIDE_INSTANTIATION inline constexpr void swap(expected& other) noexcept(
        std::is_nothrow_swappable_v<E> &&
        std::is_nothrow_move_constructible_v<E>)
    requires std::swappable<E> && std::is_move_constructible_v<E>
    {
        if (this->has_value() == other.has_value()) {
            if (!this->has_value()) {
                std::ranges::swap(this->value_ref(), other.value_ref());
            } else {
                std::ranges::swap(this->error_ref(), other.error_ref());
            }
        } else if (this->has_value()) {
            this->cross_swap(other);
        } else {
            other.cross_swap(*this);
        }
    }

    LEV_HIDE_INSTANTIATION friend inline constexpr void swap(
        expected& left, expected& right) noexcept(noexcept(left.swap(right)))
    requires requires { left.swap(right); }
    {
        left.swap(right);
    }

    template <void_type T2, typename E2>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool
    operator==(expected const& left, expected<T2, E2> const& right) noexcept(
        noexcept(std::declval<E const&>() == std::declval<E2 const&>())) {
        static_assert(requires(E left, E2 right) { true && (left == right); });
        return left.has_value() == right.has_value() &&
            (left.has_value() || left.error_ref() == right.error());
    }

    template <typename E2>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool
    operator==(expected const& left, unexpected<E2> const& right) noexcept(
        noexcept(std::declval<E const&>() == std::declval<E2 const&>())) {
        static_assert(requires(E left, E2 right) { true && (left == right); });
        return !left.has_value() && left.error_ref() == right.error();
    }

    template <void_type T2, typename E2>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool
    operator!=(expected const& left, expected<T2, E2> const& right) noexcept(
        noexcept(std::declval<E const&>() == std::declval<E2 const&>())) {
        return !(left == right);
    }

    template <typename E2>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool
    operator!=(expected const& left, unexpected<E2> const& right) noexcept(
        noexcept(std::declval<E const&>() == std::declval<E2 const&>())) {
        return !(left == right);
    }
};

} // namespace ltl
