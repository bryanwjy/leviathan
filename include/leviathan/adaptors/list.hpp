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

template <typename T, auto P = container_flags::none>
class basic_list;
template <typename T>
using basic_readonly_list = basic_list<T, container_flags::readonly>;

using list = basic_list<PyObject>;
using readonly_list = basic_readonly_list<PyObject>;

namespace borrowed {
using list = __LEV py::basic_list<PyObject, container_flags::borrowed>;
using readonly_list = __LEV py::basic_list<PyObject,
    container_flags::borrowed | container_flags::readonly>;

template <typename T, container_flags_type auto P>
using basic_list = __LEV py::basic_list<T, container_flags::borrowed | P>;
template <typename T, container_flags_type auto P>
using basic_readonly_list = __LEV py::basic_list<T,
    container_flags::borrowed | container_flags::readonly | P>;

} // namespace borrowed

template <pyobj_type T, container_flags_type auto P>
class LEV_API basic_list<T, P> {
    using flags_type = std::remove_cv_t<decltype(P)>;
    using borrowed_flag = container_flags::borrowed_t;
    using nothrow_flag = container_flags::nothrow_t;
    using readonly_flag = container_flags::readonly_t;
    LEV_HIDE_INSTANTIATION static constexpr bool is_nothrow_v =
        with_container_flags<flags_type, nothrow_flag>;

    template <container_flags_type auto O>
    LEV_HIDE_INSTANTIATION static constexpr bool is_explicit_construction_v =
        details::container::removal<decltype(O), flags_type, borrowed_flag> ||
        details::container::removal<decltype(O), flags_type, readonly_flag> ||
        details::container::acquisition<decltype(O), flags_type, nothrow_flag>;

    using span_type =
        std::conditional_t<without_container_flags<flags_type, readonly_flag>,
            std::span<PyObject*>, std::span<PyObject* const>>;
    using reference_proxy = element_reference<
        details::random_access_reference<T, cast_policy::safe>>;
    using instance_type =
        std::conditional_t<with_container_flags<flags_type, borrowed_flag>,
            unmanaged_ptr<PyListObject>, python_ptr<PyListObject>>;

    template <typename T>
    using result_type =
        std::conditional_t<is_nothrow_v, __LTL expected<T, result_code>, T>;

    struct private_tag_t {};

    LEV_HIDE_INSTANTIATION static constexpr private_tag_t private_tag{};

    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        private_tag_t tag [[maybe_unused]], auto&& ptr) noexcept
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(ptr) : instance_{std::forward<decltype(ptr)>(ptr)} {
        // Only assert when mutable
        // Immutable list has no operations with side-effects, thus no need
        // for a stricly non-null pre-condition
        LEV_ASSERT(instance_);
    }

    LEV_HIDE_INSTANTIATION inline result_type<iterator> insert(private_tag_t, const_iterator pos,
        unmanaged_ptr<T> obj) noexcept(is_nothrow_v)
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(instance_) LEV_CONSTRACT_PRE(obj) {
        LEV_ASSERT(instance_);
        auto const idx = pos - cbegin();
        LEV_CONTRACT_ASSERT(idx <= size());
        if constexpr (is_nothrow_v) {
            exception_checkpoint _;
            if (PyList_Insert(instance_.get(), idx, obj.get()) !=
                __LEV result_code::success) {
                return result_type<iterator>{
                    __LTL unexpect, result_code::failed};
            }
        } else {
            if (PyList_Insert(instance_.get(), idx, obj.get()) !=
                __LEV result_code::success) {
                unhandled_error<std::runtime_error>();
            }
        }
        return begin() + idx;
    }

    LEV_HIDE_INSTANTIATION LEV_UNSAFE_API inline result_type<iterator> insert(private_tag_t,
        const_iterator pos, size_t count,
        unmanaged_ptr<T> obj) noexcept(is_nothrow_v)
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(instance_) LEV_CONSTRACT_PRE(obj) {
        LEV_ASSERT(instance_);
        auto const idx = pos - cbegin();
        LEV_CONTRACT_ASSERT(idx <= size());

        if constexpr (is_nothrow_v) {
            if (auto exp = reserve(new_size); !exp) {
                return result_type<iterator>{
                    __LTL unexpect, result_code::failed};
            }
        } else {
            reserve(new_size);
        }

        critical_section _{instance_.get(), obj};

        auto const new_size = size() + count;
        auto const to_move = size() - idx;
        auto* const src = instance_->ob_item + idx;
        auto* const dst = src + count;
        memmove(dst, src, sizeof(PyObject*) * to_move);
        as_pyobject(obj)->ob_refcnt += count;
        std::span new_elements{src, dst};
        for (auto& ptr : new_elements) {
            ptr = obj;
        }
        Py_SET_SIZE(instance_, new_size);

        return begin() + idx;
    }

    template <std::forward_iterator It>
    LEV_HIDE_INSTANTIATION LEV_UNSAFE_API inline result_type<iterator> insert(private_tag_t,
        const_iterator pos, It begin, It end,
        size_t const count) noexcept(is_nothrow_v)
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(instance_) {
        LEV_ASSERT(instance_);
        auto const idx = std::ranges::distance(cbegin(), pos);
        LEV_CONTRACT_ASSERT(idx <= size());
        auto const new_size = size() + count;
        auto const to_move = size() - idx;
        if constexpr (is_nothrow_v) {
            if (auto exp = reserve(new_size); !exp) {
                return result_type<iterator>{
                    __LTL unexpect, result_code::failed};
            }
        } else {
            reserve(new_size);
        }

        critical_section _{instance_.get(), obj};

        auto* const old_pos = instance_->ob_item + idx;
        auto* const new_pos = old_pos + count;
        memmove(new_pos, old_pos, sizeof(PyObject*) * to_move);
        std::ranges::copy(begin, end, iterator{old_pos});
        Py_SET_SIZE(instance_, new_size);
        return begin() + idx;
    }

    template <std::forward_iterator It>
    LEV_HIDE_INSTANTIATION inline result_type<iterator> insert(private_tag_t tag,
        const_iterator pos, It begin, It end) noexcept(is_nothrow_v)
    requires without_container_flags<flags_type, readonly_flag>
    {
        return insert(tag, pos, begin, end, std::ranges::distance(begin, end));
    }

    template <typename R>
    requires std::ranges::forward_range<R> ||
        std::ranges::sized_range<R>
        LEV_HIDE_INSTANTIATION inline result_type<iterator> insert_range(private_tag_t tag,
            const_iterator pos, R&& range) noexcept(is_nothrow_v)
    requires without_container_flags<flags_type, readonly_flag>
    {
        return insert(tag, pos, std::ranges::begin(range),
            std::ranges::end(range), std::ranges::size(range));
    }

    template <std::ranges::input_range R>
    LEV_HIDE_INSTANTIATION inline result_type<iterator> insert_range(
        private_tag_t tag, const_iterator pos, R&& range) noexcept(is_nothrow_v)
    requires without_container_flags<flags_type, readonly_flag>
    {
        return insert(
            tag, pos, std::ranges::begin(range), std::ranges::end(range));
    }

    template <std::input_iterator It>
    requires borrowable_as<std::iter_reference_t<It>, T>
    LEV_HIDE_INSTANTIATION inline result_type<iterator> insert(private_tag_t, const_iterator pos,
        It begin, It end) noexcept(is_nothrow_v)
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(instance_) {
        auto const idx = pos - cbegin();
        while (begin != end) {
            auto ptr = __LEV borrow(*begin);
            LEV_CONSTRACT_PRE(ptr);
            if constexpr (is_nothrow_v) {
                if (auto exp = insert(private_tag, pos, ptr.get()); !exp) {
                    return exp;
                }
            } else {
                insert(private_tag, pos, ptr.get());
            }
            ++begin;
        }

        return begin() + idx;
    }

    LEV_HIDE_INSTANTIATION inline iterator erase(
        private_tag_t, const_iterator first, const_iterator const last) noexcept
    requires without_container_flags<flags_type, readonly_flag>
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
    using iterator =
        std::conditional_t<with_container_flags<flags_type, readonly_flag>,
            const_iterator, random_access_iterator<T>>;
    using const_iterator = random_access_const_iterator<T>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using value_type = unmanaged_ptr<T>;

    LEV_HIDE_INSTANTIATION constexpr inline basic_list(basic_list const&) noexcept = default;
    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr inline basic_list& operator=(
        basic_list const&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline basic_list(basic_list&&) noexcept = default;
    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr inline basic_list& operator=(
        basic_list&&) noexcept = default;

    LEV_HIDE_INSTANTIATION inline constexpr basic_list(python_ptr<PyListObject>&& ptr) noexcept
    requires without_container_flags<flags_type, borrowed_flag>
        : basic_list(private_tag, std::move(ptr)) {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        python_ptr<PyListObject> const& ptr) noexcept
        : basic_list(private_tag, ptr) {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        python_ptr<PyListObject> const& ptr LEV_LIFETIMEBOUND) noexcept
    requires with_container_flags<flags_type, borrowed_flag>
        : basic_list(private_tag, ptr.get()) {}

    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_list(
        unmanaged_ptr<PyListObject> ptr) noexcept
        : basic_list{__LEV adopt(ptr)} {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_list(unmanaged_ptr<PyListObject> ptr) noexcept
    requires with_container_flags<flags_type, borrowed_flag>
        : basic_list{ptr} {}

    template <pyobj_derived_from<T> U,
        details::container::exclusion<flags_type, borrowed_flag> auto Opt>
    LEV_HIDE_INSTANTIATION explicit(is_explicit_construction_v<Opt>) inline constexpr basic_list(
        basic_list<U, Opt>&& other) noexcept
    requires (!details::container::acquisition<decltype(Opt), flags_type,
                  nothrow_flag> &&
        !details::container::removal<decltype(Opt), flags_type, readonly_flag>)
        : basic_list{std::move(other.instance_)} {}

    template <pyobj_derived_from<T> U, container_flags_type auto Opt>
    LEV_HIDE_INSTANTIATION explicit(is_explicit_construction_v<Opt>) inline constexpr basic_list(
        basic_list<U, Opt> const& other) noexcept
    requires (!details::container::acquisition<decltype(Opt), flags_type,
                  nothrow_flag> &&
        !details::container::removal<decltype(Opt), flags_type, readonly_flag>)
        : basic_list{other.instance_} {}

    template <pyobj_derived_from<T> U,
        details::container::acquisition<flags_type, borrowed_flag> auto Opt>
    LEV_HIDE_INSTANTIATION inline constexpr basic_list(
        basic_list<U, Opt> const& other LEV_LIFETIMEBOUND) noexcept
    requires (!details::container::acquisition<decltype(Opt), flags_type,
                  nothrow_flag> &&
        !details::container::removal<decltype(Opt), flags_type, readonly_flag>)
        : basic_list{other.instance()} {}

    template <pyobj_derived_from<T> U, container_flags_type auto Opt>
    LEV_HIDE_INSTANTIATION explicit(is_explicit_construction_v<Opt>) inline constexpr basic_list(
        basic_list<U, Opt> other) noexcept
    requires (!details::container::acquisition<decltype(Opt), flags_type,
                  nothrow_flag> &&
        !details::container::removal<decltype(Opt), flags_type, readonly_flag>)
        : basic_list{__LEV adopt(other.instance())} {}

    template <pyobj_derived_from<T> U,
        details::container::maintenance<flags_type, borrowed_flag> auto Opt>
    LEV_HIDE_INSTANTIATION inline constexpr basic_list(basic_list<U, Opt> other) noexcept
    requires (!details::container::acquisition<decltype(Opt), flags_type,
                  nothrow_flag> &&
        !details::container::removal<decltype(Opt), flags_type, readonly_flag>)
        : basic_list{other.instance()} {}

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto begin() const noexcept LEV_LIFETIMEBOUND {
        return cbegin();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto end() const noexcept LEV_LIFETIMEBOUND {
        return cend();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto rbegin() const noexcept LEV_LIFETIMEBOUND {
        return crbegin();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto rend() const noexcept LEV_LIFETIMEBOUND {
        return crend();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto cbegin() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_iterator{instance_->ob_item}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto cend() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_iterator{instance_->ob_item + size()}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto crbegin() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_reverse_iterator{end()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto crend() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? const_reverse_iterator{begin()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto begin() noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    {
        return instance_ ? iterator{instance_->ob_item} : iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto end() noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    {
        return instance_ ? iterator{instance_->ob_item + size()} : iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto rbegin() noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    {
        return instance_ ? reverse_iterator{end()} : reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto rend() noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    {
        return instance_ ? reverse_iterator{begin()} : reverse_iterator{};
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

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto rbegin() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return crbegin();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto rend() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return crend();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto cbegin() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return instance_ ? const_iterator{instance_->ob_item}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto cend() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return instance_ ? const_iterator{instance_->ob_item + size()}
                         : const_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto crbegin() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return instance_ ? const_reverse_iterator{end()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto crend() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return instance_ ? const_reverse_iterator{begin()}
                         : const_reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto begin() noexcept
    requires with_container_flags<flags_type, borrowed_flag> &&
        without_container_flags<flags_type, readonly_flag>
    {
        return instance_ ? iterator{instance_->ob_item} : iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto end() noexcept
    requires with_container_flags<flags_type, borrowed_flag> &&
        without_container_flags<flags_type, readonly_flag>
    {
        return instance_ ? iterator{instance_->ob_item + size()} : iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto rbegin() noexcept
    requires with_container_flags<flags_type, borrowed_flag> &&
        without_container_flags<flags_type, readonly_flag>
    {
        return instance_ ? reverse_iterator{end()} : reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto rend() noexcept
    requires with_container_flags<flags_type, borrowed_flag> &&
        without_container_flags<flags_type, readonly_flag>
    {
        return instance_ ? reverse_iterator{begin()} : reverse_iterator{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<T> operator[](
        size_type idx) const noexcept LEV_LIFETIMEBOUND LEV_CONSTRACT_PRE(instance_ && !empty() && idx < size()) {
        LEV_ASSERT(instance_ && !empty() && idx < size());
        return dynamic_ptr_cast<T>(instance_->ob_item[idx]);
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<T> operator[](
        size_type idx) const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    LEV_CONSTRACT_PRE(instance_ && !empty() && idx < size()) {
        LEV_ASSERT(instance_ && !empty() && idx < size());
        return dynamic_ptr_cast<T>(instance_->ob_item[idx]);
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto operator[](
        size_type idx) noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(instance_ && !empty() && idx < size()) {
        LEV_ASSERT(instance_ && !empty() && idx < size());
        return reference_proxy{instance_->ob_item[idx]};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto operator[](
        size_type idx) noexcept
    requires without_container_flags<flags_type, readonly_flag> &&
        with_container_flags<flags_type, borrowed_flag>
    LEV_CONSTRACT_PRE(instance_ && !empty() && idx < size()) {
        LEV_ASSERT(instance_ && !empty() && idx < size());
        return reference_proxy{instance_->ob_item[idx]};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool empty() const noexcept {
        return !size();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_type size() const noexcept {
        return instance_ ? Py_SIZE(instance_.get()) : 0u;
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_type
    capacity() const noexcept {
        return instance_ ? static_cast<size_type>(instance_->allocated) : 0u;
    }

    auto front() noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[0];
    }

    auto front() noexcept
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[0];
    }
    auto front() const noexcept LEV_LIFETIMEBOUND LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[0];
    }

    auto front() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[0];
    }

    auto back() noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[size() - 1];
    }

    auto back() noexcept
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[size() - 1];
    }

    auto back() const noexcept LEV_LIFETIMEBOUND LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[size() - 1];
    }

    auto back() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        return *this[size() - 1];
    }

    LEV_HIDE_INSTANTIATION void clear()
    requires without_container_flags<flags_type, readonly_flag, nothrow_flag>
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

    LEV_HIDE_INSTANTIATION result_type<void> clear() noexcept
    requires without_container_flags<flags_type, readonly_flag>
    {
        if (!instance_) {
            return result_type<void>{__LTL unexpect, result_code::failed};
        }

        exception_checkpoint _;
        auto const result =
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 13
            PyList_Clear(instance_.get());
#else
            PyList_SetSlice(
                instance_.get(), 0, static_cast<Py_ssize_t>(size()), nullptr);
#endif

        if (result != result_code::success) {
            return result_type<void>{__LTL unexpect, result};
        }

        return result_type<void>{};
    }

    LEV_HIDE_INSTANTIATION
        LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<PyListObject>
    instance() const noexcept LEV_LIFETIMEBOUND {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION
        LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<PyListObject>
    instance() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr span_type span() const noexcept LEV_LIFETIMEBOUND {
        return instance_ ? span_type{instance_->ob_item, size()} : span_type{};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr span_type span() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return instance_ ? span_type{instance_->ob_item, size()} : span_type{};
    }

    LEV_HIDE_INSTANTIATION friend void swap(basic_list& lhs, basic_list& rhs) noexcept {
        lhs.swap(other);
    }

    LEV_HIDE_INSTANTIATION void swap(basic_list& other) noexcept {
        instance_.swap(other.instance_);
    }

    LEV_HIDE_INSTANTIATION LEV_UNSAFE_API void reserve(size_t new_capacity)
    requires without_container_flags<flags_type, readonly_flag, nothrow_flag>
    {
        if (!reserve<nothrow>(new_capacity)) {
            failure<std::bad_alloc, PyExc_NoMemory>("List reallocation failed");
        }
    }

    LEV_HIDE_INSTANTIATION LEV_UNSAFE_API result_type<void> reserve(size_t new_capacity) noexcept
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(instance_) {
        LEV_ASSERT(instance_);
        if (new_capacity < capacity()) {
            return result_type<void>{};
        }

        critical_section _(instance_.get());
        auto items =
            PyMem_Realloc(instance_->ob_item, sizeof(PyObject*) * new_capacity);
        if (items == nullptr) {
            return result_type<void>{__LTL unexpect, result_code::failed};
        }

        instance_->allocated = new_capacity;
        instance_->ob_item = items;
        return result_type<void>{};
    }

    LEV_HIDE_INSTANTIATION result_type<void> push_back(borrowable_as<T> auto&& obj) noexcept(
        is_nothrow_v)
    requires without_container_flags<flags_type, readonly_flag>
    LEV_CONSTRACT_PRE(instance_) {
        LEV_ASSERT(instance_);
        auto obj_ptr = __LEV borrow<T>(std::forward<decltype(obj)>(obj));
        LEV_CONSTRACT_ASSERT(obj_ptr);
        auto const result = PyList_Append(instance_.get(), obj_ptr);

        if constexpr (is_nothrow_v) {
            if (result != __LEV result_code::success) {
                return result_type<void>{__LTL unexpect, result};
            }

            return result_type<void>{};
        } else if (result != __LEV result_code::success) {
            unhandled_error<std::runtime_error>();
        }
    }

    LEV_HIDE_INSTANTIATION result_type<void> push_front(borrowable_as<T> auto&& obj)
    requires with_container_flags<flags_type, access::writable_t>
    LEV_CONSTRACT_PRE(instance_) {
        LEV_ASSERT(instance_);
        auto obj_ptr = __LEV borrow<T>(std::forward<decltype(obj)>(obj));
        LEV_CONSTRACT_ASSERT(obj_ptr);

        auto const result = PyList_Append(instance_.get(), obj_ptr);

        if constexpr (is_nothrow_v) {
            if (result != __LEV result_code::success) {
                return result_type<void>{__LTL unexpect, result};
            }
        } else if (result != __LEV result_code::success) {
            unhandled_error<std::runtime_error>();
        }

        auto last = back();
        memmove(instance_->ob_item + 1, instance_->ob_item,
            sizeof(PyObject*) * (size() - 1));
        instance_->ob_item[0] = last;

        if constexpr (is_nothrow_v) {
            return result_type<void>{};
        }
    }

    LEV_HIDE_INSTANTIATION void pop_front() noexcept
    requires with_container_flags<flags_type, access::writable_t>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        auto const first = __LEV steal(instance_->ob_item[0]);
        auto const new_size = size() - 1;
        memmove(instance_->ob_item, instance_->ob_item + 1,
            sizeof(PyObject*) * new_size);
        Py_SET_SIZE(instance_.get(), new_size);
    }

    LEV_HIDE_INSTANTIATION void pop_back() noexcept
    requires with_container_flags<flags_type, access::writable_t>
    LEV_CONSTRACT_PRE(!empty()) {
        LEV_ASSERT(!empty());
        auto const new_size = size() - 1;
        auto const last = __LEV steal(instance_->ob_item[new_size]);
        Py_SET_SIZE(instance_.get(), new_size);
    }

    LEV_HIDE_INSTANTIATION result_type<iterator> insert(
        const_iterator pos, borrowable_as<T> auto&& obj) LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    {
        return insert(private_tag, pos,
            __LEV borrow<T>(std::forward<decltype(obj)>(obj)));
    }

    LEV_HIDE_INSTANTIATION result_type<iterator> insert(
        const_iterator pos, borrowable_as<T> auto&& obj)
    requires without_container_flags<flags_type, readonly_flag>
    {
        return insert(private_tag, pos,
            __LEV borrow<T>(std::forward<decltype(obj)>(obj)));
    }

    LEV_HIDE_INSTANTIATION result_type<iterator> insert(
        const_iterator pos, size_t count, borrowable_as<T> auto&& obj)
        LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    {
        return insert(private_tag, pos, count,
            __LEV borrow<T>(std::forward<decltype(obj)>(obj)));
    }

    LEV_HIDE_INSTANTIATION result_type<iterator> insert(
        const_iterator pos, size_t count, borrowable_as<T> auto&& obj)
    requires without_container_flags<flags_type, readonly_flag>
    {
        return insert(private_tag, pos, count,
            __LEV borrow<T>(std::forward<decltype(obj)>(obj)));
    }

    template <std::input_iterator It>
    LEV_HIDE_INSTANTIATION result_type<iterator> insert(const_iterator pos, It begin, It end)
        LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag,
                 borrowed_flag> &&
        requires(basic_list& list, const_iterator pos, It it) {
            list.insert(private_tag, pos, it, it);
        }
    {
        return insert(private_tag, pos, begin, end);
    }

    template <std::input_iterator It>
    LEV_HIDE_INSTANTIATION result_type<iterator> insert(const_iterator pos, It begin, It end)
    requires without_container_flags<flags_type, readonly_flag> &&
        requires(basic_list& list, const_iterator pos, It it) {
            list.insert(private_tag, pos, it, it);
        }
    {
        return insert(private_tag, pos, begin, end);
    }

    template <typename U>
    requires borrowable_as<U const&, T>
    LEV_HIDE_INSTANTIATION result_type<iterator> insert(const_iterator pos,
        std::initializer_list<U> ilist) noexcept(is_nothrow_v) LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag,
                 borrowed_flag> &&
        requires(basic_list& list, const_iterator pos, U const* ptr) {
            list.insert(private_tag, pos, ptr, ptr);
        }
    {
        return insert(
            private_tag, pos, ilist.begin(), ilist.end(), ilist.size());
    }

    template <typename U>
    requires borrowable_as<U const&, T>
    LEV_HIDE_INSTANTIATION result_type<iterator> insert(const_iterator pos,
        std::initializer_list<U> ilist) noexcept(is_nothrow_v)
    requires without_container_flags<flags_type, readonly_flag> &&
        requires(basic_list& list, const_iterator pos, U const* ptr) {
            list.insert(private_tag, pos, ptr, ptr);
        }
    {
        return insert(
            private_tag, pos, ilist.begin(), ilist.end(), ilist.size());
    }

    template <std::ranges::input_range R>
    LEV_HIDE_INSTANTIATION result_type<iterator> insert_range(
        const_iterator pos, R&& range) noexcept(is_nothrow_v) LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag,
                 borrowed_flag> &&
        requires(basic_list& list, iterator pos) {
            list.insert_range(private_tag, pos, std::declval<R>());
        }
    {
        return insert_range(private_tag, pos, std::forward<R>(range));
    }

    template <std::ranges::input_range R>
    LEV_HIDE_INSTANTIATION result_type<iterator> insert_range(
        const_iterator pos, R&& range) noexcept(is_nothrow_v)
    requires without_container_flags<flags_type, readonly_flag> &&
        requires(basic_list& list, iterator pos) {
            list.insert_range(private_tag, pos, std::declval<R>());
        }
    {
        return insert_range(private_tag, pos, std::forward<R>(range));
    }

    template <std::ranges::input_range R>
    LEV_HIDE_INSTANTIATION void append_range(R&& range) noexcept(is_nothrow_v)
    requires without_container_flags<flags_type, readonly_flag> &&
        requires(basic_list& list) {
            list.insert(private_tag, list.cend(),
                std::ranges::begin(std::declval<R>()),
                std::ranges::end(std::declval<R>()));
        }
    {
        insert(private_tag, list.cend(), std::ranges::begin(range),
            std::ranges::end(range));
    }

    LEV_HIDE_INSTANTIATION iterator erase(iterator pos) noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    {
        return erase(pos, pos + 1);
    }

    LEV_HIDE_INSTANTIATION iterator erase(const_iterator pos) noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    {
        return erase(pos, pos + 1);
    }

    LEV_HIDE_INSTANTIATION iterator erase(iterator first, iterator last) noexcept LEV_LIFETIMEBOUND
    requires without_container_flags<flags_type, readonly_flag, borrowed_flag>
    {
        return erase(first, last);
    }

    LEV_HIDE_INSTANTIATION iterator erase(const_iterator first, const_iterator last) noexcept LEV_LIFETIMEBOUND
    requires with_container_flags<flags_type, access::writable_t>
    {
        return erase(private_tag, first, last);
    }

    LEV_HIDE_INSTANTIATION iterator erase(iterator pos) noexcept
    requires without_container_flags<flags_type, readonly_flag>
    {
        return erase(pos, pos + 1);
    }

    LEV_HIDE_INSTANTIATION iterator erase(const_iterator pos) noexcept
    requires without_container_flags<flags_type, readonly_flag>
    {
        return erase(pos, pos + 1);
    }

    LEV_HIDE_INSTANTIATION iterator erase(iterator first, iterator last) noexcept
    requires without_container_flags<flags_type, readonly_flag>
    {
        return erase(first, last);
    }

    LEV_HIDE_INSTANTIATION iterator erase(const_iterator first, const_iterator last) noexcept
    requires without_container_flags<flags_type, readonly_flag>
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
