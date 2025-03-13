// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/string.hpp"

#include <Python.h>

#include <concepts>
#include <ranges>

namespace lev::argument {
namespace details {

class argument_root {
protected:
    LEV_HIDDEN constexpr ~argument_root() noexcept = default;
};

template <auto str>
class argument_base;

template <pystring_literal auto str>
class argument_base<str> : public argument_root {
    using source_type = static_storage<str>;

public:
    LEV_HIDE_INSTANTIATION [[nodiscard]] explicit
    operator PyASCIIObject*() const noexcept {
        return reinterpret_cast<PyASCIIObject*>(&source_type::value);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] explicit
    operator PyObject*() const noexcept {
        return reinterpret_cast<PyObject*>(&source_type::value);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] constexpr explicit
    operator std::string_view() const noexcept {
        return source_type::value;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] constexpr explicit
    operator char const*() const noexcept {
        return static_cast<char const*>(source_type::value.data);
    }

    consteval argument_base() noexcept = default;

protected:
    LEV_HIDE_INSTANTIATION constexpr ~argument_base() noexcept = default;
};

template <auto PyStr>
class argument : public argument_base<PyStr> {
public:
    using argument_base<PyStr>::argument_base;
};

template <auto PyStr>
class optional_argument : public argument_base<PyStr> {
public:
    using argument_base<PyStr>::argument_base;
};

void raise_count_error(size_t needed, size_t opt, size_t received) noexcept {
    PyErr_Format(PyExc_ValueError,
        "Invocation failed, Reason=[Unexpected argument count], "
        "Expected=[%zu + %zu optional], Received=[%zu]",
        needed, opt, received);
}

} // namespace details

struct opt_t {
    explicit consteval opt_t() noexcept = default;
};

template <typename Base>
struct optional;

template <auto str, typename T>
struct named;

template <typename... T>
struct type;

template <typename T, typename... Ts>
struct type {
    explicit consteval type() noexcept = default;

    template <pystring_literal auto str>
    consteval named<str, type> operator()(
        details::argument<str>) const noexcept {
        return named<str, type>{};
    }

    consteval optional<type> operator=(opt_t) noexcept {
        return optional<type>{};
    }
};

template <pystring_literal auto str, typename T, typename... Ts>
struct named<str, type<T, Ts...>> : private type<T, Ts...> {
    explicit consteval named() noexcept = default;

    consteval optional<named> operator=(opt_t) noexcept {
        return optional<named>{};
    }
};

template <typename Base>
struct optional : private Base {
    friend Base;

private:
    explicit consteval optional() noexcept = default;
};

template <typename>
LEV_HIDDEN inline constexpr bool is_declaration_v = false;

template <typename... Ts>
LEV_HIDDEN inline constexpr bool is_declaration_v<type<Ts...>> =
    is_complete_v<type<Ts...>>;

template <typename>
LEV_HIDDEN inline constexpr bool is_named_declaration_v = false;

template <typename U>
LEV_HIDDEN inline constexpr bool is_named_declaration_v<named<Name, U>> =
    is_declaration_v<U>;

template <typename U>
LEV_HIDDEN inline constexpr bool is_named_declaration_v<optional<U>> =
    is_named_declaration_v<U>;

template <typename>
LEV_HIDDEN inline constexpr bool is_optional_declaration_v = false;

template <typename U>
LEV_HIDDEN inline constexpr bool is_optional_declaration_v<optional<U>> =
    is_complete_v<optional<U>>;

template <typename>
LEV_HIDDEN inline constexpr bool is_variant_declaration_v = false;

template <typename T0, typename T1, typename... Ts>
LEV_HIDDEN inline constexpr bool is_variant_declaration_v<type<T0, T1, Ts...>> =
    true;

template <pystring_literal auto Name, typename U>
LEV_HIDDEN inline constexpr bool is_variant_declaration_v<named<Name, U>> =
    is_variant_declaration_v<U>;

template <typename U>
LEV_HIDDEN inline constexpr bool is_variant_declaration_v<optional<U>> =
    is_variant_declaration_v<U>;

template <typename T>
concept declaration = is_declaration_v<T> || is_named_declaration_v<T> ||
    is_optional_declaration_v<T>;
template <typename T>
concept named_declaration = is_named_declaration_v<T>;
template <typename T>
concept optional_declaration = is_optional_declaration_v<T>;
template <typename T>
concept variant_declaration = is_variant_declaration_v<T>;

} // namespace lev::argument

namespace lev {

template <typename... Ts>
LEV_HIDDEN inline constexpr argument::type arg{};

LEV_HIDDEN inline constexpr argument::type<PyObject> pyobj_arg{};

LEV_HIDDEN inline constexpr argument::opt_t opt{};

inline namespace literals {
inline namespace argument_literals {
template <string::details::pyliteral S>
[[nodiscard]] consteval auto operator""_a() noexcept {
    return argument::details::argument<S>{};
}
} // namespace argument_literals
} // namespace literals

namespace argument {
template <typename>
struct name_of {};

template <pystring_literal auto Name, typename U>
struct name_of<named<Name, U>> {
    LEV_HIDDEN static constexpr auto value = details::argument<Name>;
};

template <typename U>
struct name_of<optional<U>> : name_of<U> {};

template <named_declaration T>
LEV_HIDDEN inline constexpr auto name_of_v = name_of<T>::value;

template <typename>
struct type_of {};

template <declaration T>
using type_of_t = typename type_of<T>::type;

template <typename T>
struct type_of<type<T>> {
    using type = lev::expected<T, conversion_error>;
};

template <auto Name, typename T>
struct type_of<named<Name, T>> : type_of<T> {};

template <typename T>
struct type_of<optional<T>> : type_of<T> {};

template <typename T>
class variant;

template <typename... Ts>
struct type_of<type<Ts...>> {
    using type = variant<type<Ts...>>;
};

namespace details {
template <typename T, size_t... Ns>
LEV_HIDE_INSTANTIATION [[nodiscard]] std::array<T, sizeof...(Ns)> as_array(
    argument<Ns>... args) noexcept {
    return {static_cast<T>(args)...};
}
} // namespace details

template <named_declaration... Ns, optional_declaration... Os>
requires (... && !optional_declaration<Ns>) && (... && named_declaration<Ns>)
LEV_HIDE_INSTANTIATION
    [[nodiscard]] std::array<PyObject*, sizeof...(Ns) + sizeof...(Os)> sort(
        basic_args args, Ns..., Os...) noexcept {
    return args.sort(
        std::span{details::as_array<PyASCIIObject*>(name_of_v<Ns>...)},
        std::span{details::as_array<PyASCIIObject*>(name_of_v<Os>...)});
}

template <named_declaration... Ns>
requires (... && !optional_declaration<Ns>)
LEV_HIDE_INSTANTIATION [[nodiscard]] std::array<PyObject*, sizeof...(Ns)> sort(
    basic_args args, Ns...) noexcept {
    return args.sort(
        std::span{details::as_array<PyASCIIObject*>(name_of_v<Ns>...)},
        std::span<PyASCIIObject*, 0>{});
}

template <named_declaration... Ns, optional_declaration... Os>
requires (... && !optional_declaration<Ns>) && (... && named_declaration<Ns>)
LEV_HIDE_INSTANTIATION
    [[nodiscard]] std::array<PyObject*, sizeof...(Ns) + sizeof...(Os)> sort(
        vector_args args, Ns... names, Os... onames) noexcept {
    return args.sort(
        std::span{adetails::as_array<char const*>(name_of_v<Ns>...)},
        std::span{details::as_array<char const*>(name_of_v<Os>...)});
}

template <named_declaration... Ns>
requires (... && !optional_declaration<Ns>)
LEV_HIDE_INSTANTIATION [[nodiscard]] std::array<PyObject*, sizeof...(Ns)> sort(
    vector_args args, Ns... names) noexcept {
    return args.sort(
        std::span{details::as_array<char const*>(name_of_v<Ns>...)},
        std::span<char const*, 0>{});
}

} // namespace argument
} // namespace lev
