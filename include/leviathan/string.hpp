// Copyright 2025, Bryan Wong
#pragma once

#include <Python.h>

#include <type_traits>
#include <utility>

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

template <size_t N>
struct literal {
    char const data[N];
    consteval literal(char const (&str)[N]) noexcept
        : literal{str, std::make_index_sequence<N>{}} {}

    consteval operator std::string_view() const noexcept {
        return std::string_view{static_cast<char const*>(data), N - 1};
    }

    consteval explicit operator char const*() const noexcept {
        return static_cast<char const*>(data);
    }

    consteval literal(literal const&) noexcept = default;
    consteval literal& operator=(literal const&) noexcept = default;

private:
    template <size_t... Is>
    requires (sizeof...(Is) == N)
    consteval literal(char const (&str)[N], std::index_sequence<Is...>) noexcept
        : data{str[Is]...} {}
};
} // namespace lev::string::details

namespace lev {
inline namespace literals {
inline namespace string_literals {
template <string::details::pyliteral S>
inline consteval decltype(auto) operator""_pystr() noexcept {
    return std::as_const(static_storage<S>::value);
}

template <string::details::literal S>
inline consteval auto operator""_str() noexcept {
    return S;
}
} // namespace string_literals
} // namespace literals

template <typename T>
LEV_HIDDEN inline constexpr bool is_string_literal_v = false;
template <typename T>
LEV_HIDDEN inline constexpr bool is_pystring_literal_v = false;

template <size_t N>
LEV_HIDDEN inline constexpr bool is_string_literal_v<string::literal<N>> = true;
template <size_t N>
LEV_HIDDEN inline constexpr bool is_pystring_literal_v<string::pyliteral<N>> =
    true;

template <typename T>
concept string_literal = is_string_literal_v<T>;
template <typename T>
concept pystring_literal = is_pystring_literal_v<T>;

class string_hash_t {
    static constexpr uint32_t kPrime = 0x1000193U;
    static constexpr uint32_t kOffsetBasis = 0x811C9DC5U;

    static inline constexpr uint32_t Load(
        char const* data, size_t bytes) noexcept {
        uint32_t chunk = 0U;
        assert(bytes <= sizeof(uint32_t));
        if (std::is_constant_evaluated()) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            for (auto i = 0U; i < bytes; ++i, ++data) {
                chunk |= (static_cast<uint32_t>(*data) << (i * CHAR_BIT));
            }
        } else {
            ::memcpy(&chunk, data, bytes);
        }

        return chunk;
    }

public:
    inline constexpr uint32_t operator()(std::string_view str) const noexcept {
        uint32_t hash = kOffsetBasis;
        // Process 4-byte chunks
        auto offset = 0U;
        while (offset + sizeof(uint32_t) <= str.size()) {
            uint32_t const chunk = Load(str.data() + offset,
                sizeof(uint32_t)); // Copy 4 bytes into chunk
            hash ^= chunk;
            hash *= kPrime;
            offset += sizeof(uint32_t); // Move to the next 4-byte chunk
        }

        // Process any remaining bytes
        str = str.substr(offset);
        if (!str.empty()) {
            uint32_t const last_chunk =
                Load(str.data(), str.size()); // Copy remaining bytes
            hash ^= last_chunk;
            hash *= kPrime;
        }

        return hash;
    }
};

LEV_HIDDEN inline constexpr string_hash_t string_hash{};

} // namespace lev

namespace lev::details {
template <typename>
struct first_member_object_of;
// partial specialization because pyliteral does not meet the requirements
// of an aggregate type, which is required for leviathan's basic pyobject
// hierarchy exploration
template <size_t N>
struct first_member_object_of<::lev::string::pyliteral<N>> {
    using type = PyASCIIObject;
};
} // namespace lev::details
