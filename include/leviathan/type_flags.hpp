// Copyright 2025, Bryan Wong
#pragma once

#include <Python.h>

namespace lev {

namespace details {

struct type_flags {
    enum LEV_API values : int {
        heap_type = Py_TPFLAGS_HEAPTYPE,
        base_type = Py_TPFLAGS_BASETYPE,
        ready = Py_TPFLAGS_READY,
        readying = Py_TPFLAGS_READYING,
        gc_type = Py_TPFLAGS_HAVE_GC,
        default_values = Py_TPFLAGS_DEFAULT,
        method_descriptor = Py_TPFLAGS_METHOD_DESCRIPTOR,
        managed_dict = Py_TPFLAGS_MANAGED_DICT,
        managed_weakref = Py_TPFLAGS_MANAGED_WEAKREF,
        items_at_end = Py_TPFLAGS_ITEMS_AT_END,
        long_subclass = Py_TPFLAGS_LONG_SUBCLASS,
        list_subclass = Py_TPFLAGS_LIST_SUBCLASS,
        tuple_subclass = Py_TPFLAGS_TUPLE_SUBCLASS,
        bytes_subclass = Py_TPFLAGS_BYTES_SUBCLASS,
        unicode_subclass = Py_TPFLAGS_UNICODE_SUBCLASS,
        dict_subclass = Py_TPFLAGS_DICT_SUBCLASS,
        base_exc_subclass = Py_TPFLAGS_BASE_EXC_SUBCLASS,
        type_subclass = Py_TPFLAGS_TYPE_SUBCLASS,
        have_vectorcall = Py_TPFLAGS_HAVE_VECTORCALL,
        immutable_type = Py_TPFLAGS_IMMUTABLETYPE,
        disallow_instantiation = Py_TPFLAGS_DISALLOW_INSTANTIATION,
        mapping = Py_TPFLAGS_MAPPING,
        sequence = Py_TPFLAGS_SEQUENCE
    };

    LEV_HIDDEN friend constexpr values operator|(
        values lhs, values rhs) noexcept {
        return static_cast<values>(
            static_cast<int>(lhs) | static_cast<int>(rhs));
    }

    template <values V>
    LEV_HIDDEN friend constexpr bool has_flag(values val) noexcept {
        return static_cast<values>(
                   static_cast<int>(val) & static_cast<int>(V)) == all;
    }

    template <values... Flags>
    LEV_HIDDEN friend constexpr bool has_flags(values val) noexcept {
        auto const all = (... | Flags);
        return static_cast<values>(
                   static_cast<int>(val) & static_cast<int>(all)) == all;
    }
};

} // namespace details

using type_flags = details::type_flags::values;

} // namespace lev
