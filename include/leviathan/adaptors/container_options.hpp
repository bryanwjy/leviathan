
// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/common.hpp"
#include "utils/type_traits.hpp"

namespace lev {
namespace py {

template <typename... T>
struct container_options_t;

struct container_option_t {
protected:
    LEV_HIDDEN ~container_option_t() = default;
};

consteval void option_base(...) noexcept = delete;

template <typename D>
using container_option_type_t = decltype(option_base(std::declval<D>()));

template <typename D, std::derived_from<container_option_t> B>
struct container_option_base_t : B {
    friend inline consteval B option_base(D) noexcept { return {}; }

protected:
    LEV_HIDE_INSTANTIATION constexpr ~container_option_base_t() = default;
};

struct ownership_option : container_option_t {
protected:
    LEV_HIDDEN ~ownership_option() = default;
};

namespace ownership {
struct owned_t : container_option_base_t<owned_t, ownership_option> {
    explicit inline consteval owned_t() = default;
};

struct borrowed_t : container_option_base_t<borrowed_t, ownership_option> {
    explicit inline consteval borrowed_t() = default;
};

inline constexpr auto owned = container_options_t<owned_t>{};
inline constexpr auto borrowed = container_options_t<borrowed_t>{};

} // namespace ownership

struct access_option : container_option_t {
protected:
    LEV_HIDDEN constexpr ~access_option() = default;
};

namespace access {
struct readonly_t : container_option_base_t<readonly_t, access_option> {
    explicit inline consteval readonly_t() = default;
};

struct writable_t : container_option_base_t<writable_t, access_option> {
    explicit inline consteval writable_t() = default;
};
;
inline constexpr auto readonly = container_options_t<readonly_t>{};
inline constexpr auto writable = container_options_t<writable_t>{};

} // namespace access

namespace details {
template <typename T>
LEV_HIDDEN inline constexpr bool is_container_option_type_v = false;

template <typename T>
LEV_HIDDEN inline constexpr bool is_container_option_type_v<T const> =
    is_container_option_type_v<T>;

template <typename... Ts>
LEV_HIDDEN inline constexpr bool
    is_container_option_type_v<container_options_t<Ts...>> = true;
} // namespace details

template <typename T>
concept container_option_type =
    is_container_option_type_v<T> && std::derived_from<T, ownership_option> &&
    std::derived_from<T, access_option>;

template <typename T, typename... P>
concept with_container_options = container_option_type<T> &&
    (... && std::is_base_of_v<container_option_t, P>)&&(
        ... && std::derived_from<T, P>);

template <typename... Os>
struct container_options_t<Os...> : public Os... {
    static_assert((... && std::is_base_of_v<container_option_t, Os>),
        "Invalid option type");

public:
    explicit inline consteval container_options_t() = default;

    template <typename U>
    requires requires(U u) { requires (... || std::same_as<U, Os>); }
    inline consteval container_options_t<Os...> operator|(
        container_options_t<U>) const noexcept {
        return *this;
    }

    template <typename U>
    requires requires(U u) {
        typename container_option_type_t<U>;
        requires !(... || std::same_as<U, Os>);
        requires !(... ||
            std::same_as<container_option_type_t<U>,
                container_option_type_t<Os>>);
        container_options_t<Os..., U>{};
    }
    inline consteval container_options_t<Os..., U> operator|(
        container_options_t<U>) const noexcept {
        return container_options_t<Os..., U>{};
    }
};
} // namespace py
} // namespace lev
