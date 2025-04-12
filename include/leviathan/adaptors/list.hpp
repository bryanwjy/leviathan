// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/common.hpp"
#include "leviathan/pointer.hpp"
#include "leviathan/utility.hpp"

#include <array>
#include <compare>
#include <iterator>
#include <ranges>
#include <stdexcept>

namespace lev {

template <>
inline constexpr PyTypeObject* type_object<PyListObject>() noexcept {
    return &PyList_Type;
}

namespace py {

template <typename T, auto P = ownership::owned | access::writable>
class basic_list;
template <typename T>
using basic_readonly_list = basic_list<T, ownership::owned | access::readonly>;

using list = basic_list<PyObject>;
using readonly_list = basic_readonly_list<PyObject>;

namespace borrowed {
using list =
    __LEV py::basic_list<PyObject, ownership::borrowed | access::writable>;
using readonly_list =
    __LEV py::basic_list<PyObject, ownership::borrowed | access::readonly>;

template <typename T>
using basic_list =
    __LEV py::basic_list<T, ownership::borrowed | access::writable>;
template <typename T>
using basic_readonly_list =
    __LEV py::basic_list<T, ownership::borrowed | access::readonly>;

} // namespace borrowed

template <pyobj_type T, container_options_type auto P>
class LEV_API basic_list<T, P> {
    using span_type = std::conditional_t<
        with_container_options<options_type, ownership::writable_t>,
        std::span<PyObject*>, std::span<PyObject* const>>;
    using reference_proxy = element_reference<
        details::random_access_reference<T, cast_policy::safe>>;
    using options_type = std::remove_cv_t<decltype(P)>;
    using instance_type = std::conditional_t<
        with_container_options<options_type, ownership::owned_t>,
        python_ptr<PyDictObject>, unmanaged_ptr<PyDictObject>>;

    struct private_tag_t {};

    LEV_HIDE_INSTANTIATION static constexpr private_tag_t private_tag{};

    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        private_tag_t, auto&& ptr) noexcept
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(ptr) : instance_{std::forward<decltype(ptr)>(ptr)} {
        // Only assert when mutable
        // Immutable list has no operations with side-effects, thus no need
        // for a stricly non-null pre-condition
        LEV_ASSERT(instance_);
    }

    LEV_HIDE_INSTANTIATION iterator insert(
        private_tag_t, const_iterator pos, unmanaged_ptr<T> obj)
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(instance_) LEV_CONSTRACT_PRE(obj) {
        LEV_ASSERT(instance_);
        auto const idx = pos - cbegin();
        LEV_CONTRACT_ASSERT(idx <= size());
        if (PyList_Insert(instance_.get(), idx, obj.get()) !=
            __LEV result_code::success) {
            unhandled_error<std::runtime_error>();
        }

        return begin() + idx;
    }

    LEV_HIDE_INSTANTIATION LEV_UNSAFE_API iterator insert(
        private_tag_t, const_iterator pos, size_t count, unmanaged_ptr<T> obj)
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(instance_) LEV_CONSTRACT_PRE(obj) {
        LEV_ASSERT(instance_);
        auto const idx = pos - cbegin();
        LEV_CONTRACT_ASSERT(idx <= size());
        reserve(new_size);

        critical_section _{instance_.get(), obj};

        auto const new_size = size() + count;
        auto const to_move = size() - idx;
        auto* const src = instance_->ob_item + idx;
        auto* const dst = src + count;
        memmove(dst, src, sizeof(PyObject*) * to_move);
        as_pyobject(obj)->ob_refcnt += count;
        std::span to_set{src, dst};
        for (auto& ptr : to_set) {
            ptr = obj;
        }
        Py_SET_SIZE(instance_, new_size);

        return begin() + idx;
    }

    template <std::forward_iterator It>
    LEV_HIDE_INSTANTIATION LEV_UNSAFE_API iterator insert(
        private_tag_t, const_iterator pos, It begin, It end, size_t const count)
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(instance_) {
        LEV_ASSERT(instance_);
        auto const idx = std::ranges::distance(cbegin(), pos);
        LEV_CONTRACT_ASSERT(idx <= size());
        auto const new_size = size() + count;
        auto const to_move = size() - idx;
        reserve(new_size);

        critical_section _{instance_.get(), obj};

        auto* const old_pos = instance_->ob_item + idx;
        auto* const new_pos = old_pos + count;
        memmove(new_pos, old_pos, sizeof(PyObject*) * to_move);
        std::ranges::copy(begin, end, iterator{old_pos});
        Py_SET_SIZE(instance_, new_size);
        return begin() + idx;
    }

    template <std::forward_iterator It>
    LEV_HIDE_INSTANTIATION iterator insert(
        private_tag_t tag, const_iterator pos, It begin, It end)
    requires with_container_options<options_type, access::writable_t>
    {
        return insert(tag, pos, begin, end, std::ranges::distance(begin, end));
    }

    template <typename R>
    requires std::ranges::forward_range<R> ||
        std::ranges::sized_range<R>
        LEV_HIDE_INSTANTIATION iterator insert_range(
            private_tag_t tag, const_iterator pos, R&& range)
    requires with_container_options<options_type, access::writable_t>
    {
        return insert(tag, pos, std::ranges::begin(range),
            std::ranges::end(range), std::ranges::size(range));
    }

    template <std::ranges::input_range R>
    LEV_HIDE_INSTANTIATION iterator insert_range(
        private_tag_t tag, const_iterator pos, R&& range)
    requires with_container_options<options_type, access::writable_t>
    {
        return insert(
            tag, pos, std::ranges::begin(range), std::ranges::end(range));
    }

    template <std::input_iterator It>
    requires borrowable_as<std::iter_reference_t<It>, T>
    LEV_HIDE_INSTANTIATION iterator insert(
        private_tag_t, const_iterator pos, It begin, It end)
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(instance_) {
        auto const idx = pos - cbegin();
        while (begin != end) {
            auto ptr = __LEV borrow(*begin);
            LEV_CONSTRACT_PRE(ptr);
            insert(private_tag, pos, ptr.get());
            ++begin;
        }

        return begin() + idx;
    }

    // TODO (bryanwjy): nothrow insert using expected

    LEV_HIDE_INSTANTIATION iterator erase(
        private_tag_t, const_iterator first, const_iterator const last) noexcept
    requires with_container_options<options_type, access::writable_t>
    {
        if (first == last) {
            return last;
        }

        for (auto end = cend(); last != end; ++first, ++last) {
            iter_swap(first, last);
        }

        auto const idx = first - cbegin();
        PyList_SetSlice(instance_, idx, size(), nullptr);
        return begin() + idx;
    }

public:
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using const_iterator = random_access_const_iterator<T>;
    using iterator = std::conditional_t<
        with_container_options<options_type, ownership::writable_t>,
        random_access_iterator<T>, const_iterator>;
    using const_iterator = random_access_const_iterator<T>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using value_type = unmanaged_ptr<T>;

    LEV_HIDE_INSTANTIATION constexpr inline basic_list(
        basic_list const&) noexcept = default;
    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr inline basic_list&
    operator=(basic_list const&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline basic_list(
        basic_list&&) noexcept = default;
    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr inline basic_list&
    operator=(basic_list&&) noexcept = default;

    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        python_ptr<PyDictObject>&& ptr) noexcept
    requires with_container_options<options_type, access::owned_t>
        : basic_list(private_tag, std::move(ptr)) {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        python_ptr<PyDictObject> const& ptr) noexcept
        : basic_list(private_tag, ptr) {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        python_ptr<PyDictObject> const& ptr LEV_LIFETIMEBOUND) noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
        : basic_list(private_tag, ptr.get()) {}

    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_list(
        unmanaged_ptr<PyDictObject> ptr) noexcept
        : basic_list{__LEV adopt(ptr)} {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        unmanaged_ptr<PyDictObject> ptr) noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
        : basic_list{ptr} {}

    template <pyobj_derived_from<T> U,
        with_container_options<ownership::owned_t, access::writable_t> auto Opt>
    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        basic_list<U, Opt>&& other) noexcept
    requires with_container_options<options_type, ownership::owned_t,
        access::writable_t>
        : basic_list{std::move(other.instance_)} {}

    template <pyobj_derived_from<T> U,
        with_container_options<ownership::owned_t, access::writable_t> auto Opt>
    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        basic_list<U, Opt> const& other) noexcept
    requires with_container_options<options_type, ownership::owned_t,
        access::writable_t>
        : basic_list{other.instance_} {}

    template <pyobj_derived_from<T> U,
        with_container_options<ownership::owned_t, access::writable_t> auto Opt>
    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        basic_list<U, Opt> const& other LEV_LIFETIMEBOUND) noexcept
    requires with_container_options<options_type, ownership::borrowed_t,
        access::writable_t>
        : basic_list{other.instance()} {}

    template <pyobj_derived_from<T> U,
        with_container_options<ownership::borrowed_t, access::writable_t> auto
            Opt>
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_list(
        basic_list<U, Opt> other) noexcept
    requires with_container_options<options_type, ownership::owned_t,
        access::writable_t>
        : basic_list{__LEV adopt(other.instance())} {}

    template <pyobj_derived_from<T> U,
        with_container_options<ownership::borrowed_t, access::writable_t> auto
            Opt>
    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        basic_list<U, Opt> other) noexcept
    requires with_container_options<options_type, ownership::borrowed_t,
        access::writable_t>
        : basic_list{other.instance()} {}

    template <pyobj_derived_from<T> U,
        with_container_options<ownership::owned_t> auto Opt>
    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        basic_list<U, Opt>&& other) noexcept
    requires with_container_options<options_type, ownership::owned_t,
        access::readonly_t>
        : basic_list{std::move(other.instance_)} {}

    template <pyobj_derived_from<T> U,
        with_container_options<ownership::owned_t> auto Opt>
    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        basic_list<U, Opt> const& other) noexcept
    requires with_container_options<options_type, ownership::owned_t,
        access::readonly_t>
        : basic_list{other.instance_} {}

    template <pyobj_derived_from<T> U,
        with_container_options<ownership::owned_t> auto Opt>
    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        basic_list<U, Opt> const& other LEV_LIFETIMEBOUND) noexcept
    requires with_container_options<options_type, ownership::owned_t,
        access::readonly_t>
        : basic_list{other.instance()} {}

    template <pyobj_derived_from<T> U,
        with_container_options<ownership::borrowed_t> auto Opt>
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_list(
        basic_list<U, Opt> other) noexcept
    requires with_container_options<options_type, ownership::owned_t,
        access::readonly_t>
        : basic_list{__LEV adopt(other.instance())} {}

    template <pyobj_derived_from<T> U,
        with_container_options<ownership::borrowed_t> auto Opt>
    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        basic_list<U, Opt> other) noexcept
    requires with_container_options<options_type, ownership::owned_t,
        access::readonly_t>
        : basic_list{other.instance()} {}

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    begin() const noexcept LEV_LIFETIMEBOUND {
        return cbegin();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    end() const noexcept LEV_LIFETIMEBOUND {
        return cend();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rbegin() const noexcept LEV_LIFETIMEBOUND {
        return crbegin();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rend() const noexcept LEV_LIFETIMEBOUND {
        return crend();
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
    requires with_container_options<options_type, access::writable_t,
        ownership::owned_t>
    {
        return instance_ ? iterator{instance_->ob_item} : iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    end() const noexcept LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t,
        ownership::owned_t>
    {
        return instance_ ? iterator{instance_->ob_item + size()} : iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rbegin() const noexcept LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t,
        ownership::owned_t>
    {
        return instance_ ? reverse_iterator{end()} : reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rend() const noexcept LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t,
        ownership::owned_t>
    {
        return instance_ ? reverse_iterator{begin()} : reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    begin() const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    {
        return cbegin();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    end() const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    {
        return cend();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rbegin() const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    {
        return crbegin();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rend() const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    {
        return crend();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    cbegin() const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    {
        return instance_ ? const_iterator{instance_->ob_item}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    cend() const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    {
        return instance_ ? const_iterator{instance_->ob_item + size()}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    crbegin() const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    {
        return instance_ ? const_reverse_iterator{end()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    crend() const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    {
        return instance_ ? const_reverse_iterator{begin()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    begin() const noexcept
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    {
        return instance_ ? iterator{instance_->ob_item} : iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    end() const noexcept
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    {
        return instance_ ? iterator{instance_->ob_item + size()} : iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rbegin() const noexcept
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    {
        return instance_ ? reverse_iterator{end()} : reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    rend() const noexcept
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    {
        return instance_ ? reverse_iterator{begin()} : reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE
        [[nodiscard]] inline constexpr unmanaged_ptr<T>
        operator[](size_type idx) const noexcept LEV_LIFETIMEBOUND
        LEV_CONSTRACT_PRE(instance_ && !empty() && idx < size()) {
        LEV_ASSERT(instance_ && !empty() && idx < size());
        return dynamic_ptr_cast<T>(instance_->ob_item[idx]);
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    operator[](size_type idx) noexcept LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(instance_ && !empty() && idx < size()) {
        LEV_ASSERT(instance_ && !empty() && idx < size());
        return reference_proxy{instance_->ob_item[idx]};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE
        [[nodiscard]] inline constexpr unmanaged_ptr<T>
        operator[](size_type idx) const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    LEV_CONSTRACT_PRE(instance_ && !empty() && idx < size()) {
        LEV_ASSERT(instance_ && !empty() && idx < size());
        return dynamic_ptr_cast<T>(instance_->ob_item[idx]);
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto
    operator[](size_type idx) noexcept
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    LEV_CONSTRACT_PRE(instance_ && !empty() && idx < size()) {
        LEV_ASSERT(instance_ && !empty() && idx < size());
        return reference_proxy{instance_->ob_item[idx]};
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

    auto front() noexcept LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[0];
    }

    auto back() noexcept LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[size() - 1];
    }

    auto front() noexcept
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[0];
    }

    auto back() noexcept
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[size() - 1];
    }

    auto front() const noexcept LEV_LIFETIMEBOUND LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[0];
    }

    auto back() const noexcept LEV_LIFETIMEBOUND LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[size() - 1];
    }

    auto front() const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[0];
    }

    auto back() const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[size() - 1];
    }

    LEV_HIDE_INSTANTIATION void clear()
    requires with_container_options<options_type, access::writable_t>
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
    LEV_HIDE_INSTANTIATION bool clear() noexcept
    requires with_container_options<options_type, access::writable_t>
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

    LEV_HIDE_INSTANTIATION LEV_PURE
        [[nodiscard]] inline constexpr unmanaged_ptr<PyListObject>
        instance() const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr span_type
    span() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? span_type{instance_->ob_item, size()} : span_type{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr span_type
    span() const noexcept
    requires with_container_options<options_type, ownership::borrowed_t>
    {
        return instance_ ? span_type{instance_->ob_item, size()} : span_type{};
    }

    LEV_HIDE_INSTANTIATION friend void swap(
        basic_list& lhs, basic_list& rhs) noexcept {
        lhs.swap(other);
    }

    LEV_HIDE_INSTANTIATION void swap(basic_list& other) noexcept {
        instance_.swap(other.instance_);
    }

    LEV_HIDE_INSTANTIATION LEV_UNSAFE_API void reserve(size_t new_capacity)
    requires with_container_options<options_type, access::writable_t>
    {
        if (!reserve<nothrow>(new_capacity)) {
            failure<std::bad_alloc, PyExc_NoMemory>("List reallocation failed");
        }
    }

    template <nothrow_t>
    LEV_HIDE_INSTANTIATION LEV_UNSAFE_API bool reserve(
        size_t new_capacity) noexcept
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(instance_) {
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

    LEV_HIDE_INSTANTIATION void push_back(borrowable_as<T> auto&& obj)
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(instance_) {
        LEV_ASSERT(instance_);
        auto obj_ptr = __LEV borrow<T>(std::forward<decltype(obj)>(obj));
        LEV_CONSTRACT_ASSERT(obj_ptr);
        if (PyList_Append(instance_.get(), obj_ptr) !=
            __LEV result_code::success) {
            unhandled_error<std::runtime_error>();
        }
    }

    template <nothrow_t>
    LEV_HIDE_INSTANTIATION bool push_back(borrowable_as<T> auto&& obj) noexcept
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(instance_) {
        LEV_ASSERT(instance_);
        exception_checkpoint _;
        auto obj_ptr = __LEV borrow<T>(std::forward<decltype(obj)>(obj));
        LEV_CONSTRACT_ASSERT(obj_ptr);
        return PyList_Append(instance_.get(), obj_ptr) ==
            __LEV result_code::success;
    }

    LEV_HIDE_INSTANTIATION void push_front(borrowable_as<T> auto&& obj)
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(instance_) {
        LEV_ASSERT(instance_);
        auto obj_ptr = __LEV borrow<T>(std::forward<decltype(obj)>(obj));
        LEV_CONSTRACT_ASSERT(obj_ptr);
        if (PyList_Append(instance_.get(), obj_ptr) !=
            __LEV result_code::success) {
            unhandled_error<std::runtime_error>();
        }

        auto last = back();
        memmove(instance_->ob_item + 1, instance_->ob_item,
            sizeof(PyObject*) * (size() - 1));
        instance_->ob_item[0] = last;
    }

    template <nothrow_t>
    LEV_HIDE_INSTANTIATION bool push_front(borrowable_as<T> auto&& obj) noexcept
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(instance_) {
        LEV_ASSERT(instance_);
        exception_checkpoint _;
        auto obj_ptr = __LEV borrow<T>(std::forward<decltype(obj)>(obj));
        LEV_CONSTRACT_ASSERT(obj_ptr);
        if (PyList_Append(instance_.get(), obj_ptr) !=
            __LEV result_code::success) {
            return false;
        }

        auto last = back();
        memmove(instance_->ob_item + 1, instance_->ob_item,
            sizeof(PyObject*) * (size() - 1));
        instance_->ob_item[0] = last;
        return true;
    }

    LEV_HIDE_INSTANTIATION void pop_front() noexcept
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        python_ptr const first{retain_object, instance_->ob_item[0]};
        auto const new_size = size() - 1;
        memmove(instance_->ob_item, instance_->ob_item + 1,
            sizeof(PyObject*) * new_size);
        Py_SET_SIZE(instance_.get(), new_size);
    }

    LEV_HIDE_INSTANTIATION void pop_back() noexcept
    requires with_container_options<options_type, access::writable_t>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        auto const new_size = size() - 1;
        python_ptr const last{retain_object, instance_->ob_item[new_size]};
        Py_SET_SIZE(instance_.get(), new_size);
    }

    LEV_HIDE_INSTANTIATION iterator insert(
        const_iterator pos, borrowable_as<T> auto&& obj) LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t>
    {
        return insert(private_tag, pos,
            __LEV borrow<T>(std::forward<decltype(obj)>(obj)));
    }

    LEV_HIDE_INSTANTIATION iterator insert(
        const_iterator pos, borrowable_as<T> auto&& obj)
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    {
        return insert(private_tag, pos,
            __LEV borrow<T>(std::forward<decltype(obj)>(obj)));
    }

    LEV_HIDE_INSTANTIATION iterator insert(const_iterator pos, size_t count,
        borrowable_as<T> auto&& obj) LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t>
    {
        return insert(private_tag, pos, count,
            __LEV borrow<T>(std::forward<decltype(obj)>(obj)));
    }

    LEV_HIDE_INSTANTIATION iterator insert(
        const_iterator pos, size_t count, borrowable_as<T> auto&& obj)
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    {
        return insert(private_tag, pos, count,
            __LEV borrow<T>(std::forward<decltype(obj)>(obj)));
    }

    template <std::input_iterator It>
    requires with_container_options<options_type, access::writable_t>
    LEV_HIDE_INSTANTIATION iterator insert(
        const_iterator pos, It begin, It end) LEV_LIFETIMEBOUND
    requires requires(basic_list& list, const_iterator pos, It it) {
        list.insert(private_tag, pos, it, it);
    }
    {
        return insert(private_tag, pos, begin, end);
    }

    template <std::input_iterator It>
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    LEV_HIDE_INSTANTIATION iterator insert(const_iterator pos, It begin, It end)
    requires requires(basic_list& list, const_iterator pos, It it) {
        list.insert(private_tag, pos, it, it);
    }
    {
        return insert(private_tag, pos, begin, end);
    }

    template <typename U>
    requires borrowable_as<U const&, T>
    LEV_HIDE_INSTANTIATION iterator insert(
        const_iterator pos, std::initializer_list<U> ilist) LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t> &&
        requires(basic_list& list, const_iterator pos, U const* ptr) {
            list.insert(private_tag, pos, ptr, ptr);
        }
    {
        return insert(
            private_tag, pos, ilist.begin(), ilist.end(), ilist.size());
    }

    template <typename U>
    requires borrowable_as<U const&, T>
    LEV_HIDE_INSTANTIATION iterator insert(
        const_iterator pos, std::initializer_list<U> ilist)
    requires with_container_options<options_type, access::writable_t,
                 ownership::borrowed_t> &&
        requires(basic_list& list, const_iterator pos, U const* ptr) {
            list.insert(private_tag, pos, ptr, ptr);
        }
    {
        return insert(
            private_tag, pos, ilist.begin(), ilist.end(), ilist.size());
    }

    template <std::ranges::input_range R>
    LEV_HIDE_INSTANTIATION iterator insert_range(
        const_iterator pos, R&& range) LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t> &&
        requires(basic_list& list, iterator pos) {
            list.insert_range(private_tag, pos, std::declval<R>());
        }
    {
        return insert_range(private_tag, pos, std::forward<R>(range));
    }

    template <std::ranges::input_range R>
    LEV_HIDE_INSTANTIATION iterator insert_range(const_iterator pos, R&& range)
    requires with_container_options<options_type, access::writable_t,
                 ownership::borrowed_t> &&
        requires(basic_list& list, iterator pos) {
            list.insert_range(private_tag, pos, std::declval<R>());
        }
    {
        return insert_range(private_tag, pos, std::forward<R>(range));
    }

    template <std::ranges::input_range R>
    LEV_HIDE_INSTANTIATION void append_range(R&& range)
    requires with_container_options<options_type, access::writable_t> &&
        requires(basic_list& list) {
            list.insert(private_tag, list.cend(),
                std::ranges::begin(std::declval<R>()),
                std::ranges::end(std::declval<R>()));
        }
    {
        insert(private_tag, list.cend(), std::ranges::begin(range),
            std::ranges::end(range));
    }

    LEV_HIDE_INSTANTIATION iterator erase(
        iterator pos) noexcept LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t>
    {
        return erase(pos, pos + 1);
    }

    LEV_HIDE_INSTANTIATION iterator erase(
        const_iterator pos) noexcept LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t>
    {
        return erase(pos, pos + 1);
    }

    LEV_HIDE_INSTANTIATION iterator erase(
        iterator first, iterator last) noexcept LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t>
    {
        return erase(first, last);
    }

    LEV_HIDE_INSTANTIATION iterator erase(
        const_iterator first, const_iterator last) noexcept LEV_LIFETIMEBOUND
    requires with_container_options<options_type, access::writable_t>
    {
        return erase(private_tag, first, last);
    }

    LEV_HIDE_INSTANTIATION iterator erase(iterator pos) noexcept
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    {
        return erase(pos, pos + 1);
    }

    LEV_HIDE_INSTANTIATION iterator erase(const_iterator pos) noexcept
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    {
        return erase(pos, pos + 1);
    }

    LEV_HIDE_INSTANTIATION iterator erase(
        iterator first, iterator last) noexcept
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    {
        return erase(first, last);
    }

    LEV_HIDE_INSTANTIATION iterator erase(
        const_iterator first, const_iterator last) noexcept
    requires with_container_options<options_type, access::writable_t,
        ownership::borrowed_t>
    {
        return erase(private_tag, first, last);
    }

private:
    instance_type instance_;
};

} // namespace py
} // namespace lev

template <pyobj_type T>
inline constexpr bool
    std::ranges::enable_borrowed_range<__LEV py::borrowed::basic_list<T>> =
        true;
template <pyobj_type T>
inline constexpr bool std::ranges::enable_borrowed_range<
    __LEV py::borrowed::basic_readonly_list<T>> = true;
