// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/type_traits.hpp"

#include <Python.h>

#include <concepts>
#include <utility>

namespace lev {
template <std::move_constructible T, typename U = T>
requires std::assignable_from<T&, U>
LEV_HIDDEN [[nodiscard]] inline constexpr T exchange(
    T& obj, U&& new_value) noexcept(std::is_nothrow_move_constructible<T> &&
    std::is_nothrow_assignable_v<T&, U>) {
    T previous(std::move(obj));
    obj = __UTL forward<U>(new_value);
    return previous;
}

template <typename AdaptorType>
consteval bool always_false() noexcept {
    return false;
}

template <typename AdaptorType>
consteval bool always_true() noexcept {
    return true;
}

template <typename...>
struct typelist {};

template <typename From, typename To>
struct copy_cv {
    using type = To;
};

template <typename From, typename To>
using copy_cv_t = copy_cv<From, To>;

template <typename From, typename To>
struct copy_cv<From const, To> {
    using type = To const;
};

template <typename From, typename To>
struct copy_cv<From const volatile, To> {
    using type = To const volatile;
};

template <typename From, typename To>
struct copy_cv<From volatile, To> {
    using type = To volatile;
};

template <typename From, typename To>
struct copy_cvref : copy_cv<From, To> {};

template <typename From, typename To>
using copy_cvref_t = copy_cvref<From, To>;

template <typename From, typename To>
struct copy_cvref<From&, To> :
    std::add_lvalue_reference<copy_cv_t<From, To>> {};

template <typename From, typename To>
struct copy_cvref<From&&, To> :
    std::add_rvalue_reference<copy_cv_t<From, To>> {};

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
    non_owning_ptr<PyTupleObject> ptr) noexcept {
    if (!ptr) {
        return {};
    }

    return {ptr->ob_item, ptr->ob_item + PyTuple_GET_SIZE(ptr)};
}

LEV_HIDDEN std::strign_view to_string_view(
    non_owning_ptr<PyUnicodeObject> ptr) noexcept {
    if (!ptr) {
        return {};
    }

    Py_ssize_t size = 0;
    auto str = PyUnicode_AsUTF8AndSize(ptr, &size);
    return std::string_view{str, static_cast<size_t>(size)};
}

namespace details {

template <typename T, typename THead, typename TMid, typename... TTail>
requires requires(void (*func)(T)) {
    func({std::declval<THead>(), std::declval<TMid>(),
        std::declval<TTail>()...});
}
LEV_HIDE_INSTANTIATION auto explicit_test(int) noexcept -> std::false_type;

template <typename T, typename THead>
requires (!is_convertible_v<THead, T> && std::is_constructible<T, THead>)
LEV_HIDE_INSTANTIATION auto explicit_test(int) noexcept -> std::true_type;

template <typename T>
requires requires(void (*func)(T)) { func({}); }
LEV_HIDE_INSTANTIATION auto explicit_test(int) noexcept
    -> std::is_default_constructible<T>;

template <typename T, typename... TArgs>
LEV_HIDE_INSTANTIATION auto explicit_test(...) noexcept
    -> std::is_constructible<T, TArgs...>;

template <typename TTarget, typename... TArgs>
using is_explicit = decltype(explicit_test<TTarget, TArgs...>(0));
} // namespace details

template <typename TTarget, typename... TArgs>
LEV_HIDE_INSTANTIATION inline constexpr is_explicit_constructible_v =
    decltype(details::explicit_test<TTarget, TArgs...>(0))::value;

template <typename TTarget, typename... TArgs>
struct LEV_API is_explicit_constructible :
    details::is_explicit<TTarget, TArgs...> {};

template <typename TTarget, typename... TArgs>
struct LEV_API is_nothrow_explicit_constructible :
    conjunction<is_explicit_constructible<TTarget, TArgs...>,
        is_nothrow_constructible<TTarget, TArgs...>> {};

} // namespace lev
