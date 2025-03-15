// Copyright 2025, Bryan Wong

#include "leviathan/pointer.hpp"

#include <Python.h>

#include <type_traits>
#include <utility>

namespace lev {
namespace py {
struct nothrow_t {
    explicit inline constexpr nothrow_t() noexcept = default;
};
LEV_HIDDEN inline constexpr nothrow_t nothrow{};

using size_t = decltype(sizeof(0));
using ptrdiff_t = decltype(static_cast<char*>(0) - static_cast<char*>(0));
using ssize_t = std::make_signed_t<size_t>;

LEV_HIDDEN inline constexpr size_t dynamic_extent = static_cast<size_t>(-1);

template <typename T, PyObject*& Exc, typename... Args>
requires requires(T const& error) {
    { error.what() } noexcept -> std::convertible_to<char const*>;
}
LEV_HIDE_INSTANTIATION [[gnu::noinline, noreturn]] void failure(
    Args&&... args) {

    T exc(std::forward<Args>(args)...);
    PyErr_Format(Exc,
        "Operation failed, Reason=[Exception thrown], Message=[%s]",
        static_cast<char const*>(std::as_const(exc).what()));
    LEV_THROW(std::move(exc));
}

} // namespace py
} // namespace lev
