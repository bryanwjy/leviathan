// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/ownership_policy.hpp"
#include "leviathan/pointer.hpp"

#include <array>
#include <compare>
#include <iterator>

namespace lev {
namespace py {
template <typename Key, typename T = PyObject,
    ownership_policy policy = ownership_policy::strong>
class basic_dict;

template <pyobj_type Key, pyobj_type T, ownership_policy policy>
class basic_dict<Key, T, policy> {
    // if Key is a leviathan_pyobj, assert that it is hashable

    template <pyobj_type K, pyobj_type V, ownership_policy P>
    friend class basic_dict;

public:
    using key_type = Key*;
    using mapped_type = T*;
    using value_type = std::pair<unmanaged_ptr<Key>, unmanaged_ptr<T>>;
    using reference = value_type;
    using const_reference = value_type;
    using size_type = decltype(sizeof(0));
    using difference_type =
        decltype(static_cast<char*>(0) - static_cast<char*>(0));
    class const_iterator;
    using iterator = const_iterator;

    class const_iterator {
    public:
        using value_type = typename basic_dict::value_type;
        using reference = typename basic_dict::value_type;
        using difference_type = typename basic_dict::difference_type;
        using iterator_concept = std::forward_iterator_tag;

        inline constexpr const_iterator() = default;

    private:
        const_iterator(unmanaged_ptr<PyDictObject> ptr) noexcept
            : ptr(ptr)
            , pos(ptr ? 0 : -1) {}

        const_reference operator*() const noexcept {
            PyObject *key, *value;
            auto local_pos = pos;
            auto const result = PyDict_Next(ptr, &local_pos, &key, &value);
            LEV_ASSERT(result);
            return const_reference{key, value};
        }

        const_iterator& operator++() noexcept {
            if (!PyDict_Next(ptr, &pos, nullptr, nullptr)) {
                pos = -1;
            }
            return *this;
        }

        const_iterator operator++(int) noexcept {
            const_iterator copy(*this);
            ++*this;
            return copy;
        }

        bool operator==(const_iterator const&) const noexcept = default;
        bool operator!=(const_iterator const&) const noexcept = default;

        std::partial_ordering operator<=>(
            const_iterator const& other) const noexcept {
            if (ptr != other.ptr) {
                return std::partial_ordering::unordered;
            }

            return pos <=> other.pos;
        }

        unmanaged_ptr<PyDictObject> ptr;
        Py_ssize_t pos = -1;
    };

    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict() noexcept
        : instance_{} {}
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict(
        python_ptr<PyDictObject>&& ptr) noexcept
    requires (policy == ownership_policy::strong)
        : instance_{std::move(ptr)} {}
    LEV_HIDE_INSTANTIATION explicit inline basic_dict(
        python_ptr<PyDictObject> const& ptr) noexcept
    requires (policy == ownership_policy::strong)
        : instance_{ptr} {}

    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict(
        python_ptr<PyDictObject> const& ptr LEV_LIFETIMEBOUND) noexcept
    requires (policy == ownership_policy::none)
        : instance_{ptr.get()} {}

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V>
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict<K, V, policy> const& other) noexcept
    requires (policy == ownership_policy::strong)
        : instance_{other.instance_} {}
    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V>
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict<K, V, policy>&& other) noexcept
    requires (policy == ownership_policy::strong)
        : instance_{other.instance_} {}
    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V,
        ownership_policy P>
    LEV_HIDE_INSTANTIATION explicit inline basic_dict(
        basic_dict<K, V, P> const& other) noexcept
    requires (policy == ownership_policy::strong)
        : instance_{adopt_object, other.instance_.get()} {}

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V>
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict<K, V, ownership_policy::strong>&& other) noexcept
    requires (policy == ownership_policy::none)
        : instance_{other.instance_.get()} {}

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V,
        ownership_policy P>
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict<K, V, P> const& other LEV_LIFETIMEBOUND) noexcept
    requires (policy == ownership_policy::none)
        : instance_{other.instance_.get()} {}

    template <pyobj_related_to<Key> K, pyobj_related_to<T> V>
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict(
        basic_dict<K, V, policy> const& other) noexcept
    requires (policy == ownership_policy::strong)
        : instance_{other.instance_} {}
    template <pyobj_related_to<Key> K, pyobj_related_to<T> V>
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict(
        basic_dict<K, V, policy>&& other) noexcept
    requires (policy == ownership_policy::strong)
        : instance_{other.instance_} {}
    template <pyobj_related_to<Key> K, pyobj_related_to<T> V,
        ownership_policy P>
    LEV_HIDE_INSTANTIATION explicit inline basic_dict(
        basic_dict<K, V, P> const& other) noexcept
    requires (policy == ownership_policy::strong)
        : instance_{adopt_object, other.instance_.get()} {}
    template <pyobj_related_to<Key> K, pyobj_related_to<T> V>
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict(
        basic_dict<K, V, ownership_policy::strong>&& other) noexcept
    requires (policy == ownership_policy::none)
        : instance_{other.instance_.get()} {}
    template <pyobj_related_to<Key> K, pyobj_related_to<T> V,
        ownership_policy P>
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict(
        basic_dict<K, V, P> const& other LEV_LIFETIMEBOUND) noexcept
    requires (policy == ownership_policy::none)
        : instance_{other.instance_.get()} {}

    basic_dict(basic_dict const&) = default;
    basic_dict(basic_dict&&) noexcept = default;

    bool operator==(basic_dict const&) const noexcept = default;
    bool operator!=(basic_dict const&) const noexcept = default;

    const_iterator begin() const noexcept LEV_LIFETIMEBOUND {
        return const_iterator{instance_};
    }
    const_iterator cbegin() const noexcept LEV_LIFETIMEBOUND {
        return const_iterator{instance_};
    }
    iterator begin() noexcept LEV_LIFETIMEBOUND { return iterator{instance_}; }

    const_iterator end() const noexcept LEV_LIFETIMEBOUND {
        return const_iterator();
    }
    const_iterator end() const noexcept LEV_LIFETIMEBOUND {
        return const_iterator();
    }
    iterator end() noexcept LEV_LIFETIMEBOUND { return iterator(); }

    template <pyobj_derived_from<Key> K>
    [[nodiscard]] LEV_HIDE_INSTANTIATION inline constexpr bool contains(
        unmanaged_ptr<K> key) const noexcept {
        return instance_ && PyDict_Contains(instance_, key) == 1;
    }

    template <pyobj_derived_from<Key> K>
    [[gnu::pure, nodiscard]] LEV_HIDE_INSTANTIATION mapped_type operator[](
        unmanaged_ptr<K> key) const noexcept LEV_LIFETIMEBOUND
    requires (ownership_policy::strong == policy)
    {
        auto ptr = PyDict_GetItem(instance_, key);
        return py_cast<T>(ptr);
    }

    template <pyobj_derived_from<Key> K>
    [[nodiscard]] LEV_HIDE_INSTANTIATION mapped_type at(
        unmanaged_ptr<K> key) const noexcept LEV_LIFETIMEBOUND
    requires (ownership_policy::strong == policy)
    {
        auto ptr = PyDict_GetItemWithError(instance_, key);
        if (!ptr) {
            return nullptr;
        }

        return py_cast<T>(ptr);
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V>
    LEV_HIDE_INSTANTIATION unmanaged_ptr<T> set_value(
        unmanaged_ptr<K> key, unmanaged_ptr<V> val) noexcept LEV_LIFETIMEBOUND
    requires (ownership_policy::strong == policy)
    {
        if (PyDict_SetItem(instance_, key, val) == result_code::success) {
            return vptr.release();
        }

        return nullptr;
    }

    template <pyobj_derived_from<Key> K>
    [[gnu::pure, nodiscard]] LEV_HIDE_INSTANTIATION mapped_type operator[](
        unmanaged_ptr<K> key) const noexcept {
        auto ptr = PyDict_GetItem(instance_, key);
        return py_cast<T>(ptr);
    }

    template <pyobj_derived_from<Key> K>
    [[nodiscard]] LEV_HIDE_INSTANTIATION mapped_type at(
        unmanaged_ptr<K> key) const noexcept {
        auto ptr = PyDict_GetItemWithError(instance_, key);
        if (!ptr) {
            return nullptr;
        }

        return py_cast<T>(ptr);
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V>
    LEV_HIDE_INSTANTIATION unmanaged_ptr<T> set_value(
        unmanaged_ptr<K> key, unmanaged_ptr<V> val) noexcept {
        if (PyDict_SetItem(instance_, key, val) == result_code::success) {
            return vptr.release();
        }

        return nullptr;
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION bool remove(unmanaged_ptr<K> key) const noexcept {
        return PyDict_DelItem(instance_, key) == result_code::success;
    }

    LEV_HIDE_INSTANTIATION [[gnu::pure, nodiscard]] inline constexpr bool
    empty() const noexcept {
        return size() == 0;
    }

    LEV_HIDE_INSTANTIATION [[gnu::pure, nodiscard]] inline constexpr size_type
    size() const noexcept {
        return instance_ ? static_cast<size_type>(PyDict_Size(instance_)) : 0;
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard]] inline constexpr unmanaged_ptr<PyDictObject>
    get() const noexcept LEV_LIFETIMEBOUND
    requires (policy == ownership_policy::strong)
    {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard]] inline constexpr unmanaged_ptr<PyDictObject>
    release() noexcept
    requires (ownership_policy::strong == policy)
    {
        return instance_.release();
    }

    LEV_HIDE_INSTANTIATION [[clang::reninitializes]] inline constexpr void
    reset(python_ptr<PyDictObject> other = {}) noexcept
    requires (ownership_policy::strong == policy)
    {
        instance_ = std::move(other);
    }

    LEV_HIDE_INSTANTIATION
    [[nodiscard]] inline constexpr unmanaged_ptr<PyDictObject>
    get() const noexcept {
        return instance_;
    }

    LEV_HIDE_INSTANTIATION [[clang::reninitializes]] void clear() noexcept {
        if (instance_) {
            PyDict_Clear(instance_);
        }
    }

private:
    using instance_type = std::conditional_t<ownership_policy::none == policy,
        unmanaged_ptr<PyDictObject>, python_ptr<PyDictObject>>;
    python_ptr<PyDictObject> instance_;
};

using dict = basic_dict<PyObject, PyObject>;
using kwargs = basic_dict<PyASCIIObject>;

namespace views {
using dict = basic_dict<PyObject, PyObject, ownership_policy::none>;
using kwargs = basic_dict<PyASCIIObject, ownership_policy::none>;
} // namespace views
} // namespace py
} // namespace lev
