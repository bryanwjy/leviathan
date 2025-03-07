// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/utility.hpp"

#include <Python.h>

#include <concepts>
#include <new>

namespace lev {
template <typename T>
class python_ptr;
struct adopt_t;

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

template <standard_struct T>
struct init_arg_proxy<T> {
    friend consteval auto map(init_arg_proxy) noexcept;
    template <standard_struct U>
    requires (!std::same_as<U, T>)
    operator U() const noexcept(sizeof(init_arg_map<T, U>));
};

template <typename T>
struct first_member_object_of {};

template <typename T>
using first_member_object_of_t = typename first_member_object_of<T>::type;

template <typename T>
requires requires(void* p) {
    { ::new (p) T{init_arg_proxy<T>{}} } noexcept -> std::same_as<T*>;
    typename decltype(map(init_arg_proxy<T>{}))::type;
}
struct first_member_object_of<T> {
    using type = typename decltype(map(init_arg_proxy<T>{}))::type;
};

template <typename Sub, typename Obj>
LEV_HIDDEN inline constexpr bool is_aliasable_subobject_of_v = false;

template <typename Sub, typename Obj>
requires requires(Obj* ptr) {
    requires std::same_as<std::remove_cv_t<Sub>, std::remove_cv_t<Obj>>;
    reinterpret_cast<Sub*>(ptr);
}
LEV_HIDDEN inline constexpr bool is_aliasable_subobject_of_v<Sub, Obj> = true;

template <typename Sub, typename Obj>
requires requires { typename first_member_object_of_t<Obj>; }
LEV_HIDDEN inline constexpr bool is_aliasable_subobject_of_v<Sub, Obj> =
    is_aliasable_subobject_of_v<Sub, first_member_object_of_t<Obj>>;

template <typename, typename>
struct append {};
template <typename, typename>
using append_t = typename append<T>::type;
template <typename... Ts, typename... Us>
struct append<typelist<Ts...>, typelist<Us...>> {
    using type = typelist<Ts..., Us...>;
};
template <typename T>
struct hierarchy {};
template <typename T>
using hierarchy_t = typename hierarchy<std::remove_cv_t<T>>::type;
template <>
struct hierarchy<PyObject> {
    using type = typelist<PyObject>;
};

template <standard_struct T>
struct hierarchy :
    append<typelist<T>, hierarchy_t<first_member_object_of_t<T>>> {};

template <typename T, typename H>
struct is_within_hierarchy : std::false_type {};

template <typename T, typename... Hs>
struct is_within_hierarchy<T, typelist<Hs...>> :
    std::disjunction<std::is_same<Hs, T>...> {};
} // namespace details

template <typename AdaptorType>
struct AdaptorTraits;

template <typename T>
concept native_pyobj =
    details::is_aliasable_subobject_of_v<PyObject, std::remove_cv_t<T>>;

template <typename T>
concept leviathan_pyobj =
    std::derived_from<std::remove_cv_t<T>, object_root> && requires {
        {
            adaptor_traits<std::remove_cv_t<T>>::type_object()
        } noexcept -> std::same_as<PyTypeObject*>;
    };

template <typename Sub, typename Obj>
concept aliasable_subobject_of = native_pyobj<Sub> && native_pyobj<Obj> &&
    details::is_aliasable_subobject_of<Sub, Obj>::value;

template <typename T, typename U>
concept interaliasable_with =
    aliasable_subobject_of<T, U> || aliasable_subobject_of<U, T>;

template <typename T>
LEV_HIDDEN inline constexpr bool is_opaque_pyobj_v = false;
template <typename T>
LEV_HIDDEN inline constexpr bool is_opaque_pyobj_v<T const> =
    is_opaque_pyobj_v<T>;
template <typename T>
LEV_HIDDEN inline constexpr bool is_opaque_pyobj_v<T volatile> =
    is_opaque_pyobj_v<T>;

template <typename T>
concept opaque_pyobj = is_opaque_pyobj_v<T>;

template <typename T>
concept pyobj_type = opaque_pyobj<T> || native_pyobj<T> || leviathan_pyobj<T>;

} // namespace lev
