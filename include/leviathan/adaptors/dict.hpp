// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/container_options.hpp"
#include "leviathan/adaptors/hash.hpp"
#include "leviathan/pointer.hpp"
#include "utils/expected.hpp"

#include <array>
#include <compare>
#include <iterator>
#include <ranges>
#include <string_view>

namespace lev {
template <>
inline constexpr PyTypeObject* type_object<PyDictObject>() noexcept {
    return &PyDict_Type;
}

namespace py {

template <typename Key, typename T = PyObject, auto P = container_flags::none>
class basic_dict;

using dict = basic_dict<PyObject>;
using readonly_dict = basic_dict<PyObject, PyObject, container_flags::readonly>;
using kwargs = basic_readonly_dict<PyASCIIObject>;

template <typename Key, typename T = PyObject, auto P = container_flags::none>
using basic_readonly_dict = basic_dict<Key, T, container_flags::readonly | P>;

namespace borrowed {
using dict =
    __LEV py::basic_dict<PyObject, PyObject, container_flags::borrowed>;
using readonly_dict = __LEV py::basic_dict<PyObject, PyObject,
    container_flags::borrowed | container_flags::readonly>;
using kwargs = __LEV py::basic_dict<PyASCIIObject, PyObject,
    container_flags::borrowed | container_flags::readonly>;

template <typename Key, typename T = PyObject, auto P = container_flags::none>
using basic_readonly_dict = __LEV py::basic_dict<Key, T,
    container_flags::borrowed | container_flags::readonly | P>;
template <typename Key, typename T = PyObject, auto P = container_flags::none>
using basic_dict = __LEV py::basic_dict < Key,
      T, container_flags::borrowed | P;
} // namespace borrowed

namespace details::dict {

/**
 * Proxy type to assign references of the value pointer in a dictionary
 * key-value pair
 */
template <pyobj_type Key, pyobj_type T>
class value_reference {
public:
    LEV_HIDE_INSTANTIATION constexpr value_reference(unmanaged_ptr<PyDictObject> container,
        unmanaged_ptr<Key> key, unmanaged_ptr<PyObject> val) noexcept
        : dict_{container}
        , key_{key}
        , value_{val} {}

protected:
    using type = T;

    LEV_HIDE_INSTANTIATION inline constexpr ~value_reference() = default;

    LEV_HIDE_INSTANTIATION inline constexpr bool valid() const noexcept {
        if constexpr (std::same_as<PyObject, T>) {
            return value_ != nullptr;
        } else {
            return value_ && dynamic_ptr_cast<T>(value_);
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr unmanaged_ptr<T> get_pointer() const noexcept(
        std::is_convertible_v<unmanaged_ptr<PyObject>, unmanaged_ptr<T>>) {
        LEV_CONSTRACT_ASSERT(value_ != nullptr);

        if constexpr (std::is_convertible_v<unmanaged_ptr<PyObject>,
                          unmanaged_ptr<T>>) {
            return value_;
        } else {
            auto ptr = dynamic_ptr_cast<T>(value_);
            if (!ptr) {
                failure<type_error, PyExc_TypeError>(format_cstring(
                    "Unexpected mapped type, Expected=[%s], Found=[%s]",
                    type_object<T>()->tp_name, Py_TYPE(value_)->tp_name)
                                                         .data());
            }

            return ptr;
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr void set_pointer(unmanaged_ptr<T> val) const
        LEV_CONSTRACT_PRE(val) {
        LEV_ASSERT(val);
        if (value_ == nullptr) {
            failure<std::runtime_error, PyExc_RuntimeError>(
                "Invalid reference proxy state for dictionary");
        }

        PyDict_SetItem(dict_, key_, val);
        value_ = val;
    }

    template <pyobj_derived_from<T> U>
    LEV_HIDE_INSTANTIATION inline constexpr void set_pointer(unmanaged_ptr<U> val) const
        LEV_CONSTRACT_PRE(val) {
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

struct iterator_sentinel_tag_t {
    LEV_HIDE_INSTANTIATION explicit inline constexpr iterator_sentinel_tag_t() noexcept =
        default;
};

LEV_HIDDEN inline constexpr iterator_sentinel_tag_t iterator_sentinel_tag{};

/**
 * Dictionary Iterator class
 */
template <pyobj_type Key, pyobj_type T>
class iterator {
    template <typename, typename, auto>
    friend class __LEV py::basic_dict;

    using reference_proxy = element_reference<value_reference<Key, T>>;

    LEV_HIDE_INSTANTIATION explicit inline constexpr iterator(
        unmanaged_ptr<PyDictObject> ptr, Py_ssize_t pos) noexcept
        : instance_{ptr}
        , pos_{pos} {}

public:
    using value_type = std::pair<unmanaged_ptr<Key>, reference_proxy>;
    using difference_type = ptrdiff_t;
    using iterator_concept = std::forward_iterator_tag;

    LEV_HIDE_INSTANTIATION explicit inline constexpr iterator(
        unmanaged_ptr<PyDictObject> ptr) noexcept
        : iterator{ptr, 0} {}
    LEV_HIDE_INSTANTIATION explicit inline constexpr iterator(
        iterator_sentinel_tag_t, unmanaged_ptr<PyDictObject> ptr) noexcept
        : iterator{ptr, -1} {}

    LEV_HIDE_INSTANTIATION inline constexpr iterator() noexcept = default;

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline value_type operator*() const noexcept
        LEV_CONSTRACT_PRE(instance_&& pos_ != -1) {
        Py_ssize_t pos = pos_;
        PyObject *key, *value;
        PyDict_Next(instance_, pos, &key, &value);
        return {
            key, reference_proxy{instance_, key, value}
        };
    }

    LEV_HIDE_INSTANTIATION inline iterator& operator++() noexcept
        LEV_CONSTRACT_PRE(instance_&& pos_ != -1) {
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
    template <typename It, typename K>
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

    template <typename, typename, auto>
    friend class __LEV py::basic_dict;

public:
    LEV_HIDE_INSTANTIATION explicit inline constexpr const_iterator(
        unmanaged_ptr<PyDictObject> ptr) noexcept
        : const_iterator{ptr, 0} {}
    LEV_HIDE_INSTANTIATION explicit inline constexpr const_iterator(
        iterator_sentinel_tag_t, unmanaged_ptr<PyDictObject> ptr) noexcept
        : const_iterator{ptr, -1} {}

    using value_type = std::pair<unmanaged_ptr<Key>, unmanaged_ptr<T>>;
    using difference_type = ptrdiff_t;
    using iterator_concept = std::forward_iterator_tag;

    LEV_HIDE_INSTANTIATION inline constexpr const_iterator() noexcept = default;

    LEV_HIDE_INSTANTIATION [[nodiscard]] inline value_type operator*() const noexcept
        LEV_CONSTRACT_PRE(instance_&& pos_ != -1) {
        Py_ssize_t pos = pos_;
        PyObject *key, *value;
        PyDict_Next(instance_, pos, &key, &value);
        return {key, value};
    }

    LEV_HIDE_INSTANTIATION inline iterator& operator++() noexcept
        LEV_CONSTRACT_PRE(instance_&& pos_ != -1) {
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

    template <typename It, typename K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend auto find(
        unmanaged_ptr<PyDictObject>, unmanaged_ptr<K>) noexcept;
};

template <typename It, typename K>
requires requires(unmanaged_ptr<K> key) { __LEV py::hash(key) }
LEV_HIDE_INSTANTIATION inline auto find(
    unmanaged_ptr<PyDictObject> dict, unmanaged_ptr<K> key) noexcept
    LEV_CONSTRACT_PRE(dict) LEV_CONSTRACT_PRE(key) {
    LEV_ASSERT(dict);
    LEV_ASSERT(key);
    exception_checkpoint _;
    auto const value = __LEV py::hash(key);
    if (value == -1) {
        return It{iterator_sentinel_tag, dict};
    }

    [[maybe_unused]] PyObject* unused = nullptr;
    auto const idx = dict->ma_keys->dk_lookup(dict, key, value, &unused);
    if (idx < 0) {
        return It{iterator_sentinel_tag, dict};
    }

    return It{dict, idx};
}

} // namespace details::dict

template <pyobj_type Key, identifiable_pyobj_type T,
    container_flags_type auto P>
class basic_dict<Key, T, P> {
    static_assert(
        !leviathan_pyobj<Key> || adaptor_traits<Key>::hashable::value);
    template <typename, typename, auto>
    friend class basic_dict;

    using reference_proxy =
        element_reference<details::dict::value_reference<Key, T>>;
    using flags_type = std::remove_cv_t<decltype(P)>;
    using borrowed_flag = container_flags::borrowed_t;
    using nothrow_flag = container_flags::nothrow_t;
    using readonly_flag = container_flags::readonly_t;
    using instance_type =
        std::conditional_t<with_container_flags<flags_type, borrowed_flag>,
            unmanaged_ptr<PyDictObject>, python_ptr<PyDictObject>>;
    LEV_HIDE_INSTANTIATION static constexpr bool is_nothrow_v =
        with_container_flags<flags_type, nothrow_flag>;
    template <typename T>
    using result_type =
        std::conditional_t<is_nothrow_v, __LTL expected<T, result_code>, T>;

    struct private_tag_t {};

    LEV_HIDE_INSTANTIATION static constexpr private_tag_t private_tag{};

    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(private_tag_t, auto&& ptr) noexcept
        : instance_{std::forward<decltype(ptr)>(ptr)} {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(private_tag_t, auto&& ptr) noexcept
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(ptr) : instance_{std::forward<decltype(ptr)>(ptr)} {
        // Only assert when mutable
        // Immutable dict has no operations with side-effects, thus no need
        // for a stricly non-null pre-condition
        LEV_ASSERT(instance_);
    }

public:
    using key_type = unmanaged_ptr<Key>;
    using mapped_type = unmanaged_ptr<T>;
    using value_type = std::pair<unmanaged_ptr<Key> const, unmanaged_ptr<T>>;
    using reference =
        std::conditional_t<with_container_flags<flags_type, access::writable>,
            std::pair<unmanaged_ptr<Key> const, reference_proxy>, value_type>;
    using const_reference = value_type;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using const_iterator = details::dict::const_iterator<Key, T>;
    using iterator =
        std::conditional_t<with_container_flags<flags_type, access::writable_t>,
            details::dict::iterator<Key, T>, const_iterator>;

    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(decltype(nullptr)) noexcept = delete;
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict const& other) noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(basic_dict&& other) noexcept = default;
    LEV_HIDE_INSTANTIATION LEV_REINITIALIZES inline constexpr basic_dict& operator=(
        basic_dict const& other) noexcept = default;
    LEV_HIDE_INSTANTIATION LEV_REINITIALIZES inline constexpr basic_dict& operator=(
        basic_dict&& other) noexcept = default;

    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(python_ptr<PyDictObject>&& ptr) noexcept
    requires without_container_flags<flags_type, borrowed_flag>
        : basic_dict(private_tag, std::move(ptr)) {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        python_ptr<PyDictObject> const& ptr) noexcept
        : basic_dict(private_tag, ptr) {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        python_ptr<PyDictObject> const& ptr LEV_LIFETIMEBOUND) noexcept
    requires with_container_flags<flags_type, borrowed_flag>
        : basic_dict(private_tag, ptr.get()) {}

    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict(
        unmanaged_ptr<PyDictObject> ptr) noexcept
        : basic_dict{__LEV adopt(ptr)} {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(unmanaged_ptr<PyDictObject> ptr) noexcept
    requires with_container_flags<flags_type, borrowed_flag>
        : basic_dict{ptr} {}

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::exclusion<decltype(Opt), flags_type,
        borrowed_flag>
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(basic_dict<K, V, Opt>&& other) noexcept
        : basic_dict{std::move(other.instance_)} {}

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::exclusion<decltype(Opt), flags_type,
        borrowed_flag>
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict<K, V, Opt> const& other) noexcept
        : basic_dict{other.instance_} {}

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::acquisition<decltype(Opt), flags_type,
        borrowed_flag>
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(
        basic_dict<K, V, Opt> const& other LEV_LIFETIMEBOUND) noexcept
        : basic_dict{other.instance()} {}

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::removal<decltype(Opt), flags_type,
        borrowed_flag>
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_dict(
        basic_dict<K, V, Opt> other) noexcept
        : basic_dict{__LEV adopt(other.instance())} {}

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::maintenance<decltype(Opt), flags_type,
        borrowed_flag>
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict(basic_dict<K, V, Opt> other) noexcept
        : basic_dict{other.instance()} {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_dict& operator=(
        python_ptr<PyListObject>&& ptr) noexcept
    requires without_container_flags<flags_type, borrowed_flag>
    {
        instance_ = std::move(ptr);
        return *this;
    }

    LEV_HIDE_INSTANTIATION inline constexpr basic_dict& operator=(
        python_ptr<PyListObject> const& ptr) noexcept
    requires without_container_flags<flags_type, borrowed_flag>
    {
        instance_ = ptr;
        return *this;
    }

    LEV_HIDE_INSTANTIATION inline constexpr basic_dict& operator=(
        unmanaged_ptr<PyListObject> ptr) noexcept
    requires without_container_flags<flags_type, borrowed_flag>
    {
        instance_ = __LEV adopt(ptr);
        return *this;
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::removal<decltype(Opt), flags_type,
        borrowed_flag>
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict& operator=(
        basic_dict<K, V, Opt>&& other) noexcept {
        instance_ = std::move(other.instance_);
        return *this;
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::removal<decltype(Opt), flags_type,
        borrowed_flag>
    LEV_HIDE_INSTANTIATION inline constexpr basic_dict& operator=(
        basic_dict<K, V, Opt> const& other) noexcept {
        instance_ = other.instance_;
        return *this;
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr inline auto begin() const noexcept LEV_LIFETIMEBOUND {
        return cbegin();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr inline auto end() const noexcept LEV_LIFETIMEBOUND {
        return cend();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr inline auto
    cbegin() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_iterator{instance_} : end();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr inline auto cend() const noexcept LEV_LIFETIMEBOUND {
        return const_iterator{iterator_sentinel_tag, instance_};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto begin() noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    {
        return iterator{instance_};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto end() noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    {
        return iterator{iterator_sentinel_tag, instance_};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto begin() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return cbegin();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto end() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return cend();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto cbegin() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return instance_ ? const_iterator{instance_} : end();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto cend() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return const_iterator{iterator_sentinel_tag, instance_};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto begin() noexcept
    requires with_container_flags<flags_type, borrowed_flag> &&
        without_container_flags<flags_type, readonly_flag>
    {
        return iterator{instance_};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto end() noexcept
    requires with_container_flags<flags_type, borrowed_flag> &&
        without_container_flags<flags_type, readonly_flag>
    {
        return iterator{iterator_sentinel_tag, instance_};
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto find(
        unmanaged_ptr<K> key) noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag>
    {
        return details::dict::find<iterator>(instance_.get(), key);
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr auto find(
        unmanaged_ptr<K> key) const noexcept LEV_LIFETIMEBOUND {
        if (!instance_) {
            return const_iterator{iterator_sentinel_tag, instance_};
        }

        return details::dict::find<const_iterator>(instance_.get(), key);
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool empty() const noexcept {
        return size() == 0;
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_t size() const noexcept {
        return instance_ ? PyDict_Size(instance_) : 0;
    }

    LEV_HIDE_INSTANTIATION
        LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<PyDictObject>
    instance() const noexcept LEV_LIFETIMEBOUND {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION
        LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<PyDictObject>
    instance() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return instance_.get();
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION inline unsigned remove(unmanaged_ptr<K> key) noexcept
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(key) LEV_CONSTRACT_PRE(instance_) {
        LEV_ASSERT(key);
        LEV_ASSERT(instance_);
        exception_checkpoint _;
        return PyDict_DelItem(instance_, key) == __LEV result_code::success;
    }

    LEV_HIDE_INSTANTIATION inline void clear() noexcept
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(instance_) {
        LEV_ASSERT(instance_);
        return PyDict_Clear(instance_);
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline auto at(unmanaged_ptr<K> key) noexcept(
        is_nothrow_v) LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag>
    {
        return at_impl(key);
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline auto at(unmanaged_ptr<K> key) noexcept(
        is_nothrow_v)
    requires with_container_flags<flags_type, borrowed_flag> &&
        without_container_flags<flags_type, readonly_flag>
    {
        return at_impl(key);
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline auto at(unmanaged_ptr<K> key) const
        noexcept(is_nothrow_v) LEV_LIFETIMEBOUND {
        return at_impl(key);
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline auto at(unmanaged_ptr<K> key) const
        noexcept(is_nothrow_v)
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return at_impl(key);
    }

    template <typename Char>
    requires __LEV pyobj_derived_from<Key, PyASCIIObject>
    LEV_HIDE_INSTANTIATION inline unsigned remove(
        __LEV py::basic_string_view<Char> key) noexcept
    requires without_container_flags<flags_type, readonly_flag>
    {
        return remove(unmanaged_ptr<PyUnicodeObject>{key});
    }

    template <typename Char>
    requires __LEV pyobj_derived_from<Key, PyASCIIObject>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline auto at(
        __LEV py::basic_string_view<Char> key) noexcept(is_nothrow_v)
        LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag>
    {
        return at(unmanaged_ptr<PyUnicodeObject>{key});
    }

    template <typename Char>
    requires __LEV pyobj_derived_from<Key, PyASCIIObject>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline auto at(
        __LEV py::basic_string_view<Char> key) noexcept(is_nothrow_v)
    requires with_container_flags<flags_type, borrowed_flag> &&
        without_container_flags<flags_type, readonly_flag>
    {
        return at(unmanaged_ptr<PyUnicodeObject>{key});
    }

    template <typename Char>
    requires __LEV pyobj_derived_from<Key, PyASCIIObject>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline auto at(
        __LEV py::basic_string_view<Char> key) const noexcept(is_nothrow_v)
        LEV_LIFETIMEBOUND {
        return at(unmanaged_ptr<PyUnicodeObject>{key});
    }

    template <typename Char>
    requires __LEV pyobj_derived_from<Key, PyASCIIObject>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline auto at(
        __LEV py::basic_string_view<Char> key) const noexcept(is_nothrow_v)
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return at(unmanaged_ptr<PyUnicodeObject>{key});
    }

    // TODO add string_literal?　

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline reference_proxy operator[](
        unmanaged_ptr<K> key) noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(key) {
        LEV_ASSERT(key);
        unmanaged_ptr<PyObject> value =
            PyDict_SetDefault(instance_, key, Py_None);
        return reference_proxy{instance_, key, value};
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline reference_proxy operator[](
        unmanaged_ptr<K> key) noexcept
    requires without_container_flags<flags_type, borrowed_flag, readonly_flag>
    {
        LEV_ASSERT(key);
        unmanaged_ptr<PyObject> value =
            PyDict_SetDefault(instance_, key, Py_None);
        return reference_proxy{instance_, key, value};
    }

    template <typename Char>
    requires __LEV pyobj_derived_from<Key, PyASCIIObject>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline reference_proxy operator[](
        __LEV py::basic_string_view<Char> key) noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag>
    {
        return operator[](unmanaged_ptr<PyUnicodeObject>{key});
    }

    template <typename Char>
    requires __LEV pyobj_derived_from<Key, PyASCIIObject>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline reference_proxy operator[](
        __LEV py::basic_string_view<Char> key) noexcept
    requires without_container_flags<flags_type, borrowed_flag, readonly_flag>
    {
        return operator[](unmanaged_ptr<PyUnicodeObject>{key});
    }

    template <pyobj_derived_from<Key> K, pyobj_derived_from<T> V>
    LEV_HIDE_INSTANTIATION inline result_type<void> set_value(
        unmanaged_ptr<K> key, unmanaged_ptr<V> value)
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(key) LEV_CONSTRACT_PRE(value)
        LEV_CONSTRACT_PRE(instance_) {
        LEV_ASSERT(key);
        LEV_ASSERT(value);
        LEV_ASSERT(instance_);
        if constexpr (is_nothrow_v) {
            if (PyDict_SetItem(instance_, key, value) ==
                __LEV result_code::success) {
                return result_type<void>{};
            }

            PyErr_Clear();
            return result_type<void>{__LTL unexpect, __LEV result_code::failed};
        } else {
            if (PyDict_SetItem(instance_, key, value) !=
                __LEV result_code::success) {
                unhandled_error<std::runtime_error>();
            }
        }
    }

    template <typename Char, pyobj_derived_from<T> V>
    requires __LEV pyobj_derived_from<Key, PyASCIIObject>
    LEV_HIDE_INSTANTIATION inline result_type<void> set_value(
        __LEV py::basic_string_view<Char> key, unmanaged_ptr<V> value)
    requires without_container_flags<flags_type, readonly_flag>
    {
        return set_value(unmanaged_ptr<PyUnicodeObject>{key}, value);
    }

private:
    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline reference_proxy at_impl(
        unmanaged_ptr<K> key) noexcept(is_nothrow_v) {
        return reference_proxy{
            instance_, key, std::as_const(*this).at_impl(key)};
    }

    template <pyobj_derived_from<Key> K>
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline unmanaged_ptr<T> at_impl(
        unmanaged_ptr<K> key) const noexcept(is_nothrow_v)
        LEV_CONSTRACT_PRE(key) {
        LEV_ASSERT(key);
        if constexpr (is_nothrow_v) {
            return instance_
                ? dynamic_ptr_cast<T>(PyDict_GetItem(instance_, key))
                : nullptr;
        } else {
            LEV_CONTRACT_ASSERT(instance_);

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
                        type_object<T>()->tp_name, Py_TYPE(pyobj)->tp_name)
                                                             .data());
                }
            }

            return ptr;
        }
    }

    instance_type instance_;
};
} // namespace py
} // namespace lev

template <pyobj_type K, pyobj_type V>
inline constexpr bool
    std::ranges::enable_borrowed_range<__LEV py::borrowed::basic_dict<K, V>> =
        true;

template <pyobj_type K, pyobj_type V>
inline constexpr bool std::ranges::enable_borrowed_range<
    __LEV py::borrowed::basic_readonly_dict<K, V>> = true;
