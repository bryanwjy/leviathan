// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/common.h"
#include "utils/zstring_view.h"

#include <cstring>

namespace lev {

template <>
inline constexpr PyTypeObject* type_object<PyUnicodeObject>() noexcept {
    return &PyUnicode_Type;
}

template <>
inline constexpr PyTypeObject* type_object<PyASCIIObject>() noexcept {
    return &PyUnicode_Type;
}

namespace py {

template <typename C, auto P = container_flags::none>
class basic_unicode_t;

template <typename C, auto P = container_flags::none>
using basic_unicode = basic_unicode_t<C, container_flags::readonly | P>;

namespace details::unicode {

template <typename Char>
LEV_HIDDEN inline __LTL basic_zstring_view<Char> start_lifetime(
    void const* ptr, size_t length) noexcept {
    if constexpr (std::is_same_v<Char, char>) {
        // char can alias anything, no need to start lifetime
        return __LTL zstring_view{reinterpret_cast<char const*>(ptr), length};
    } else {
        auto const size = length + 1;
        using view_type = __LTL basic_zstring_view<Char>;
#if __cpp_lib_start_lifetime_as >= 202207L
        // TODO: figure out how to avoid including <memory>
        return view_type{std::start_lifetime_as_array<Char>(ptr, size), length};
#else
        auto unconst = const_cast<void*>(ptr);
        // optimized turn the following lines into no-op on optimized builds
        Char const* str = reinterpret_cast<Char const*>(
            memmove(unconst, unconst, size * sizeof(Char)));

        view_type view{str, length};
        for (auto c : view) {}
        return view;
#endif
    }
}
} // namespace details::unicode

using unicode = basic_unicode<void>;
using string = basic_unicode<char, container_flags::nothrow>;
using wstring = basic_unicode<wchar_t, container_flags::nothrow>;
using u8string = basic_unicode<char8_t, container_flags::nothrow>;
using u16string = basic_unicode<char16_t, container_flags::nothrow>;
using u32string = basic_unicode<char32_t, container_flags::nothrow>;

namespace borrowed {
template <typename C, auto P = container_flags::none>
using basic_unicode = basic_unicode_t<C,
    container_flags::readonly | container_flags::borrowed | P>;

using unicode = basic_unicode<void>;
using string = basic_unicode<char, container_flags::nothrow>;
using wstring = basic_unicode<wchar_t, container_flags::nothrow>;
using u8string = basic_unicode<char8_t, container_flags::nothrow>;
using u16string = basic_unicode<char16_t, container_flags::nothrow>;
using u32string = basic_unicode<char32_t, container_flags::nothrow>;
} // namespace borrowed

/**
 * Due to C++'s strict aliasing rules, it is undefined behaviour for the same
 * PyUnicodeObject instance to have multiple adaptors and string views of
 * different character types, except for combinations involving `char`, which
 * is permitted to alias with any type.
 *
 * For example, on platforms where `sizeof(wchar_t) == sizeof(char16_t)`,
 * instantiating both `wstring` and `u16string` adaptors for the same
 * PyUnicodeObject is undefined behaviour. The same applies to `u32string` and
 * `wstring` on platforms where `sizeof(wchar_t) == sizeof(char32_t)`.
 *
 * The type-erased `unicode` adaptor may coexist with strict adaptors, but
 * care must be taken when using `unicode::view<T>()`. For a given
 * PyUnicodeObject instance, it is undefined behaviour to invoke `view<T>()`,
 * where `T` is a valid character type, if there exists another live view of
 * character type `U` (created via either a strict or type-erased adaptor) where
 * `T != U`, unless one of the types is `char`.
 */
template <typename C, container_flags_type auto P>
requires std::is_void_v<C> || __LTL is_complete_v<__LTL basic_zstring_view<C>>
class basic_unicode_t<C, P> {
    template <typename, auto>
    friend class basic_unicode_t;
    static_assert(with_container_flags<flags_type, readonly_flag>,
        "Python strings must be immutable");
    using char_type = C;
    using flags_type = std::remove_cv_t<decltype(P)>;
    using borrowed_flag = container_flags::borrowed_t;
    using nothrow_flag = container_flags::nothrow_t;
    using readonly_flag = container_flags::readonly_t;
    LEV_HIDE_INSTANTIATION static constexpr bool is_nothrow_v =
        with_container_flags<flags_type, nothrow_flag>;
    using instance_type =
        std::conditional_t<with_container_flags<flags_type, borrowed_flag>,
            unmanaged_ptr<PyUnicodeObject>, python_ptr<PyUnicodeObject>>;
    using view_type = __LTL basic_zstring_view<char_type>;

    struct private_tag_t {};

    LEV_HIDE_INSTANTIATION static constexpr private_tag_t private_tag{};

    LEV_HIDE_INSTANTIATION inline constexpr basic_unicode_t(
        private_tag_t tag [[maybe_unused]], auto&& ptr) noexcept
    LEV_CONTRACT_PRE(ptr) :
        instance_{std::forward<decltype(ptr)>(ptr)} {
        LEV_ASSERT(instance_);
        if constexpr (!std::is_void_v<char_type>) {
            auto const ascii = static_ptr_cast<PyASCIIObject>(instance());
#if LEV_PYTHON_VERSION_LT(3, 12, 0)
            if (ascii->state.kind == PyUnicode_Kind::PyUnicode_WCHAR_KIND &&
                std::is_same_v<char_type, wchar_t>) {
                return;
            }
#endif
            if (ascii->state.kind != sizeof(char_type)) {
                failure<type_error, PyExc_TypeError>(
                    "Invalid character type requested");
            }
        }
    }

public:
    LEV_HIDE_INSTANTIATION inline constexpr basic_unicode_t(decltype(nullptr)) noexcept = delete;
    LEV_HIDE_INSTANTIATION inline constexpr basic_unicode_t(
        basic_unicode_t const&) noexcept = default;
    LEV_HIDE_INSTANTIATION LEV_REINITIALIZES inline constexpr basic_unicode_t& operator=(
        basic_unicode_t const&) noexcept = default;
    LEV_HIDE_INSTANTIATION inline constexpr basic_unicode_t(
        basic_unicode_t&&) noexcept = default;
    LEV_HIDE_INSTANTIATION LEV_REINITIALIZES inline constexpr basic_unicode_t& operator=(
        basic_unicode_t&&) noexcept = default;

    LEV_HIDE_INSTANTIATION inline constexpr basic_unicode_t(
        python_ptr<PyUnicodeObject>&& ptr) noexcept
    requires without_container_flags<flags_type, borrowed_flag>
        : basic_unicode_t{private_tag, std::move(ptr)} {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_unicode_t(
        python_ptr<PyUnicodeObject> const& ptr) noexcept
        : basic_unicode_t{private_tag, ptr} {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_unicode_t(
        python_ptr<PyUnicodeObject> const& ptr LEV_LIFETIMEBOUND) noexcept
    requires with_container_flags<flags_type, borrowed_flag>
        : basic_unicode_t{private_tag, ptr.get()} {}

    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_unicode_t(
        unmanaged_ptr<PyUnicodeObject> ptr) noexcept
        : basic_unicode_t{private_tag, __LEV adopt(ptr)} {}

    LEV_HIDE_INSTANTIATION inline constexpr basic_unicode_t(
        unmanaged_ptr<PyUnicodeObject> ptr) noexcept
    requires with_container_flags<flags_type, borrowed_flag>
        : basic_unicode_t{private_tag, ptr} {}

    template <container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::exclusion<decltype(Opt), flags_type,
        borrowed_flag>
    LEV_HIDE_INSTANTIATION inline constexpr basic_unicode_t(
        basic_unicode_t<char_type, Opt>&& other) noexcept
        : basic_unicode_t{std::move(other.instance_)} {}

    template <container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::exclusion<decltype(Opt), flags_type,
        borrowed_flag>
    LEV_HIDE_INSTANTIATION inline constexpr basic_unicode_t(
        basic_unicode_t<char_type, Opt> const& other) noexcept
        : basic_unicode_t{other.instance_} {}

    template <container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::acquisition<decltype(Opt), flags_type,
        borrowed_flag>
    LEV_HIDE_INSTANTIATION inline constexpr basic_unicode_t(
        basic_unicode_t<char_type, Opt> const& other LEV_LIFETIMEBOUND) noexcept
        : basic_unicode_t{other.instance()} {}

    template <container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::removal<decltype(Opt), flags_type,
        borrowed_flag>
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_unicode_t(
        basic_unicode_t<char_type, Opt> other) noexcept
        : basic_unicode_t{__LEV adopt(other.instance())} {}

    template <container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::retention<decltype(Opt), flags_type,
        borrowed_flag>
    LEV_HIDE_INSTANTIATION inline constexpr basic_unicode_t(
        basic_unicode_t<char_type, Opt> other) noexcept
        : basic_unicode_t{other.instance()} {}

    /* Character type aliasing */

    template <different_from<char_type> Char,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::exclusion<decltype(Opt), flags_type,
                 borrowed_flag> &&
        (std::is_void_v<Char> || std::is_void_v<char_type> ||
            std::same_as<Char, char> && sizeof(Char) == sizeof(char_type))
    LEV_HIDE_INSTANTIATION explicit(!std::is_void_v<char_type>) inline constexpr basic_unicode_t(
        basic_unicode_t<Char, Opt>&& other) noexcept
        : basic_unicode_t{std::move(other.instance_)} {}

    template <different_from<char_type> Char,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::exclusion<decltype(Opt), flags_type,
                 borrowed_flag> &&
        (std::is_void_v<Char> || std::is_void_v<char_type> ||
            std::same_as<Char, char> && sizeof(Char) == sizeof(char_type))
    LEV_HIDE_INSTANTIATION explicit(!std::is_void_v<char_type>) inline constexpr basic_unicode_t(
        basic_unicode_t<Char, Opt> const& other) noexcept
        : basic_unicode_t{other.instance_} {}

    template <different_from<char_type> Char,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::acquisition<decltype(Opt), flags_type,
                 borrowed_flag> &&
        (std::is_void_v<Char> || std::is_void_v<char_type> ||
            std::same_as<Char, char> && sizeof(Char) == sizeof(char_type))
    LEV_HIDE_INSTANTIATION explicit(!std::is_void_v<char_type>) inline constexpr basic_unicode_t(
        basic_unicode_t<Char, Opt> const& other LEV_LIFETIMEBOUND) noexcept
        : basic_unicode_t{other.instance()} {}

    template <different_from<char_type> Char,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::removal<decltype(Opt), flags_type,
                 borrowed_flag> &&
        (std::is_void_v<Char> || std::is_void_v<char_type> ||
            std::same_as<Char, char> && sizeof(Char) == sizeof(char_type))
    LEV_HIDE_INSTANTIATION explicit inline constexpr basic_unicode_t(
        basic_unicode_t<Char, Opt> other) noexcept
        : basic_unicode_t{__LEV adopt(other.instance())} {}

    template <different_from<char_type> Char,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::retention<decltype(Opt), flags_type,
                 borrowed_flag> &&
        (std::is_void_v<Char> || std::is_void_v<char_type> ||
            std::same_as<Char, char> && sizeof(Char) == sizeof(char_type))
    LEV_HIDE_INSTANTIATION explicit(!std::is_void_v<char_type>) inline constexpr basic_unicode_t(
        basic_unicode_t<Char, Opt> other) noexcept
        : basic_unicode_t{other.instance()} {}

    template <different_from<char_type> Char,
        container_flags_convertible_to<flags_type> auto Opt>
    requires details::container::retention<decltype(Opt), flags_type,
                 borrowed_flag> &&
        (std::is_void_v<Char> || std::is_void_v<char_type> ||
            std::same_as<Char, char> && sizeof(Char) == sizeof(char_type))
    LEV_HIDE_INSTANTIATION explicit(!std::is_void_v<char_type>) inline constexpr basic_unicode_t(
        basic_unicode_t<Char, Opt> other) noexcept
        : basic_unicode_t{other.instance()} {}

    LEV_HIDE_INSTANTIATION
        LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<PyUnicodeObject>
    instance() const noexcept LEV_LIFETIMEBOUND {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION
        LEV_PURE [[nodiscard]] inline constexpr unmanaged_ptr<PyUnicodeObject>
    instance() const noexcept
    requires with_container_flags<flags_type, borrowed_flag>
    {
        return instance_.get();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] operator view_type() const noexcept
    requires (!std::is_void_v<char_type>)
    {
        return view();
    }

    template <typename Char>
    requires std::is_void_v<char_type>
    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] explicit
    operator __LTL basic_zstring_view<Char>() const noexcept {
        return view<Char>();
    }

    template <typename Char>
    requires std::is_void_v<char_type>
    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] explicit
    operator std::basic_string_view<Char>() const noexcept {
        return view<Char>();
    }

    LEV_HIDE_INSTANTIATION inline size_t copy(char_type* dst, size_t maxlen) const noexcept
    requires (!std::is_void_v<char_type>)
    {
        return instance() ? view().copy(dst, maxlen) : 0;
    }

    template <typename Char>
    requires std::is_void_v<char_type>
    LEV_HIDE_INSTANTIATION inline size_t copy(Char* dst, size_t maxlen) const
        noexcept(is_nothrow_v) {
        return instance() ? view<Char>().copy(dst, maxlen) : 0;
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr bool empty() const noexcept {
        return !size();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_t size() const noexcept {
        return instance_ ? static_ptr_cast<PyASCIIObject>(instance())->length
                         : 0u;
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline constexpr size_t length() const noexcept {
        return size();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] friend inline constexpr hash_value hash(
        basic_unicode const& unicode) const noexcept {
        return instance_ ? PyUnicode_Type.tp_hash(instance_.get())
                         : hash_value::invalid;
    }

    LEV_HIDE_INSTANTIATION friend void swap(basic_unicode& lhs, basic_unicode& rhs) noexcept {
        lhs.swap(other);
    }

    LEV_HIDE_INSTANTIATION LEV_REINITIALIZES void swap(basic_unicode& other) noexcept {
        instance_.swap(other.instance_);
    }

    LEV_HIDE_INSTANTIATION
        LEV_PURE [[nodiscard]] inline constexpr char_type const*
    data() const noexcept
    requires (!std::is_void_v<char_type>)
    {
        return view().data();
    }

    template <typename Char>
    requires std::is_void_v<char_type> &&
        __LTL is_complete_v<__LTL basic_zstring_view<Char>>
    LEV_HIDE_INSTANTIATION
        LEV_PURE [[nodiscard]] inline constexpr char_type const* data() const
        noexcept(is_nothrow_v) {
        return view<Char>().data();
    }

    LEV_HIDE_INSTANTIATION LEV_PURE [[nodiscard]] inline view_type view() const noexcept
    requires (!std::is_void_v<char_type>)
    {
        if (!instance_) {
            return view_type{};
        }

        auto const ascii = static_ptr_cast<PyASCIIObject>(instance());
        if constexpr (sizeof(char_type) == 1) {
            if (ascii->state.ascii) {
                auto str = ascii->state.compact
                    ? reinterpret_cast<char_type const*>(ascii.get() + 1)
                    : reinterpret_cast<char_type const*>(instance_->data.any);
                return details::unicode::start_lifetime<char_type>{
                    str, ptr->length};
            }
        }
#if LEV_PYTHON_VERSION_LT(3, 12, 0)
        else if constexpr (std::is_same_v<char_type, wchar_t>) {
            if (ascii->state.kind == PyUnicode_Kind::PyUnicode_WCHAR_KIND) {
                return details::unicode::start_lifetime<char_type>{
                    ascii->wstr, ptr->length};
            }
        }
#endif
        auto const compact =
            static_ptr_cast<PyCompactUnicodeObject>(instance());
        auto ptr = ascii->state.compact
            ? reinterpret_cast<char_type const*>(compact.get() + 1)
            : reinterpret_cast<char_type const*>(instance_->data.any);
        return details::unicode::start_lifetime<char_type>(ptr, ascii->length);
    }

    template <typename Char>
    requires std::is_void_v<char_type> &&
        __LTL is_complete_v<__LTL basic_zstring_view<Char>>
    LEV_HIDE_INSTANTIATION result_type<__LTL basic_zstring_view<C>> view() const
        noexcept(is_nothrow_v) {
        if (!instance_) {
            return view_type{};
        }

        auto const ascii = static_ptr_cast<PyASCIIObject>(instance());
#if LEV_PYTHON_VERSION_LT(3, 12, 0)
        if (ascii->state.kind == PyUnicode_Kind::PyUnicode_WCHAR_KIND &&
            std::is_same_v<Char, wchar_t>) {
            return details::unicode::start_lifetime<Char>{
                ascii->wstr, ptr->length};
        }
#endif
        if (ascii->state.kind != sizeof(Char)) {
            if constexpr (is_nothrow_v) {
                return result_type<__LTL basic_zstring_view<C>>{
                    __LTL unexpect, result_code::failed};
            } else {
                failure<type_error, PyExc_TypeError>(
                    "Invalid character type requested");
            }
        }

        if constexpr (std::same_as<Char, char>) {
            if (ascii->state.ascii) {
                auto str = ascii->state.compact
                    ? reinterpret_cast<Char const*>(ascii.get() + 1)
                    : reinterpret_cast<Char const*>(instance_->data.any);
                return details::unicode::start_lifetime<Char>{str, ptr->length};
            }
        }

        auto const compact =
            static_ptr_cast<PyCompactUnicodeObject>(instance());
        auto str = ascii->state.compact
            ? reinterpret_cast<Char const*>(compact.get() + 1)
            : reinterpret_cast<Char const*>(instance_->data.any);
        return details::unicode::start_lifetime<Char>(str, ascii->length);
    }

private:
    instance_type instance_;
};

} // namespace py
} // namespace lev
