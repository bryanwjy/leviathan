// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/common.hpp"
#include "leviathan/pointer.hpp"

#include <array>
#include <compare>
#include <iterator>
#include <ranges>
#include <stdexcept>

namespace lev {
namespace py {

template <typename T, bool Mutable = true>
class basic_list;
template <typename T, bool Mutable = true>
class basic_borrowed_list;
using list = basic_list<PyObject>;
using borrowed_list = basic_borrowed_list<PyObject>;
using const_list = basic_list<PyObject, false>;
using const_borrowed_list = basic_borrowed_list<PyObject, false>;

template <typename T>
using basic_const_list = basic_list<T, false>;
template <typename T>
using basic_const_borrowed_list = basic_borrowed_list<T, false>;

namespace borrowed {
template <typename T>
using basic_list = basic_borrowed_list<T>;
template <typename T>
using basic_const_list = basic_const_borrowed_list<T>;
using list = borrowed_list;
using const_list = const_borrowed_list;
} // namespace borrowed

namespace details::list {
LEV_HIDDEN constexpr char const* null_instance_message_v =
    "List instance is null";
LEV_HIDDEN constexpr char const* invalid_type_message_v =
    "List-Adaptor element type mismatch";
} // namespace details::list

template <pyobj_type T, bool M>
class LEV_API basic_list<T, M> {
    using span_type =
        std::conditional_t<M, std::span<PyObject*>, std::span<PyObject* const>>;

public:
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using const_iterator = random_access_const_iterator<T>;
    using iterator =
        std::conditional_t<M, random_access_iterator<T>, const_iterator>;
    using const_iterator = random_access_const_iterator<T>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    LEV_HIDE_INSTANTIATION constexpr inline basic_list(
        basic_list const&) noexcept = default;
    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr inline basic_list&
    operator=(basic_list const&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline basic_list(
        basic_list&&) noexcept = default;
    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr inline basic_list&
    operator=(basic_list&&) noexcept = default;

    LEV_HIDE_INSTANTIATION explicit inline basic_list(
        python_ptr<PyListObject>&& ptr)
        : instance_{std::move(ptr)} {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    LEV_HIDE_INSTANTIATION explicit inline basic_list(
        python_ptr<PyListObject> const& ptr)
        : basic_list(adopt(ptr)) {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<PyListObject> U>
    LEV_HIDE_INSTANTIATION explicit inline basic_list(python_ptr<U>&& ptr)
        : instance_{static_ptr_cast<PyListObject>(std::move(ptr))} {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<PyListObject> U>
    LEV_HIDE_INSTANTIATION explicit inline basic_list(python_ptr<U> const& ptr)
        : instance_{static_ptr_cast<PyListObject>(ptr)} {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<T> U, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        basic_list<U, Mutable> const& other) noexcept
        : instance_{other.instance_} {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<T> U, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        basic_list<U, Mutable>&& other) noexcept
        : instance_{std::move(other.instance_)} {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<T> U, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION explicit inline basic_list(
        adopt_t tag, basic_borrowed_list<U, Mutable> other) noexcept
        : instance_{tag, other.instance()} {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    LEV_HIDE_INSTANTIATION LEV_PURE
        [[nodiscard]] inline constexpr unmanaged_ptr<T>
        operator[](difference_type idx) const noexcept LEV_LIFETIMEBOUND {
        LEV_ASSERT(instance_ && !empty());
        return dynamic_ptr_cast<T>(instance_->ob_item[idx]);
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    operator[](difference_type idx) const noexcept LEV_LIFETIMEBOUND
    requires M
    {
        LEV_ASSERT(instance_ && !empty());
        return details::random_access_reference<T>{instance_->ob_item[idx]};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool
    empty() const noexcept {
        return !size();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_type
    size() const noexcept {
        return instance_ ? Py_SIZE(instance_.get()) : 0u;
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_type
    capacity() const noexcept {
        return instance_ ? static_cast<size_type>(instance_->allocated) : 0u;
    }

    LEV_HIDE_INSTANTIATION void clear() const
    requires (M)
    {
        if (!instance_) {
            return;
        }

#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 13
        if (PyList_Clear(instance_.get()) != result_code::success) {
            unhandled_error<std::runtime_error>();
        }
#else
        if (PyList_SetSlice(instance_.get(), 0, static_cast<Py_ssize_t>(size()),
                nullptr) != result_code::success) {
            unhandled_error<std::runtime_error>();
        }
#endif
    }

    template <nothrow_t>
    LEV_HIDE_INSTANTIATION bool clear() const noexcept
    requires (M)
    {
        if (!instance_) {
            return true;
        }

        exception_checkpoint _;
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 13
        return PyList_Clear(instance_.get()) == result_code::success;
#else
        return PyList_SetSlice(instance_.get(), 0,
                   static_cast<Py_ssize_t>(size()),
                   nullptr) == result_code::success;
#endif
    }

    LEV_HIDE_INSTANTIATION LEV_PURE
        [[nodiscard]] inline constexpr unmanaged_ptr<PyListObject>
        instance() const noexcept LEV_LIFETIMEBOUND {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr span_type
    span() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? span_type{instance_->ob_item, size()} : span_type{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    begin() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_iterator{instance_->ob_item}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    end() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_iterator{instance_->ob_item + size()}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    cbegin() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_iterator{instance_->ob_item}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    cend() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_iterator{instance_->ob_item + size()}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rbegin() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_reverse_iterator{end()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rend() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_reverse_iterator{begin()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    crbegin() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_reverse_iterator{end()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    crend() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_reverse_iterator{begin()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    begin() const noexcept LEV_LIFETIMEBOUND
    requires M
    {
        return instance_ ? iterator{instance_->ob_item} : iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    end() const noexcept LEV_LIFETIMEBOUND
    requires M
    {
        return instance_ ? iterator{instance_->ob_item + size()} : iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rbegin() const noexcept LEV_LIFETIMEBOUND
    requires M
    {
        return instance_ ? reverse_iterator{end()} : reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rend() const noexcept LEV_LIFETIMEBOUND
    requires M
    {
        return instance_ ? reverse_iterator{begin()} : reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_UNSAFE_API void reserve(
        size_t new_capacity) const
    requires M
    {
        LEV_ASSERT(instance_);
        if (new_capacity < capacity()) {
            return;
        }
        critical_section _(instance_.get());
        auto items =
            PyMem_Realloc(instance_->ob_item, sizeof(PyObject*) * new_capacity);
        if (items == nullptr) {
            failure<std::bad_alloc, PyExc_NoMemory>("List reallocation failed");
        }

        instance_->allocated = new_capacity;
        instance_->ob_item = items;
    }

    template <nothrow_t>
    LEV_HIDE_INSTANTIATION LEV_UNSAFE_API bool reserve(
        size_t new_capacity) const noexcept
    requires M
    {
        LEV_ASSERT(instance_);
        if (new_capacity < capacity()) {
            return true;
        }

        critical_section _(instance_.get());
        auto items =
            PyMem_Realloc(instance_->ob_item, sizeof(PyObject*) * new_capacity);
        if (items == nullptr) {
            return false;
        }

        instance_->allocated = new_capacity;
        instance_->ob_item = items;
        return true;
    }

private:
    python_ptr<PyListObject> instance_;
};

template <pyobj_type T, bool M>
class LEV_API basic_borrowed_list<T, M> {
    using span_type =
        std::conditional_t<M, std::span<PyObject*>, std::span<PyObject* const>>;

public:
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using const_iterator = random_access_const_iterator<T>;
    using iterator =
        std::conditional_t<M, random_access_iterator<T>, const_iterator>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    LEV_HIDE_INSTANTIATION constexpr inline basic_borrowed_list(
        basic_borrowed_list const&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline basic_borrowed_list& operator=(
        basic_borrowed_list const&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline basic_borrowed_list(
        basic_borrowed_list&&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline basic_borrowed_list& operator=(
        basic_borrowed_list&&) noexcept = default;

    LEV_HIDE_INSTANTIATION explicit inline basic_borrowed_list(
        unmanaged_ptr<PyListObject> ptr)
        : instance_{ptr} {}

    LEV_HIDE_INSTANTIATION explicit inline basic_borrowed_list(
        python_ptr<PyListObject> const& ptr LEV_LIFETIMEBOUND)
        : basic_borrowed_list{ptr.get()} {}

    template <pyobj_derived_from<T> U, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION inline constexpr basic_borrowed_list(
        basic_borrowed_list<U, Mutable> other) noexcept
        : instance_{other.instance_} {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    template <pyobj_derived_from<T> U, bool Mutable>
    requires (!M || Mutable)
    LEV_HIDE_INSTANTIATION inline constexpr basic_borrowed_list(
        basic_list<U, Mutable> const& other LEV_LIFETIMEBOUND) noexcept
        : instance_{other.instance()} {
        if constexpr (M) {
            LEV_ASSERT(instance_);
        }
    }

    LEV_HIDE_INSTANTIATION LEV_PURE
        [[nodiscard]] inline constexpr unmanaged_ptr<T>
        operator[](size_type idx) const noexcept {
        LEV_ASSERT(instance_ && !empty() && idx < size());
        return dynamic_ptr_cast<T>(instance_->ob_item[idx]);
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    operator[](size_type idx) const noexcept
    requires M
    {
        LEV_ASSERT(instance_ && !empty() && idx < size());
        return details::random_access_reference<T>{instance_->ob_item[idx]};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool
    empty() const noexcept {
        return !size();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_type
    size() const noexcept {
        return instance_ ? Py_SIZE(instance_.get()) : 0u;
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_type
    capacity() const noexcept {
        return instance_ ? static_cast<size_type>(instance_->allocated) : 0u;
    }

    LEV_HIDE_INSTANTIATION void clear() const
    requires M
    {
        if (!instance_) {
            return;
        }

#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 13
        if (PyList_Clear(instance_.get()) != result_code::success) {
            unhandled_error<std::runtime_error>();
        }
#else
        if (PyList_SetSlice(instance_.get(), 0, static_cast<Py_ssize_t>(size()),
                nullptr) != result_code::success) {
            unhandled_error<std::runtime_error>();
        }
#endif
    }

    template <nothrow_t>
    LEV_HIDE_INSTANTIATION bool clear() const noexcept
    requires M
    {
        if (!instance_) {
            return true;
        }

        exception_checkpoint _;
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 13
        return PyList_Clear(instance_.get()) == result_code::success;
#else
        return PyList_SetSlice(instance_.get(), 0,
                   static_cast<Py_ssize_t>(size()),
                   nullptr) == result_code::success;
#endif
    }

    LEV_HIDE_INSTANTIATION LEV_PURE
        [[nodiscard]] inline constexpr unmanaged_ptr<PyListObject>
        instance() const noexcept LEV_LIFETIMEBOUND {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    span() const noexcept {
        return instance_ ? span_type{instance_->ob_item, size()} : span_type{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    begin() const noexcept {
        return instance_ ? const_iterator{instance_->ob_item}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    end() const noexcept {
        return instance_ ? const_iterator{instance_->ob_item + size()}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    cbegin() const noexcept {
        return instance_ ? const_iterator{instance_->ob_item}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    cend() const noexcept {
        return instance_ ? const_iterator{instance_->ob_item + size()}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rbegin() const noexcept {
        return instance_ ? const_reverse_iterator{end()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rend() const noexcept {
        return instance_ ? const_reverse_iterator{begin()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    crbegin() const noexcept {
        return instance_ ? const_reverse_iterator{end()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    crend() const noexcept {
        return instance_ ? const_reverse_iterator{begin()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    begin() const noexcept
    requires M
    {
        return instance_ ? iterator{instance_->ob_item} : iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    end() const noexcept
    requires M
    {
        return instance_ ? iterator{instance_->ob_item + size()} : iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rbegin() const noexcept
    requires M
    {
        return instance_ ? reverse_iterator{end()} : reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rend() const noexcept
    requires M
    {
        return instance_ ? reverse_iterator{begin()} : reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_UNSAFE_API void reserve(
        size_t new_capacity) const
    requires M
    {
        LEV_ASSERT(instance_);
        if (new_capacity < capacity()) {
            return;
        }
        critical_section _(instance_.get());
        auto items =
            PyMem_Realloc(instance_->ob_item, sizeof(PyObject*) * new_capacity);
        if (items == nullptr) {
            failure<std::bad_alloc, PyExc_NoMemory>("List reallocation failed");
        }

        instance_->allocated = new_capacity;
        instance_->ob_item = items;
    }

    template <nothrow_t>
    LEV_HIDE_INSTANTIATION LEV_UNSAFE_API bool reserve(
        size_t new_capacity) const noexcept
    requires M
    {
        LEV_ASSERT(instance_);
        if (new_capacity < capacity()) {
            return true;
        }

        critical_section _(instance_.get());
        auto items =
            PyMem_Realloc(instance_->ob_item, sizeof(PyObject*) * new_capacity);
        if (items == nullptr) {
            return false;
        }

        instance_->allocated = new_capacity;
        instance_->ob_item = items;
        return true;
    }

private:
    unmanaged_ptr<PyListObject> instance_;
};

} // namespace py
} // namespace lev
