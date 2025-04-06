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
class value_reference;

template <typename Impl>
class value_reference : Impl {
    using Impl::set_value;

public:
    using type = typename Impl::type;

    template <typename... Args>
    requires std::constructible_from<Impl, Args...>
    LEV_HIDE_INSTANTIATION explicit(is_explicit_constructible_v<Impl,
        Args...>) inline constexpr value_reference(Args&&... args) noexcept
        : Impl{std::forward<Args>(args)...} {}

    LEV_HIDE_INSTANTIATION inline constexpr unmanaged_ptr<type>
    value() const noexcept {
        static_assert(requires(Impl const& val) {
            {
                val.value()
            } noexcept -> std::convertible_to<unmanaged_ptr<type>>;
        });

        return static_cast<unmanaged_ptr<type>>(Impl::value());
    }

    LEV_HIDE_INSTANTIATION inline constexpr operator bool() const noexcept
    requires std::convertible_to<Impl, bool>
    {
        static_assert(
            noexcept(static_cast<bool>(static_cast<Impl const&>(*this))),
            "bool conversion operator must be noexcept");
        return static_cast<bool>(static_cast<Impl const&>(*this));
    }

    LEV_HIDE_INSTANTIATION inline constexpr operator bool() const noexcept {
        return static_cast<bool>(value());
    }

    LEV_HIDE_INSTANTIATION inline constexpr
    operator unmanaged_ptr<type>() const noexcept {
        return value();
    }

    LEV_HIDE_INSTANTIATION inline constexpr value_reference& operator=(
        python_ptr<type> val) noexcept(noexcept(set_value(std::move(val)))) {

        static_assert(requires(Impl const& val, python_ptr<type> arg) {
            { val.set_value(std::move(arg)) };
        });
        Impl::set_value(std::move(val));
        return *this;
    }

    template <pyobj_derived_from<type> U>
    LEV_HIDE_INSTANTIATION inline constexpr value_reference&
    operator=(python_ptr<U> val) noexcept(
        noexcept(set_value(static_ptr_cast<type>(std::move(val))))) {
        static_assert(requires(Impl const& val, python_ptr<type> arg) {
            { val.set_value(std::move(arg)) };
        });
        Impl::set_value(static_ptr_cast<type>(std::move(val)));
        return *this;
    }
};

template <pyobj_type T, typename Span>
class transforming_const_iterator : typename Span::const_iterator {
    using base_iterator = typename Span::const_iterator;

public:
    using value_type = unmanaged_ptr<T>;
    using difference_type = ptrdiff_t;
    using iterator_concept = std::random_access_iterator_tag;

    using base_iterator::base_iterator;

    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline auto
    operator*() const noexcept {
        return static_ptr_cast<T>(base_iterator::operator*());
    }

    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline auto operator[](
        size_t idx) const noexcept {
        return static_ptr_cast<T>(base_iterator::operator[](idx));
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr transforming_iterator
    operator+(difference_type offset) const noexcept {
        return transforming_iterator{base_iterator::base() + offset};
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard]] friend inline constexpr transforming_iterator operator+(
        difference_type offset, transforming_iterator const& it) noexcept {
        return transforming_iterator{it.base() + offset};
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr transforming_iterator
    operator-(difference_type offset) const noexcept {
        return transforming_iterator{base_iterator::base() - offset};
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard]] friend inline constexpr difference_type operator-(
        transforming_iterator const& left,
        transforming_iterator const& right) noexcept {
        return static_cast<base_iterator const&>(left) -
            static_cast<base_iterator const&>(right);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr transforming_iterator&
    operator+=(difference_type offset) noexcept {
        static_cast<base_iterator&>(*this) += offset;
        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr transforming_iterator&
    operator-=(difference_type offset) noexcept {
        static_cast<base_iterator&>(*this) -= offset;
        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr transforming_iterator&
    operator++() noexcept {
        ++static_cast<base_iterator&>(*this);
        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr transforming_iterator
    operator++(int) noexcept {
        transforming_iterator before = *this;
        ++static_cast<base_iterator&>(*this);
        return before;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr transforming_iterator&
    operator--() noexcept {
        --static_cast<base_iterator&>(*this);
        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr transforming_iterator
    operator--(int) noexcept {
        transforming_iterator before = *this;
        --static_cast<base_iterator&>(*this);
        return before;
    }
};

} // namespace py
} // namespace lev
