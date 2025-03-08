// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/method_traits.hpp"

#include <Python.h>

#include <concepts>
#include <limits>
#include <span>
#include <type_traits>

namespace lev {
namespace native_conversions {
template <typename To>
void from_pyobject(PyObject*) noexcept = delete;

template <pyobj_type To>
python_ptr<To> to_pyobject(...) noexcept = delete;
} // namespace native_conversions

namespace details {
template <typename To>
void from_pyobject(std::in_place_type_t<To>, ...) noexcept = delete;

template <pyobj_type To>
void to_pyobject(std::in_place_type_t<To>, ...) noexcept = delete;

template <typename From, typename To>
concept unqualified_from_pyobject = pyobj_type<From> && requires(From* from) {
    { from_pyobject(std::in_place_type<To>, from) } -> std::same_as<To>;
};

template <typename From, typename To>
concept natively_from_pyobject = pyobj_type<From> && requires(From* from) {
    {
        ::lev::native_conversions::from_pyobject<To>(from)
    } noexcept -> std::same_as<To>;
};

template <typename From, typename To>
concept unqualified_to_pyobject = pyobj_type<To> && requires(From&& from) {
    {
        to_pyobject(std::in_place_type<To>, std::forward<From>(from))
    } noexcept -> std::same_as<python_ptr<To>>;
};

template <typename From, typename To>
concept natively_to_pyobject = pyobj_type<To> && requires(From&& from) {
    {
        ::lev::native_conversions::to_pyobject<To>(std::forward<From>(from))
    } noexcept -> std::same_as<python_ptr<To>>;
};

template <typename To>
struct from_pyobject_t {

    LEV_HIDE_INSTANTIATION explicit inline constexpr from_pyobject_t() noexcept =
        default;

    template <unqualified_from_pyobject<To> From>
    LEV_HIDE_INSTANTIATION To operator()(From* obj) const
        noexcept(noexcept(from_pyobject(std::in_place_type<To>, obj))) {
        return static_cast<To>(from_pyobject(std::in_place_type<To>, obj));
    }

    template <natively_from_pyobject<To> From>
    requires (!unqualified_from_pyobject<From, To>)
    LEV_HIDE_INSTANTIATION To operator()(From* obj) const noexcept {
        return static_cast<To>(
            ::lev::native_conversions::from_pyobject<To>(obj));
    }
};

template <pyobj_type To>
struct to_pyobject_t {

    LEV_HIDE_INSTANTIATION explicit inline constexpr to_pyobject_t() noexcept =
        default;

    template <unqualified_from_pyobject<To> From>
    LEV_HIDE_INSTANTIATION python_ptr<To> operator()(
        From&& obj) const noexcept {
        return to_pyobject(std::in_place_type<To>, std::forward<From>(obj));
    }

    template <natively_to_pyobject<To> From>
    requires (!unqualified_to_pyobject<From, To>)
    LEV_HIDE_INSTANTIATION To operator()(From* obj) const noexcept {
        return static_cast<To>(::lev::native_conversions::to_pyobject<To>(obj));
    }
};

} // namespace details
inline namespace cpo {
template <typename To>
LEV_HIDE_INSTANTIATION inline constexpr auto from_pyobject =
    details::from_pyobject_t<To>{};

template <pyobj_type To>
LEV_HIDE_INSTANTIATION inline constexpr auto to_pyobject =
    details::from_pyobject_t<To>{};
} // namespace cpo

template <typename From>
concept convertible_to_pyobject = requires(
    From&& from) { cpo::to_pyobject<PyObject>(std::forward<From>(from)); };

template <typename To>
concept convertible_from_pyobject =
    requires(PyObject* from) { cpo::from_pyobject<To>(from); };

namespace native_conversions {

LEV_HIDDEN [[gnu::noinline]] inline void raise_invalid_type(
    unmanaged_ptr<PyObject> ptr) noexcept {
    PyErr_Format(PyExc_TypeError,
        "Argument conversion failed, Reason=[Unexpected argument type], "
        "Type=[%s]",
        ptr ? Py_TYPE(ptr)->tp_name : "nullptr");
}

template <pyobj_type T>
requires (!std::same_as<PyObject> &&
    requires(unmanaged_ptr<PyObject> ptr) { dynamic_ptr_cast(ptr); })
LEV_HIDDEN inline T* from_pyobject(unmanaged_ptr<PyObject> ptr) noexcept {
    return dynamic_ptr_cast<T>(ptr);
}

template <std::same_as<PyObject> T>
LEV_HIDDEN inline T* from_pyobject(unmanaged_ptr<T> ptr) noexcept {
    return ptr;
}

template <typename T>
LEV_HIDDEN inline T from_pyobject(unmanaged_ptr<PyUnicodeObject> src) noexcept;

template <>
LEV_HIDDEN inline T from_pyobject<std::string_view>(
    unmanaged_ptr<PyUnicodeObject> src) noexcept {
    if (PyUnicode_KIND(src) == PyUnicode_1BYTE_KIND) [[likely]] {
        Py_ssize_t size = 0;
        auto str = PyUnicode_AsUTF8AndSize(src, &size);
        return std::string_view{
            str, static_cast<size_t>(std::max<Py_ssize_t>(0, size))};
    }

    return {};
}

template <>
LEV_HIDDEN inline char from_pyobject<char>(
    unmanaged_ptr<PyUnicodeObject> src) noexcept {
    if (PyUnicode_KIND(src) == PyUnicode_1BYTE_KIND) {
        return *PyUnicode_AsUTF8(src);
    }

    return '\0';
}

template <std::floating_point T>
LEV_HIDDEN inline T from_pyobject(unmanaged_ptr<PyFloatObject> src) noexcept {
    return static_cast<T>(PyFloat_AS_DOUBLE(src));
}

template <std::floating_point T>
LEV_HIDDEN inline T from_pyobject(unmanaged_ptr<PyObject> ptr) noexcept {
    auto value = PyFloat_AsDouble(src);
    if (value == -1.0 && PyErr_Occurred()) {
        return std::numeric_limits<T>::quiet_NaN();
    }

    return static_cast<T>(value);
}

template <typename T>
LEV_HIDDEN inline T from_pyobject(unmanaged_ptr<PyObject> ptr) noexcept;

template <>
LEV_HIDDEN inline std::string_view from_pyobject<std::string_view>(
    unmanaged_ptr<PyObject> src) noexcept {
    if (auto str = dynamic_ptr_cast<PyUnicodeObject>(src)) {
        return from_pyobject<T>(str);
    }

    auto str = static_ptr_cast<PyUnicodeObject>(
        python_ptr{retain_object, PyObject_Str(src)});
    if (str) {
        return from_pyobject<T>(str.get());
    }

    raise_invalid_type(src);
    return {};
}

template <>
LEV_HIDDEN inline char from_pyobject<char>(
    unmanaged_ptr<PyObject> src) noexcept {
    if (auto ptr = dynamic_ptr_cast<PyLongObject>(src)) {
        return from_pyobject<char>(ptr);
    }

    if (auto ptr = dynamic_ptr_cast<PyUnicodeObject>(src)) {
        return from_pyobject<char>(ptr);
    }

    raise_invalid_type(src);
    return '\0';
}

template <>
LEV_HIDDEN inline bool from_pyobject<bool>(
    unmanaged_ptr<PyObject> src) noexcept {
    return PyObject_IsTrue(src);
}

template <std::unsigned_integral T>
LEV_HIDDEN inline T from_pyobject(unmanaged_ptr<PyLongObject> src) noexcept {
    static constexpr auto kInvalid = static_cast<unsigned long long>(-1);
    auto value = PyLong_AsUnsignedLongLong(src);
    if (value == kInvalid && PyErr_Occurred()) {
        return false;
    }

    return static_cast<To>(value);
}

template <std::signed_integral T>
LEV_HIDDEN inline T from_pyobject(unmanaged_ptr<PyLongObject> src) noexcept {
    static constexpr auto kInvalid = static_cast<signed long long>(-1);
    auto value = PyLong_AsLongLong(src);
    if (value == kInvalid && PyErr_Occurred()) {
        return false;
    }

    return static_cast<To>(value);
}

template <std::integral T>
LEV_HIDDEN inline T from_pyobject(unmanaged_ptr<PyObject> src) noexcept {
    if (auto ptr = dynamic_ptr_cast<PyLongObject>(src)) {
        return from_pyobject<T>(ptr);
    }

    raise_invalid_type(src);
    return static_cast<T>(-1);
}

template <typename>
LEV_HIDDEN inline constexpr bool is_variant_v = false;
template <typename... Ts>
LEV_HIDDEN inline constexpr bool is_variant_v<std::variant<Ts...>> = true;

namespace details {
template <typename Type, typename... Ts>
LEV_HIDDEN inline bool try_emplace(
    std::variant<Ts...>& variant, unmanaged_ptr<PyObject> ptr) noexcept {
    static_assert(noexcept(variant.emplace<Type>(from_pyobject<Type>(ptr))),
        "Variant Types must be nothrow convertible from pyobject");

    variant.emplace<Type>(from_pyobject<Type>(ptr));
    if (!PyErr_Occurred()) {
        return true;
    }

    PyErr_Clear();
    return false;
}

} // namespace details

template <typename T>
requires (is_variant_v<T> && std::is_nothrow_default_constructible_v<T>)
LEV_HIDDEN inline T from_pyobject(unmanaged_ptr<PyObject> src) noexcept {
    T output;
    auto success = [&]<size_t I, size_t... Is>(std::index_sequence<I, Is...>) {
        if constexpr (!std::same_as<std::variant_alternative_t<0, T>,
                          std::monostate>) {
            if (details::try_emplace<std::variant_alternative_t<I, T>>(
                    output, src)) {
                return true;
            }
        }

        return (... ||
            details::try_emplace<std::variant_alternative_t<Is, T>>(
                output, src));
    }(std::make_index_sequence<std::variant_size_v<T>>{});

    if (!success) {
        raise_invalid_type(src);
    }

    return T{};
}

} // namespace native_conversions
} // namespace lev
