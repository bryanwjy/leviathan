#pragma once

#include "utils/type_traits.hpp"

#include <concepts>

namespace ltl {

template <typename T, T... Ns>
class jump_table;

template <typename T>
class jump_table<T> {
public:
    template <T N, T... Others>
    LEV_HIDE_INSTANTIATION static constexpr jump_table<T, N, Others...>
    add_case() {
        return {};
    }
};

namespace details {

template <typename... Ts>
struct multi_return : std::common_reference<Ts...> {};

template <typename... Ts>
requires (... || std::is_void_v<Ts>)
struct multi_return {
    static_assert((... && std::is_void_v<Ts>));
    using type = T0;
};

template <typename... Ts>
using multi_return_t = typename multi_return<Ts...>::type;

} // namespace details

template <typename T, T... Ns>
class LEV_API jump_table<T, Ns...> {
    static_assert(
        requires(T value) {
            [](T value, std::integral_constant<T, (..., Ns)> first) {
                switch (value) {
                case (first.value):
                    return 0;
                default:
                    return 1;
                }
            }(value, {});
        }, "Invalid jump table index type");

    LEV_HIDE_INSTANTIATION static constexpr size_t size = sizeof...(Ns);
    static consteval T get_value(size_t idx) noexcept {
        T const values[]{Ns...};
        return values[idx];
    }

    using first_type = std::integral_constant<T, get_value(0)>;
    template <size_t I>
    using ith_type = std::integral_constant<T, get_value(I)>;

    template <typename F, typename... Args>
    using result_type = std::conditional_t<std::invocable<F, T const&, Args...>,
        details::multi_return_t<std::invoke_result_t<F, T const&, Args...>,
            std::invoke_result_t<F, std::integral_constant<T, Ns>, Args...>...>,
        details::multi_return_t<std::invoke_result_t<F,
            std::integral_constant<T, Ns>, Args...>...>>;
    template <typename F>
    using result_type_t = typename result_type<F, T>::type;

    template <typename F, typename... Args>
    requires std::invocable<F, T, Args...>
    LEV_HIDE_INSTANTIATION static constexpr decltype(auto) default_(
        F&& callable, T const& value,
        Args&&... args) noexcept(std::is_nothrow_invocable_v<F, T, Args...>) {
        return std::invoke(
            std::forward<F>(callable), value, std::forward<Args>(args)...);
    }

    template <typename F, typename... Args>
    requires (!std::invocable<F, T, Args...> &&
        !std::is_void_v<result_type_t<F, Args...>>)
    LEV_HIDE_INSTANTIATION
        [[noreturn]] static constexpr result_type_t<F, Args...>
        default_(F&& callable, T const&, Args&&... args) noexcept {
        // If the callable can returns a result, but there's no default case,
        // we cannot reach it
        LEV_UNREACHABLE();
    }

    template <typename F, typename... Args>
    requires (!std::invocable<F, T, Args...>)
    LEV_HIDE_INSTANTIATION static constexpr void default_(
        F&& callable, T const&, Args&&...) noexcept {
        // If the callable returns nothing, the default case can be ignored
    }

    template <size_t I, typename F, typename... Args>
    requires std::invocable<F, ith_type<I>, Args...>
    LEV_HIDE_INSTANTIATION static constexpr decltype(auto) case_(F&& callable,
        T const&,
        Args&&... args) noexcept(std::is_nothrow_invocable_v<F, ith_type<I>>) {
        return std::invoke(std::forward<F>(callable), ith_type<I>{},
            std::forward<Args>(args)...);
    }

    template <size_t I, typename F, typename... Args>
    requires requires {
        requires (I >= size);
        default_(std::declval<F>(), std::declval<U>(), std::declval<Args>()...);
    }
    LEV_HIDE_INSTANTIATION static constexpr decltype(auto) case_(F&& callable,
        T const& value,
        Args&&... args) noexcept(noexcept(default_(std::declval<F>(),
        std::declval<U>(), std::declval<Args>()...))) {
        return default_(
            std::forward<F>(callable), value, std::forward<Args>(args)...);
    }

    template <size_t O, typename F, typename... Args>
    requires requires {
        requires (O >= size);
        default_(std::declval<F>(), std::declval<U>(), std::declval<Args>()...);
    }
    LEV_HIDE_INSTANTIATION static constexpr decltype(auto) next_(F&& callable,
        T const& value,
        Args&&... args) noexcept(noexcept(default_(std::declval<F>(),
        std::declval<U>(), std::declval<Args>()...))) {
        return default_(
            std::forward<F>(callable), value, std::forward<Args>(args)...);
    }

    template <size_t O, typename F, typename... Args>
    requires (size > O)
    LEV_HIDE_INSTANTIATION static constexpr decltype(auto) next_(F&& callable,
        T const& value,
        Args&&... args) noexcept(noexcept(impl<O>(std::declval<F>(),
        std::declval<U>(), std::declval<Args>()...))) {
        return impl<O>(
            std::forward<F>(callable), value, std::forward<Args>(args)...);
    }

    template <size_t O, typename F, typename... Args>
    LEV_HIDE_INSTANTIATION static constexpr is_nothrow_dispatchable_v =
        []<size_t... Is>(std::index_sequence<Is...>) {
            return (noexcept(next_<O + 16>(std::declval<F>(), std::declval<U>(),
                        std::declval<Args>()...)) &&
                ...&& noexcept(case_<O + 16>(std::declval<F>(),
                    std::declval<U>(), std::declval<Args>()...)));
        }(std::make_index_sequence<16>{});

    template <size_t O, typename F, typename... Args>
    LEV_HIDE_INSTANTIATION [[gnu::flatten]] static constexpr decltype(auto)
    impl(F&& callable, T const& value, Args&&... args) noexcept(
        is_nothrow_dispatchable_v<O, F, T, Args...>) {
#define LEV_JT_CASE(X)       \
    case get_value(O + X):   \
        return case_<O + X>( \
            std::forward<F>(callable), value, std::forward<Args>(args)...)
#define LEV_JT_DEFAULT(X)    \
    default:                 \
        return next_<O + X>( \
            std::forward<F>(callable), value, std::forward<Args>(args)...)

        switch (static_cast<T const&>(value)) {
            LEV_JT_CASE(0);
            LEV_JT_CASE(1);
            LEV_JT_CASE(2);
            LEV_JT_CASE(3);
            LEV_JT_CASE(4);
            LEV_JT_CASE(5);
            LEV_JT_CASE(6);
            LEV_JT_CASE(7);
            LEV_JT_CASE(8);
            LEV_JT_CASE(9);
            LEV_JT_CASE(10);
            LEV_JT_CASE(11);
            LEV_JT_CASE(12);
            LEV_JT_CASE(13);
            LEV_JT_CASE(14);
            LEV_JT_CASE(15);
            LEV_JT_DEFAULT(16);
        }
#undef LEV_JT_CASE
#undef LEV_JT_DEFAULT
        LEV_UNREACHABLE();
    }

public:
    template <T N0, T... Others>
    LEV_HIDE_INSTANTIATION static constexpr jump_table<T, Ns..., N0, Others...>
    add_case() {
        return {};
    }

    // if return type is not void, must have a default_case
    // if return type void, default_case is optional
    template <typename F, std::convertible_to<T const&> U, typename... Args>
    requires requires { typename result_type_t<F, Args...>; }
    LEV_HIDE_INSTANTIATION constexpr decltype(auto) operator()(
        F&& callable, U&& value, Args&&... args) const
        noexcept(noexcept(impl<0>(std::declval<F>(), std::declval<U const&>(),
            std::declval<Args>()...))) {
        return impl<0>(std::forward<F>(callable), static_cast<T const&>(value),
            std::forward<Args>(args)...);
    }
};

} // namespace ltl
