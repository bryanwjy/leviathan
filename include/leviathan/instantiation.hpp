// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/object_traits.hpp"

#include <Python.h>

#include <cfloat>
#include <concepts>
#include <cstdint>
#include <optional>
#include <string_view>

namespace lev {

struct LEV_API vector_instantiation {
    LEV_HIDE_INSTANTIATION explicit inline constexpr vector_instantiation() noexcept =
        default;
};

struct LEV_API basic_instantiation {
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_instantiation() noexcept =
        default;
};

struct LEV_API disabled_instantiation {
    LEV_HIDE_INSTANTIATION explicit inline constexpr disabled_instantiation() noexcept =
        default;
};

template <typename>
LEV_HIDDEN inline constexpr bool is_instantiation_concept_v = false;
template <>
LEV_HIDDEN inline constexpr bool
    is_instantiation_concept_v<vector_instantiation> = true;
template <>
LEV_HIDDEN inline constexpr bool
    is_instantiation_concept_v<basic_instantiation> = true;
template <>
LEV_HIDDEN inline constexpr bool
    is_instantiation_concept_v<disabled_instantiation> = true;

template <typename T>
concept instantiation_concept = is_instantiation_concept_v<T>;

namespace details {
template <typename AdaptorType>
concept defines_instantiation_concept = requires {
    typename AdaptorType::instantiation_concept;
    requires instantiation_concept<typename AdaptorType::instantiation_concept>;
};
}

template <typename AdaptorType>
concept basic_instantiable = leviathan_pyobj<AdaptorType> &&
    details::defines_instantiation_concept<AdaptorType> &&
    requires(AdaptorType obj) {
        requires std::same_as<typename AdaptorType::instantiation_concept,
            basic_instantiation>;
    };

template <typename AdaptorType>
concept uninstantiable = leviathan_pyobj<AdaptorType> &&
    details::defines_instantiation_concept<AdaptorType> &&
    requires(AdaptorType obj) {
        requires std::same_as<typename AdaptorType::instantiation_concept,
            disabled_instantiation>;
    };

template <typename AdaptorType>
concept vectorcall_instantiable = leviathan_pyobj<AdaptorType> &&
    details::defines_instantiation_concept<AdaptorType> &&
    requires(AdaptorType obj) {
        typename AdaptorType::instantiation_concept;
        requires std::same_as<typename AdaptorType::instantiation_concept,
            vector_instantiation>;
    };

template <typename S, auto D, typename R, typename... Ts>
struct adapted_constructor;

namespace details {

template <typename S, auto D, typename R>
LEV_HIDDEN inline constexpr bool valid_initializer_traits = false;

template <typename S, method::decorated_with<static_method> auto D,
    std::same_as<python_ptr<S>> R>
LEV_HIDDEN inline constexpr bool valid_initializer_traits<S, D, R> = true;

template <typename S, method::decorator auto D, std::same_as<result_code> R>
requires (!method::decorated_with<static_method>)
LEV_HIDDEN inline constexpr bool valid_initializer_traits<S, D, R> = true;

template <typename S, method::decorator auto D, std::same_as<void> R>
requires (!method::decorated_with<static_method>)
LEV_HIDDEN inline constexpr bool valid_initializer_traits<S, D, R> = true;

template <typename T>
struct initializer_arguments;

template <leviathan_pyobj S, method::decorator auto D, typename R,
    argument::declaration T, argument::declaration... Ts>
struct initializer_arguments<adapted_constructor<S, D, R, T, Ts...>> {
    using type = argument::tuple<T, Ts...>;
};

template <leviathan_pyobj S, method::decorator auto D, typename R>
struct initializer_arguments<adapted_constructor<S, D, R>> {
    using type = void;
};

template <typename T>
struct initializer_decorator;

template <leviathan_pyobj S, method::decorator auto D, typename R,
    argument::declaration... Ts>
struct initializer_decorator<adapted_constructor<S, D, R, Ts...>> {
    LEV_HIDE_INSTANTIATION static constexpr auto value = D;
};

template <typename T>
struct initializer_result;

template <leviathan_pyobj S, method::decorator auto D, typename R,
    argument::declaration... Ts>
struct initializer_result<adapted_constructor<S, D, R, Ts...>> {
    using type = R;
};

} // namespace details

template <leviathan_pyobj T>
using initializer_result_t = typename details::initializer_result<decltype(map(
    initializer_entry<T>{}))>::type;

template <leviathan_pyobj T>
using initializer_arguments_t =
    typename details::initializer_arguments<decltype(map(
        initializer_entry<T>{}))>::type;

template <leviathan_pyobj S>
struct initializer_entry {
    friend consteval auto map(initializer_entry) noexcept;
};

template <leviathan_pyobj S, method::decorator auto D, typename R,
    argument::declaration... Ts>
struct adapted_constructor {
    static_assert(details::valid_initializer_traits<S, D, R>,
        "Invalid decorator/result combination");
};

template <typename S, auto D, typename R, typename... Ts>
concept define_initializer = requires {
    requires sizeof(adapted_constructor<S, D, R, Ts...>) > 0;
    requires sizeof(type_map<initializer_entry<S>,
                 adapted_constructor<S, D, R, Ts...>>) > 0;
};

template <leviathan_pyobj T>
requires requires {
    requires vectorcall_instantiable<T>;
    typename initializer_arguments_t<T>;
    requires std::is_void_v<initializer_arguments_t<T>>;
}
python_ptr<T> initialize() noexcept {
    LEV_TRY {
        return T::init<initializer_decorator<T>::value, python_ptr<T>>();
    } LEV_CATCH(std::exception const& error) {
        PyErr_Format(PyExc_TypeError,
            "Initialization failed for object '%s', Reason=[Exception caught], "
            "Message=[%s]",
            adaptor_traits<T>::type_name(), error.what());
        return nullptr;
    } LEV_CATCH(...) {
        PyErr_Format(PyExc_TypeError,
            "Initialization failed for object '%s', Reason=[Unknown exception "
            "caught]",
            adaptor_traits<T>::type_name());
        return nullptr;
    }
}

template <leviathan_pyobj T>
requires requires {
    requires vectorcall_instantiable<T>;
    typename initializer_arguments_t<T>;
}
python_ptr<T> initialize(initializer_arguments_t<T>&& tuple) noexcept {
    LEV_TRY {
        return T::init<initializer_decorator<T>::value, python_ptr<T>>(
            std::move(tuple));
    } LEV_CATCH(std::exception const& error) {
        PyErr_Format(PyExc_TypeError,
            "Initialization failed for object '%s', Reason=[Exception caught], "
            "Message=[%s]",
            adaptor_traits<T>::type_name(), error.what());
        return nullptr;
    } LEV_CATCH(...) {
        PyErr_Format(PyExc_TypeError,
            "Initialization failed for object '%s', Reason=[Unknown exception "
            "caught]",
            adaptor_traits<T>::type_name());
        return nullptr;
    }
}

template <leviathan_pyobj T>
requires requires {
    requires basic_instantiable<T>;
    typename initializer_arguments_t<T>;
    requires std::is_void_v<initializer_arguments_t<T>>;
}
result_code initialize(non_owning_ptr<T> ptr) noexcept {
    using result_type = initializer_result_t<T>;
    if constexpr (std::is_same_v<result_type, result_code>) {
        return ptr->init<initializer_decorator<T>::value, result_type>();
    } else LEV_TRY {
        ptr->init<initializer_decorator<T>::value, result_type>();
        return result_code::success;
    } LEV_CATCH(std::exception const& error) {
        PyErr_Format(PyExc_TypeError,
            "Initialization failed for object '%s', Reason=[Exception caught], "
            "Message=[%s]",
            adaptor_traits<T>::type_name(), error.what());
        return result_code::failed;
    } LEV_CATCH(...) {
        PyErr_Format(PyExc_TypeError,
            "Initialization failed for object '%s', Reason=[Unknown exception "
            "caught]",
            adaptor_traits<T>::type_name());
        return result_code::failed;
    }
}

template <leviathan_pyobj T>
requires requires {
    requires basic_instantiable<T>;
    typename initializer_arguments_t<T>;
}
result_code initialize(
    non_owning_ptr<T> ptr, initializer_arguments_t<T>&& tuple) noexcept {
    using result_type = initializer_result_t<T>;
    if constexpr (std::is_same_v<result_type, result_code>) {
        return ptr->init<initializer_decorator<T>::value, result_code>(
            std::move(tuple));
    } else LEV_TRY {
        ptr->init<initializer_decorator<T>::value, result_code>(
            std::move(tuple));
        return result_code::success;
    } LEV_CATCH(std::exception const& error) {
        PyErr_Format(PyExc_TypeError,
            "Initialization failed for object '%s', Reason=[Exception caught], "
            "Message=[%s]",
            adaptor_traits<T>::type_name(), error.what());
        return result_code::failed;
    } LEV_CATCH(...) {
        PyErr_Format(PyExc_TypeError,
            "Initialization failed for object '%s', Reason=[Unknown exception "
            "caught]",
            adaptor_traits<T>::type_name());
        return result_code::failed;
    }
}

} // namespace lev
