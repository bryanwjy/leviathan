// Copyright 2025, Bryan Wong

#include "leviathan/pointer.hpp"

#include <Python.h>

#include <exception>
#include <type_traits>
#include <utility>

namespace lev {
namespace py {
struct nothrow_t {
    explicit inline constexpr nothrow_t() noexcept = default;
};
LEV_HIDDEN inline constexpr nothrow_t nothrow{};
struct end_tag_t {
    LEV_HIDE_INSTANTIATION explicit inline constexpr end_tag_t() noexcept =
        default;
};

LEV_HIDDEN inline constexpr end_tag_t end_tag{};

using size_t = decltype(sizeof(0));
using ptrdiff_t = decltype(static_cast<char*>(0) - static_cast<char*>(0));
using ssize_t = std::make_signed_t<size_t>;

LEV_HIDDEN inline constexpr size_t dynamic_extent = static_cast<size_t>(-1);

template <typename T, PyObject*& Exc, typename... Args>
requires requires(T const& error) {
    { error.what() } noexcept -> std::convertible_to<char const*>;
}
LEV_HIDE_INSTANTIATION [[noreturn]] void failure(Args&&... args) {
    LEV_THROW([&]() {
        T exc(std::forward<Args>(args)...);
        PyErr_Format(Exc,
            "Operation failed, Reason=[Exception thrown], Message=[%s]",
            static_cast<char const*>(std::as_const(exc).what()));
        return exc;
    }());
}

template <typename T>
requires requires(T const& error) {
    std::constructible_from<T, char const*>;
    { error.what() } noexcept -> std::convertible_to<char const*>;
}
LEV_HIDE_INSTANTIATION [[noreturn]] void unhandled_error() {
    LEV_THROW(T("Unhandled python error"));
}

class LEV_API type_error : public std::exception {
    static constexpr size_t buffer_size = 256;

public:
    LEV_HIDE_INSTANTIATION type_error(char const* str) noexcept {
        strncpy(buffer, str, sizeof(buffer));
        buffer[buffer_size - 1] = 0;
    }

    LEV_HIDE_INSTANTIATION char const* what() const noexcept { return buffer; }

private:
    char buffer[buffer_size];
};

template <typename Value>
class element_reference;

template <typename Impl>
class element_reference : Impl {
    using pointer = typename Impl::type*;
    using reference = typename Impl::type&;

public:
    using type = typename Impl::type;

    template <typename... Args>
    requires std::constructible_from<Impl, Args...>
    LEV_HIDE_INSTANTIATION explicit(is_explicit_constructible_v<Impl,
        Args...>) inline constexpr element_reference(Args&&... args) noexcept
        : Impl{std::forward<Args>(args)...} {}

    LEV_HIDE_INSTANTIATION
    inline constexpr unmanaged_ptr<type> get() const
        noexcept(noexcept(Impl::get_pointer()))
    requires requires(element_reference const& ref) {
        { impl.get_pointer() } -> std::convertible_to<unmanaged_ptr<type>>;
    }
    {
        return static_cast<unmanaged_ptr<type>>(Impl::get_pointer());
    }

    LEV_HIDE_INSTANTIATION inline constexpr operator pointer() const noexcept
    requires requires(element_reference const& ref) { impl.get(); }
    {
        return get();
    }

    LEV_HIDE_INSTANTIATION inline constexpr pointer operator->() const noexcept
    requires requires(element_reference const& ref) { impl.get(); }
    {
        return get();
    }

    LEV_HIDE_INSTANTIATION inline constexpr reference operator*() const
        noexcept(noexcept(Impl::get_reference()))
    requires requires(element_reference const& impl) {
        { impl.get_reference() } -> std::convertible_to<reference>;
    }
    {
        return static_cast<reference>(Impl::get_reference());
    }

    LEV_HIDE_INSTANTIATION explicit inline constexpr
    operator bool() const noexcept
    requires requires(element_reference const& ref) {
        { impl.valid() } -> std::convertible_to<bool>;
    }
    {
        return static_cast<bool>(Impl::valid());
    }

    template <typename U>
    requires requires(
        element_reference& impl) { impl.set_pointer(std::declval<U>()); }
    LEV_HIDE_INSTANTIATION inline constexpr element_reference& operator=(
        U&& val) noexcept(noexcept(Impl::set_pointer(std::declval<U>()))) {
        Impl::set_pointer(std::forward(val));
        return *this;
    }
};
} // namespace py
} // namespace lev
