// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/type_traits.hpp"

#include <Python.h>

#include <concepts>
#include <utility>

namespace lev {
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
struct static_storage {
    // we cannot const this because python can initializes the hash value to -1
    // and updates to the actual hash value only when it is needed at runtime
    LEV_HIDE_INSTANTIATION static inline constinit std::remove_const_t<
        decltype(S)>
        value = S;
};

template <auto S>
struct LEV_PUBLIC public_static_storage {
    // we cannot const this because python can initializes the hash value to -1
    // and updates to the actual hash value only when it is needed at runtime
    static inline constinit std::remove_const_t<decltype(S)> value = S;
};

template <typename AdaptorType>
consteval bool always_false() noexcept {
    return false;
}

template <typename AdaptorType>
consteval bool always_true() noexcept {
    return true;
}

template <typename T, typename U>
LEV_HIDDEN [[nodiscard]] constexpr auto forward_like(U&& u) noexcept
    -> std::add_rvalue_reference_t<
        copy_cvref_t<T, std::remove_reference_t<U>>> {
    return static_cast<std::add_rvalue_reference_t<
        copy_cvref_t<T, std::remove_reference_t<U>>>>(u);
}

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

namespace details {

template <typename>
void py_cast(...) noexcept = delete;

template <typename To>
struct py_cast_t {
    LEV_HIDE_INSTANTIATION explicit inline constexpr py_cast_t() noexcept =
        default;

    template <typename From>
    requires std::same_as<remove_cv_t<To>, remove_cv_t<From>>
    LEV_HIDE_INSTANTIATION inline constexpr auto operator()(
        From* ptr) const noexcept {
        return static_cast<copy_cv_t<From, To>*>(ptr);
    }

    LEV_HIDE_INSTANTIATION inline constexpr auto operator()(
        decltype(nullptr) ptr) const noexcept {
        return static_cast<To*>(ptr);
    }

    template <leviathan_pyobj From>
    requires requires(From* ptr) { static_cast<copy_cv_t<From, To>*>(ptr); }
    LEV_HIDE_INSTANTIATION inline constexpr auto operator()(
        From* ptr) const noexcept {
        return static_cast<copy_cv_t<From, To>*>(ptr);
    }

    template <interaliasable_with<To> From>
    LEV_HIDE_INSTANTIATION inline auto operator()(From* ptr) const noexcept {
        return reinterpret_cast<copy_cv_t<From, To>*>(ptr);
    }

    template <opaque_pyobj T>
    requires requires(
        T* ptr) { py_cast<To>(ptr)->std::same_as<copy_cv_t<T, To>*>; }
    LEV_HIDE_INSTANTIATION inline constexpr auto operator()(
        T* ptr) const noexcept {
        return py_cast<To>(ptr);
    }
};
} // namespace details

inline namespace cpo {
template <typename To>
LEV_HIDE_INSTANTIATION inline constexpr details::py_cast_t<To> py_cast{};
}

template <typename From, typename To>
concept castable_to = requires(From* ptr) {
    { py_cast<To>(ptr) } -> copy_cv_t<From, To>*;
};

template <typename To, typename From>
concept castable_from = castable_to<From, To>;

template <castable_to<PyObject> T>
LEV_HIDE_INSTANTIATION auto as_pyobject(T* ptr) noexcept(
    noexcept(py_cast<PyObject>(ptr))) {
    return py_cast<PyObject>(ptr);
}

LEV_HIDDEN std::span<PyObject* const> to_span(
    unmanaged_ptr<PyTupleObject> ptr) noexcept {
    if (!ptr) {
        return {};
    }

    return {ptr->ob_item, ptr->ob_item + PyTuple_GET_SIZE(ptr)};
}

LEV_HIDDEN std::strign_view to_string_view(
    unmanaged_ptr<PyUnicodeObject> ptr) noexcept {
    if (!ptr) {
        return {};
    }

    Py_ssize_t size = 0;
    auto str = PyUnicode_AsUTF8AndSize(ptr, &size);
    return std::string_view{str, static_cast<size_t>(size)};
}

} // namespace lev
