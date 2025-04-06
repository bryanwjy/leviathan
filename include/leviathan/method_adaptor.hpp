// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/argument_tuple.hpp"
#include "leviathan/conversions.hpp"
#include "leviathan/utility.hpp"

#include <Python.h>

#include <concepts>

namespace lev {

template <typename Obj, size_t I>
struct method_entry {
    friend consteval auto map(method_entry) noexcept;
};

template <size_t V>
struct method_count;

template <typename Obj, size_t V,
    typename = decltype(map(method_entry<Obj, V>{})),
    typename R = decltype(method_count<V>::template recur<Obj>(0))>
LEV_HIDE_INSTANTIATION auto method_count_impl(int) noexcept -> R;

template <typename Obj, size_t V>
LEV_HIDE_INSTANTIATION auto method_count_impl(short) noexcept
    -> size_constant<V>;

template <size_t V>
struct method_count {
    template <typename Obj,
        typename R = decltype(method_count_impl<Obj, V + 1>(0))>
    LEV_HIDE_INSTANTIATION static auto recur(int) noexcept -> R;
    template <typename Obj>
    LEV_HIDE_INSTANTIATION static auto recur(short) noexcept -> void;
};

template <typename Scope, string_literal auto Name, method::decorator auto D,
    typename R, argument::declaration... Args>
struct adapted_method {
    using adaptor_type = Scope;
    using value_type = typename adaptor_type::value_type;
    using module_type = typename adaptor_type::modeule_type;
    using return_type = R;
    using arguments = argument::tuple<Args...>;
    static_assert(convertible_to_pyobject<return_type>,
        "Return type is not convertible to pyobject");
    static_assert((... && convertible_from_pyobject<typename Args::type>),
        "Argument type not convertible from pyobject");

    LEV_HIDE_INSTANTIATION static constexpr auto method_name = Name;
    LEV_HIDE_INSTANTIATION static constexpr method_flags flags = []() {
        auto const static_flag = method::decorated_with<D, static_method>
            ? method_flags::static_method
            : static_cast<method_flags>(0);

        auto const arg_flags = sizeof...(Args) == 0 ? method_flags::no_args
            : sizeof...(Args) == 1                  ? method_flags::one_arg
            : method::decorated_with<D, baisc_call> ? method_flags::var_args
                                                    : method_flags::fastcall;
        auto const kw_flags = sizeof...(Args) > 1
            ? method_flags::keywords
            : static_cast<method_flags>(0);

        return static_flag | arg_flags | kw_flags;
    }();

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        std::span<PyObject* const> args,
        std::span<PyObject* const> kwnames) noexcept
    requires (has_flags<method_flags::keywords, method_flags::fastcall>(flags))
    {
        arguments tuple;
        if (argument::populate(args, kwnames, tuple) != result_code::success) {
            return nullptr;
        }

        if constexpr (method::decorated_with<D, static_method>) {
            return to_pyobject<PyObject>(
                adaptor_type::def<Name, D, R>(std::move(tuple)))
                .release();
        } else {
            return to_pyobject<PyObject>(
                py_cast<adaptor_type>(self)->def<Name, D, R>(std::move(tuple)))
                .release();
        }
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        std::span<PyObject* const> args, PyDictObject* kwargs) noexcept
    requires (has_flags<method_flags::keywords, method_flags::var_args>(flags))
    {
        arguments tuple;
        if (argument::populate(args, kwargs, tuple) != result_code::success) {
            return nullptr;
        }

        if constexpr (method::decorated_with<D, static_method>) {
            return to_pyobject<PyObject>(
                adaptor_type::def<Name, D, R>(std::move(tuple)))
                .release();
        } else {
            return to_pyobject<PyObject>(
                py_cast<adaptor_type>(self)->def<Name, D, R>(std::move(tuple)))
                .release();
        }
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        std::span<PyObject* const> args) noexcept
    requires (!has_flags<method_flags::keywords>(flags))
    {
        arguments tuple;
        if (argument::populate(args, tuple) != result_code::success) {
            return nullptr;
        }

        if constexpr (method::decorated_with<D, static_method>) {
            return to_pyobject<PyObject>(
                adaptor_type::def<Name, D, R>(std::move(tuple)))
                .release();
        } else {
            return to_pyobject<PyObject>(
                py_cast<adaptor_type>(self)->def<Name, D, R>(std::move(tuple)))
                .release();
        }
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject* self, PyObject* arg) noexcept
    requires (has_flags<method_flags::one_arg>(flags))
    {
        arguments tuple;
        if (argument::populate(std::span{&args, 1}, tuple) !=
            result_code::success) {
            return nullptr;
        }

        if constexpr (method::decorated_with<D, static_method>) {
            return to_pyobject<PyObject>(
                adaptor_type::def<Name, D, R>(std::move(tuple)))
                .release();
        } else {
            return to_pyobject<PyObject>(
                py_cast<adaptor_type>(self)->def<Name, D, R>(std::move(tuple)))
                .release();
        }
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(PyObject* self) noexcept
    requires (has_flags<method_flags::no_args>(flags))
    {
        if constexpr (method::decorated_with<D, static_method>) {
            return to_pyobject<PyObject>(adaptor_type::def<Name, D, R>())
                .release();
        } else {
            return to_pyobject<PyObject>(
                py_cast<adaptor_type>(self)->def<Name, D, R>())
                .release();
        }
    }
};

template <typename T, typename I = decltype(method_count_impl<S, 0>(0))>
struct add_adapted_method;

template <typename S, size_t I, string_literal auto Name,
    method::decorator auto D, typename R, argument::declaration... Args>
struct add_adapted_method<adapted_method<S, Name, D, R, Args...>,
    size_constant<I>> : std::true_type {
    static_assert(sizeof(
        type_map<method_entry<S, I>, adapted_method<S, Name, D, R, Args...>>));
};

template <typename Obj, typename R = decltype(method_count_impl<Obj, 0>(0))>
using method_count_t = R;

// DEMO: https://godbolt.org/z/s5fqhfGqK

} // namespace lev
