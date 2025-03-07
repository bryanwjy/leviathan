// Copyright 2025, Bryan Wong
#pragma once

#include <Python.h>

#include <concepts>
#include <span>
#include <type_traits>

namespace lev {

namespace details {
struct method_flags {
    enum LEV_API values : int {
        var_args = METH_VARARGS,
        keywords = METH_KEYWORDS,
        no_args = METH_NOARGS,
        one_arg = METH_O,
        class_method = METH_CLASS,
        static_method = METH_STATIC,
        fastcall = METH_FASTCALL
    };

    LEV_HIDDEN friend constexpr values operator|(
        values lhs, values rhs) noexcept {
        return static_cast<values>(
            static_cast<int>(lhs) | static_cast<int>(rhs));
    }

    template <values V>
    LEV_HIDDEN friend constexpr bool has_flag(values val) noexcept {
        return static_cast<values>(
                   static_cast<int>(val) & static_cast<int>(V)) == all;
    }

    template <values... Flags>
    LEV_HIDDEN friend constexpr bool has_flags(values val) noexcept {
        auto const all = (... | Flags);
        return static_cast<values>(
                   static_cast<int>(val) & static_cast<int>(all)) == all;
    }
};

template <typename>
struct class_of {};
template <typename T>
using class_of_t = typename class_of<T>::type;
template <typename T>
struct class_of<T const> : class_of<T> {};
template <typename C, typename T>
struct class_of<T C::*> {
    using type = C;
};
template <typename F>
struct function_equivalent_of {};

template <typename F>
using function_equivalent_of_t = typename function_equivalent_of<F>::type;

template <typename R, typename C, typename... Args>
struct function_equivalent_of<R (C::*)(Args...) const noexcept> {
    using type = R (*)(Args...);
};

template <typename F>
concept stateless_lambda =
    std::is_class_v<F> && std::is_empty_v<F> && requires {
        &F::operator();
        requires std::is_convertible_v<F,
            function_equivalent_of_t<decltype(&F::operator())>>;
    };
} // namespace details

template <typename F>
concept function_like =
    std::is_pointer_v<std::remove_pointer_t<std::decay_t<F>>> ||
    std::is_member_function_pointer_v<F> || details::stateless_lambda<F>;

template <typename F, typename... Args>
concept member_invocable = std::is_member_function_pointer_v<F> &&
    requires(details::class_of_t<F>* obj, F func, Args... args) {
        { (obj->*func)(args...) } noexcept -> std::same_as<PyObject*>;
    };

template <typename Function, typename... Args>
concept static_invocable =
    std::is_function_v<std::remove_pointer_t<std::decay_t<Function>>> &&
    requires(std::decay_t<Function> ptr, Args... args) {
        { ptr(args...) } noexcept -> std::same_as<PyObject*>;
    };

template <typename Function, typename... Args>
concept class_invocable =
    std::is_function_v<std::remove_pointer_t<std::decay_t<Function>>> &&
    requires(std::decay_t<Function> ptr, PyTypeObject* type, Args... args) {
        { ptr(type, args...) } noexcept -> std::same_as<PyObject*>;
    };

namespace details {
template <method_flags, typename>
struct is_valid_signature : std::false_type {};
template <member_invocable<std::span<PyObject* const>, PyDictObject*> Func>
struct is_valid_signature<method_flags::var_args | method_flags::keywords,
    Func> : std::true_type {};
template <member_invocable<std::span<PyObject* const>> Func>
struct is_valid_signature<method_flags::var_args, Func> : std::true_type {};
template <
    member_invocable<std::span<PyObject* const>, std::span<PyObject* const>>
        Func>
struct is_valid_signature<method_flags::fastcall | method_flags::keywords,
    Func> : std::true_type {};
template <member_invocable<std::span<PyObject* const>> Func>
struct is_valid_signature<method_flags::fastcall, Func> : std::true_type {};
template <member_invocable Func>
struct is_valid_signature<method_flags::no_args, Func> : std::true_type {};
template <member_invocable<PyObject*> Func>
struct is_valid_signature<method_flags::one_arg, Func> : std::true_type {};

template <static_invocable<std::span<PyObject* const>, PyDictObject*> Func>
struct is_valid_signature<method_flags::static_method | method_flags::var_args |
        method_flags::keywords,
    Func> : std::true_type {};
template <static_invocable<std::span<PyObject* const>> Func>
struct is_valid_signature<method_flags::static_method | method_flags::var_args,
    Func> : std::true_type {};
template <
    static_invocable<std::span<PyObject* const>, std::span<PyObject* const>>
        Func>
struct is_valid_signature<method_flags::static_method | method_flags::fastcall |
        method_flags::keywords,
    Func> : std::true_type {};
template <static_invocable<std::span<PyObject* const>> Func>
struct is_valid_signature<method_flags::static_method | method_flags::fastcall,
    Func> : std::true_type {};
template <static_invocable Func>
struct is_valid_signature<method_flags::static_method | method_flags::no_args,
    Func> : std::true_type {};
template <static_invocable<PyObject*> Func>
struct is_valid_signature<method_flags::static_method | method_flags::one_arg,
    Func> : std::true_type {};

template <class_invocable<std::span<PyObject* const>, PyDictObject*> Func>
struct is_valid_signature<method_flags::class_method | method_flags::var_args |
        method_flags::keywords,
    Func> : std::true_type {};
template <class_invocable<std::span<PyObject* const>> Func>
struct is_valid_signature<method_flags::class_method | method_flags::var_args,
    Func> : std::true_type {};
template <
    class_invocable<std::span<PyObject* const>, std::span<PyObject* const>>
        Func>
struct is_valid_signature<method_flags::class_method | method_flags::fastcall |
        method_flags::keywords,
    Func> : std::true_type {};
template <class_invocable<std::span<PyObject* const>> Func>
struct is_valid_signature<method_flags::class_method | method_flags::fastcall,
    Func> : std::true_type {};
template <class_invocable Func>
struct is_valid_signature<method_flags::class_method | method_flags::no_args,
    Func> : std::true_type {};
template <class_invocable<PyObject*> Func>
struct is_valid_signature<method_flags::class_method | method_flags::one_arg,
    Func> : std::true_type {};
} // namespace details

using method_flags = details::method_flags::values;

template <typename F, method_flags Flags>
concept valid_signature = details::is_valid_signature<Flags, F>::value;

template <typename>
struct method_flag_of {};
template <method_flags Value>
using method_flag_type = enum_constant<Value>;

template <member_invocable<std::span<PyObject* const>, PyDictObject*> Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::var_args | method_flags::keywords> {};
template <member_invocable<std::span<PyObject* const>> Func>
struct method_flag_of<Func> : method_flag_type<method_flags::var_args> {};
template <
    member_invocable<std::span<PyObject* const>, std::span<PyObject* const>>
        Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::fastcall | method_flags::keywords> {};
template <member_invocable<std::span<PyObject* const>> Func>
struct method_flag_of<Func> : method_flag_type<method_flags::fastcall> {};
template <member_invocable Func>
struct method_flag_of<Func> : method_flag_type<method_flags::no_args> {};
template <member_invocable<PyObject*> Func>
struct method_flag_of<Func> : method_flag_type<method_flags::one_arg> {};

template <static_invocable<std::span<PyObject* const>, PyDictObject*> Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::static_method | method_flags::var_args |
        method_flags::keywords> {};
template <static_invocable<std::span<PyObject* const>> Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::static_method | method_flags::var_args> {};
template <
    static_invocable<std::span<PyObject* const>, std::span<PyObject* const>>
        Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::static_method | method_flags::fastcall |
        method_flags::keywords> {};
template <static_invocable<std::span<PyObject* const>> Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::static_method | method_flags::fastcall> {};
template <static_invocable Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::static_method | method_flags::no_args> {};
template <static_invocable<PyObject*> Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::static_method | method_flags::one_arg> {};

template <class_invocable<std::span<PyObject* const>, PyDictObject*> Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::class_method | method_flags::var_args |
        method_flags::keywords> {};
template <class_invocable<std::span<PyObject* const>> Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::class_method | method_flags::var_args> {};
template <
    class_invocable<std::span<PyObject* const>, std::span<PyObject* const>>
        Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::class_method | method_flags::fastcall |
        method_flags::keywords> {};
template <class_invocable<std::span<PyObject* const>> Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::class_method | method_flags::fastcall> {};
template <class_invocable Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::class_method | method_flags::no_args> {};
template <class_invocable<PyObject*> Func>
struct method_flag_of<Func> :
    method_flag_type<method_flags::class_method | method_flags::one_arg> {};

} // namespace lev
