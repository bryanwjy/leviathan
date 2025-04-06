// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/common.hpp"
#include "leviathan/adaptors/random_access_iterator.hpp"
#include "leviathan/pointer.hpp"

#include <array>
#include <compare>
#include <iterator>
#include <ranges>
#include <stdexcept>

namespace lev {
namespace py {

template <typename... Ts>
class tuple;
template <typename... Ts>
class tuple_view;
template <typename T, size_t N = dynamic_extent>
class array;
template <typename T, size_t N = dynamic_extent>
class array_view;
namespace views {
template <pyobj_type... Ts>
using tuple = tuple_view<Ts...>;
template <pyobj_type T, size_t N = dynamic_extent>
using array = array_view<T, N>;
} // namespace views

namespace details::tuple {
LEV_HIDDEN constexpr char const* null_instance_message_v =
    "Tuple instance is null";
LEV_HIDDEN constexpr char const* size_mismatch_message_v =
    "Tuple-Adaptor size mismatch";
LEV_HIDDEN constexpr char const* invalid_type_message_v =
    "Tuple-Adaptor element type mismatch";

LEV_HIDDEN std::span<PyObject* const> items(
    unmanaged_ptr<PyTupleObject> tuple, size_t size) noexcept {
    // In C++, performing index operations on ob_items for indices > 0 is UB
    LEV_ASSERT(tuple);
    LEV_ASSERT(size > 0);
    // The following is a poor man's std::start_lifetime_as_array to "reboot"
    // the lifetime of the array because PyTupleObject defines it as a
    // PyObject*[1] member

    // Unknown: std::launder may work?
    PyObject** ptr = reinterpret_cast<PyObject**>(
        memmove(tuple->ob_item, tuple->ob_item, size * sizeof(PyObject*)));

    // See intro.object.12
    // pointer arithmetic iteration forces creation of the array of the
    // pyobjects since an array of pyobjects must exist for the for-loop
    // to be defined behaviour
    std::span<PyObject* const> span{ptr, ptr + size};
    // This should get optimize out but is here to give the program defined
    // behaviour
    for (auto p : span) {
        if (p) {
            (void)*p;
        }
    }

    return span;
}

template <typename... Ts>
LEV_HIDE_INSTANTIATION std::span<PyObject* const, sizeof...(Ts)>
instance_to_span(unmanaged_ptr<PyTupleObject> ptr) {
    using span_type = std::span<PyObject* const, sizeof...(Ts)>;
    static constexpr auto E = sizeof...(Ts);

    if constexpr (E > 0) {
        if (!ptr) {
            failure<std::invalid_argument, PyExc_RuntimeError>(
                details::tuple::null_instance_message_v);
        }
    }

    auto const size = ptr ? static_cast<size_t>(PyTuple_GET_SIZE(ptr)) : 0;
    if (size != E) {
        failure<std::invalid_argument, PyExc_ValueError>(
            details::tuple::size_mismatch_message_v);
    }

    if constexpr (E == 0) {
        if (size == 0) {
            return span_type{static_cast<PyObject* const*>(ptr->ob_item), 0};
        }
    }

    auto items = details::tuple::items(ptr, size);
    auto const types_valid = [&]<size_t... Is>(std::index_sequence<Is...>) {
        return (... && dynamic_ptr_cast<Ts>(items[Is]));
    }(std::make_index_sequence<E>{});

    if (!types_valid) {
        failure<type_error, PyExc_TypeError>(
            details::tuple::invalid_type_message_v);
    }

    return span_type{items};
}

template <typename T, size_t E>
LEV_HIDE_INSTANTIATION std::span<PyObject* const, E> instance_to_span(
    unmanaged_ptr<PyTupleObject> ptr) {
    using span_type = std::span<PyObject* const, E>;
    if constexpr (E > 0) {
        if (!ptr) {
            failure<std::invalid_argument, PyExc_RuntimeError>(
                details::tuple::null_instance_message_v);
        }
    }

    auto const size = ptr ? static_cast<size_t>(PyTuple_GET_SIZE(ptr)) : 0;
    if constexpr (E != dynamic_extent) {
        if (size != E) {
            failure<std::invalid_argument, PyExc_ValueError>(
                details::tuple::size_mismatch_message_v);
        }
    }

    if constexpr (E == 0 || E == dynamic_extent) {
        if (size == 0) {
            return span_type{static_cast<PyObject* const*>(ptr->ob_item), 0};
        }
    }

    auto items = details::tuple::items(ptr, size);

    auto const types_valid = [&]() {
        if constexpr (E != dynamic_extent) {
            return [&]<size_t... Is>(std::index_sequence<Is...>) {
                return (... && dynamic_ptr_cast<T>(items[Is]));
            }(std::make_index_sequence<E>{});
        }

        // Don't check for dynamic extent
        return true;
    }();

    if (!types_valid) {
        failure<type_error, PyExc_TypeError>(
            details::tuple::invalid_type_message_v);
    }

    return span_type{items};
}
} // namespace details::tuple

template <pyobj_type... Ts>
class LEV_API tuple<Ts...> {
    using span_type = std::span<PyObject* const, sizeof...(Ts)>;

    template <typename...>
    friend tuple;
    template <typename, size_t>
    friend array;

public:
    LEV_HIDE_INSTANTIATION constexpr inline tuple(
        tuple const&) noexcept = default;
    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr inline tuple&
    operator=(tuple const&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline tuple(tuple&&) noexcept = default;
    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr inline tuple&
    operator=(tuple&&) noexcept = default;

    LEV_HIDE_INSTANTIATION explicit inline tuple(
        python_ptr<PyTupleObject>&& ptr)
        : instance_{std::move(ptr)}
        , span_{details::tuple::instance_to_span<Ts...>(instance_.get())} {}

    LEV_HIDE_INSTANTIATION explicit inline tuple(
        python_ptr<PyTupleObject> const& ptr)
        : tuple(python_ptr<PyTupleObject>(ptr)) {}

    template <pyobj_derived_from<PyTupleObject> U>
    LEV_HIDE_INSTANTIATION explicit inline tuple(python_ptr<U>&& ptr)
        : instance_{static_ptr_cast<PyTupleObject>(std::move(ptr))}
        , span_{details::tuple::instance_to_span<Ts...>(instance_.get())} {}

    template <pyobj_derived_from<PyTupleObject> U>
    LEV_HIDE_INSTANTIATION explicit inline tuple(python_ptr<U> const& ptr)
        : tuple(python_ptr<U>(ptr)) {}

    template <pyobj_derived_from<Ts>... Us>
    LEV_HIDE_INSTANTIATION explicit inline tuple(
        adopt_t tag, tuple_view<Us...> other) noexcept
        : instance_{tag, other.instance()}
        , span_{other.span()} {}

    template <pyobj_derived_from<Ts>... Us>
    LEV_HIDE_INSTANTIATION inline tuple(tuple<Us...> const& other) noexcept
        : instance_{other.instance()}
        , span_{other.span()} {}

    template <pyobj_derived_from<Ts>... Us>
    LEV_HIDE_INSTANTIATION inline tuple(tuple<Us...>&& other) noexcept
        : instance_{std::move(other.instance_)}
        , span_{other.span()} {}

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool
    empty() const noexcept {
        return sizeof...(Ts) == 0;
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_t
    size() const noexcept {
        return sizeof...(Ts);
    }

    LEV_HIDE_INSTANTIATION
    LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<PyTupleObject>
    instance() const noexcept LEV_LIFETIMEBOUND {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline span_type
    span() const noexcept LEV_LIFETIMEBOUND {
        return span_;
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[LEV_PURE,
        nodiscard]] friend unmanaged_ptr<tuple_element_t<I, typelist<Ts...>>>
    get(tuple const& tuple) noexcept LEV_LIFETIMEBOUND {
        using target_type = tuple_element_t<I, typelist<Ts...>>;
        unmanaged_ptr<PyObject*> data(tuple.span_[I]);
        return static_ptr_cast<target_type>(data);
    }

    template <typename U>
    requires (1 == template_count_v<U, typelist<Ts...>>)
    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] friend unmanaged_ptr<U> get(
        tuple const& tuple) noexcept LEV_LIFETIMEBOUND {
        unmanaged_ptr<PyObject*> data(
            tuple.span_[template_index_v<U, typelist<Ts...>>]);
        return static_ptr_cast<U>(data);
    }

private:
    python_ptr<PyTupleObject> instance_;
    span_type span_;
};

template <pyobj_type... Ts>
class LEV_API tuple_view<Ts...> {
    using span_type = std::span<PyObject* const, sizeof...(Ts)>;

public:
    LEV_HIDE_INSTANTIATION constexpr inline tuple_view(
        tuple_view const&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline tuple_view& operator=(
        tuple_view const&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline tuple_view(
        tuple_view&&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline tuple_view& operator=(
        tuple_view&&) noexcept = default;

    LEV_HIDE_INSTANTIATION explicit inline tuple_view(
        unmanaged_ptr<PyTupleObject> ptr)
        : instance_{ptr}
        , span_{details::tuple::instance_to_span<Ts...>(ptr)} {}

    LEV_HIDE_INSTANTIATION explicit inline tuple_view(
        python_ptr<PyTupleObject> const& ptr LEV_LIFETIMEBOUND)
        : tuple_view{ptr.get()} {}

    template <pyobj_derived_from<Ts>... Us>
    LEV_HIDE_INSTANTIATION inline tuple_view(
        tuple<Us...> const& other LEV_LIFETIMEBOUND) noexcept
        : instance_{other.instance()}
        , span_{other.span()} {}

    template <pyobj_derived_from<Ts>... Us>
    LEV_HIDE_INSTANTIATION inline tuple_view(tuple_view<Us...> other) noexcept
        : instance_{other.instance()}
        , span_{other.span()} {}

    LEV_HIDE_INSTANTIATION
    LEV_PURE [[nodiscard]] inline unmanaged_ptr<PyTupleObject>
    instance() const noexcept {
        return instance_;
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool
    empty() const noexcept {
        return span_.empty();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_t
    size() const noexcept {
        return span_.size();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline span_type
    span() const noexcept LEV_LIFETIMEBOUND {
        return span_;
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION [[LEV_PURE,
        nodiscard]] friend unmanaged_ptr<tuple_element_t<I, typelist<Ts...>>>
    get(tuple const& tuple) noexcept LEV_LIFETIMEBOUND {
        using target_type = tuple_element_t<I, typelist<Ts...>>;
        unmanaged_ptr<PyObject*> data(tuple.span_[I]);
        return static_ptr_cast<target_type>(data);
    }

    template <typename U>
    requires (1 == template_count_v<U, typelist<Ts...>>)
    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] friend unmanaged_ptr<U> get(
        tuple const& tuple) noexcept LEV_LIFETIMEBOUND {
        unmanaged_ptr<PyObject*> data(
            tuple.span_[template_index_v<U, typelist<Ts...>>]);
        return static_ptr_cast<U>();
    }

private:
    unmanaged_ptr<PyTupleObject> instance_;
    span_type span_;
};

template <pyobj_type T, size_t E>
class LEV_API array {
    using span_type = std::span<PyObject* const, E>;
    template <typename T, size_t E>
    friend array;

    static constexpr auto cast_type =
        E == dynamic_extent ? cast_policy::safe : cast_policy::unsafe;

public:
    using value_type = unmanaged_ptr<T>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using const_iterator = random_access_const_iterator<T, cast_type>;
    using iterator = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = std::reverse_iterator<iterator>;

    LEV_HIDE_INSTANTIATION constexpr inline array(
        array const&) noexcept = default;
    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr inline array&
    operator=(array const&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline array(array&&) noexcept = default;
    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr inline array&
    operator=(array&&) noexcept = default;

    LEV_HIDE_INSTANTIATION explicit inline array(
        python_ptr<PyTupleObject>&& ptr)
        : instance_{std::move(ptr)}
        , span_{details::tuple::instance_to_span<T, E>(instance_.get())} {}

    LEV_HIDE_INSTANTIATION explicit inline array(
        python_ptr<PyTupleObject> const& ptr)
        : array(python_ptr<PyTupleObject>(ptr)) {}

    template <pyobj_derived_from<T> U, size_t N>
    requires (E != dynamic_extent && N == E)
    LEV_HIDE_INSTANTIATION inline array(
        adopt_t tag, array_view<U, N> other) noexcept
        : instance_{tag, other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T> U, size_t N>
    requires (E != dynamic_extent && N == E)
    LEV_HIDE_INSTANTIATION inline array(array<U, N> const& other) noexcept
        : instance_{tag, other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T> U, size_t N>
    requires (E != dynamic_extent && N == E)
    LEV_HIDE_INSTANTIATION inline array(array<U, N>&& other) noexcept
        : instance_{tag, other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T> U>
    LEV_HIDE_INSTANTIATION explicit inline array(
        adopt_t tag, array_view<U, dynamic_extent> other)
    requires (E != dynamic_extent)
        : instance_{tag, other.instance()}
        , span_{other.size() == E
                  ? span_type{other.span()}
                  : failure<std::invalid_argument, PyExc_ValueError>(
                        details::tuple::size_mismatch_message_v)} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T> U>
    LEV_HIDE_INSTANTIATION explicit inline array(
        array<U, dynamic_extent> const& other)
    requires (E != dynamic_extent)
        : instance_{other.instance_}
        , span_{other.size() == E
                  ? span_type{other.span()}
                  : failure<std::invalid_argument, PyExc_ValueError>(
                        details::tuple::size_mismatch_message_v)} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T> U>
    LEV_HIDE_INSTANTIATION explicit inline array(
        array<U, dynamic_extent>&& other)
    requires (E != dynamic_extent)
        : instance_{std::move(other.instance_)}
        , span_{other.size() == E
                  ? span_type{other.span()}
                  : failure<std::invalid_argument, PyExc_ValueError>(
                        details::tuple::size_mismatch_message_v)} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T>... Us>
    requires (E != dynamic_extent && sizeof...(Us) == E)
    LEV_HIDE_INSTANTIATION explicit inline array(
        adopt_t tag, tuple_view<Us...> other) noexcept
        : instance_{tag, other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T>... Us>
    requires (E != dynamic_extent && sizeof...(Us) == E)
    LEV_HIDE_INSTANTIATION inline array(tuple<Us...> const& other) noexcept
        : instance_{other.instance_}
        , span_{other.span()} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T>... Us>
    requires (E != dynamic_extent && sizeof...(Us) == E)
    LEV_HIDE_INSTANTIATION inline array(tuple<Us...>&& other) noexcept
        : instance_{std::move(other.instance_)}
        , span_{other.span()} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T> U, size_t N>
    requires (E == dynamic_extent)
    LEV_HIDE_INSTANTIATION explicit inline array(
        adopt_t tag, array_view<U, N> other) noexcept
        : instance_{tag, other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(empty() || instance_);
    }

    template <pyobj_derived_from<T> U, size_t N>
    requires (E == dynamic_extent)
    LEV_HIDE_INSTANTIATION inline array(array<U, N> const& other) noexcept
        : instance_{other.instance_}
        , span_{other.span()} {
        LEV_ASSERT(empty() || instance_);
    }

    template <pyobj_derived_from<T> U, size_t N>
    requires (E == dynamic_extent)
    LEV_HIDE_INSTANTIATION inline array(array<U, N>&& other) noexcept
        : instance_{std::move(other.instance_)}
        , span_{other.span()} {
        LEV_ASSERT(empty() || instance_);
    }

    template <pyobj_derived_from<T>... Us>
    requires (E == dynamic_extent)
    LEV_HIDE_INSTANTIATION inline array(tuple<Us...> const& other) noexcept
        : instance_{other.instance_}
        , span_{other.span()} {
        LEV_ASSERT(empty() || instance_);
    }

    template <pyobj_derived_from<T>... Us>
    requires (E == dynamic_extent)
    LEV_HIDE_INSTANTIATION inline array(tuple<Us...>&& other) noexcept
        : instance_{std::move(other.instance_)}
        , span_{other.span()} {
        LEV_ASSERT(empty() || instance_);
    }

    template <pyobj_derived_from<T>... Us>
    requires (E == dynamic_extent)
    LEV_HIDE_INSTANTIATION explicit inline array(
        adopt_t tag, tuple_view<Us...> other) noexcept
        : instance_{tag, other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(empty() || instance_);
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    begin() const LEV_LIFETIMEBOUND {
        return iterator{span_.data()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    end() const LEV_LIFETIMEBOUND {
        return iterator{span_.data() + span_.size()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    cbegin() const LEV_LIFETIMEBOUND {
        return const_iterator{span_.data()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    cend() const LEV_LIFETIMEBOUND {
        return const_iterator{span_.data() + span_.size()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    rbegin() const LEV_LIFETIMEBOUND {
        return reverse_iterator{end()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    rend() const LEV_LIFETIMEBOUND {
        return reverse_iterator{begin()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    crbegin() const LEV_LIFETIMEBOUND {
        return const_reverse_iterator{end()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    crend() const LEV_LIFETIMEBOUND {
        return const_reverse_iterator{begin()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool
    empty() const noexcept {
        return span_.empty();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_t
    size() const noexcept {
        return span_.size();
    }

    LEV_HIDE_INSTANTIATION
    LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<PyTupleObject>
    instance() const noexcept LEV_LIFETIMEBOUND {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr span_type
    span() const noexcept LEV_LIFETIMEBOUND {
        return span_;
    }

    LEV_HIDE_INSTANTIATION
    LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<T> operator[](
        size_t idx) const noexcept {
        return static_ptr_cast<T>(span_[idx]);
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION LEV_PURE
        [[nodiscard]] friend inline constexpr unmanaged_ptr<T>
        get(array const& array) noexcept LEV_LIFETIMEBOUND
    requires (E != dynamic_extent)
    {
        return static_ptr_cast<T>(array.span_[I]);
    }

private:
    python_ptr<PyTupleObject> instance_;
    span_type span_;
};

template <pyobj_type T, size_t E>
class LEV_API array_view {
    using span_type = std::span<PyObject* const, E>;
    static constexpr auto cast_type =
        E == dynamic_extent ? cast_policy::safe : cast_policy::unsafe;

public:
    using value_type = unmanaged_ptr<T>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using const_iterator = random_access_const_iterator<T, cast_type>;
    using iterator = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = std::reverse_iterator<iterator>;

    LEV_HIDE_INSTANTIATION constexpr inline array_view(
        array_view const&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline array_view& operator=(
        array_view const&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline array_view(
        array_view&&) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr inline array_view& operator=(
        array_view&&) noexcept = default;

    LEV_HIDE_INSTANTIATION explicit inline array_view(
        python_ptr<PyTupleObject> const& ptr)
        : instance_{ptr.get()}
        , span_{details::tuple::instance_to_span<T, E>(instance_.get())} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T> U, size_t N>
    requires (E != dynamic_extent && N == E)
    LEV_HIDE_INSTANTIATION explicit inline array_view(
        array_view<U, N> other) noexcept
        : instance_{other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T> U, size_t N>
    requires (E != dynamic_extent && N == E)
    LEV_HIDE_INSTANTIATION explicit inline array_view(
        array<U, N> const& other LEV_LIFETIMEBOUND) noexcept
        : instance_{other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T>... Us>
    requires (E != dynamic_extent && sizeof...(Us) == E)
    LEV_HIDE_INSTANTIATION explicit inline array_view(
        tuple<Us...> const& other LEV_LIFETIMEBOUND) noexcept
        : instance_{other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T>... Us>
    requires (E != dynamic_extent && sizeof...(Us) == E)
    LEV_HIDE_INSTANTIATION explicit inline array_view(
        tuple_view<Us...> other) noexcept
        : instance_{other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T> U>
    LEV_HIDE_INSTANTIATION explicit inline array_view(
        array_view<U, dynamic_extent> other)
    requires (E != dynamic_extent)
        : instance_{other.instance()}
        , span_{other.size() == E
                  ? span_type{other.span()}
                  : failure<std::invalid_argument, PyExc_ValueError>(
                        details::tuple::size_mismatch_message_v)} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T> U>
    LEV_HIDE_INSTANTIATION explicit inline array_view(
        array<U, dynamic_extent> const& other LEV_LIFETIMEBOUND)
    requires (E != dynamic_extent)
        : instance_{other.instance()}
        , span_{other.size() == E
                  ? span_type{other.span()}
                  : failure<std::invalid_argument, PyExc_ValueError>(
                        details::tuple::size_mismatch_message_v)} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T> U, size_t N>
    LEV_HIDE_INSTANTIATION inline array_view(array_view<U, N> other)
    requires (E == dynamic_extent)
        : instance_{other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(instance_);
    }

    template <pyobj_derived_from<T> U, size_t N>
    LEV_HIDE_INSTANTIATION inline array_view(
        array<U, N> const& other LEV_LIFETIMEBOUND) noexcept
    requires (E == dynamic_extent)
        : instance_{other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(empty() || instance_);
    }

    template <pyobj_derived_from<T>... Us>
    LEV_HIDE_INSTANTIATION inline array_view(
        tuple<Us...> const& other LEV_LIFETIMEBOUND) noexcept
    requires (E == dynamic_extent)
        : instance_{other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(empty() || instance_);
    }

    template <pyobj_derived_from<T>... Us>
    LEV_HIDE_INSTANTIATION inline array_view(tuple_view<Us...> other) noexcept
    requires (E == dynamic_extent)
        : instance_{other.instance()}
        , span_{other.span()} {
        LEV_ASSERT(empty() || instance_);
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    begin() const LEV_LIFETIMEBOUND {
        return iterator{span_.data()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    end() const LEV_LIFETIMEBOUND {
        return iterator{span_.data() + span_.size()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    cbegin() const LEV_LIFETIMEBOUND {
        return const_iterator{span_.data()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    cend() const LEV_LIFETIMEBOUND {
        return const_iterator{span_.data() + span_.size()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    rbegin() const LEV_LIFETIMEBOUND {
        return reverse_iterator{end()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    rend() const LEV_LIFETIMEBOUND {
        return reverse_iterator{begin()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    crbegin() const LEV_LIFETIMEBOUND {
        return const_reverse_iterator{end()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto
    crend() const LEV_LIFETIMEBOUND {
        return const_reverse_iterator{begin()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool
    empty() const noexcept {
        return span_.empty();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_t
    size() const noexcept {
        return span_.size();
    }

    LEV_HIDE_INSTANTIATION
    LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<PyTupleObject>
    instance() const noexcept {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline span_type
    span() const noexcept {
        return span_;
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] constexpr unmanaged_ptr<T>
    operator[](size_t idx) const noexcept {
        return static_ptr_cast<T>(span_[idx]);
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] friend unmanaged_ptr<T> get(
        array_view const& view) noexcept
    requires (E != dynamic_extent)
    {
        return static_ptr_cast<T>(view.span_[I]);
    }

private:
    unmanaged_ptr<PyTupleObject> instance_;
    span_type span_;
};
} // namespace py
} // namespace lev

template <lev::pyobj_type... Ts>
struct std::tuple_size<lev::py::tuple<Ts...>> :
    lev::size_constant<sizeof...(Ts)> {};
template <lev::pyobj_type... Ts>
struct std::tuple_size<lev::py::tuple_view<Ts...>> :
    lev::size_constant<sizeof...(Ts)> {};

template <lev::pyobj_type T, size_t N>
requires (N != lev::dynamic_extent)
struct std::tuple_size<lev::py::array<T, N>> : lev::size_constant<N> {};

template <lev::pyobj_type T, size_t N>
requires (N != lev::dynamic_extent)
struct std::tuple_size<lev::py::array_view<T, N>> : lev::size_constant<N> {};

template <size_t I, lev::pyobj_type... Ts>
struct std::tuple_element<I, lev::py::tuple<Ts...>> {
    using type = lev::template_element_t<I, lev::py::tuple<Ts...>>;
};

template <size_t I, lev::pyobj_type... Ts>
struct std::tuple_element<I, lev::py::tuple_view<Ts...>> {
    using type = lev::template_element_t<I, lev::py::tuple<Ts...>>;
};

template <size_t I, lev::pyobj_type T, size_t N>
requires (N != lev::dynamic_extent)
struct std::tuple_element<I, lev::py::array<T, N>> {
    using type = unmanaged_ptr<T>;
};

template <lev::pyobj_type T, size_t N>
requires (N != lev::dynamic_extent)
struct std::tuple_element<I, lev::py::array_view<T, N>> {
    using type = unmanaged_ptr<T>;
};
