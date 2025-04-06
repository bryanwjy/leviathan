// Copyright 2025, Bryan Wong
#pragma once

#include "utils/type_traits.hpp"
#include "utils/utility.hpp"

#include <Python.h>

namespace lev {

namespace details {

template <typename>
void py_cast(...) noexcept = delete;

template <typename To>
struct py_cast_t {
    LEV_HIDE_INSTANTIATION explicit inline constexpr py_cast_t() noexcept =
        default;

    template <typename From>
    requires std::same_as<remove_cv_t<To>, remove_cv_t<From>>
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::const]] inline constexpr auto
    operator()(From* ptr) const noexcept {
        return static_cast<copy_cv_t<From, To>*>(ptr);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::const]] inline constexpr auto
    operator()(decltype(nullptr) ptr) const noexcept {
        return static_cast<To*>(ptr);
    }

    template <leviathan_pyobj From>
    requires requires(From* ptr) { static_cast<copy_cv_t<From, To>*>(ptr); }
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::const]] inline constexpr auto
    operator()(From* ptr) const noexcept {
        return static_cast<copy_cv_t<From, To>*>(ptr);
    }

    template <pointer_interconvertible_with<To> From>
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::const]] inline auto operator()(
        From* ptr) const noexcept {
        return reinterpret_cast<copy_cv_t<From, To>*>(ptr);
    }

    template <opaque_pyobj T>
    requires requires(T* ptr) {
        { py_cast<To>(ptr) } -> std::same_as<copy_cv_t<T, To>*>;
    }
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
