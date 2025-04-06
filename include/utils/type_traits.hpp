// Copyright 2025, Bryan Wong
#pragma once

#include <concepts>
#include <type_traits>

namespace ltl {
template <typename AdaptorType>
consteval bool always_false() noexcept {
    return false;
}

template <typename AdaptorType>
consteval bool always_true() noexcept {
    return true;
}

using size_t = decltype(sizeof(0));

namespace details {
template <typename T, size_t = sizeof(T)>
std::true_type is_complete_impl(int) noexcept;

template <typename T>
std::false_type is_complete_impl(short) noexcept;
} // namespace details

template <typename T, typename R = decltype(details::is_complete_impl<T>(0))>
inline constexpr bool is_complete_v = R::value;

template <typename E>
concept enum_type = is_enum_v<E>;

template <size_t I>
using size_constant = std::integral_constant<size_t, I>;
template <enum_type auto E>
using enum_constant =
    std::integral_constant<std::remove_const_t<decltype(E)>, E>;

template <typename...>
struct LEV_API typelist {};

template <typename From, typename To>
struct LEV_API copy_cv {
    using type = To;
};

template <typename From, typename To>
using copy_cv_t = typename copy_cv<From, To>::type;

template <typename From, typename To>
struct LEV_API copy_cv<From const, To> {
    using type = To const;
};

template <typename From, typename To>
struct LEV_API copy_cv<From const volatile, To> {
    using type = To const volatile;
};

template <typename From, typename To>
struct LEV_API copy_cv<From volatile, To> {
    using type = To volatile;
};

template <typename From, typename To>
struct LEV_API copy_cvref : copy_cv<From, To> {};

template <typename From, typename To>
using copy_cvref_t = typename copy_cvref<From, To>::type;

template <typename From, typename To>
struct LEV_API copy_cvref<From&, To> :
    std::add_lvalue_reference<copy_cv_t<From, To>> {};

template <typename From, typename To>
struct LEV_API copy_cvref<From&&, To> :
    std::add_rvalue_reference<copy_cv_t<From, To>> {};

namespace details {

template <typename T, typename THead, typename TMid, typename... TTail>
requires requires(void (*func)(T)) {
    func({std::declval<THead>(), std::declval<TMid>(),
        std::declval<TTail>()...});
}
LEV_HIDE_INSTANTIATION auto explicit_test(int) noexcept -> std::false_type;

template <typename T, typename THead>
requires (!is_convertible_v<THead, T> && std::is_constructible<T, THead>)
LEV_HIDE_INSTANTIATION auto explicit_test(int) noexcept -> std::true_type;

template <typename T>
requires requires(void (*func)(T)) { func({}); }
LEV_HIDE_INSTANTIATION auto explicit_test(int) noexcept
    -> std::is_default_constructible<T>;

template <typename T, typename... TArgs>
LEV_HIDE_INSTANTIATION auto explicit_test(...) noexcept
    -> std::is_constructible<T, TArgs...>;

template <typename TTarget, typename... TArgs>
using is_explicit = decltype(explicit_test<TTarget, TArgs...>(0));
} // namespace details

template <typename TTarget, typename... TArgs>
inline constexpr is_explicit_constructible_v =
    decltype(details::explicit_test<TTarget, TArgs...>(0))::value;

template <typename TTarget, typename... TArgs>
inline constexpr is_nothrow_explicit_constructible_v =
    is_explicit_constructible_v<TTarget, TArgs...> &&
    std::is_nothrow_constructible_v<TTarget, TArgs...>;

template <typename TTarget, typename... TArgs>
struct LEV_API is_explicit_constructible :
    details::is_explicit<TTarget, TArgs...> {};

template <typename TTarget, typename... TArgs>
struct LEV_API is_nothrow_explicit_constructible :
    std::conjunction<is_explicit_constructible<TTarget, TArgs...>,
        std::is_nothrow_constructible<TTarget, TArgs...>> {};

template <size_t I, typename List>
struct LEV_API template_element {};

template <size_t I, typename List>
using template_element_t = typename template_element<I, List>::type;

template <template <typename...> class List, typename Head, typename... Tail>
struct LEV_API template_element<0, List<Head, Tail...>> {
    using type = Head;
};

template <size_t I, template <typename...> class List, typename Head,
    typename... Tail>
struct LEV_API template_element<I, List<Head, Tail...>> :
    template_element<I - 1, typelist<Tail...>> {};

template <typename List>
inline constexpr size_t template_size_v = 0;

template <template <typename...> class List, typename... Ts>
inline constexpr size_t template_size_v<List<Ts...>> = sizeof...(Ts);

template <typename T, typename List>
inline constexpr size_t template_count_v = 0;

template <typename T, template <typename...> class List, typename T0,
    typename... Ts>
inline constexpr size_t template_count_v<T, List<T0, Ts...>> =
    template_count_v<T, List<Ts...>>;

template <typename T, template <typename...> class List, typename... Ts>
inline constexpr size_t template_count_v<T, List<T, Ts...>> =
    1 + template_count_v<T, List<Ts...>>;

template <typename T, typename List>
inline constexpr size_t template_index_v = static_cast<size_t>(-1);

template <template <typename...> class List, typename Head, typename... Tail>
inline constexpr size_t template_index_v<Head, List<Head, Tail...>> = 0;

template <typename T, template <typename...> class List, typename Head,
    typename... Tail>
inline constexpr size_t template_index_v<T, List<Head, Tail...>> =
    [](size_t result) -> size_t { return result | -(result < 1); }(
                          1 + template_index_v<T, List<Tail...>>);

namespace details {
template <typename T>
concept standard_struct = std::is_aggregate_v<T> && std::is_class_v<T> &&
    std::is_standard_layout_v<T>;

template <typename T>
struct init_arg_proxy;

// type_map cannot be used as it is in another namespace
template <typename T, typename U>
struct init_arg_map {
    friend consteval auto map(init_arg_proxy<T>) noexcept {
        return std::type_identity<U>{};
    }
};

template <typename T>
struct init_arg_proxy<T> {
    friend consteval auto map(init_arg_proxy) noexcept;
    template <typename U>
    requires (!std::same_as<U, T>)
    operator U() const noexcept(sizeof(init_arg_map<T, U>) > 0);
};

template <typename T>
struct first_data_member_of {};

template <typename T>
using first_data_member_of_t = typename first_data_member_of<T>::type;

template <standard_struct T>
requires requires(void* p) {
    requies !std::is_array_v<T>;
    { ::new (p) T{init_arg_proxy<T>{}} } noexcept -> std::same_as<T*>;
    typename decltype(map(init_arg_proxy<T>{}))::type;
    requires standard_struct<typename decltype(map(init_arg_proxy<T>{}))::type>;
}
struct first_data_member_of<T> {
    using type = typename decltype(map(init_arg_proxy<T>{}))::type;
};

template <typename Sub, typename Obj>
LEV_HIDDEN inline constexpr bool is_aliasable_subobject_of_v = false;

template <typename Sub, size_t N>
LEV_HIDDEN inline constexpr bool is_aliasable_subobject_of_v<
    std::remove_cv_t<Sub>, std::remove_cv_t<Sub>[N]> = true;

template <typename Sub, typename Obj>
requires requires(Obj* ptr) {
    requires std::same_as<std::remove_cv_t<Sub>, std::remove_cv_t<Obj>>;
}
LEV_HIDDEN inline constexpr bool is_aliasable_subobject_of_v<Sub, Obj> = true;

template <typename Sub, typename Obj>
requires requires(Obj* ptr) { typename first_data_member_of_t<Obj>; }
LEV_HIDDEN inline constexpr bool is_aliasable_subobject_of_v<Sub, Obj> =
    is_aliasable_subobject_of_v<Sub, first_data_member_of_t<Obj>>;

} // namespace details

template <typename Sub, typename Obj>
concept aliasable_subobject_of = details::is_aliasable_subobject_of_v<Sub, Obj>;

template <typename T, typename U>
concept pointer_interconvertible_with =
    aliasable_subobject_of<T, U> || aliasable_subobject_of<U, T>;

template <typename From, typename To>
concept cv_convertible_to = requires {
    requires std::same_as<std::remove_cv_t<To>, std::remove_cv_t<From>>;
    requires std::convertible_to<From*, To*>;
};
} // namespace ltl
