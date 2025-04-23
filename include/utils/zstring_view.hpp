// Copyright 2023-2024 Bryan Wong

#pragma once

#include "utils/common.hpp"

#include <exception>
#include <ranges>
#include <string_view>

namespace ltl {

template <typename CharType, typename Traits = std::char_traits<CharType>>
class LEV_API basic_zstring_view :
    public std::basic_string_view<CharType, Traits> {
    using base_type = std::basic_string_view<CharType, Traits>;
    using base_type::remove_suffix;
    using base_type::swap;

public:
    using base_type::npos;
    using typename base_type::const_iterator;
    using typename base_type::const_pointer;
    using typename base_type::const_reference;
    using typename base_type::const_reverse_iterator;
    using typename base_type::difference_type;
    using typename base_type::iterator;
    using typename base_type::pointer;
    using typename base_type::reference;
    using typename base_type::reverse_iterator;
    using typename base_type::size_type;
    using typename base_type::traits_type;
    using typename base_type::value_type;

    basic_zstring_view(decltype(nullptr)) = delete;
    LEV_HIDE_INSTANTIATION inline constexpr basic_zstring_view() noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr basic_zstring_view(
        basic_zstring_view const&) noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr basic_zstring_view(const_pointer data,
        size_type size)
    LEV_CONTRACT_PRE(data != nullptr && data[size] == 0) :
        base_type(data, size) {
        LEV_ASSERT(data != nullptr && data[size] == 0);
    }

    LEV_HIDE_INSTANTIATION inline constexpr basic_zstring_view(const_pointer data) noexcept(
        noexcept(traits_type::length(data)))
        : base_type(data, traits_type::length(data)) {}

    template <std::contiguous_iterator It, std::sized_sentinel_for<It> E>
    requires std::is_convertible_v<std::add_pointer_t<std::iter_value_t<It>>,
        char const*>
    LEV_HIDE_INSTANTIATION inline constexpr basic_zstring_view(It begin, E end)
        : basic_zstring_view(__UTL to_address(begin), end - begin) {}

    LEV_HIDE_INSTANTIATION explicit constexpr basic_zstring_view(base_type const& other)
        : basic_zstring_view(other.data(), other.size()) {}

    // TODO: ranges ctor

    LEV_HIDE_INSTANTIATION inline constexpr basic_zstring_view& operator=(
        basic_zstring_view const&) noexcept = default;

    using base_type::back;
    using base_type::begin;
    using base_type::cbegin;
    using base_type::cend;
    using base_type::crbegin;
    using base_type::crend;
    using base_type::data;
    using base_type::empty;
    using base_type::end;
    using base_type::front;
    using base_type::length;
    using base_type::max_size;
    using base_type::rbegin;
    using base_type::rend;
    using base_type::size;
    using base_type::operator[];
    using base_type::at;
    using base_type::compare;
    using base_type::contains;
    using base_type::copy;
    using base_type::ends_with;
    using base_type::find;
    using base_type::find_first_not_of;
    using base_type::find_first_of;
    using base_type::find_last_not_of;
    using base_type::find_last_of;
    using base_type::remove_prefix;
    using base_type::rfind;
    using base_type::starts_with;
    using base_type::substr;

    LEV_HIDE_INSTANTIATION inline constexpr void swap(basic_zstring_view& other) noexcept {
        base_type::swap(other);
    }

    LEV_HIDE_INSTANTIATION friend inline constexpr void swap(
        basic_zstring_view& l, basic_zstring_view& r) noexcept {
        l.swap(r);
    }

private:
    template <typename T, size_t Stack = 256>
    LEV_HIDE_INSTANTIATION [[noreturn]] static R throw_exception(char const* fmt, ...) {
        char buffer[Stack];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, Stack, fmt, args);
        va_start(end);
        throw T(buffer);
    }
};

template <typename CharType, typename Traits>
LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool operator==(
    basic_zstring_view<CharType, Traits> lhs,
    basic_zstring_view<CharType, Traits> rhs) noexcept {
    return lhs.compare(rhs) == 0;
}

template <typename CharType, typename Traits>
LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool operator!=(
    basic_zstring_view<CharType, Traits> lhs,
    basic_zstring_view<CharType, Traits> rhs) noexcept {
    return !(lhs == rhs);
}

template <typename CharType, typename Traits>
LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool operator<(
    basic_zstring_view<CharType, Traits> lhs,
    basic_zstring_view<CharType, Traits> rhs) noexcept {
    return lhs.compare(rhs) < 0;
}

template <typename CharType, typename Traits>
LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool operator>(
    basic_zstring_view<CharType, Traits> lhs,
    basic_zstring_view<CharType, Traits> rhs) noexcept {
    return rhs < lhs;
}

template <typename CharType, typename Traits>
LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool operator>=(
    basic_zstring_view<CharType, Traits> lhs,
    basic_zstring_view<CharType, Traits> rhs) noexcept {
    return !(lhs < rhs);
}

template <typename CharType, typename Traits>
LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool operator<=(
    basic_zstring_view<CharType, Traits> lhs,
    basic_zstring_view<CharType, Traits> rhs) noexcept {
    return !(rhs < lhs);
}

template <typename CharType, typename Traits>
LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto operator<=>(
    basic_zstring_view<CharType, Traits> lhs,
    basic_zstring_view<CharType, Traits> rhs) noexcept {
    return lhs.compare(rhs) <=> 0;
}

using zstring_view = basic_zstring_view<char>;
using zu8string_view = basic_zstring_view<char8_t>;
using zu16string_view = basic_zstring_view<char8_t>;
using zu32string_view = basic_zstring_view<char8_t>;
using zwstring_view = basic_zstring_view<wchar_t>;

} // namespace ltl

template <typename C, typename T>
inline constexpr bool
    std::enable_borrowed_range<ltl::basic_zstring_view<C, T>> = true;
