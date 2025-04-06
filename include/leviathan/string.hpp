// Copyright 2025, Bryan Wong
#pragma once

#include <Python.h>

namespace lev::string::details {
template <size_t N>
struct pyliteral {
    PyASCIIObject ob_base;
    // null-terminated, python strings are immutable, so this can be const
    char const data[N];
    consteval pyliteral(char const (&str)[N]) noexcept
        : pyliteral{str, std::make_index_sequence<N>{}} {}

    consteval operator std::string_view() const noexcept {
        return std::string_view{static_cast<char const*>(data), N - 1};
    }

    consteval pyliteral(pyliteral const&) noexcept = default;
    pyliteral& operator=(pyliteral const&) = delete;

private:
    LEV_HIDE_INSTANTIATION static consteval PyASCIIObject Base() noexcept {
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 12
        return {
            .ob_base = PyObject_HEAD_INIT(         &PyUnicode_Type)
            .length = static_cast<Py_ssize_t>(N - 1),
            .hash = -1,
            .state = { .kind = 1,
                      .compact = 1,
                      .ascii = 1,
                      .statically_allocated = 1}
        };
#elif PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 9
        return {
            .ob_base = PyObject_HEAD_INIT(         &PyUnicode_Type)
            .length = static_cast<Py_ssize_t>(N - 1),
            .hash = -1,
            .state = { .kind = 1, .compact = 1, .ascii = 1, .ready = 1}
        };
#else
#  error Unimplmented
#endif
    }

    template <size_t... Is>
    requires (sizeof...(Is) == N)
    consteval pyliteral(
        char const (&str)[N], std::index_sequence<Is...>) noexcept
        : ob_base{Base()}
        , data{str[Is]...} {}
};
} // namespace lev::string::details

namespace ltl::details {
template <typename>
struct first_member_object_of;
// partial specialization because pyliteral does not meet the requirements
// of an aggregate type, which is required for leviathan's basic pyobject
// hierarchy exploration
template <size_t N>
struct first_member_object_of<::lev::string::pyliteral<N>> {
    using type = PyASCIIObject;
};
} // namespace ltl::details
