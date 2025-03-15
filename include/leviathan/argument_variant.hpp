// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/argument_declaration.hpp"
#include "leviathan/variant.hpp"

namespace lev {
namespace argument {
template <typename T>
class variant;
}

template <typename>
inline constexpr bool is_argument_variant_v = false;

template <typename T>
inline constexpr bool is_argument_variant_v<T const> = is_argument_variant_v<T>;
template <typename T>
inline constexpr bool is_argument_variant_v<T const volatile> =
    is_argument_variant_v<T>;
template <typename T>
inline constexpr bool is_argument_variant_v<T volatile> =
    is_argument_variant_v<T>;

template <variant_declaration T>
inline constexpr bool is_argument_variant_v<argument::variant<T>> = true;

template <typename T>
concept argument_variant = is_argument_variant_v<T>;

namespace argument {
template <variant_declaration T, typename E = conversion_error>
class variant {
    template <typename F, typename... Vs>
    requires (... && argument_variant<std::remove_reference_t<Vs>>)
    LEV_HIDE_INSTANTIATION friend constexpr decltype(auto) visit(
        F&& callable, Vs&&... values);

    template <typename R, typename F, typename... Vs>
    requires (... && argument_variant<std::remove_reference_t<Vs>>)
    LEV_HIDE_INSTANTIATION friend constexpr R visit(
        F&& callable, Vs&&... values);

    using value_variant = decltype([]<typename... Us>(type<Us...>) {
        return lev::variant<Us...>{};
    }(std::declval<T>()));

public:
    LEV_HIDE_INSTANTIATION inline constexpr size_t index() const noexcept {
        return storage_.index();
    }

    LEV_HIDE_INSTANTIATION inline constexpr bool
    valueless_by_exception() const noexcept {
        return storage_.valueless_by_exception();
    }

    LEV_HIDE_INSTANTIATION inline constexpr bool has_value() const noexcept {
        // Silence exception leakage
        // NOLINTBEGIN
        return !valueless_by_exception() &&
            storage_.visit(
                [](auto const& result) noexcept { return result.has_value(); });
        // NOLINTEND
    }

    LEV_HIDE_INSTANTIATION explicit inline constexpr
    operator bool() const noexcept {
        return has_value();
    }

    using underlying_type =
        decltype([]<auto... Name, typename... Us>(typed<Name..., Us...>) {
            return lev::variant<lev::expected<Us, E>...>{};
        }(std::declval<T>()));

    template <typename F>
    requires requires(
        underlying_type const& var) { var.visit(std::declval<F>()); }
    LEV_HIDE_INSTANTIATION inline constexpr decltype(auto) visit(
        F&& callable) const& {
        return storage_.visit([&](auto&& result) {
            return std::invoke(std::forward<F>(callable),
                std::forward<decltype(result)>(result).value());
        });
    }

    template <typename F>
    requires requires(underlying_type& var) { var.visit(std::declval<F>()); }
    LEV_HIDE_INSTANTIATION inline constexpr decltype(auto) visit(
        F&& callable) & {
        return storage_.visit([&](auto&& result) {
            return std::invoke(std::forward<F>(callable),
                std::forward<decltype(result)>(result).value());
        });
    }

    template <typename F>
    requires requires(underlying_type const&& var) {
        std::move(var).visit(std::declval<F>());
    }
    LEV_HIDE_INSTANTIATION inline constexpr decltype(auto) visit(
        F&& callable) const&& {
        return std::move(storage_).visit([&](auto&& result) {
            return std::invoke(std::forward<F>(callable),
                std::forward<decltype(result)>(result).value());
        });
    }

    template <typename F>
    requires requires(
        underlying_type&& var) { std::move(var).visit(std::declval<F>()); }
    LEV_HIDE_INSTANTIATION inline constexpr decltype(auto) visit(
        F&& callable) && {
        return std::move(storage_).visit([&](auto&& result) {
            return std::invoke(std::forward<F>(callable),
                std::forward<decltype(result)>(result).value());
        });
    }

    template <typename R, typename F>
    requires requires(
        underlying_type const& var) { var.visit<R>(std::declval<F>()); }
    LEV_HIDE_INSTANTIATION inline constexpr R visit(F&& callable) const& {
        return storage_.visit<R>([&](auto&& result) {
            return std::invoke_r<R>(std::forward<F>(callable),
                std::forward<decltype(result)>(result).value());
        });
    }

    template <typename R, typename F>
    requires requires(underlying_type& var) { var.visit<R>(std::declval<F>()); }
    LEV_HIDE_INSTANTIATION inline constexpr R visit(F&& callable) & {
        return storage_.visit<R>([&](auto&& result) {
            return std::invoke_r<R>(std::forward<F>(callable),
                std::forward<decltype(result)>(result).value());
        });
    }

    template <typename R, typename F>
    requires requires(underlying_type const&& var) {
        std::move(var).visit<R>(std::declval<F>());
    }
    LEV_HIDE_INSTANTIATION inline constexpr R visit(F&& callable) const&& {
        return std::move(storage_).visit<R>([&](auto&& result) {
            return std::invoke_r<R>(std::forward<F>(callable),
                std::forward<decltype(result)>(result).value());
        });
    }

    template <typename R, typename F>
    requires requires(
        underlying_type&& var) { std::move(var).visit<R>(std::declval<F>()); }
    LEV_HIDE_INSTANTIATION inline constexpr R visit(F&& callable) && {
        return std::move(storage_).visit<R>([&](auto&& result) {
            return std::invoke_r<R>(std::forward<F>(callable),
                std::forward<decltype(result)>(result).value());
        });
    }

    template <std::invocable<E&> F>
    requires std::is_constructible_v<value_variant, value_variant&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto or_else(
        F&& callable) & -> std::invoke_result_t<F, E&> {
        using return_type = std::invoke_result_t<F, E&>;
        static_assert(is_expected_type_v<return_type>, "Invalid return type");
        using error_type = typename return_type::error_type;
        return storage_.visit([&]<typename U>(U& result) {
            return variant<T, error_type>{generate_type<U>,
                [&]() { return result.or_else(std::forward<F>(callable)); }};
        });
    }

    template <std::invocable<E const&> F>
    requires std::is_constructible_v<value_variant, value_variant const&>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto or_else(
        F&& callable) const& -> std::invoke_result_t<F, E const&> {
        using return_type = std::invoke_result_t<F, E const&>;
        static_assert(is_expected_type_v<return_type>, "Invalid return type");
        using error_type = typename return_type::error_type;
        return storage_.visit([&]<typename U>(U const& result) {
            return variant<T, error_type>{generate_type<U>,
                [&]() { return result.or_else(std::forward<F>(callable)); }};
        });
    }

    template <std::invocable<E> F>
    requires std::is_constructible_v<value_variant, value_variant>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto or_else(
        F&& callable) && -> std::invoke_result_t<F, E> {
        using return_type = std::invoke_result_t<F, E>;
        static_assert(is_expected_type_v<return_type>, "Invalid return type");
        using error_type = typename return_type::error_type;
        return std::move(storage_).visit([&]<typename U>(U&& result) {
            return variant<T, error_type>{generate_type<U>, [&]() {
                                              return std::move(result).or_else(
                                                  std::forward<F>(callable));
                                          }};
        });
    }

    template <std::invocable<E const> F>
    requires std::is_constructible_v<value_variant, value_variant const>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto or_else(
        F&& callable) const&& -> std::invoke_result_t<F, E const> {
        using return_type = std::invoke_result_t<F, E const>;
        static_assert(is_expected_type_v<return_type>, "Invalid return type");
        using error_type = typename return_type::error_type;
        return std::move(storage_).visit([&]<typename U>(U const&& result) {
            return variant<T, error_type>{generate_type<U>, [&]() {
                                              return std::move(result).or_else(
                                                  std::forward<F>(callable));
                                          }};
        });
    }

    template <typename F>
    requires requires(value_variant& var) {
        requires std::is_constructible_v<E, E&>;
        var.visit(std::declval<F>());
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    and_then(F&& callable) & -> decltype(std::declval<value_variant&>().visit(
        std::declval<F>())) {
        using return_type =
            decltype(std::declval<value_variant&>().visit(std::declval<F>()));
        static_assert(is_expected_type_v<return_type>, "Invalid return type");

        using value_type = typename return_type::value_type;
        return storage_.visit([&](auto& result) {
            return result.and_then(std::forward<F>(callable));
        });
    }

    template <typename F>
    requires requires(value_variant const& var) {
        requires std::is_constructible_v<E, E const&>;
        var.visit(std::declval<F>());
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto and_then(
        F&& callable) const& -> decltype(std::declval<value_variant const&>()
                                             .visit(std::declval<F>())) {
        using return_type = decltype(std::declval<value_variant const&>().visit(
            std::declval<F>()));
        static_assert(is_expected_type_v<return_type>, "Invalid return type");

        using value_type = typename return_type::value_type;
        return storage_.visit([&](auto const& result) {
            return result.and_then(std::forward<F>(callable));
        });
    }

    template <typename F>
    requires requires(value_variant&& var) {
        requires std::is_constructible_v<E, E>;
        var.visit(std::declval<F>());
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto
    and_then(F&& callable) && -> decltype(std::declval<value_variant>().visit(
        std::declval<F>())) {
        using return_type =
            decltype(std::declval<value_variant>().visit(std::declval<F>()));
        static_assert(is_expected_type_v<return_type>, "Invalid return type");

        using value_type = typename return_type::value_type;
        return std::move(storage_).visit([&](auto&& result) {
            return std::move(result).and_then(std::forward<F>(callable));
        });
    }

    template <typename F>
    requires requires(value_variant const&& var) {
        requires std::is_constructible_v<E, E const>;
        var.visit(std::declval<F>());
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto and_then(
        F&& callable) const&& -> decltype(std::declval<value_variant const>()
                                              .visit(std::declval<F>())) {
        using return_type = decltype(std::declval<value_variant const>().visit(
            std::declval<F>()));
        static_assert(is_expected_type_v<return_type>, "Invalid return type");

        using value_type = typename return_type::value_type;
        return std::move(storage_).visit([&](auto&& result) {
            return std::move(result).and_then(std::forward<F>(callable));
        });
    }

private:
    underlying_type storage_;
};

template <typename F, typename... Vs>
requires (... && argument_variant<std::remove_reference_t<Vs>>)
LEV_HIDE_INSTANTIATION inline constexpr decltype(auto) visit(
    F&& callable, Vs&&... values) {
    return visit(
        [&]<typename... Us>(Us&&... results) {
            return std::invoke(
                std::forward<F>(callable), std::forward<U>(results).value()...);
        },
        std::forward<Vs>(values).storage_...);
}

template <typename R, typename F, typename... Vs>
requires (... && argument_variant<std::remove_reference_t<Vs>>)
LEV_HIDE_INSTANTIATION inline constexpr R visit(F&& callable, Vs&&... values) {
    return visit<R>(
        [&]<typename... Us>(Us&&... results) {
            return std::invoke_r<R>(
                std::forward<F>(callable), std::forward<U>(results).value()...);
        },
        std::forward<Vs>(values).storage_...);
}
} // namespace argument
} // namespace lev
