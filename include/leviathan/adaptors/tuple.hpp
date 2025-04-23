// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/common.hpp"
#include "leviathan/adaptors/random_access_iterator.hpp"
#include "leviathan/adaptos/container_options.hpp"
#include "leviathan/pointer.hpp"
#include "leviathan/utility.hpp"

#include <array>
#include <compare>
#include <iterator>
#include <ranges>
#include <stdexcept>

namespace lev {
template <>
LEV_HIDDEN inline constexpr PyTypeObject*
type_object<PyTupleObject>() noexcept {
    return &PyTuple_Type;
}

namespace py {

namespace details::aggregate {
LEV_HIDDEN constexpr char const* null_instance_message_v =
    "Tuple instance is null";
LEV_HIDDEN constexpr char const* size_mismatch_message_v =
    "Tuple-Adaptor size mismatch";
LEV_HIDDEN constexpr char const* invalid_type_message_v =
    "Tuple-Adaptor element type mismatch";

LEV_HIDDEN std::span<PyObject* const> items(unmanaged_ptr<PyTupleObject> tuple,
    size_t size) noexcept LEV_CONTRACT_PRE(tuple) LEV_CONTRACT_PRE(size > 0) {
    // In C++, performing index operations on ob_items for indices > 0 is UB
    LEV_ASSERT(tuple);
    LEV_ASSERT(size > 0);
    // The following is a poor man's std::start_lifetime_as_array to "reboot"
    // the lifetime of the array because PyTupleObject defines it as a
    // PyObject*[1] member

#if __cpp_lib_start_lifetime_as >= 202207L
    // TODO: figure out how to avoid including <memory>
    auto ptr = std::start_lifetime_as_array<PyObject*>(
        static_cast<PyObject**>(tuple->ob_item), size);
    return std::span<PyObject* const>{ptr, ptr + size};
#else
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
    for (auto ptr : span) {}
#endif

    return span;
}

template <size_t E>
LEV_HIDDEN inline constexpr auto default_flag() noexcept {
    return container_flags::nothrow;
}

template <>
LEV_HIDDEN constexpr auto default_flag<dynamic_extent>() noexcept {
    return container_flags::none;
}

} // namespace details::aggregate

template <identifiable_pyobj_type... Ts>
class LEV_API tuple_policy {
public:
    static constexpr size_t extent = sizeof...(Ts);

protected:
    using span_type = std::span<PyObject* const, sizeof...(Ts)>;

    LEV_HIDE_INSTANTIATION constexpr ~tuple_policy() noexcept = default;

    LEV_HIDE_INSTANTIATION static std::span<PyObject* const, sizeof...(Ts)> to_span(
        unmanaged_ptr<PyTupleObject> ptr) {

        static constexpr auto E = sizeof...(Ts);

        if constexpr (E > 0) {
            if (!ptr) {
                failure<std::invalid_argument, PyExc_RuntimeError>(
                    details::aggregate::null_instance_message_v);
            }
        }

        auto const size = ptr ? static_cast<size_t>(PyTuple_GET_SIZE(ptr)) : 0;
        if (size != E) {
            failure<std::invalid_argument, PyExc_ValueError>(
                details::aggregate::size_mismatch_message_v);
        }

        if constexpr (E == 0) {
            if (size == 0) {
                return span_type{
                    static_cast<PyObject* const*>(ptr->ob_item), 0};
            }
        }

        auto items = details::aggregate::items(ptr, size);
        auto const types_valid = [&]<size_t... Is>(std::index_sequence<Is...>) {
            return (... && __LEV dynamic_ptr_cast<Ts>(items[Is]));
        }(std::make_index_sequence<E>{});

        if (!types_valid) {
            failure<type_error, PyExc_TypeError>(
                details::aggregate::invalid_type_message_v);
        }

        return span_type{items};
    }

    template <typename... Us>
    LEV_HIDE_INSTANTIATION static constexpr bool is_pyobject_base_of_v =
        (... && pyobj_derived_from<Us, Ts>);
    template <typename U>
    LEV_HIDE_INSTANTIATION static constexpr bool is_all_pyobject_base_of_v =
        (... && pyobj_derived_from<U, Ts>);
    template <size_t I>
    using element_t = template_element_t<I, tuple_policy>;
    template <typename U>
    LEV_HIDE_INSTANTIATION static constexpr size_t count_v = template_count_v<U, tuple_policy>;
    template <typename U>
    LEV_HIDE_INSTANTIATION static constexpr size_t index_v = template_index_v<U, tuple_policy>;
};

template <identifiable_pyobj_type T, size_t E = dynamic_extent>
class LEV_API array_policy {
public:
    using value_type = unmanaged_ptr<T>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using const_iterator = random_access_const_iterator<T, cast_type>;
    using iterator = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    static constexpr size_t extent = E;

protected:
    using span_type = std::span<PyObject* const, E>;
    using element_type = T;

    LEV_HIDE_INSTANTIATION constexpr ~array_policy() noexcept = default;

    LEV_HIDE_INSTANTIATION static std::span<PyObject* const, E> to_span(
        unmanaged_ptr<PyTupleObject> ptr) {
        auto const size = ptr ? static_cast<size_t>(PyTuple_GET_SIZE(ptr)) : 0;
        if constexpr (E != dynamic_extent) {
            if (size != E) {
                failure<std::invalid_argument, PyExc_ValueError>(
                    details::aggregate::size_mismatch_message_v);
            }
        }

        if constexpr (E == 0 || E == dynamic_extent) {
            if (size == 0) {
                return span_type{};
            }
        }

        if constexpr (E > 0) {
            if (!ptr) {
                failure<std::invalid_argument, PyExc_RuntimeError>(
                    details::aggregate::null_instance_message_v);
            }
        }

        auto items = details::aggregate::items(ptr, size);

        if constexpr (E != dynamic_extent) {
            auto const types_valid = [&]() {
                return [&]<size_t... Is>(std::index_sequence<Is...>) {
                    return (... && __LEV dynamic_ptr_cast<T>(items[Is]));
                }(std::make_index_sequence<E>{});
            }();

            if (!types_valid) {
                failure<type_error, PyExc_TypeError>(
                    details::aggregate::invalid_type_message_v);
            }
        }

        return span_type{items};
    }

    template <typename... Us>
    LEV_HIDE_INSTANTIATION static constexpr bool is_pyobject_base_of_v =
        (... && pyobj_derived_from<Us, T>);

    template <typename U>
    LEV_HIDE_INSTANTIATION static constexpr bool is_all_pyobject_base_of_v =
        pyobj_derived_from<U, T>;
};

template <typename Desc, auto P = container_flags::none>
class LEV_API basic_aggregate;

template <pyobj_type... Ts>
using aggregate = basic_aggregate<tuple_policy<Ts...>,
    container_flags::readonly | container_flags::nothrow>;
template <pyobj_type... Ts>
using readonly_aggregate = aggregate<Ts...>;

template <pyobj_type T, size_t N = dynamic_extent, auto P = default_flag<N>()>
using array =
    basic_aggregate<array_policy<T, N>, container_flags::readonly | P>;

template <pyobj_type T, size_t N = dynamic_extent, auto P = default_flag<N>()>
using readonly_array = array<T, N, P>;

using tuple = array<PyObject>;
using readonly_tuple = array<PyObject>;

namespace borrowed {
template <pyobj_type... Ts>
using aggregate = basic_aggregate<tuple_policy<Ts...>,
    container_flags::borrowed | container_flags::readonly |
        container_flags::nothrow>;
template <pyobj_type... Ts>
using readonly_aggregate = tuple<Ts...>;

template <pyobj_type T, size_t N = dynamic_extent, auto P = default_flag<N>()>
using array = basic_aggregate<array_policy<T, N>,
    container_flags::borrowed | container_flags::readonly | P>;

template <pyobj_type T, size_t N = dynamic_extent, auto P = default_flag<N>()>
using readonly_array = array<T, N, P>;

using tuple = array<PyObject>;
using readonly_tuple = array<PyObject>;
} // namespace borrowed

namespace details::aggregate {
template <typename>
LEV_HIDDEN inline constexpr bool is_aggregate_policy_v = false;
template <typename>
LEV_HIDDEN inline constexpr bool is_tuple_policy_v = false;
template <typename>
LEV_HIDDEN inline constexpr bool is_array_policy_v = false;

template <pyobj_type... Ts>
LEV_HIDDEN inline constexpr bool is_aggregate_policy_v<tuple_policy<Ts...>> =
    true;
template <pyobj_type... Ts>
LEV_HIDDEN inline constexpr bool is_tuple_policy_v<tuple_policy<Ts...>> = true;

template <pyobj_type T, size_t E>
LEV_HIDDEN inline constexpr bool is_aggregate_policy_v<array_policy<T, E>> =
    true;
template <pyobj_type T, size_t E>
LEV_HIDDEN inline constexpr bool is_array_policy_v<array_policy<T, E>> = true;
} // namespace details::aggregate

template <typename T>
concept aggregate_policy = details::aggregate::is_aggregate_policy_v<T>;

template <aggregate_policy D, container_flags_type auto P>
class LEV_API basic_aggregate<D, P> : private adaptor_base, public D {
    using flags_type = std::remove_cv_t<decltype(P)>;
    using borrowed_flag = container_flags::borrowed_t;
    using nothrow_flag = container_flags::nothrow_t;
    using readonly_flag = container_flags::readonly_t;
    using instance_type =
        std::conditional_t<with_container_flags<flags_type, borrowed_flag>,
            unmanaged_ptr<PyDictObject>, python_ptr<PyDictObject>>;
    LEV_HIDE_INSTANTIATION static constexpr bool is_nothrow_v =
        with_container_flags<flags_type, nothrow_flag>;

    using descriptor_type = D;
    using typename descriptor_type::span_type;
    static_assert(descriptor_type::extent == dynamic_extent
            ? with_container_flags<flags_type, readonly_flag>
            : with_container_flags<flags_type, readonly_flag, nothrow_flag>,
        "Invalid container flags");

    template <typename, auto>
    friend class basic_aggregate;
    struct private_tag_t {};

    LEV_HIDE_INSTANTIATION static constexpr private_tag_t private_tag{};

    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        private_tag_t tag [[maybe_unused]], auto&& ptr, span_type span) noexcept
    requires (span_type::extent == dynamic_extent &&
                 descriptor_type::extent != dynamic_extent)
    LEV_CONTRACT_PRE(span.empty() || ptr)
        LEV_CONTRACT_PRE(span.size() == descriptor_type::extent) :
        instance_{std::forward<decltype(ptr)>(ptr)},
        span_{span} {
        LEV_ASSERT(span_.empty() || instance_);
        LEV_ASSERT(span_.size() == descriptor_type::extent);
    }

    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(private_tag_t tag [[maybe_unused]],
        auto&& ptr, span_type span) noexcept
        LEV_CONTRACT_PRE(span.empty() || ptr) :
        instance_{std::forward<decltype(ptr)>(ptr)},
        span_{span} {
        LEV_ASSERT(span_.empty() || instance_);
    }

public:
    using type = PyTupleObject;

    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(decltype(nullptr)) noexcept = delete;
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        basic_aggregate const&) noexcept = default;
    LEV_HIDE_INSTANTIATION LEV_REINITIALIZES inline constexpr basic_aggregate& operator=(
        basic_aggregate const&) noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        basic_aggregate&&) noexcept = default;
    LEV_HIDE_INSTANTIATION LEV_REINITIALIZES inline constexpr basic_aggregate& operator=(
        basic_aggregate&&) noexcept = default;

    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(python_ptr<PyTupleObject>&& ptr)
    requires without_container_flags<flags_type, borrowed_flag>
        : basic_aggregate{[&]() {
            auto const span = descriptor_type::to_span(ptr.get());
            return basic_aggregate{private_tag, std::move(ptr), span};
        }()} {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(python_ptr<PyTupleObject> const& ptr)
        : basic_aggregate{private_tag, python_ptr<PyTupleObject>(ptr),
              descriptor_type::to_span(ptr.get())} {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        python_ptr<PyTupleObject> const& ptr LEV_LIFETIMEBOUND)
    requires with_container_flags<flags_type, borrowed_flag>
        : basic_aggregate{
              private_tag, ptr.get(), descriptor_type::to_span(ptr.get())} {}

    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_aggregate(
        unmanaged_ptr<PyTupleObject> ptr)
        : basic_aggregate{__LEV adopt(ptr)} {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(unmanaged_ptr<PyTupleObject> ptr)
    requires with_container_flags<flags_type, borrowed_flag>
        : basic_aggregate(private_tag, ptr, descriptor_type::to_span(ptr)) {}

    /**
     * Construct owned tuple from tuple
     */

    template <pyobj_type... Us>
    requires requires {
        requires without_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::template is_pyobject_base_of_v<Us...>;
        requires (descriptor_type::extent == sizeof...(Us) ||
            descriptor_type::extent == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_aggregate(
        borrowed::aggregate<Us...> other) noexcept
        : basic_aggregate{
              private_tag, __LEV adopt(other.instance()), other.span()} {}

    template <pyobj_type... Us>
    requires requires {
        requires without_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::template is_pyobject_base_of_v<Us...>;
        requires (descriptor_type::extent == sizeof...(Us) ||
            descriptor_type::extent == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        aggregate<Us...> const& other) noexcept
        : basic_aggregate{private_tag, other.instance_, other.span()} {}

    template <pyobj_type... Us>
    requires requires {
        requires without_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::template is_pyobject_base_of_v<Us...>;
        requires (descriptor_type::extent == sizeof...(Us) ||
            descriptor_type::extent == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(aggregate<Us...>&& other) noexcept
        : basic_aggregate{
              private_tag, std::move(other.instance_), other.span()} {}

    /**
     * Construct owned tuple from array
     */

    template <pyobj_type U, size_t E,
        container_flags_convertible_to<flags_type> auto Opt>
    requires requires {
        requires details::container::acquisition<decltype(Opt), flags_type,
            borrowed_flag>;
        requires descriptor_type::template is_all_pyobject_base_of_v<U>;
        requires descriptor_type::extent == E || E == dynamic_extent;
    }
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_aggregate(
        basic_aggregate<array_policy<U, E>, Opt> other) noexcept
        : basic_aggregate{
              private_tag, __LEV adopt(other.instance()), other.span()} {}

    template <pyobj_type U, size_t E,
        container_flags_convertible_to<flags_type> auto Opt>
    requires requires {
        requires without_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::template is_all_pyobject_base_of_v<U>;
        requires descriptor_type::extent == E || E == dynamic_extent;
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        basic_aggregate<array_policy<U, E>, Opt> const& other) noexcept
        : basic_aggregate{private_tag, other.instance_, other.span()} {}

    template <pyobj_type U, size_t E,
        container_flags_convertible_to<flags_type> auto Opt>
    requires requires {
        requires without_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::template is_all_pyobject_base_of_v<U>;
        requires descriptor_type::extent == E || E == dynamic_extent;
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        basic_aggregate<array_policy<U, E>, Opt>&& other) noexcept
        : basic_aggregate{
              private_tag, std::move(other.instance_), other.span()} {}

    /**
     * Construct borrowed tuple from tuple
     */

    template <pyobj_type... Us>
    requires requires {
        requires with_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::template is_pyobject_base_of_v<Us...>;
        requires (descriptor_type::extent == sizeof...(Us) ||
            descriptor_type::extent == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        aggregate<Us...> const& other LEV_LIFETIMEBOUND) noexcept
        : basic_aggregate{private_tag, other.instance(), other.span()} {}

    template <pyobj_type... Us>
    requires requires {
        requires with_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::template is_pyobject_base_of_v<Us...>;
        requires (descriptor_type::extent == sizeof...(Us) ||
            descriptor_type::extent == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        borrowed::aggregate<Us...> other) noexcept
        : basic_aggregate{private_tag, other.instance(), other.span()} {}

    /**
     * Construct borrowed tuple from array
     */

    template <pyobj_type U, size_t E,
        container_flags_convertible_to<flags_type> auto Opt>
    requires requires {
        requires details::container::removal<decltype(Opt), flags_type,
            borrowed_flag>;
        requires descriptor_type::template is_all_pyobject_base_of_v<U>;
        requires descriptor_type::extent == E || E == dynamic_extent;
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        basic_aggregate<array_policy<U, E>, Opt> const& other
            LEV_LIFETIMEBOUND) noexcept
        : basic_aggregate{private_tag, other.instance(), other.span()} {}

    template <pyobj_type U, size_t E,
        container_flags_convertible_to<flags_type> auto Opt>
    requires requires {
        requires details::container::retention<decltype(Opt), flags_type,
            borrowed_flag>;
        requires descriptor_type::template is_all_pyobject_base_of_v<U>;
        requires descriptor_type::extent == E || E == dynamic_extent;
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        basic_aggregate<array_policy<U, E>, Opt> other) noexcept
        : basic_aggregate{private_tag, other.instance(), other.span()} {}

    /**
     * Construct owned array from array
     */

    template <pyobj_type U, size_t E,
        container_flags_convertible_to<flags_type> auto Opt>
    requires requires {
        requires details::container::acquisition<decltype(Opt), flags_type,
            borrowed_flag>;
        requires descriptor_type::template is_all_pyobject_base_of_v<U>;
        requires (descriptor_type::extent == E ||
            descriptor_type::extent == dynamic_extent || E == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_aggregate(
        basic_aggregate<array_policy<U, E>, Opt> other) noexcept
        : basic_aggregate{
              private_tag, __LEV adopt(other.instance()), other.span()} {}

    template <pyobj_type U, size_t E,
        container_flags_convertible_to<flags_type> auto Opt>
    requires requires {
        requires details::container::exclusion<decltype(Opt), flags_type,
            borrowed_flag>;
        requires descriptor_type::template is_all_pyobject_base_of_v<U>;
        requires (descriptor_type::extent == E ||
            descriptor_type::extent == dynamic_extent || E == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        basic_aggregate<array_policy<U, E>, Opt> const& other) noexcept
        : basic_aggregate{private_tag, other.instance_, other.span()} {}

    template <pyobj_type U, size_t E,
        container_flags_convertible_to<flags_type> auto Opt>
    requires requires {
        requires details::container::exclusion<decltype(Opt), flags_type,
            borrowed_flag>;
        requires descriptor_type::template is_all_pyobject_base_of_v<U>;
        requires (descriptor_type::extent == E ||
            descriptor_type::extent == dynamic_extent || E == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        basic_aggregate<array_policy<U, E>, Opt>&& other) noexcept
        : basic_aggregate{
              private_tag, std::move(other.instance_), other.span()} {}

    /**
     * Construct owned array from tuple
     */

    template <pyobj_type... Us>
    requires requires {
        requires without_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::template is_pyobject_base_of_v<Us...>;
        requires (descriptor_type::extent == sizeof...(Us) ||
            descriptor_type::extent == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_aggregate(
        borrowed::aggregate<Us...> other) noexcept
        : basic_aggregate{
              private_tag, __LEV adopt(other.instance()), other.span()} {}

    template <pyobj_type... Us>
    requires requires {
        requires without_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::template is_pyobject_base_of_v<Us...>;
        requires (descriptor_type::extent == sizeof...(Us) ||
            descriptor_type::extent == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        aggregate<Us...> const& other) noexcept
        : basic_aggregate{private_tag, other.instance_, other.span()} {}

    template <pyobj_type... Us>
    requires requires {
        requires without_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::template is_pyobject_base_of_v<Us...>;
        requires (descriptor_type::extent == sizeof...(Us) ||
            descriptor_type::extent == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(aggregate<Us...>&& other) noexcept
        : basic_aggregate{
              private_tag, std::move(other.instance_), other.span()} {}

    /**
     * Construct borrowed array from array
     */

    template <pyobj_type U, size_t E,
        container_flags_convertible_to<flags_type> auto Opt>
    requires requires {
        requires details::container::acquisition<decltype(Opt), flags_type,
            borrowed_flag>;
        requires descriptor_type::template is_all_pyobject_base_of_v<U>;
        requires (descriptor_type::extent == E ||
            descriptor_type::extent == dynamic_extent || E == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        basic_aggregate<array_policy<U, E>, Opt> const& other
            LEV_LIFETIMEBOUND) noexcept
        : basic_aggregate{private_tag, other.instance(), other.span()} {}

    template <pyobj_type U, size_t E,
        container_flags_convertible_to<flags_type> auto Opt>
    requires requires {
        requires details::container::retention<decltype(Opt), flags_type,
            borrowed_flag>;
        requires descriptor_type::template is_all_pyobject_base_of_v<U>;
        requires (descriptor_type::extent == E ||
            descriptor_type::extent == dynamic_extent || E == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_aggregate(
        basic_aggregate<array_policy<U, E>, Opt> other) noexcept
        : basic_aggregate{private_tag, other.instance(), other.span()} {}

    /**
     * Construct borrowed array from tuple
     */
    template <pyobj_type... Us>
    requires requires {
        requires with_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::template is_pyobject_base_of_v<Us...>;
        requires (descriptor_type::extent == sizeof...(Us) ||
            descriptor_type::extent == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_aggregate(
        borrowed::aggregate<Us...> other) noexcept
        : basic_aggregate{private_tag, other.instance(), other.span()} {}

    template <pyobj_type... Us>
    requires requires {
        requires with_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::template is_pyobject_base_of_v<Us...>;
        requires (descriptor_type::extent == sizeof...(Us) ||
            descriptor_type::extent == dynamic_extent);
    }
    LEV_HIDE_INSTANTIATION inline constexpr basic_aggregate(
        aggregate<Us...> const& other LEV_LIFETIMEBOUND) noexcept
        : basic_aggregate{private_tag, other.instance(), other.span()} {}

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool empty() const noexcept {
        return size() == 0;
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_t size() const noexcept {
        if constexpr (without_container_flags<flags_type, borrowed_flag>) {
            // move operation can clear the instance but leave the span
            return instance_ ? span_.size() : 0u;
        } else {
            return span_.size();
        }
    }

    LEV_HIDE_INSTANTIATION
        LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<PyTupleObject>
    instance() const noexcept LEV_LIFETIMEBOUND {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION
        LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<PyTupleObject>
    instance() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr span_type span() const noexcept LEV_LIFETIMEBOUND
        LEV_CONTRACT_PRE(span_.empty() || instance_) {
        return span_;
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr span_type span() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    LEV_CONTRACT_PRE(span_.empty() || instance_) {
        return span_;
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] friend auto get(
        basic_aggregate const& tuple LEV_LIFETIMEBOUND) noexcept
    requires requires {
        requires without_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::extent != dynamic_extent;
    }
    {
        if constexpr (is_array_policy_v<descriptor_type>) {
            return __LEV static_ptr_cast<
                typename descriptor_type::element_type>(tuple.span_[I]);
        } else {
            return __LEV static_ptr_cast<
                typename descriptor_type::element_t<I>>(tuple.span_[I]);
        }
    }

    template <pyobj_type U>
    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] friend unmanaged_ptr<U> get(
        basic_aggregate const& tuple LEV_LIFETIMEBOUND) noexcept
    requires requires {
        requires without_container_flags<flags_type, borrowed_flag>;
        requires details::aggregate::is_tuple_policy_v<descriptor_type>;
        requires descriptor_type::template count_v<U> == 1;
    }
    {
        return __LEV static_ptr_cast<U>(
            tuple.span_[descriptor_type::template index_v<U>]);
    }

    template <size_t I>
    LEV_HIDE_INSTANTIATION
        LEV_PURE [[nodiscard]] friend void get(basic_aggregate&& tuple) noexcept
    requires requires {
        requires without_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::extent != dynamic_extent;
    }
    = delete;

    template <pyobj_type U>
    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] friend unmanaged_ptr<U> get(
        basic_aggregate&& tuple) noexcept
    requires requires {
        requires without_container_flags<flags_type, borrowed_flag>;
        requires details::aggregate::is_tuple_policy_v<descriptor_type>;
    }
    = delete;

    template <size_t I>
    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] friend auto get(
        basic_aggregate const& tuple) noexcept
    requires requires {
        requires with_container_flags<flags_type, borrowed_flag>;
        requires descriptor_type::extent != dynamic_extent;
    }
    {
        if constexpr (is_array_policy_v<descriptor_type>) {
            return __LEV static_ptr_cast<
                typename descriptor_type::element_type>(tuple.span_[I]);
        } else {
            return __LEV static_ptr_cast<
                typename descriptor_type::element_t<I>>(tuple.span_[I]);
        }
    }

    template <pyobj_type U>
    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] friend unmanaged_ptr<U> get(
        basic_aggregate const& tuple) noexcept
    requires requires {
        requires with_container_flags<flags_type, borrowed_flag>;
        requires details::aggregate::is_tuple_policy_v<descriptor_type>;
        requires descriptor_type::template count_v<U> == 1;
    }
    {
        return __LEV static_ptr_cast<U>(
            tuple.span_[descriptor_type::template index_v<U>]);
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto operator[](
        size_t idx) const noexcept
    requires details::aggregate::is_array_policy_v<descriptor_type>
    LEV_CONTRACT_PRE(idx < this->size()) {
        LEV_ASSERT(idx < this->size());
        if constexpr (descriptor_type::extent != dynamic_extent) {
            return __LEV static_ptr_cast<
                typename descriptor_type::element_type>(span_[idx]);
        } else {
            return __LEV dynamic_ptr_cast<
                typename descriptor_type::element_type>(span_[idx]);
        }
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr auto at(size_t idx) const
        noexcept(is_nothrow_v)
    requires details::aggregate::is_array_policy_v<descriptor_type>
    LEV_CONTRACT_PRE(idx < this->size()) {
        LEV_ASSERT(idx < this->size());
        if constexpr (descriptor_type::extent != dynamic_extent) {
            return __LEV static_ptr_cast<
                typename descriptor_type::element_type>(span_[idx]);
        } else if (auto ptr = __LEV dynamic_ptr_cast<
                       typename descriptor_type::element_type>(span_[idx])) {
            return ptr;
        } else if constexpr (is_nothrow_v) {
            return ptr;
        } else if (span_[idx] == nullptr) {
            return ptr;
        } else {
            failure<type_error, PyExc_TypeError>(
                format_cstring("The associated value does not have the "
                               "expected type, Expected=[%s], Retrieved=[%s]",
                    type_object<T>()->tp_name, Py_TYPE(span_[idx])->tp_name)
                    .data());
        }
    }

    LEV_HIDE_INSTANTIATION friend void swap(
        basic_aggregate& lhs, basic_aggregate& rhs) noexcept {
        lhs.swap(other);
    }

    LEV_HIDE_INSTANTIATION void swap(basic_aggregate& other) noexcept {
        instance_.swap(other.instance_);
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto begin() const LEV_LIFETIMEBOUND
    requires details::aggregate::is_array_policy_v<descriptor_type>
    {
        return typename descriptor_type::iterator{span_.data()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto end() const LEV_LIFETIMEBOUND
    requires details::aggregate::is_array_policy_v<descriptor_type>
    {
        return typename descriptor_type::iterator{span_.data() + span_.size()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto cbegin() const LEV_LIFETIMEBOUND
    requires details::aggregate::is_array_policy_v<descriptor_type>
    {
        return typename descriptor_type::const_iterator{span_.data()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto cend() const LEV_LIFETIMEBOUND
    requires details::aggregate::is_array_policy_v<descriptor_type>
    {
        return typename descriptor_type::const_iterator{
            span_.data() + span_.size()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto rbegin() const LEV_LIFETIMEBOUND
    requires details::aggregate::is_array_policy_v<descriptor_type>
    {
        return typename descriptor_type::reverse_iterator{end()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto rend() const LEV_LIFETIMEBOUND
    requires details::aggregate::is_array_policy_v<descriptor_type>
    {
        return typename descriptor_type::reverse_iterator{begin()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto crbegin() const LEV_LIFETIMEBOUND
    requires details::aggregate::is_array_policy_v<descriptor_type>
    {
        return typename descriptor_type::const_reverse_iterator{end()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto crend() const LEV_LIFETIMEBOUND
    requires details::aggregate::is_array_policy_v<descriptor_type>
    {
        return typename descriptor_type::const_reverse_iterator{begin()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto begin() const
    requires details::aggregate::is_array_policy_v<descriptor_type> &&
        with_container_flags<flags_type, borrowed_flag>
    {
        return typename descriptor_type::iterator{span_.data()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto end() const
    requires details::aggregate::is_array_policy_v<descriptor_type> &&
        with_container_flags<flags_type, borrowed_flag>
    {
        return typename descriptor_type::iterator{span_.data() + span_.size()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto cbegin() const
    requires details::aggregate::is_array_policy_v<descriptor_type> &&
        with_container_flags<flags_type, borrowed_flag>
    {
        return typename descriptor_type::const_iterator{span_.data()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto cend() const
    requires details::aggregate::is_array_policy_v<descriptor_type> &&
        with_container_flags<flags_type, borrowed_flag>
    {
        return typename descriptor_type::const_iterator{
            span_.data() + span_.size()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto rbegin() const
    requires details::aggregate::is_array_policy_v<descriptor_type> &&
        with_container_flags<flags_type, borrowed_flag>
    {
        return typename descriptor_type::reverse_iterator{end()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto rend() const
    requires details::aggregate::is_array_policy_v<descriptor_type> &&
        with_container_flags<flags_type, borrowed_flag>
    {
        return typename descriptor_type::reverse_iterator{begin()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto crbegin() const
    requires details::aggregate::is_array_policy_v<descriptor_type> &&
        with_container_flags<flags_type, borrowed_flag>
    {
        return typename descriptor_type::const_reverse_iterator{end()};
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline auto crend() const
    requires details::aggregate::is_array_policy_v<descriptor_type> &&
        with_container_flags<flags_type, borrowed_flag>
    {
        return typename descriptor_type::const_reverse_iterator{begin()};
    }

private:
    instance_type instance_;
    span_type span_;
};

} // namespace py
} // namespace lev

template <__LEV pyobj_type... Ts>
struct std::tuple_size<__LEV py::aggregate<Ts...>> :
    __LTL size_constant<sizeof...(Ts)> {};
template <__LEV pyobj_type... Ts>
struct std::tuple_size<__LEV py::borrowed::aggregate<Ts...>> :
    __LTL size_constant<sizeof...(Ts)> {};

template <__LEV pyobj_type T, size_t N>
requires (N != __LEV dynamic_extent)
struct std::tuple_size<__LEV py::array<T, N>> : __LEV size_constant<N> {};

template <__LEV pyobj_type T, size_t N>
requires (N != __LEV dynamic_extent)
struct std::tuple_size<__LEV py::borrowed::array<T, N>> :
    __LTL size_constant<N> {};

template <size_t I, __LEV pyobj_type... Ts>
struct std::tuple_element<I, __LEV py::tuple<Ts...>> {
    using type = __LEV template_element_t<I, __LEV py::aggregate<Ts...>>;
};

template <size_t I, __LEV pyobj_type... Ts>
struct std::tuple_element<I, __LEV py::borrowed_tuple<Ts...>> {
    using type = __LEV template_element_t<I, __LEV py::aggregate<Ts...>>;
};

template <size_t I, __LEV pyobj_type T, size_t N>
requires (N != __LEV dynamic_extent)
struct std::tuple_element<I, __LEV py::array<T, N>> {
    using type = unmanaged_ptr<T>;
};

template <__LEV pyobj_type T, size_t N>
requires (N != __LEV dynamic_extent)
struct std::tuple_element<I, __LEV py::borrowed::array<T, N>> {
    using type = unmanaged_ptr<T>;
};

template <pyobj_type T>
inline constexpr bool
    std::ranges::enable_borrowed_range<__LEV py::borrowed::array<T>> = true;
