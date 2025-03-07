// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/string.hpp"

#include <bit>
#include <climits>

namespace lev {
namespace details {

namespace bit {
consteval size_t ceil_div(size_t val, size_t div) noexcept {
    return (val + div - 1) / div;
}
consteval size_t min(size_t lhs, size_t rhs) noexcept {
    return lhs < rhs ? lhs : rhs;
}
consteval size_t alignment(size_t width) noexcept {
    auto const bytes = ceil_div(width, CHAR_BIT);
    auto const max = 2 * alignof(void*);
    return min(bytes, max);
}

template <size_t N>
struct underlying;

template <size_t N>
using underlying_t = typename underlying<N>::type;

template <size_t N>
requires (N <= CHAR_BIT)
struct underlying<N> {
    using type = unsigned char;
};

template <size_t N>
requires (N > CHAR_BIT && N <= 2 * CHAR_BIT)
struct underlying<N> {
    using type = unsigned short;
    static_assert(sizeof(type) == 2 * CHAR_BIT);
};

template <size_t N>
requires (N > 2 * CHAR_BIT && N <= 4 * CHAR_BIT)
struct underlying<N> {
    using type = unsigned int;
    static_assert(sizeof(type) == 4 * CHAR_BIT);
};

template <size_t N>
requires (N > 4 * CHAR_BIT && N <= 8 * CHAR_BIT)
struct underlying<N> {
    using type = unsigned long long;
    static_assert(sizeof(type) == 8 * CHAR_BIT);
};

template <size_t N>
requires (N > 8 * CHAR_BIT)
struct underlying<N> {
    using type = unsigned long long[ceil_div(N, CHAR_BIT)];
};

constexpr size_t npos = static_cast<size_t>(-1);

} // namespace bit

} // namespace details
template <size_t N>
class alignas(details::bit::alignment(N)) bitset {
    using underlying_type = underlying_t<N>;

    static constexpr size_t element_width = std::is_array_v<underlying_type>
        ? sizeof(unsigned long long) * CHAR_BIT
        : sizeof(underlying_type) * CHAR_BIT;

    static constexpr auto one_at(size_t idx) noexcept {
        if constexpr (!std::is_array_v<underlying_type>) {
            return static_cast<underlying_type>(1) << idx;
        } else {
            return static_cast<std::remove_extent_t<underlying_type>>(1) << idx;
        }
    }

public:
    constexpr bitset() noexcept : storage_{} {}

    template <std::unsigned_integral... Args>
    requires std::is_array_v<underlying_type> && requires(Args... args) {
        { underlying_type{args...} } noexcept;
    }
    constexpr explicit bitset(Args... args) noexcept : storage_{args...} {}

    template <std::unsigned_integral T>
    requires (!std::is_array_v<underlying_type> &&
        std::constructible_from<underlying_type, T>)
    constexpr explicit bitset(T val) noexcept : storage_{val} {}

    constexpr underlying_type value() const noexcept
    requires (!std::is_array_v<underlying_type>)
    {
        return storage_;
    }

    constexpr void copy(
        std::span<std::byte, sizeof(underlying_type)> bytes) noexcept
    requires (std::is_array_v<underlying_type>)
    {
        memcpy(bytes.data(), &storage_, sizeof(underlying_type));
    }

    constexpr void set(size_t idx) noexcept {
        LEV_ASSERT(idx < N);
        if constexpr (!std::is_array_v<underlying_type>) {
            storage_ |= one_at(idx);
        } else {
            auto ptr = storage_ + idx / element_width;
            *ptr |= one_at(idx % element_width);
        }
    }

    constexpr void clear(size_t idx) noexcept {
        LEV_ASSERT(idx < N);
        if constexpr (!std::is_array_v<underlying_type>) {
            storage_ &= ~one_at(idx);
        } else {
            auto ptr = storage_ + idx / element_width;
            *ptr &= ~one_at(idx % element_width);
        }
    }

    constexpr bool test(size_t idx) noexcept {
        LEV_ASSERT(idx < N);
        if constexpr (!std::is_array_v<underlying_type>) {
            return storage_ & one_at(idx);
        } else {
            auto ptr = storage_ + idx / element_width;
            return *ptr & one_at(idx % element_width);
        }
    }

    constexpr size_t size() const noexcept { return N; }

    constexpr size_t size_bytes() const noexcept {
        return sizeof(underlying_type);
    }

    template <size_t Offset, size_t Width = npos>
    requires (N > Offset)
    constexpr auto subset() const noexcept {
        using return_type = bitset<details::bit::min(Width, N - Offset)>;
        if constexpr (!std::is_array_v<underlying_type>) {
            using return_underlying =
                underlying_t<details::bit::min(Width, N - Offset)>;
            return return_type{static_cast<return_underlying>(
                (storage_ >> Offset) & (one_at(width) - 1))};
        } else {
            static_assert(always_false<return_type>(), "Unsupported");
        }
    }

private:
    underlying_type storage_;
};

} // namespace lev
