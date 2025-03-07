// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/method_traits.hpp"
#include "leviathan/utility.hpp"

#include <Python.h>

#include <concepts>
#include <span>
#include <type_traits>

namespace lev {

template <auto Func>
class LEV_API cxx_function_adaptor;

template <auto Func>
requires requires {
    { method_flag_of<Func>::value } -> std::same_as<method_flags const&>;
}
class LEV_API cxx_function_adaptor<Func> : public ::PyMethodDef {
    static constexpr method_flags flags = method_flag_of<Func>::value;

    template <typename... Args>
    LEV_HIDE_INSTANTIATION static PyObject* invoke_on_self(
        PyObject* self, Args... args) noexcept {
        return (py_cast<details::ClassOf_t<decltype(Func)>>(self)->*Func)(
            args...);
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject* self, PyObject* args, PyObject* kwargs) noexcept
    requires (flags == (method_flags::var_args | method_flags::keywords))
    {
        return invoke_on_self(self, to_span(py_cast<PyTupleObject>(args)),
            py_cast<PyDictObject>(kwargs));
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject* self, PyObject* args) noexcept
    requires (flags == method_flags::var_args)
    {
        return invoke_on_self(self, to_span(py_cast<PyTupleObject>(args)));
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject* self, PyObject* arg) noexcept
    requires (flags == method_flags::one_arg)
    {
        return invoke_on_self(self, arg);
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject* self, PyObject*) noexcept
    requires (flags == method_flags::no_args)
    {
        return invoke_on_self(self);
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(PyObject* self,
        PyObject* const* args, Py_ssize_t nargs, PyObject* kwnames) noexcept
    requires (flags == (method_flags::fastcall | method_flags::keywords))
    {
        std::span args_span{args, args + nargs};
        if (kwnames != nullptr) {
            return invoke_on_self(
                self, args_span, to_span(py_cast<PyTupleObject>(kwnames)));
        }

        return invoke_on_self(self, args_span, std::span<PyObject* const>{});
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject* self, PyObject* const* args, Py_ssize_t nargs) noexcept
    requires (flags == method_flags::fastcall)
    {
        std::span<PyObject* const> args_span{args, static_cast<size_t>(nargs)};
        return invoke_on_self(self, args_span);
    }

    /* STATIC */

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject*, PyObject* args, PyObject* kwargs) noexcept
    requires (flags ==
        (method_flags::static_method | method_flags::var_args |
            method_flags::keywords))
    {
        return Func(to_span(py_cast<PyTupleObject>(args)),
            py_cast<PyDictObject>(kwargs));
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject*, PyObject* args) noexcept
    requires (flags == method_flags::static_method | method_flags::var_args)
    {
        return Func(to_span(py_cast<PyTupleObject>(args)));
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject*, PyObject* arg) noexcept
    requires (flags == method_flags::static_method | method_flags::one_arg)
    {
        return Func(arg);
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject*, PyObject*) noexcept
    requires (flags == method_flags::static_method | method_flags::no_args)
    {
        return Func();
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(PyObject*,
        PyObject* const* args, Py_ssize_t nargs, PyObject* kwnames) noexcept
    requires (flags ==
        (method_flags::static_method | method_flags::fastcall |
            method_flags::keywords))
    {
        std::span args_span{args, args + nargs};
        if (kwnames != nullptr) {
            return Func(args_span, to_span(py_cast<PyTupleObject>(kwnames)));
        }

        return Func(args_span, std::span<PyObject* const>{});
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject*, PyObject* const* args, Py_ssize_t nargs) noexcept
    requires (flags == method_flags::static_method | method_flags::fastcall)
    {
        return Func(
            std::span<PyObject* const>{args, static_cast<size_t>(nargs)});
    }

    /* CLASS */

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject* type, PyObject* args, PyObject* kwargs) noexcept
    requires (flags ==
        (method_flags::class_method | method_flags::var_args |
            method_flags::keywords))
    {
        return Func(py_cast<PyTypeObject>(type),
            to_span(py_cast<PyTupleObject>(args)),
            py_cast<PyDictObject>(kwargs));
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject* type, PyObject* args) noexcept
    requires (flags == method_flags::class_method | method_flags::var_args)
    {
        return Func(
            py_cast<PyTypeObject>(type), to_span(py_cast<PyTupleObject>(args)));
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject* type, PyObject* arg) noexcept
    requires (flags == method_flags::class_method | method_flags::one_arg)
    {
        return Func(py_cast<PyTypeObject>(type), arg);
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject* type, PyObject*) noexcept
    requires (flags == method_flags::class_method | method_flags::no_args)
    {
        return Func(py_cast<PyTypeObject>(type));
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(PyObject* type,
        PyObject* const* args, Py_ssize_t nargs, PyObject* kwnames) noexcept
    requires (flags ==
        (method_flags::class_method | method_flags::fastcall |
            method_flags::keywords))
    {
        std::span args_span{args, args + nargs};
        if (kwnames != nullptr) {
            Func(py_cast<PyTypeObject>(type), args_span,
                to_span(py_cast<PyTupleObject>(kwnames)));
        }

        return Func(py_cast<PyTypeObject>(type), args_span,
            std::span<PyObject* const>{});
    }

    LEV_HIDE_INSTANTIATION static PyObject* invoke(
        PyObject*, PyObject* const* args, Py_ssize_t nargs) noexcept
    requires (flags == method_flags::class_method | method_flags::fastcall)
    {
        return Func(
            std::span<PyObject* const>{args, static_cast<size_t>(nargs)});
    }

public:
    template <size_t N, size_t M>
    requires std::same_as<decltype(&cxx_function_adaptor::invoke), PyCFunction>
    LEV_HIDE_INSTANTIATION constexpr explicit cxx_function_adaptor(
        char const* name, char const* doc) noexcept
        : PyMethodDef{name, &cxx_function_adaptor::invoke, flags, doc} {}

    LEV_HIDE_INSTANTIATION explicit cxx_function_adaptor(
        char const* name, char const* doc) noexcept
        : PyMethodDef{name,
              (PyCFunction)&cxx_function_adaptor::invoke, // NOLINT
              flags, doc} {}
};

template <method_flags Flags, auto Func>
requires (!has_flags<method_flags::class_method>(Flags))
using static_function_adaptor =
    cxx_function_adaptor<Flags | method_flags::static_method, Func>;
template <method_flags Flags, auto Func>
requires (!has_flags<method_flags::static_method>(Flags))
using class_function_adaptor =
    cxx_function_adaptor<Flags | method_flags::class_method, Func>;
template <method_flags Flags, auto Func>
requires (!has_flags<method_flags::static_method, method_flags::class_method>(
             Flags))
using instance_function_adaptor = cxx_function_adaptor<Flags, Func>;

} // namespace lev
