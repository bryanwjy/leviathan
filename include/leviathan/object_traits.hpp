// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/utility.hpp"

#include <Python.h>

#include <concepts>
#include <new>

namespace lev {
namespace details {
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

template <typename D, typename B>
concept pyobj_derived_from = pyobj_type<D> && pyobj_type<B> &&
    (std::derived_from<D, B> || aliasable_subobject_of<D, B>);

template <typename B, typename D>
concept pyobj_base_of = pyobj_derived_from<D, B>;

template <typename B, typename D>
concept pyobj_related_to = pyobj_derived_from<D, B> || pyobj_derived_from<B, D>;

template <typename T>
PyTypeObject* type_object_v = nullptr;

template <leviathan_pyobj T>
PyTypeObject* type_object_v<T> =
    adaptor_traits<std::remove_cv_t<T>>::type_object();

} // namespace lev
