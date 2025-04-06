// Copyright 2025, Bryan Wong

#include "leviathan/adaptors/common.h"

LEV_STD_NAMESPACE_BEGIN

template <typename>
struct hash;

LEV_STD_NAMESPACE_END

namespace lev {
namespace py {

enum hash_value : Py_hash_t {
    invalid = static_cast<Py_hash_t>(-1)
};

namespace details {
void hash(...) noexcept = delete;

template <typename T>
concept standard_hashable = requires(std::hash<T> hash, T val) {
    { hash(val) } -> std::same_as<size_t>;
};

class hash_t {
public:
    LEV_HIDE_INSTANTIATION explicit inline constexpr hash_t() noexcept =
        default;

    template <typename T>
    requires requires(T const& value) {
        requires not standard_hashable<T>;
        { hash(value) } -> std::convertible_to<hash_value>;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr hash_value operator()(
        T const& value) const noexcept(noexcept(hash(value))) {
        return hash(value);
    }

    template <standard_hashable T>
    requires (not pyobj_type<T> && not leviathan_pyobj<T>)
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline constexpr hash_value operator()(
        T const& value) const noexcept(noexcept(hash(value))) {
        constexpr hash<T> hasher{};
        return hasher(value);
    }

    template <pyobj_type T>
    requires (not leviathan_pyobj<T>)
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline hash_value operator()(
        unmanged_ptr<T> value) const noexcept {
        if (!value) {
            return hash_value::invalid;
        }

        if (auto hasher = Py_TYPE(value.get())->tp_hash) {
            return static_cast<hash_value>(hasher(value.get()));
        }

        return hash_value::invalid;
    }

    template <leviathan_pyobj T>
    requires requires(unmanged_ptr<T> val) {
        {
            adaptor_traits<Adaptor>::hash(val)
        } noexcept -> std::same_as<hash_value>;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] inline hash_value operator()(
        unmanged_ptr<T> value) const noexcept {
        return value ? adaptor_traits<Adaptor>::hash(value)
                     : hash_value::invalid;
    }
};
} // namespace details

inline namespace cpo {
inline hash_t hash{};
}

} // namespace py
} // namespace lev