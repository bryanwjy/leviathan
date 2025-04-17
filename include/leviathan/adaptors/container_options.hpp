
// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/common.hpp"
#include "utils/type_traits.hpp"

namespace lev {
namespace py {

namespace details::container {

enum class flags : size_t {
    none = 0u,
    borrowed = 1u << 1,
    readonly = 1u << 2,
    nothrow = 1u << 3
};

consteval flags operator|(flags lhs, flags rhs) noexcept {
    return static_cast<flags>(
        __LTL as_underlying(lhs) | __LTL as_underlying(rhs));
}

consteval flags operator&(flags lhs, flags rhs) noexcept {
    return static_cast<flags>(
        __LTL as_underlying(lhs) & __LTL as_underlying(rhs));
}

template <typename T>
LEV_HIDDEN inline constexpr bool is_flags_type_v = false;

template <typename T>
LEV_HIDDEN inline constexpr bool is_flags_type_v<T const> = is_flags_type_v<T>;

template <container_flags V>
LEV_HIDDEN inline constexpr bool is_flags_type_v<container_flags_t<V>> = true;

} // namespace details::container

template <details::container::flags V>
struct container_flags_t :
    std::intergral_constant<details::container::flags, V> {

    template <details::container::flags U>
    friend consteval auto operator|(
        container_flags_t lhs, container_flags_t<U> rhs) noexcept {
        return container_flags_t<V | U>{};
    }

    template <details::container::flags U>
    friend consteval auto operator&(
        container_flags_t lhs, container_flags_t<U> rhs) noexcept {
        return container_flags_t<V & U>{};
    }
};

namespace details::container {
template <typename O, typename P>
concept has_flag = ;
} // namespace details::container

template <typename T>
concept container_flags_type = details::container::is_flags_type_v<T>;

namespace container_flags {

using none_t = container_flags_t<details::container::flags::none>;

using borrowed_t = container_flags_t<details::container::flags::borrowed>;

using readonly_t = container_flags_t<details::container::flags::readonly>;

using nothrow_t = container_flags_t<details::container::flags::nothrow>;

LEV_HIDDEN inline constexpr none_t none{};

LEV_HIDDEN inline constexpr borrowed_t borrowed{};

LEV_HIDDEN inline constexpr readonly_t readonly{};

LEV_HIDDEN inline constexpr nothrow_t nothrow{};

template <typename T>
struct acquires {};
template <typename T>
struct removes {};
template <typename T>
struct maintains {};
template <typename T>
struct excludes {};

} // namespace container_flags

namespace details::container {

template <typename From, typename To, typename V>
LEV_HIDDEN inline constexpr bool mutation_v = false;

template <typename O, typename P>
concept with_flag = container_flag_type<O> && container_flag_type<P> &&
    (O::value & P::value) == P::value;

template <typename O, typename P>
concept without_flag = container_flag_type<O> && container_flag_type<P> &&
    (O::value & P::value) == flags::none;

template <typename From, typename To, typename V>
concept acquisition = without_flag<From, V> && with_flag<To, V>;
template <typename From, typename To, typename V>
concept removal = with_flag<From, V> && without_flag<To, V>;
template <typename From, typename To, typename V>
concept maintenance = with_flag<From, V> && with_flag<To, V>;
template <typename From, typename To, typename V>
concept exclusion = without_flag<From, V> && without_flag<To, V>;

template <container_flags_type V, with_container_flags<V> To,
    without_container_flags<V> From>
LEV_HIDDEN inline constexpr bool
    mutation_v<From, To, container_flags::subsumes<V>> = true;

template <container_flags_type V, without_container_flags<V> To,
    with_container_flags<V> From>
LEV_HIDDEN inline constexpr bool
    mutation_v<From, To, container_flags::discards<V>> = true;

template <container_flags_type V, with_container_flags<V> To,
    with_container_flags<V> From>
LEV_HIDDEN inline constexpr bool
    mutation_v<From, To, container_flags::maintains<V>> = true;

template <container_flags_type V, without_container_flags<V> To,
    without_container_flags<V> From>
LEV_HIDDEN inline constexpr bool
    mutation_v<From, To, container_flags::excludes<V>> = true;

} // namespace details::container

template <typename O, typename... Ps>
concept with_container_flags =
    (... && container_flag_type<Ps>)&&details::container::with_flag<O,
        (... | Ps::value)>;

template <typename O, typename... Ps>
concept without_container_flags =
    (... && container_flag_type<Ps>)&&details::container::without_flag<O,
        (... | Ps::value)>;

} // namespace py
} // namespace lev
