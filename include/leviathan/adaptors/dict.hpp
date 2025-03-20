// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/ownership_policy.hpp"
#include "leviathan/pointer.hpp"

#include <array>
#include <compare>
#include <iterator>
#include <system_error>

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

        if constexpr (std::is_convertible_v<unmanaged_ptr<PyObject>,
                          unmanaged_ptr<T>>) {
            return value_;
        } else {
            auto ptr = dynamic_ptr_cast<T>(value_);
            if (!ptr) {
                failure<type_error, PyExc_TypeError>(format_cstring(
                    "Unexpected mapped type, Expected=[%s], Found=[%s]",
                    type_object_v<T>->tp_name, Py_TYPE(value_)->tp_name)
                                                         .data());
            }

            return ptr;
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr void set_value(
        unmanaged_ptr<T> val) const {
        LEV_ASSERT(val);
        if (value_ == nullptr) {
            failure<std::runtime_error, PyExc_RuntimeError>(
                "Invalid reference proxy state for dictionary");
        }

        PyDict_SetItem(dict_, key_, val);
        value_ = val;
    }

private:
    unmanaged_ptr<PyDictObject> dict_;
    unmanaged_ptr<Key> key_;
    unmanaged_ptr<PyObject> value_;
};

template <pyobj_type Key, pyobj_type T>
class iterator {
    friend basic_dict<Key, T, true>;
    friend basic_dict_view<Key, T, true>;

    LEV_HIDE_INSTANTIATION explicit inline constexpr iterator(
        unmanaged_ptr<PyDictObject> ptr, Py_ssize_t pos) noexcept
        : instance_{ptr}
        , pos_{pos} {}

public:
    using value_type =
        std::pair<unmanaged_ptr<Key>, value_reference<value_setter<Key, T>>>;
    using difference_type = ptrdiff_t;
    using iterator_concept = std::forward_iterator_tag;

    LEV_HIDE_INSTANTIATION explicit inline constexpr iterator(
        unmanaged_ptr<PyDictObject> ptr) noexcept
        : instance_{ptr}
        , pos_{0} {}
    LEV_HIDE_INSTANTIATION explicit inline constexpr iterator(
        end_tag_t, unmanaged_ptr<PyDictObject> ptr) noexcept
        : instance_{ptr}
        , pos_{-1} {}

    LEV_HIDE_INSTANTIATION inline constexpr iterator() noexcept = default;

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline value_type
    operator*() const noexcept {
        using proxy_type = value_reference<value_setter<Key, T>>;
        Py_ssize_t pos = pos_;
        PyObject *key, *value;
        PyDict_Next(instance_, pos, &key, &value);
        return {
            key, proxy_type{instance_, key, value}
        };
    }

    LEV_HIDE_INSTANTIATION inline iterator& operator++() noexcept {
        if (!PyDict_Next(instance_, pos_, nullptr, nullptr)) {
            pos_ = -1;
        }
    }

    LEV_HIDE_INSTANTIATION inline iterator operator++(int) noexcept {
        iterator output = *this;
        ++*this;
        return output;
    }

    LEV_HIDE_INSTANTIATION inline bool operator==(
        const_iterator const& other) const noexcept = default;
    LEV_HIDE_INSTANTIATION inline bool operator!=(
        const_iterator const& other) const noexcept = default;

    LEV_HIDE_INSTANTIATION inline std::partial_ordering operator<=>(
        const_iterator const& other) const noexcept {
        if (instance_ == other.instance_) {
            return static_cast<size_t>(pos_) <=>
                static_cast<size_t>(other.pos_);
        }

        return std::partial_ordering::unordered;
    }

private:
    template <typename T, typename K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend auto find(
        unmanaged_ptr<PyDictObject>, unmanaged_ptr<K>) noexcept;

    unmanaged_ptr<PyDictObject> instance_;
    Py_ssize_t pos_ = -1;
};

template <pyobj_type Key, pyobj_type T>
class const_iterator {

    LEV_HIDE_INSTANTIATION explicit inline constexpr const_iterator(
        unmanaged_ptr<PyDictObject> ptr, Py_ssize_t pos = 0) noexcept
        : instance_{ptr}
        , pos_{pos} {}

    friend basic_dict<Key, T, true>;
    friend basic_dict_view<Key, T, true>;
    friend basic_dict<Key, T, false>;
    friend basic_dict_view<Key, T, false>;

public:
    LEV_HIDE_INSTANTIATION explicit inline constexpr const_iterator(
        unmanaged_ptr<PyDictObject> ptr) noexcept
        : instance_{ptr}
        , pos_{0} {}
    LEV_HIDE_INSTANTIATION explicit inline constexpr const_iterator(
        end_tag_t, unmanaged_ptr<PyDictObject> ptr) noexcept
        : instance_{ptr}
        , pos_{-1} {}
    using value_type = std::pair<unmanaged_ptr<Key>, unmanaged_ptr<T>>;
    using difference_type = ptrdiff_t;
    using iterator_concept = std::forward_iterator_tag;

    LEV_HIDE_INSTANTIATION inline constexpr const_iterator() noexcept = default;

    operator const_iterator<Key, T>() const noexcept {
        return const_iterator<Key, T>{instance_};
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline value_type
    operator*() const noexcept {
        Py_ssize_t pos = pos_;
        PyObject *key, *value;
        PyDict_Next(instance_, pos, &key, &value);
        return {key, value};
    }

    LEV_HIDE_INSTANTIATION inline iterator& operator++() noexcept {
        if (!PyDict_Next(instance_, pos_, nullptr, nullptr)) {
            pos_ = -1;
        }
    }

    LEV_HIDE_INSTANTIATION inline iterator operator++(int) noexcept {
        iterator output = *this;
        ++*this;
        return output;
    }

    LEV_HIDE_INSTANTIATION inline bool operator==(
        iterator const& other) const noexcept = default;
    LEV_HIDE_INSTANTIATION inline bool operator!=(
        iterator const& other) const noexcept = default;

    LEV_HIDE_INSTANTIATION inline std::partial_ordering operator<=>(
        iterator const& other) const noexcept {
        if (instance_ == other.instance_) {
            return static_cast<size_t>(pos_) <=>
                static_cast<size_t>(other.pos_);
        }

        return std::partial_ordering::unordered;
    }

private:
    unmanaged_ptr<PyDictObject> instance_;
    Py_ssize_t pos_ = -1;

    template <typename T, typename K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend auto find(
        unmanaged_ptr<PyDictObject>, unmanaged_ptr<K>) noexcept;
};

template <typename T, typename K>
LEV_HIDE_INSTANTIATION inline auto find(
    unmanaged_ptr<PyDictObject> dict, unmanaged_ptr<K> key) noexcept {
    LEV_ASSERT(key);
    auto const hasher = Py_TYPE(key)->tp_hash;
    if (!hasher) {
        return T{end_tag, dict};
    }

    exception_checkpoint _;
    auto const hash = hasher(key);
    if (hash == -1) {
        return T{end_tag, dict};
    }

    [[maybe_unused]] PyObject* unused = nullptr;
    auto const idx = dict->ma_keys->dk_lookup(dict, key, hash, &unused);
    if (idx < 0) {
        return T{end_tag, dict};
    }

    return T{dict, idx};
}

} // namespace details::dict

template <pyobj_type Key, pyobj_type T, bool M>
requires std::same_as<PyObject, T> || requires(unmanaged_ptr<PyObject> ptr) {
    type_object_v<T>;
    { dynamic_ptr_cast<T>(ptr) } noexcept;
}
class basic_dict<Key, T, M> {
    static_assert(
        !leviathan_pyobj<Key> || adaptor_traits<Key>::hashable::value);
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
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict(
        python_ptr<PyDictObject> const& ptr) noexcept
        : instance_{ptr} {
        if constexpr (M) {
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
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict<K, V, Mutable>&& other) noexcept
        : instance_{std::move(other.instance_)} {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION explicit inline basic_dict(
        adopt_t tag, basic_dict_view<K, V, Mutable> other) noexcept
        : instance_{tag, other.instance()} {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    LEV_HIDE_INSTANTIATION inline auto begin() const LEV_LIFETIMEBOUND {
        return const_iterator{instance_};
    }

    LEV_HIDE_INSTANTIATION inline auto end() const LEV_LIFETIMEBOUND {
        return const_iterator{end_tag, instance_};
    }

    LEV_HIDE_INSTANTIATION inline auto begin() LEV_LIFETIMEBOUND
    requires (M)
    {
        return iterator{instance_};
    }

    LEV_HIDE_INSTANTIATION inline auto end() LEV_LIFETIMEBOUND
    requires (M)
    {
        return iterator{end_tag, instance_};
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION inline auto find(
        unmanaged_ptr<K> key) noexcept LEV_LIFETIMEBOUND
    requires (M)
    {
        return details::dict::find<iterator>(instance_.get(), key);
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION inline auto find(
        unmanaged_ptr<K> key) const noexcept LEV_LIFETIMEBOUND {
        if constexpr (!M) {
            if (!instance_) {
                return const_iterator{end_tag, instance_};
            }
        }

        return details::dict::find<const_iterator>(instance_.get(), key);
    }

    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline constexpr bool
    empty() const noexcept {
        return size() == 0;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline constexpr size_t
    size() const noexcept {
        if constexpr (M) {
            return static_cast<size_t>(PyDict_Size(instance_));
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
    LEV_HIDE_INSTANTIATION inline bool remove(unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        if (PyDict_DelItem(instance_, key) == result_code::success) {
            return true;
        }

        PyErr_Clear();
        return false;
    }

    template <nothrow_t, pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION inline bool remove(unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        return remove(key);
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline mapped_reference at(
        unmanaged_ptr<K> key)
    requires (M)
    {
        return mapped_reference{instance_, key, std::as_const(*this).at(key)};
    }

    template <nothrow_t Tag, pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline mapped_reference at(
        unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        return mapped_reference{
            instance_, key, std::as_const(*this).at<Tag>(key)};
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline unmanaged_ptr<T> at(
        unmanaged_ptr<K> key) const {
        LEV_ASSERT(key);

        if constexpr (M) {
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
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline unmanaged_ptr<T> at(
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
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline mapped_reference operator[](
        unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        LEV_ASSERT(key);
        unmanaged_ptr<PyObject> value =
            PyDict_SetDefault(instance_, key, Py_None);
        return mapped_reference{instance_, key, value};
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V>
    LEV_HIDE_INSTANTIATION inline void set_value(
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
    LEV_HIDE_INSTANTIATION inline bool set_value(
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

template <pyobj_type Key, pyobj_type T, bool M>
requires std::same_as<PyObject, T> || requires(unmanaged_ptr<PyObject> ptr) {
    type_object_v<T>;
    { dynamic_ptr_cast<T>(ptr) } noexcept;
}
class basic_dict_view<Key, T, M> {
    static_assert(
        !leviathan_pyobj<Key> || adaptor_traits<Key>::hashable::value);
    template <typename, typename, bool>
    friend class basic_dict_view;

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

    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict_view(
        python_ptr<PyDictObject> const& ptr LEV_LIFETIMEBOUND) noexcept
        : instance_{ptr} {
        if constexpr (!M) {
            LEV_ASSERT(instance_);
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr basic_dict_view(
        basic_dict_view const& other) noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict_view(
        basic_dict_view&& other) noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict_view& operator=(
        basic_dict_view const& other) noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict_view& operator=(
        basic_dict_view&& other) noexcept = default;

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict_view(
        basic_dict_view<K, V, Mutable> other) noexcept
        : instance_{other.instance_} {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict_view(
        basic_dict<K, V, Mutable> const& other) noexcept
        : instance_{other.instance()} {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    LEV_HIDE_INSTANTIATION inline auto begin() const {
        return const_iterator{instance_};
    }

    LEV_HIDE_INSTANTIATION inline auto end() const {
        return const_iterator{end_tag, instance_};
    }

    LEV_HIDE_INSTANTIATION inline auto begin() { return iterator{instance_}; }

    LEV_HIDE_INSTANTIATION inline auto end() {
        return iterator{end_tag, instance_};
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION inline auto find(unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        return details::dict::find<iterator>(instance_.get(), key);
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION inline auto find(
        unmanaged_ptr<K> key) const noexcept {
        if constexpr (!M) {
            if (!instance_) {
                return const_iterator{end_tag, instance_};
            }
        }

        return details::dict::find<const_iterator>(instance_.get(), key);
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
    instance() const noexcept {
        return instance_.get();
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION inline bool remove(unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        if (PyDict_DelItem(instance_, key) == result_code::success) {
            return true;
        }

        PyErr_Clear();
        return false;
    }

    template <nothrow_t, pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION inline bool remove(unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        return remove(key);
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline mapped_reference at(
        unmanaged_ptr<K> key)
    requires (M)
    {
        return mapped_reference{instance_, key, std::as_const(*this).at(key)};
    }

    template <nothrow_t Tag, pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline mapped_reference at(
        unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        return mapped_reference{
            instance_, key, std::as_const(*this).at<Tag>(key)};
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline unmanaged_ptr<T> at(
        unmanaged_ptr<K> key) const {
        LEV_ASSERT(key);

        if (!instance_) {
            failure<std::runtime_error, PyExc_ValueError>(
                "Key lookup attempted on invalid dictionary instance");
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
    LEV_HIDE_INSTANTIATION [[nodiscard, gnu::pure]] inline unmanaged_ptr<T> at(
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
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline mapped_reference operator[](
        unmanaged_ptr<K> key) noexcept
    requires (M)
    {
        LEV_ASSERT(key);
        unmanaged_ptr<PyObject> value =
            PyDict_SetDefault(instance_, key, Py_None);
        return mapped_reference{instance_, key, value};
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V>
    LEV_HIDE_INSTANTIATION inline void set_value(
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
    LEV_HIDE_INSTANTIATION inline bool set_value(
        unmanaged_ptr<K> key, unmanaged_ptr<V> value) noexcept
    requires (M)
    {
        LEV_ASSERT(key);
        LEV_ASSERT(value);

        exception_checkpoint _;
        if (PyDict_SetItem(instance_, key, value) == result_code::success) {
            return true;
        }

        PyErr_Clear();
        return false;
    }

private:
    python_ptr<PyDictObject> instance_;
};

} // namespace py
} // namespace lev
