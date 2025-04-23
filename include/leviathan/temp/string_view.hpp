// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/common.h"
#include "utils/zstring_view.h"

#include <Python.h>

#include <cstring>

namespace lev::py::temp {

template <typename C>
class basic_string_view;
template <typename C>
class basic_string_view<C> : public PyUnicodeObject {
    LEV_HIDE_INSTANTIATION [[nodiscard]] static constexpr __LTL basic_zstring_view<C>
    empty_str() noexcept
    requires std::same_as<C, char>
    {
        return "";
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] static constexpr __LTL basic_zstring_view<C>
    empty_str() noexcept
    requires std::same_as<C, char8_t>
    {
        return u8"";
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] static constexpr __LTL basic_zstring_view<C>
    empty_str() noexcept
    requires std::same_as<C, char16_t>
    {
        return u"";
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] static constexpr __LTL basic_zstring_view<C>
    empty_str() noexcept
    requires std::same_as<C, char32_t>
    {
        return U"";
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] static constexpr __LTL basic_zstring_view<C>
    empty_str() noexcept
    requires std::same_as<C, wchar_t>
    {
        return L"";
    }

public:
    LEV_HIDE_INSTANTIATION inline constexpr basic_string_view(
        __LTL basic_zstring_view<C> view) noexcept
        : PyUnicodeObject{Base(view.empty() ? empty_str() : view)} {}

    LEV_HIDE_INSTANTIATION constexpr inline
    operator std::add_pointer_t<PyUnicodeObject>() noexcept LEV_LIFETIMEBOUND {
        return this;
    }
    LEV_HIDE_INSTANTIATION constexpr inline operator unmanaged_ptr<PyUnicodeObject>() noexcept LEV_LIFETIMEBOUND {
        return this;
    }

    LEV_HIDE_INSTANTIATION constexpr inline
    operator std::add_pointer_t<PyUnicodeObject>() const noexcept LEV_LIFETIMEBOUND {
        return this;
    }
    LEV_HIDE_INSTANTIATION constexpr inline
    operator unmanaged_ptr<PyUnicodeObject>() const noexcept LEV_LIFETIMEBOUND {
        return this;
    }

    LEV_HIDE_INSTANTIATION ~basic_string_view() noexcept {
        if (Py_REFCNT(reinterpret_cast<PyObject*>(this)) != 1) {
            // Fatal error on leak
            std::terminate();
        }
    }

private:
#if LEV_PYTHON_VERSION_LT(3, 12, 0)
    static consteval int Kind() noexcept
    requires std::same_as<C, wchar_t>
    {
        return 0;
    }
#endif

    static consteval int Kind() noexcept {
        static_assert(sizeof(C) <= 4);
        return sizeof(C);
    }

    LEV_HIDE_INSTANTIATION static constexpr PyASCIIObject AsciiBase(
        __LTL basic_zstring_view<C> str) noexcept {
#if LEV_PYTHON_VERSION_GE(3, 12, 0)
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
#elif LEV_PYTHON_VERSION_GE(3, 9, 0)
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

#if LEV_PYTHON_VERSION_LT(3, 12, 0)
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
#if LEV_PYTHON_VERSION_LT(3, 12, 0)
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

} // namespace lev::py::temp
