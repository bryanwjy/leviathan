// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/ownership_policy.hpp"
#include "leviathan/pointer.hpp"

#include <array>
#include <compare>
#include <iterator>

namespace lev {
namespace py {
template <typename Key, typename T = PyObject, bool Mutable = true>
class basic_dict;
template <typename Key, typename T = PyObject, bool Mutable = true>
class basic_dict_view;

template <typename Key, typename T = PyObject>
using dict = basic_dict<Key, T, true>;
template <typename Key, typename T = PyObject>
using const_dict = basic_dict<Key, T, false>;
template <typename Key, typename T = PyObject>
using dict_view = basic_dict_view<Key, T, true>;
template <typename Key, typename T = PyObject>
using const_dict_view = basic_dict_view<Key, T, false>;
using kwargs = const_dict<PyASCIIObject>;

namespace views {
template <pyobj_type Key, pyobj_type T>
using dict = dict_view<Key, T, true>;
template <pyobj_type Key, pyobj_type T>
using const_dict = dict_view<Key, T, false>;
using kwargs = const_dict_view<PyASCIIObject>;
} // namespace views

namespace details::dict {

template <pyobj_type Key, pyobj_type T>
class value_setter {
public:
    constexpr value_setter(unmanaged_ptr<PyDictObject> container,
        unmanaged_ptr<Key> key, unmanaged_ptr<PyObject> val) noexcept
        : dict_{container}
        , key_{key}
        , value_{val} {}

protected:
    using type = T;

    LEV_HIDE_INSTANTIATION inline constexpr operator bool() const noexcept {
        if constexpr (std::same_as<PyObject, T>) {
            return value_ != nullptr;
        } else {
            return value_ && dynamic_ptr_cast<T>(value_);
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr unmanaged_ptr<T> value() const {
        LEV_ASSERT(value_ != nullptr);

        if constexpr (std::same_as<PyObject, T>) {
            return static_ptr_cast<T>(value_);
        } else {
            if (auto ptr = dynamic_ptr_cast<T>(value_)) {
                return ptr;
            }

            failure<type_error, PyExc_TypeError>(format_cstring(
                "Unexpected mapped type, Expected=[%s], Found=[%s]",
                type_object_v<T>->tp_name, Py_TYPE(value_)->tp_name)
                                                     .data());
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr void set_value(
        unmanaged_ptr<T> val) const {
        LEV_ASSERT(val);
        if (value_ == nullptr) {
            if (PyErr_Occured()) {
                unhandled_error<std::runtime_error>();
            }

            failure<std::runtime_error, PyExc_RuntimeError>(
                "Invalid reference proxy state for dictionary");
        }

        PyDict_SetItem(dict_, key_, val);
    }

private:
    unmanaged_ptr<PyDictObject> dict_;
    unmanaged_ptr<Key> key_;
    unmanaged_ptr<PyObject> value_;
};

template <pyobj_type Key, pyobj_type T>
class iterator {};

template <pyobj_type Key, pyobj_type T>
class const_iterator {};

} // namespace details::dict

template <pyobj_type Key, pyobj_type T, bool M>
requires requires(unmanaged_ptr<PyObject> ptr) {
    { dynamic_ptr_cast<T>(ptr) } noexcept;
    type_object_v<T>;
}
class basic_dict<Key, T, M> {
    template <typename, typename, bool>
    friend class basic_dict;

    using mapped_reference =
        value_reference<details::dict::value_setter<Key, T>>;

public:
    using key_type = unmanaged_ptr<Key>;
    using mapped_type = unmanaged_ptr<T>;
    using value_type = std::pair<unmanaged_ptr<Key> const, unmanaged_ptr<T>>;
    using reference = std::conditional_t<M,
        std::pair<unmanaged_ptr<Key> const, mapped_reference>, value_type>;
    using const_reference = value_type;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using const_iterator = details::dict::const_iterator<Key, T>;
    using iterator =
        std::conditional_t<M, details::dict::iterator<Key, T>, const_iterator>;

    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict(
        python_ptr<PyDictObject>&& ptr) noexcept
        : instance_{std::move(ptr)} {
        if constexpr (!M) {
            LEV_ASSERT(instance_);
        }
    }
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict(
        python_ptr<PyDictObject> const& ptr) noexcept
        : instance_{ptr} {
        if constexpr (!M) {
            LEV_ASSERT(instance_);
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict const& other) noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict&& other) noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict& operator=(
        basic_dict const& other) noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict& operator=(
        basic_dict&& other) noexcept = default;

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict<K, V, Mutable> const& other) noexcept
        : instance_{other.instance_} {
        if constexpr (!M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict<K, V, Mutable>&& other) noexcept
        : instance_{std::move(other.instance_)} {
        if constexpr (!M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict<K, V, Mutable> const& other) noexcept
        : instance_{other.instance_} {
        if constexpr (!M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict<K, V, Mutable>&& other) noexcept
        : instance_{std::move(other.instance_)} {
        if constexpr (!M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION explicit inline dict(
        adopt_t tag, basic_dict_view<K, V, Mutable> other) noexcept
        : instance_{tag, other.instance()} {
        if constexpr (!M) {
            LEV_ASSERT(instance_);
        }
    }

    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline constexpr bool
    empty() const noexcept {
        return size() == 0;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline constexpr size_t
    size() const noexcept {
        if constexpr (M) {
            return PyDict_Size(instance_);
        } else {
            return instance_ ? PyDict_Size(instance_) : 0;
        }
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard, gnu::pure]] inline constexpr unmanaged_ptr<PyTupleObject>
    instance() const noexcept LEV_LIFETIMEBOUND {
        return instance_.get();
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION bool remove(unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        if (PyDict_DelItem(instance_, key) == result_code::success) {
            return true;
        }

        PyErr_Clear();
        return false;
    }

    template <nothrow_t, pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION bool remove(unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        return remove(key);
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] mapped_reference at(
        unmanaged_ptr<K> key)
    requires (M)
    {
        return mapped_reference{instance_, key, std::as_const(*this).at(key)};
    }

    template <nothrow_t Tag, pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] unmanaged_ptr<T> at(
        unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        return mapped_reference{
            instance_, key, std::as_const(*this).at<Tag>(key)};
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] unmanaged_ptr<T> at(
        unmanaged_ptr<K> key) const {
        LEV_ASSERT(key);

        if constexpr (!M) {
            if (!instance_) {
                failure<std::runtime_error, PyExc_ValueError>(
                    "Invalid dictionary instance");
            }
        }

        auto pyobj = PyDict_GetItemWithError(instance_, key);
        if (PyErr_Occured()) {
            unhandled_error<std::runtime_error>();
        }

        if (pyobj == nullptr) {
            failure<std::out_of_range, PyExc_ValueError>(
                "The requested key was not found in the dictionary");
        }

        auto ptr = dynamic_ptr_cast<T>(pyobj);

        if constexpr (!std::same_as<PyObject, T>) {
            if (ptr == nullptr) {
                failure<type_error, PyExc_TypeError>(format_cstring(
                    "The associated value does not have the "
                    "expected type, Expected=[%s], Retrieved=[%s]",
                    type_object_v<T>->tp_name, Py_TYPE(pyobj)->tp_name)
                                                         .data());
            }
        }

        return ptr;
    }

    template <nothrow_t, pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] unmanaged_ptr<T> at(
        unmanaged_ptr<K> key) const noexcept {
        LEV_ASSERT(key);
        if constexpr (!M) {
            return instance_
                ? dynamic_ptr_cast<T>(PyDict_GetItem(instance_, key))
                : nullptr;
        } else {
            return dynamic_ptr_cast<T>(PyDict_GetItem(instance_, key));
        }
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] mapped_reference operator[](
        unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        LEV_ASSERT(key);
        unmanaged_ptr<PyObject> value =
            PyDict_SetDefault(instance_, key, Py_None);
        return mapped_reference{instance_, key, value};
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V>
    LEV_HIDE_INSTANTIATION void set_value(
        unmanaged_ptr<K> key, unmanaged_ptr<V> value)
    requires (M)
    {
        LEV_ASSERT(key);
        LEV_ASSERT(value);
        if (PyDict_SetItem(instance_, key, value) != result_code::success) {
            unhandled_error<std::runtime_error>();
        }
    }

    template <nothrow_t, pyobj_derived_from<Key> K, pyobj_derived_from<T> V>
    LEV_HIDE_INSTANTIATION bool set_value(
        unmanaged_ptr<K> key, unmanaged_ptr<V> value) noexcept
    requires (M)
    {
        LEV_ASSERT(key);
        LEV_ASSERT(value);
        if (PyDict_SetItem(instance_, key, value) == result_code::success) {
            return true;
        }

        PyErr_Clear();
        return false;
    }

private:
    python_ptr<PyDictObject> instance_;
};

// TODO view

} // namespace py
} // namespace lev
