// Copyright 2025, Bryan Wong

#include "leviathan/pointer.hpp"

#include <Python.h>

#include <compare>
#include <span>

namespace lev {
namespace py {

template <typename T, cast_policy = cast_policy::safe>
class random_access_iterator;
template <typename T, cast_policy = cast_policy::safe>
class random_access_const_iterator;

namespace details {

template <pyobj_type T, cast_policy P>
class random_access_reference<T, P> {
public:
    LEV_HIDE_INSTANTIATION explicit inline constexpr random_access_reference(
        PyObject*& ptr)
        : location_{ptr} {}

protected:
    LEV_HIDE_INSTANTIATION inline constexpr ~random_access_reference() =
        default;

    LEV_HIDE_INSTANTIATION inline constexpr unmanaged_ptr<T>
    get_pointer() const noexcept {
        return dispatch_cast<T, P>(location_);
    }

    LEV_HIDE_INSTANTIATION inline constexpr bool valid() const noexcept {
        return get_pointer();
    }

    LEV_HIDE_INSTANTIATION inline constexpr T& get_reference() const noexcept {
        LEV_ASSERT(location_);
        return static_ptr_cast<T>(*location_);
    }

    LEV_HIDE_INSTANTIATION inline constexpr T& get_reference() const
    requires (P == cast_policy::safe)
    {
        LEV_ASSERT(location_);
        if (auto ptr = dynamic_ptr_cast<T>(location_)) {
            return *ptr;
        }

        failure<type_error, PyExc_TypeError>(
            "Unexpected type dereferenced during random access iteration");
    }

    LEV_HIDE_INSTANTIATION inline constexpr void set_pointer(
        python_ptr<T> const& ptr) const {
        python_ptr{retain_object,
            exchange(location_, as_pyobject(adopt(ptr).release()))}
            .reset();
    }

    LEV_HIDE_INSTANTIATION inline constexpr void set_pointer(
        python_ptr<T>&& ptr) const {
        python_ptr{
            retain_object, exchange(location_, as_pyobject(ptr.release()))}
            .reset();
    }

    template <pyobj_derived_from<T> U>
    LEV_HIDE_INSTANTIATION inline constexpr void set_pointer(
        python_ptr<U> const& ptr) const {
        python_ptr{retain_object,
            exchange(location_, as_pyobject(adopt(ptr).release()))}
            .reset();
    }

    template <pyobj_derived_from<T> U>
    LEV_HIDE_INSTANTIATION inline constexpr void set_pointer(
        python_ptr<U>&& ptr) const {
        python_ptr{
            retain_object, exchange(location_, as_pyobject(ptr.release()))}
            .reset();
    }

private:
    PyObject*& location_;
};
} // namespace details

template <pyobj_type T, cast_policy P>
class random_access_iterator<T, P> {
    using this_type = random_access_const_iterator;
    using reference_proxy =
        element_reference<details::random_access_reference<T, P>>;

public:
    using value_type = unmanaged_ptr<T>;
    using difference_type = ptrdiff_t;
    using iterator_concept = std::random_access_iterator_tag;

    LEV_HIDE_INSTANTIATION inline constexpr random_access_iterator() noexcept
        : location_{nullptr} {}
    LEV_HIDE_INSTANTIATION explicit inline constexpr random_access_iterator(
        PyObject** location) noexcept
        : location_{location} {}

    LEV_HIDE_INSTANTIATION inline constexpr random_access_const_iterator(
        random_access_const_iterator const&) noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr random_access_const_iterator&
    operator=(random_access_const_iterator const&) noexcept = default;

    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline auto
    operator*() const noexcept {
        return reference_proxy(*location_);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline auto operator[](
        difference_type idx) const noexcept {
        return reference_proxy(location_[idx]);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type operator+(
        difference_type offset) const noexcept {
        return this_type{location_ + offset};
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard]] friend inline constexpr this_type operator+(
        difference_type offset, this_type const& it) noexcept {
        return this_type{location_ + offset};
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type operator-(
        difference_type offset) const noexcept {
        return this_type{location_ - offset};
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard]] friend inline constexpr difference_type operator-(
        this_type const& left, this_type const& right) noexcept {
        return static_cast<base_iterator const&>(left) -
            static_cast<base_iterator const&>(right);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type& operator+=(
        difference_type offset) noexcept {
        location_ += offset;
        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type& operator-=(
        difference_type offset) noexcept {
        location_ -= offset;
        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type&
    operator++() noexcept {
        ++location_;
        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type operator++(
        int) noexcept {
        this_type before = *this;
        ++location_;
        return before;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type&
    operator--() noexcept {
        --location_;
        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type operator--(
        int) noexcept {
        this_type before = *this;
        --location_;
        return before;
    }

    LEV_HIDE_INSTANTIATION inline constexpr
    operator random_access_const_iterator<T>() {
        return random_access_const_iterator<T>{location_};
    }

private:
    PyObject** location_;
};

template <pyobj_type T, cast_policy P>
class random_access_const_iterator<T, P> {
    using this_type = random_access_const_iterator;

public:
    using value_type = unmanaged_ptr<T>;
    using difference_type = ptrdiff_t;
    using iterator_concept = std::random_access_iterator_tag;

    LEV_HIDE_INSTANTIATION inline constexpr random_access_const_iterator() noexcept
        : location_{nullptr} {}
    LEV_HIDE_INSTANTIATION explicit inline constexpr random_access_const_iterator(
        PyObject* const* location) noexcept
        : location_{location} {}

    LEV_HIDE_INSTANTIATION inline constexpr random_access_const_iterator(
        random_access_const_iterator const&) noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr random_access_const_iterator&
    operator=(random_access_const_iterator const&) noexcept = default;

    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline auto
    operator*() const noexcept {
        return dispatch_cast<T, P>(*location_);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline auto operator[](
        difference_type idx) const noexcept {
        return dispatch_cast<T, P>(location_[idx]);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type operator+(
        difference_type offset) const noexcept {
        return this_type{location_ + offset};
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard]] friend inline constexpr this_type operator+(
        difference_type offset, this_type const& it) noexcept {
        return this_type{location_ + offset};
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type operator-(
        difference_type offset) const noexcept {
        return this_type{location_ - offset};
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard]] inline constexpr difference_type operator-(
        this_type const& right) noexcept {
        return left.location_ - right.location_;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type& operator+=(
        difference_type offset) noexcept {
        location_ += offset;
        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type& operator-=(
        difference_type offset) noexcept {
        location_ -= offset;
        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type&
    operator++() noexcept {
        ++location_;
        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type operator++(
        int) noexcept {
        this_type before = *this;
        ++location_;
        return before;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type&
    operator--() noexcept {
        --location_;
        return *this;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr this_type operator--(
        int) noexcept {
        this_type before = *this;
        --location_;
        return before;
    }

private:
    PyObject* const* location_;
}

} // namespace py
} // namespace lev
