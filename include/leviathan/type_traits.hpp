// Copyright 2025, Bryan Wong
#pragma once

#include <Python.h>

#include <concepts>
#include <type_traits>

namespace lev {
template <typename AdaptorType>
consteval bool always_false() noexcept {
    return false;
}

template <typename AdaptorType>
consteval bool always_true() noexcept {
    return true;
}

template <typename E>
concept enum_type = is_enum_v<E>;

template <size_t I>
using size_constant = std::integral_constant<size_t, I>;
template <enum_type auto E>
using enum_constant =
    std::integral_constant<std::remove_const_t<decltype(E)>, E>;

template <typename...>
struct typelist {};

template <typename From, typename To>
struct copy_cv {
    using type = To;
};

template <typename From, typename To>
using copy_cv_t = typename copy_cv<From, To>::type;

template <typename From, typename To>
struct copy_cv<From const, To> {
    using type = To const;
};

template <typename From, typename To>
struct copy_cv<From const volatile, To> {
    using type = To const volatile;
};

template <typename From, typename To>
struct copy_cv<From volatile, To> {
    using type = To volatile;
};

template <typename From, typename To>
struct copy_cvref : copy_cv<From, To> {};

template <typename From, typename To>
using copy_cvref_t = typename copy_cvref<From, To>::type;

template <typename From, typename To>
struct copy_cvref<From&, To> :
    std::add_lvalue_reference<copy_cv_t<From, To>> {};

template <typename From, typename To>
struct copy_cvref<From&&, To> :
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
LEV_HIDE_INSTANTIATION inline constexpr is_explicit_constructible_v =
    decltype(details::explicit_test<TTarget, TArgs...>(0))::value;

template <typename TTarget, typename... TArgs>
LEV_HIDE_INSTANTIATION inline constexpr is_nothrow_explicit_constructible_v =
    is_explicit_constructible_v<TTarget, TArgs...> &&
    std::is_nothrow_constructible_v<TTarget, TArgs...>;

template <typename TTarget, typename... TArgs>
struct is_explicit_constructible : details::is_explicit<TTarget, TArgs...> {};

template <typename TTarget, typename... TArgs>
struct is_nothrow_explicit_constructible :
    std::conjunction<is_explicit_constructible<TTarget, TArgs...>,
        std::is_nothrow_constructible<TTarget, TArgs...>> {};

template <size_t I, typename List>
struct template_element {};

template <size_t I, typename List>
using template_element_t = typename template_element<I, List>::type;

template <template <typename...> class List, typename Head, typename... Tail>
struct template_element<0, List<Head, Tail...>> {
    using type = Head;
};

template <size_t I, template <typename...> class List, typename Head,
    typename... Tail>
struct template_element<I, List<Head, Tail...>> :
    template_element<I - 1, typelist<Tail...>> {};

} // namespace lev
