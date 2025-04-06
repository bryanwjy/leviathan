
// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/common.hpp"
#include "utils/type_traits.hpp"

namespace lev {
namespace py {

template <typename... T>
struct container_policies_t;

namespace details::container_policies {
struct container_policy_t {
protected:
    LEV_HIDDEN ~container_policy_t() = default;
};

consteval int container_policy_base(...) noexcept = delete;

template <typename D>
using container_policy_type_t =
    decltype(container_policy_base(std::declval<D>()));

template <typename D, typename B>
struct container_policy_base_t : B {
    friend inline consteval B container_policy_base(D) noexcept { return {}; }

protected:
    LEV_HIDE_INSTANTIATION ~container_policy_base_t() = default;
};

struct ownership_policy_t : container_policy_t {
protected:
    LEV_HIDDEN ~ownership_policy_t() = default;
};

struct owned_policy_t :
    container_policy_base_t<owned_policy_t, ownership_policy_t> {
    explicit inline consteval owned_policy_t() = default;
};

struct borrowed_policy_t :
    container_policy_base_t<borrowed_policy_t, ownership_policy_t> {
    explicit inline consteval borrowed_policy_t() = default;
};

struct access_policy_t : container_policy_t {
protected:
    LEV_HIDDEN ~ownership_policy_t() = default;
};

struct immutable_policy_t :
    container_policy_base_t<immutable_policy_t, access_policy_t> {
    explicit inline consteval immutable_policy_t() = default;
};

struct mutable_policy_t :
    container_policy_base_t<mutable_policy_t, access_policy_t> {
    explicit inline consteval mutable_policy_t() = default;
};

template <typename T>
LEV_HIDDEN inline constexpr bool is_container_policy_v = false;

template <typename T>
LEV_HIDDEN inline constexpr bool is_container_policy_v<T const> =
    is_container_policy_v<T>;

template <typename... Ts>
LEV_HIDDEN inline constexpr bool
    is_container_policy_v<container_policies_t<Ts...>> = true;
} // namespace details::container_policies

template <typename... Os>
struct container_policies_t<Os...> : public Os... {
    static_assert(
        (... && std::is_base_of_v<option_t, Os>), "Invalid option type");

public:
    explicit inline consteval container_policies_t() = default;
    friend consteval Os policy(container_policies_t) noexcept
    requires (sizeof...(Os) == 1)
    {
        return Os{};
    }

    template <typename U>
    requires requires(U u) { requires (... || std::same_as<U, Os>); }
    inline consteval container_policies_t<Os...> operator|(
        container_policies_t<U>) const noexcept {
        return *this;
    }

    template <typename U>
    requires requires(U u) {
        typename container_policy_type_t<U>;
        requires !(... || std::same_as<U, Os>);
        requires !(... ||
            std::same_as<container_policy_type_t<U>,
                container_policy_type_t<Os>>);
        container_policies_t<Os..., U>{};
    }
    inline consteval container_policies_t<Os..., U> operator|(
        container_policies_t<U>) const noexcept {
        return container_policies_t<Os..., U>{};
    }
};

inline constexpr auto owned_policy =
    container_policies_t<details::container_policies::owned_policy_t>{};
inline constexpr auto borrowed_policy =
    container_policies_t<details::container_policies::borrowed_policy_t>{};
inline constexpr auto immutable_policy =
    container_policies_t<details::container_policies::immutable_policy_t>{};
inline constexpr auto mutable_policy =
    container_policies_t<details::container_policies::mutable_policy_t>{};

template <typename T>
concept container_policy =
    details::container_policies::is_container_policy_v<T>;

template <auto P, auto... Os>
concept has_container_policy = container_policy<T> &&
    (... && derived_from<decltype(Os), details::container_policy_t>)&&(
        ... && derived_from<decltype(P), decltype(policy(Os))>);

} // namespace py
} // namespace lev
