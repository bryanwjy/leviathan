// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/common.h"

#include <string_view>

namespace lev {
namespace py {

template <typename C>
class basic_unicode;

template <typename C>
requires __LTL is_complete_v<__LTL basic_zstring_view<C>>
class basic_unicode {};

template <typename C>
struct basic_string_view;

template <typename C>
class basic_string_view<C> : public PyUnicodeObject {
public:
    LEV_HIDE_INSTANTIATION inline constexpr basic_string_view(
        __LTL basic_zstring_view<C> view) noexcept
        : PyUnicodeObject{Base(view.size())} {
        LEV_CONTRACT_ASSERT(PyUnicode_DATA(this) != nullptr);
    }

    LEV_HIDE_INSTANTIATION constexpr inline operator PyUnicodeObject*() { return this; }

    LEV_HIDE_INSTANTIATION constexpr ~basic_string_view() noexcept {
        if (Py_REFCNT(reinterpret_cast<PyObject*>(this)) != 1) {
            std::terminate();
        }
    }

private:
#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 12
    static consteval int Kind() noexcept
    requires std::same_as<C, wchar_t>
    {
        return 0;
    }
#endif

    static consteval int Kind() noexcept {
        static_assert(sizeof(C) < 4);
        return sizeof(C);
    }

    LEV_HIDE_INSTANTIATION static constexpr PyASCIIObject AsciiBase(
        __LTL basic_zstring_view<C> str) noexcept {
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 12
        return {
            .ob_base = PyObject_HEAD_INIT(&PyUnicode_Type)
            .length = static_cast<Py_ssize_t>(length),
            .hash = -1,
            .state = {
                      .kind = Kind(),
                      .compact = 0,
                      .ascii = std::same_as<C, char>,
                      .statically_allocated = 1
            }
        };
#elif PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 9
        return {
            .ob_base = PyObject_HEAD_INIT(              &PyUnicode_Type)
            .length = static_cast<Py_ssize_t>(length),
            .hash = -1,
            .state = { .kind = Kind(),
                      .compact = 0,
                      .ascii = std::same_as<C, char>,
                      .ready = 1},
            .wstr = std::same_as<C, wchar_t> ? str.data() : nullptr,
        };
#else
#  error Unimplmented
#endif
    }

    LEV_HIDE_INSTANTIATION static constexpr PyCompactUnicodeObject CompactBase(
        __LTL basic_zstring_view<C> str) noexcept {

#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 12
        return {
            ._base = AsciiBase(str),
            .utf8_length =
                sizeof(C) == 1 && !std::same_as<C, wchar_t> ? length : 0,
            .utf8 = sizeof(C) == 1 && !std::same_as<C, wchar_t> ? str.data()
                                                                : nullptr,

            .wstr_length = std::same_as<C, wchar_t> ? str.size() : 0,
        };
#else
        return {
            ._base = AsciiBase(str),
            .utf8_length = sizeof(C) == 1 ? length : 0,
            .utf8 = sizeof(C) == 1 ? str.data() : nullptr,
        };
#endif
    }

    LEV_HIDE_INSTANTIATION static constexpr PyUnicodeObject Base(
        __LTL basic_zstring_view<C> str) noexcept {
#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 12
        return {
            ._base = CompactBase(str),
            .data = {.any = std::same_as<C, wchar_t> ? nullptr : str.data()},
        };
#else
        return {
            ._base = CompactBase(str),
            .data = {.any = str.data()},
        };
#endif
    }
};

} // namespace py
} // namespace lev
