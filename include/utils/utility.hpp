// Copyright 2025, Bryan Wong
#pragma once

#include "utils/type_traits.hpp"

#include <concepts>
#include <utility>

#if defined(__clang__) || defined(__INTEL_LLVM_COMPILER)
#  include <new>
/**
 * The Clang frontend allows constexpr placement new within the std namespace
 */
namespace std {
/* Technically UNDEFINED BEHAVIOUR */
template <typename T, typename... Args>
LEV_HIDE_INSTANTIATION inline constexpr T* __lev_construct_at_impl(T* location,
    Args&&... args) noexcept(noexcept(::new((void*)0) T{declval<Args>()...})) {
    // Clang front-end achieve constexpr placement-new by special-casing
    // namespace std
    return ::new (location) T{forward<Args>(args)...};
}
} // namespace std
namespace ltl {
template <typename T, typename... Args>
requires requires(
    void* ptr, Args... args) { ::new (ptr) T(forward<Args>(args)); }
LEV_HIDE_INSTANTIATION inline constexpr T* construct_at(T* location, Args&&... args) noexcept(
    noexcept(::std::__lev_construct_at_impl(location, std::declval<Args>()...)))
    LEV_CONTRACT_PRE(location != nullptr) {
    LEV_ASSERT(location != nullptr);
    return ::std::__lev_construct_at_impl(
        location, std::forward<Args>(args)...);
}
} // namespace ltl
#elif defined(__GNUC__)
// GCC achieve constexpr placement-new by special-casing std::construct_at
#  include <bits/stl_construct.h>
namespace ltl {
using std::construct_at;
}
#elif defined(_MSC_VER)
#  if __has_cpp_attributes(msvc::constexpr)
#    include <new>
namespace ltl {
template <typename T, typename... Args>
requires requires(
    void* ptr, Args... args) { ::new (ptr) T(forward<Args>(args)); }
__UTL_HIDE_FROM_ABI inline constexpr T* construct_at(
    T* location, Args&&... args) noexcept(noexcept(::new((void*)0)
        T{std::declval<Args>()...})) LEV_CONTRACT_PRE(location != nullptr) {
    LEV_ASSERT(location != nullptr);
    // MSVC achieve constexpr placement-new by using a custom attribute
    [[msvc::constexpr]] return ::new (location) T{std::forward<Args>(args)...};
}
} // namespace ltl
#  else
#    error "Unsupported"
#  endif
#else
#  error "Unsupported"
#endif

namespace ltl {

template <typename T>
T const* addressof(T const&&) = delete;

template <typename T>
LEV_HIDDEN [[nodiscard]] LEV_ALWAYS_INLINE T* addressof(T& arg) noexcept {
    return __builtin_addressof(arg);
}

template <typename T>
LEV_HIDDEN constexpr void destroy_at(T* ptr) noexcept {
    if constexpr (std::is_array_v<T>) {
        for (auto& item : *ptr) {
            (destroy_at)(addressof(item));
        }
    } else {
        ptr->~T();
    }
}

template <typename T, typename U>
LEV_HIDDEN [[nodiscard]] constexpr auto forward_like(U&& u) noexcept
    -> std::add_rvalue_reference_t<
        copy_cvref_t<T, std::remove_reference_t<U>>> {
    return static_cast<std::add_rvalue_reference_t<
        copy_cvref_t<T, std::remove_reference_t<U>>>>(u);
}

struct LEV_API generator_t {
    LEV_HIDDEN explicit inline constexpr generator_t() noexcept = default;
};
inline constexpr generator_t generator{};

template <typename T>
struct LEV_API generator_type_t {
    LEV_HIDDEN explicit inline constexpr generator_type_t() noexcept = default;
};
template <typename T>
inline constexpr generator_type_t<T> generator_type{};

template <size_t I>
struct LEV_API generator_index_t {
    LEV_HIDDEN explicit inline constexpr generator_index_t() noexcept = default;
};

template <size_t I>
inline constexpr generator_index_t<I> generator_index{};

template <std::move_constructible T, typename U = T>
requires std::assignable_from<T&, U>
LEV_HIDDEN [[nodiscard]] inline constexpr T exchange(
    T& obj, U&& new_value) noexcept(std::is_nothrow_move_constructible<T> &&
    std::is_nothrow_assignable_v<T&, U>) {
    T previous(std::move(obj));
    obj = std::forward<U>(new_value);
    return previous;
}

template <auto S>
struct LEV_HIDDEN static_storage {
    LEV_HIDE_INSTANTIATION static inline constinit std::remove_const_t<decltype(S)> value = S;
};

template <auto S>
struct LEV_PUBLIC public_static_storage {
    static inline constinit std::remove_const_t<decltype(S)> value = S;
};

namespace details {
namespace tuple {

template <size_t, typename T>
void get(T&&) = delete;

template <typename T, size_t I>
concept has_adl_get = requires(T&& t) { get<I>(std::forward<T>(t)); };
template <typename T, size_t I>
concept has_member_get =
    requires(T&& t) { std::forward<T>(t).template get<I>(); };

template <typename T, size_t I>
LEV_HIDDEN inline constexpr bool nothrow_member_get_v = false;
template <typename T, size_t I>
requires requires {
    { std::declval<T>().template get<I>() } noexcept;
}
LEV_HIDDEN inline constexpr bool nothrow_member_get_v<T> = true;

template <typename T, size_t I>
LEV_HIDDEN inline constexpr bool nothrow_adl_get_v = false;
template <typename T, size_t I>
requires requires {
    { get<I>(std::declval<T>()) } noexcept;
}
LEV_HIDDEN inline constexpr bool nothrow_adl_get_v<T> = true;

template <size_t I>
struct get_element_t {
private:
public:
    constexpr explicit get_element_t() noexcept = default;

    template <has_adl_get<I> T>
    LEV_HIDE_INSTANTIATION [[gnu::always_inline, nodiscard]] inline constexpr decltype(auto)
    operator()(T&& t LEV_LIFETIMEBOUND) const noexcept(nothrow_adl_get_v<T>) {
        return get<I>(std::forward<T>(t));
    }

    template <has_member_get<I> T>
    requires (!has_adl_get<T, I>)
    LEV_HIDE_INSTANTIATION [[gnu::always_inline, nodiscard]] inline constexpr decltype(auto)
    operator()(T&& t
            LEV_LIFETIMEBOUND) const noexcept(nothrow_member_get_v<T>) {
        return std::forward<T>(t).template get<I>();
    }
};

} // namespace tuple
} // namespace details

inline namespace cpo {
template <size_t I>
inline constexpr details::tuple::get_element_t<I> get_element{};
}

namespace details {
namespace tuple {
template <typename T>
concept has_size = requires { std::tuple_size_v<T>; };
template <typename T, size_t I>
concept has_element = has_size<T> && requires(T t) {
    requires I < tuple_size_v<T>;
    typename std::tuple_element_t<I, T>;
    get_element<I>(std::forward<T>(t));
};
} // namespace tuple
} // namespace details

template <typename T>
concept tuple_like = (!std::is_reference_v<T> && details::tuple::has_size<T> &&
    []<size_t... Is>(std::index_sequence<Is...>) {
        return (... && details::tuple::has_element<T, Is>);
    }(std::make_index_sequence<std::tuple_size_v<T>>{}));

template <typename From, typename To>
struct type_map {
    friend consteval auto map(From) noexcept {
        if constexpr (std::is_void_v<To>) {
            return;
        } else {
            return To{};
        }
    }
};

template <size_t N>
struct format_cstring_t {
    template <size_t L, typename... Args>
    explicit format_cstring_t(char const (&fmt)[L], Args... args) noexcept
        : format_cstring_t(static_cast<char const*>(fmt), args...) {}

    char const* data() const noexcept LEV_LIFETIMEBOUND {
        return static_cast<char const*>(buffer);
    }

private:
    explicit format_cstring_t(char const* fmt, ...) noexcept {
        va_list args1;
        va_start(args, fmt);
        auto str_size = vsnprintf(buffer, N, fmt, args);
        va_end(args);
        LEV_ASSERT(str_size + 1 <= N);
        buffer[N - 1] = 0;
    }

    char buffer[N];
};

template <size_t N = 128, size_t L, typename... Args>
LEV_HIDE_INSTANTIATION [[nodiscard]] format_cstring_t<N> format_cstring(
    char const (&fmt)[L], Args... args) noexcept {
    return format_cstring_t<N>{fmt, args...};
}

} // namespace ltl
