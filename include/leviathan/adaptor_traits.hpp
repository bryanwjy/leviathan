// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/instantiation.hpp"
#include "leviathan/result.hpp"

#include <Python.h>

#include <concepts>
#include <new>

namespace lev {

template <typename Adaptor>
LEV_HIDDEN inline constexpr type_flags type_flags_v = []() {
    static_assert(
        basic_instantiable<Adaptor> || vectorcall_instantiable<Adaptor>,
        "Undefined type flags");
    return type_flags::default_values | type_flags::immutable_type;
}();

template <uninstantiable Adaptor>
LEV_HIDDEN inline constexpr type_flags type_flags_v<Adaptor> =
    type_flags::default_values | type_flags::immutable_type |
    type_flags::disallow_instantiation;

// C++ classes (exlcuding fundamental types) must have an adaptor
// All adaptors are leviathan pyobjects (or opaque objects)
template <typename Adaptor>
struct adaptor_traits;

namespace details {

template <basic_instantiable Adaptor>
LEV_HIDDEN [[nodiscard]] void* default_allocate(
    PyTypeObject* type, Py_ssize_t) noexcept {
    // The size parameter is unused; leviathan does not implement VarObj
    // (intrusive array types) by default.
    auto ptr = new (std::nothrow) Adaptor{};
    if (!ptr) {
        return PyErr_NoMemory();
    }

    return ptr;
}

template <typename Adaptor>
LEV_HIDDEN [[nodiscard]] PyObject* default_new(
    PyTypeObject* type, PyObject*, PyObject*) noexcept {
    return type->tp_alloc(type, 0);
}

LEV_HIDDEN inline void default_deallocate(PyObject* self) noexcept {
    python_ptr type{retain_object, Py_TYPE(self)};
    type->tp_free(self);
}

template <typename Adaptor>
LEV_HIDDEN void default_free(void* ptr) noexcept {
    delete ptr;
}

template <typename Adaptor, typename = typename Adaptor::instantiation_concept>
LEV_HIDDEN inline constexpr allocfunc select_allocator = nullptr;
template <typename Adaptor>
LEV_HIDDEN inline constexpr allocfunc
    select_allocator<Adaptor, basic_instantiation> = &default_allocate<Adaptor>;

template <typename Adaptor, typename = typename Adaptor::instantiation_concept>
LEV_HIDDEN inline constexpr newfunc select_new = nullptr;
template <typename Adaptor>
LEV_HIDDEN inline constexpr newfunc select_new<Adaptor, basic_instantiation> =
    &default_new;

LEV_HIDDEN inline constexpr PyMethodDef empty_method_table[1] = {{}};

LEV_HIDDEN inline constexpr std::span<PyObject* const> empty_pyobj_span{};

template <typename Adaptor>
LEV_HIDDEN inline constexpr initproc select_initialize = nullptr;

template <basic_instantiable Adaptor>
requires requires(Adaptor* ptr, initializer_arguments_t<Adaptor> tuple) {
    typename initializer_arguments_t<Adaptor>;
    requires !std::is_void_v<initializer_arguments_t<Adaptor>>;
    {
        initialize<Adaptor>(ptr, std::move(tuple))
    } noexcept -> std::same_as<result_code>;
}
LEV_HIDDEN inline constexpr initproc select_initialize<Adaptor> =
    [](PyObject* self, PyObject* args, PyObject* kwargs) noexcept -> int {
        initializer_arguments_t<Adaptor> tuple;
        auto arg_span = to_span(py_cast<PyTupleObject>(args));
        auto kw = py_cast<PyDictObject>(kwargs);

        if (argument::populate(arg_span, kw, tuple) != result_code::success) {
            return result_code::failed;
        }

        return initialize<Adaptor>(py_cast<Adaptor>(self), std::move(tuple));
    };

template <basic_instantiable Adaptor>
requires requires(Adaptor* self) {
    typename initializer_arguments_t<Adaptor>;
    requires std::is_void_v<initializer_arguments_t<Adaptor>>;
    {
        initialize<AdaptAdaptororType>(self)
    } noexcept -> std::same_as<result_code>;
}
LEV_HIDDEN inline constexpr initproc select_initialize<Adaptor> =
    [](PyObject* self, PyObject* args, PyObject* kwargs) noexcept -> int {
        auto kw = py_cast<PyDictObject>(kwargs);
        if (auto arg_span = to_span(py_cast<PyTupleObject>(args));
            kw || !arg_span.empty()) {
            auto const size =
                arg_span.size() + (kwargs ? PyDict_Size(kwargs) : 0);
            PyErr_Format(PyExc_ValueError,
                "Invocation failed, Reason=[Unexpected argument count], "
                "Expected=[0], Received=[%zu]",
                size);
            return result_code::failed;
        }

        return initialize<Adaptor>(py_cast<Adaptor>(self));
    };

template <typename Adaptor>
LEV_HIDDEN inline constexpr vectorcallfunc select_vectorcall_initialize =
    nullptr;

template <vectorcall_instantiable Adaptor>
requires requires {
    typename initializer_arguments_t<Adaptor>;
    requires !std::is_void_v<initializer_arguments_t<Adaptor>>;
} && requires(initializer_arguments_t<Adaptor> tuple) {
    {
        initialize<Adaptor>(std::move(tuple))
    } noexcept -> std::same_as<python_ptr<Adaptor>>;
}
LEV_HIDDEN inline constexpr vectorcallfunc
    select_vectorcall_initialize<Adaptor> =
        [](PyObject*, PyObject* const* args, Py_ssize_t nargsf,
            PyObject* kwnames) noexcept -> PyObject* {
            auto const nargs = static_cast<size_t>(PyVectorcall_NARGS(nargsf));

            auto const arg_span = std::span{args, nargs};
            auto const kwspan = to_span(py_cast<PyTupleObject>(kwnames));
            initializer_arguments_t<Adaptor> tuple;
            if (argument::populate(arg_span, kwspan, tuple) !=
                result_code::success) {
                return nullptr;
            }

            return initialize<Adaptor>(std::move(tuple)).release();
        };

template <vectorcall_instantiable Adaptor>
requires requires {
    typename initializer_arguments_t<Adaptor>;
    requires std::is_void_v<initializer_arguments_t<Adaptor>>;
    { initialize<Adaptor>() } noexcept -> std::same_as<python_ptr<Adaptor>>;
}
LEV_HIDDEN inline constexpr vectorcallfunc
    select_vectorcall_initialize<Adaptor> =
        [](PyObject*, PyObject* const* args, Py_ssize_t nargsf,
            PyObject* kwnames) noexcept -> PyObject* {
            auto const nargs = static_cast<size_t>(PyVectorcall_NARGS(nargsf));
            if (auto names = to_span(py_cast<PyTupleObject>(kwnames));
                nargs > 0 || !names.empty()) {
                auto const size = nargs + names.size();
                PyErr_Format(PyExc_ValueError,
                    "Invocation failed, Reason=[Unexpected argument count], "
                    "Expected=[0], Received=[%zu]",
                    size);
                return result_code::failed;
            }

            return initialize<Adaptor>().release();
        };

template <typename Adaptor>
LEV_HIDDEN inline constexpr getattrofunc select_get_attribute = nullptr;

template <typename Adaptor>
requires requires(Adaptor const& obj, python_ptr<PyObject> key) {
    {
        obj.get_attribute(std::move(key)) noexcept
    } -> std::same_as<python_ptr<PyObject>>;
}
LEV_HIDDEN inline constexpr getattrofunc select_get_attribute<Adaptor> =
    [](PyObject* obj, PyObject* key) -> PyObject* {
        return py_cast<Adaptor>(obj)
            ->get_attribute(python_ptr{retain_object, key})
            .release();
    };

template <typename Adaptor>
LEV_HIDDEN inline constexpr setattrofunc select_set_attribute = nullptr;

template <typename Adaptor>
requires requires(Adaptor& obj, PyObject* key, python_ptr<PyObject> value) {
    {
        obj.set_attribute(key, std::move(value))
    } noexcept -> std::same_as<result_code>;
    { obj.remove_attribute(key) } noexcept -> std::same_as<result_code>;
}
LEV_HIDDEN inline constexpr setattrofunc select_set_attribute<Adaptor> =
    [](PyObject* obj, PyObject* key, PyObject* value) -> int {
        if (value) {
            return py_cast<Adaptor>(obj)->set_attribute(key,
            value ? python_ptr{adopt_object, value});
        } else {
            return py_cast<Adaptor>(obj)->remove_attribute(key);
        }
    };

template <typename Adaptor>
concept hashable_adaptor =
    requires {
        requires leviathan_pyobj<Adaptor>;
        typename Adaptor::element_type;
        requires std::convertible_to<Adaptor const&,
            typename Adaptor::element_type const&>;
    } &&
    requires(
        std::hash<typename Adaptor::element_type> hash, Adaptor const& val) {
        {
            hash(static_cast<typename Adaptor::element_type const&>(val))
        } -> std::convertible_to<size_t>;
    };

template <typename Adaptor>
LEV_HIDDEN inline constexpr hashfunc select_hash_function = nullptr;
template <hashable_adaptor Adaptor>
LEV_HIDDEN inline constexpr hashfunc select_hash_function<Adaptor> =
    [](PyObject* obj) -> Py_hash_t {
    static constexpr std::hash<typename Adaptor::element_type> hasher;
    using element_type = typename Adaptor::element_type;
    auto adaptor = py_cast<Adaptor>(obj);
    return adaptor ? static_cast<Py_hash_t>(
                         hasher(static_cast<element_type const&>(*adaptor)))
                   : -1;
};

} // namespace details

template <leviathan_pyobj Adaptor>
struct LEV_API adaptor_traits<Adaptor> {
    using element_type = typename Adaptor::element_type;
    using instantiation_concept = typename Adaptor::instantiation_concept;
    using hashable = bool_constant<select_hash_function<Adaptor> != nullptr>;

    template <typename... Args>
    requires std::is_constructible_v<Adaptor, Args...>
    LEV_HIDE_INSTANTIATION [[nodiscard]] static python_ptr<Adaptor> Create(
        Args&&... args) noexcept {
        LEV_TRY {
            return python_ptr{
                retain_object, new Adaptor{std::forward<Args>(args)...}};
        } LEV_CATCH(std::exception const& error) {
            PyErr_Format(PyExc_TypeError,
                "Internal initialization failed for object '%s', "
                "Reason=[Exception caught], Message=[%s]",
                type_name(), error.what());
            return nullptr;
        } LEV_CATCH(...) {
            PyErr_Format(PyExc_TypeError,
                "Internal initialization failed for object '%s', "
                "Reason=[Unknown exception caught]",
                type_name());
            return nullptr;
        }
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] static consteval char const*
    type_name() noexcept {
        static_assert(
            requires {
                { Adaptor::type_name() } -> std::same_as<char const*>;
            }, "Adaptors must be named");
        return Adaptor::type_name();
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] static constexpr PyMethodDef*
    method_descriptors() noexcept {
        if constexpr (requires {
                          {
                              Adaptor::method_descriptors()
                          } -> std::same_as<PyMethodDef*>;
                      }) {
            return Adaptor::method_descriptors();
        } else {
            return details::empty_method_table;
        }
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] static constexpr PyMemberDef*
    member_descriptors() noexcept {
        if constexpr (requires {
                          {
                              Adaptor::member_descriptors()
                          } -> std::same_as<PyMemberDef*>;
                      }) {
            static_assert(std::is_standard_layout_v<Adaptor>,
                "Types with python members must be standard_layout");
            return Adaptor::member_descriptors();
        } else {
            return nullptr;
        }
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] static constexpr PyTypeObject*
    type_object() noexcept requires {
        { Adaptor::type_object() } noexcept -> std::same_as<PyTypeObject*>;
    } {
        return Adaptor::type_object();
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] static PyTypeObject*
    type_object() noexcept {
        static PyTypeObject type_obj = {
            .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
            .tp_name = type_name(),
            .tp_basicsize = sizeof(Adaptor),
            .tp_dealloc = &details::default_deallocate,
            .tp_getattro = details::select_get_attribute<Adaptor>,
            .tp_setattro = details::select_set_attribute<Adaptor>,
            .tp_hash = details::select_hash_function<Adaptor>,
            .tp_flags = type_flags_v<Adaptor>,
            .tp_doc = type_documentation(),
            .tp_methods = method_descriptors(),
            .tp_members = member_descriptors(),
            .tp_init = details::select_initialize<Adaptor>,
            .tp_alloc = details::select_allocator<Adaptor>,
            .tp_new = details::select_new<Adaptor>,
            .tp_free = &details::default_free,
            .tp_vectorcall = details::select_vectorcall_initialize<Adaptor>
        };
        return &type_obj;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] static python_ptr<PyTypeObject>
    type_object(adopt_t tag) noexcept {
        return python_ptr{tag, type_object()};
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] static consteval char const*
    type_documentation() noexcept {
        if constexpr (requires {
                          {
                              Adaptor::type_documentation()
                          } -> std::same_as<char const*>;
                      }) {
            return Adaptor::type_documentation();
        } else {
            return "Undocumented type";
        }
    }
};

} // namespace lev
