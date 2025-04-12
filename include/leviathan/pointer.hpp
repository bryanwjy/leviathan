// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/object_traits.hpp"
#include "leviathan/utility.hpp"

#include <Python.h>

#include <compare>

namespace lev {

template <typename Obj>
class python_ptr;

struct LEV_API adopt_t {
    LEV_HIDDEN explicit constexpr adopt_t() noexcept = default;
};
LEV_HIDDEN inline constexpr adopt_t adopt_object{};

struct LEV_API retain_t {
    LEV_HIDDEN explicit constexpr retain_t() noexcept = default;
};
LEV_HIDDEN inline constexpr retain_t retain_object{};

namespace details {
template <typename T>
class pointer_comparable {
    using null_type = decltype(nullptr);

    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool operator==(
        T const& lhs, T const& rhs) noexcept {
        return lhs.get() == rhs.get();
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool operator==(
        T const& lhs, null_type) noexcept {
        return lhs.get() == nullptr;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool operator==(
        null_type, T const& lhs) noexcept {
        return nullptr == lhs.get();
    }

    template <typename U>
    requires requires(T const& lhs, U* rhs) {
        { lhs.get() == rhs } noexcept -> std::convertible_to<bool>;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool operator==(
        T const& lhs, U* rhs) noexcept {
        return lhs.get() == rhs;
    }

    template <typename U>
    requires requires(U* lhs, T const& rhs) {
        { lhs == rhs.get() } noexcept -> std::convertible_to<bool>;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool operator==(
        U* lhs, T const& rhs) noexcept {
        return lhs == rhs.get();
    }

    template <typename U>
    requires requires(T const& lhs, U const& rhs) {
        { lhs.get() == rhs.get() } noexcept -> std::convertible_to<bool>;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool operator==(
        T const& lhs, U const& rhs) noexcept {
        return lhs.get() == rhs.get();
    }

    template <typename U>
    requires requires(U const& lhs, T const& rhs) {
        { lhs.get() == rhs.get() } noexcept -> std::convertible_to<bool>;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr bool operator==(
        U const& lhs, T const& rhs) noexcept {
        return lhs.get() == rhs.get();
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr auto operator<=>(
        T const& lhs, T const& rhs) noexcept {
        return lhs.get() <=> rhs.get();
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr auto operator<=>(
        T const& lhs, decltype(nullptr)) noexcept {
        return lhs.get() <=> nullptr;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr auto operator<=>(
        decltype(nullptr), T const& lhs) noexcept {
        return nullptr <=> lhs.get();
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr auto operator<=>(
        T const& lhs, decltype(nullptr)) noexcept {
        return lhs.get() <=> nullptr;
    }

    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr auto operator<=>(
        decltype(nullptr), T const& lhs) noexcept {
        return nullptr <=> lhs.get();
    }

    template <typename U>
    requires requires(T const& lhs, U* rhs) {
        { lhs.get() <=> rhs } noexcept;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr auto operator==(
        T const& lhs, U* rhs) noexcept {
        return lhs.get() <=> rhs;
    }

    template <typename U>
    requires requires(U* lhs, T const& rhs) {
        { lhs <=> rhs.get() } noexcept;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr auto operator==(
        U* lhs, T const& rhs) noexcept {
        return lhs <=> rhs.get();
    }

    template <typename U>
    requires requires(T const& lhs, U const& rhs) {
        { lhs.get() <=> rhs.get() } noexcept;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr auto operator==(
        T const& lhs, U const& rhs) noexcept {
        return lhs.get() <=> rhs.get();
    }

    template <typename U>
    requires requires(U const& lhs, T const& rhs) {
        { lhs.get() <=> rhs.get() } noexcept;
    }
    LEV_HIDE_INSTANTIATION [[nodiscard]] friend constexpr auto operator==(
        U const& lhs, T const& rhs) noexcept {
        return lhs.get() <=> rhs.get();
    }
};
} // namespace details

template <typename Obj>
class LEV_API unmanaged_ptr<Obj>;

template <pyobj_type Obj>
class LEV_API python_ptr<Obj> : pointer_comparable<python_ptr<Obj>> {

    LEV_HIDE_INSTANTIATION [[gnu::noinline, noreturn]] void
    error_null_instance() {
        LEV_THROW(std::invalid_argument(
            "Invalid operation, Reason=[Instance is null]"));
    }

private:
    LEV_HIDE_INSTANTIATION static Obj* retain_nonnull(Obj* ptr) noexcept LEV_CONTRACT_PRE(
        ptr) {
        LEV_ASSERT(ptr);
        return ptr;
    }
    LEV_HIDE_INSTANTIATION static Obj* adopt_nonnull(Obj* ptr) noexcept LEV_CONTRACT_PRE(
        ptr) {
        LEV_ASSERT(ptr);
        return py_cast<Obj>(Py_NewRef(as_pyobject(ptr)));
    }

public:
    using element_type = Obj;

    constexpr python_ptr(retain_t, decltype(nullptr) ptr) noexcept = delete;
    constexpr python_ptr(adopt_t, decltype(nullptr) ptr) noexcept = delete;
    LEV_HIDE_INSTANTIATION constexpr python_ptr() noexcept : ptr_{nullptr} {}
    LEV_HIDE_INSTANTIATION constexpr python_ptr(decltype(nullptr)) noexcept
        : python_ptr{} {}

    LEV_HIDE_INSTANTIATION constexpr python_ptr(retain_t, Obj* ptr)
        : ptr_{retain_nonnull(ptr)} {}
    LEV_HIDE_INSTANTIATION constexpr python_ptr(adopt_t, Obj* ptr)
        : ptr_{adopt_nonnull(ptr)} {}

    LEV_HIDE_INSTANTIATION explicit constexpr python_ptr(
        retain_t, unmanaged_ptr<Obj> ptr) noexcept
        : ptr_{retain_nonnull(ptr)} {}
    LEV_HIDE_INSTANTIATION explicit constexpr python_ptr(
        adopt_t, unmanaged_ptr<Obj> ptr) noexcept
        : ptr_{adopt_nonnull(ptr)} {}

    LEV_HIDE_INSTANTIATION constexpr python_ptr(
        python_ptr const& other) noexcept
        : ptr_{py_cast<Obj>(Py_XNewRef(as_pyobject(other.ptr_)))} {}
    LEV_HIDE_INSTANTIATION constexpr python_ptr& operator=(
        python_ptr const& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = py_cast<Obj>(Py_XNewRef(as_pyobject(other.ptr_)));
        }

        return *this;
    }

    LEV_HIDE_INSTANTIATION constexpr python_ptr(python_ptr&& other) noexcept
        : ptr_(exchange(other.ptr_, nullptr)) {}
    LEV_HIDE_INSTANTIATION constexpr python_ptr& operator=(
        python_ptr&& other) noexcept
        : ptr_(exchange(other.ptr_, nullptr)) {}

    template <pyobj_derived_from<Obj> U>
    LEV_HIDE_INSTANTIATION constexpr python_ptr(
        python_ptr<U> const& other) noexcept
        : ptr_{py_cast<Obj>(Py_XNewRef(as_pyobject(other.ptr_)))} {}

    template <pyobj_derived_from<Obj> U>
    LEV_HIDE_INSTANTIATION constexpr python_ptr& operator=(
        python_ptr<U> const& other) noexcept {
        reset();
        ptr_ = py_cast<Obj>(Py_XNewRef(as_pyobject(other.ptr_)));
        return *this;
    }

    template <pyobj_derived_from<Obj> U>
    LEV_HIDE_INSTANTIATION constexpr python_ptr(python_ptr<U>&& other) noexcept
        : ptr_(py_cast<Obj>(exchange(other.ptr_, nullptr))) {}

    template <pyobj_derived_from<Obj> U>
    LEV_HIDE_INSTANTIATION constexpr python_ptr& operator=(
        python_ptr<U>&& other) noexcept {
        reset();
        ptr_ = py_cast<Obj>(exchange(other.ptr_, nullptr));
        return *this;
    }

    LEV_HIDE_INSTANTIATION constexpr Obj* operator->() const noexcept {
        return ptr_;
    }

    LEV_HIDE_INSTANTIATION constexpr Obj& operator*() const noexcept {
        return *ptr_;
    }

    LEV_HIDE_INSTANTIATION constexpr unmanaged_ptr<Obj>
    get() const noexcept LEV_LIFETIMEBOUND {
        return ptr_;
    }

    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] unmanaged_ptr<Obj>
    release() noexcept {
        return exchange(ptr_, nullptr);
    }

    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr void
    reset() noexcept {
        Py_XDECREF(as_pyobject(exchange(ptr_, nullptr)));
    }

    LEV_HIDE_INSTANTIATION void reset(retain_t, decltype(nullptr)) = delete;
    LEV_HIDE_INSTANTIATION void reset(adopt_t, decltype(nullptr)) = delete;

    template <pyobj_derived_from<Obj> U>
    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr void reset(
        retain_t, U* ptr) {
        Py_XDECREF(as_pyobject(exchange(ptr_, retain(ptr))));
    }

    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr void reset(
        adopt_t, Obj* ptr) {
        Py_XDECREF(as_pyobject(exchange(ptr_, adopt(ptr))));
    }

    LEV_HIDE_INSTANTIATION [[clang::reinitializes]] constexpr void swap(
        python_ptr& other) noexcept {
        ptr_ = exchange(other.ptr_, ptr_);
    }

    LEV_HIDE_INSTANTIATION constexpr ~python_ptr() noexcept { reset(); }

    LEV_HIDE_INSTANTIATION constexpr explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

private:
    Obj* ptr_;
};

template <pyobj_type T>
explicit python_ptr(retain_t, T*) -> python_ptr<T>;
template <pyobj_type T>
explicit python_ptr(adopt_t, T*) -> python_ptr<T>;

template <pyobj_type T>
explicit python_ptr(retain_t, unmanaged_ptr<T>) -> python_ptr<T>;
template <pyobj_type T>
explicit python_ptr(adopt_t, unmanaged_ptr<T>) -> python_ptr<T>;

template <typename Obj>
class LEV_API unmanaged_ptr<Obj> : pointer_comparable<unmanaged_ptr<Obj>> {

public:
    using element_type = Obj;

    LEV_HIDE_INSTANTIATION constexpr unmanaged_ptr() noexcept : ptr_{nullptr} {}
    LEV_HIDE_INSTANTIATION constexpr unmanaged_ptr(decltype(nullptr)) noexcept
        : unmanaged_ptr{} {}
    LEV_HIDE_INSTANTIATION constexpr unmanaged_ptr(Obj* ptr) noexcept
        : ptr_{ptr} {}

    LEV_HIDE_INSTANTIATION constexpr unmanaged_ptr(
        unmanaged_ptr const& other) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr unmanaged_ptr& operator=(
        unmanaged_ptr const& other) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr unmanaged_ptr(
        unmanaged_ptr&& other) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr unmanaged_ptr& operator=(
        unmanaged_ptr&& other) noexcept = default;

    LEV_HIDE_INSTANTIATION constexpr ~unmanaged_ptr() noexcept = default;

    template <pyobj_derived_from<T> U>
    LEV_HIDE_INSTANTIATION constexpr unmanaged_ptr(
        unmanaged_ptr<U> const& other) noexcept
        : ptr_{py_cast<T>(other.ptr_)} {}

    template <pyobj_derived_from<T> U>
    LEV_HIDE_INSTANTIATION constexpr unmanaged_ptr& operator=(
        unmanaged_ptr<U> const& other) noexcept {
        ptr_ = py_cast<T>(other.ptr_);
        return *this;
    }

    template <typename U>
    requires std::is_convertible_v<U*, T*>
    LEV_HIDE_INSTANTIATION constexpr unmanaged_ptr(
        unmanaged_ptr<U> const& other) noexcept
        : ptr_{other.get()} {}

    template <typename U>
    requires std::is_convertible_v<U*, T*>
    LEV_HIDE_INSTANTIATION constexpr unmanaged_ptr& operator=(
        unmanaged_ptr<U> const& other) noexcept {
        ptr_ = other.get();
        return *this;
    }

    LEV_HIDE_INSTANTIATION constexpr
    operator std::add_pointer_t<T>() const noexcept {
        return ptr_;
    }

    template <pyobj_base_of<T> U>
    LEV_HIDE_INSTANTIATION constexpr
    operator std::add_pointer_t<U>() const noexcept {
        return py_cast<U>(ptr_);
    }

    template <typename U>
    requires std::is_convertible_v<T*, U*> && (!pyobj_base_of<U, T>)
    LEV_HIDE_INSTANTIATION constexpr
    operator std::add_pointer_t<U>() const noexcept {
        return ptr_;
    }

    LEV_HIDE_INSTANTIATION constexpr explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

    LEV_HIDE_INSTANTIATION constexpr Obj* operator->() const noexcept {
        return ptr_;
    }

    LEV_HIDE_INSTANTIATION constexpr Obj& operator*() const noexcept {
        return *ptr_;
    }

    LEV_HIDE_INSTANTIATION constexpr Obj* get() const noexcept { return ptr_; }

    LEV_HIDE_INSTANTIATION void reset(Obj* ptr = nullptr) noexcept {
        ptr_ = ptr;
    }

private:
    Obj* ptr_;
};

template <typename T>
void static_ptr_cast(...) noexcept = delete;
template <typename T>
void dynamic_ptr_cast(...) noexcept = delete;
template <typename T>
void exact_ptr_cast(...) noexcept = delete;

namespace details {
template <typename From, typename To>
using cast_result_unmanaged_t = unmanaged_ptr<
    std::remove_pointer_t<decltype(py_cast<To>(static_cast<From*>(0)))>>;
using cast_result_python_t = python_ptr<
    std::remove_pointer_t<decltype(py_cast<To>(static_cast<From*>(0)))>>;
} // namespace details

template <pyobj_type To, castable_to<To> From>
LEV_HIDDEN [[nodiscard]] inline details::cast_result_unmanaged_t<From, To>
static_ptr_cast(From* other) noexcept {
    return py_cast<To>(other);
}

template <pyobj_type To, pyobj_type From>
requires requires(unmanaged_ptr<From> ptr) { dynamic_ptr_cast<To>(ptr); }
LEV_HIDDEN [[nodiscard]] inline details::cast_result_unmanaged_t<From, To>
dynamic_ptr_cast(From* other) noexcept {
    return dynamic_ptr_cast<To>(unmanaged_ptr<From>(other));
}

template <pyobj_type To, pyobj_type From>
requires requires(unmanaged_ptr<From> ptr) { exact_ptr_cast<To>(ptr); }
LEV_HIDDEN [[nodiscard]] inline details::cast_result_unmanaged_t<From, To>
exact_ptr_cast(From* other) noexcept {
    return exact_ptr_cast<To>(unmanaged_ptr<From>(other));
}

template <pyobj_type To, pyobj_type From>
requires requires(unmanaged_ptr<From> ptr) { static_ptr_cast<To>(ptr); }
LEV_HIDDEN [[nodiscard]] inline details::cast_result_python_t<From, To>
static_ptr_cast(python_ptr<From> const& other) noexcept {
    return python_ptr{adopt_object, static_ptr_cast<To>(other.get())};
}

template <pyobj_type To, pyobj_type From>
requires requires(unmanaged_ptr<From> ptr) { static_ptr_cast<To>(ptr); }
LEV_HIDDEN [[nodiscard]] inline details::cast_result_python_t<From, To>
static_ptr_cast(python_ptr<From>&& other) noexcept {
    return python_ptr{retain_object, static_ptr_cast<To>(other.release())};
}

template <pyobj_type To, pyobj_type From>
requires requires(unmanaged_ptr<From> ptr) { dynamic_ptr_cast<To>(ptr); }
LEV_HIDDEN [[nodiscard]] inline details::cast_result_python_t<From, To>
dynamic_ptr_cast(python_ptr<From> const& other) noexcept {
    if (auto ptr = dynamic_ptr_cast<To>(other.get())) {
        return python_ptr{adopt_object, ptr};
    } else {
        return nullptr;
    }
}

template <pyobj_type To, pyobj_type From>
requires requires(unmanaged_ptr<From> ptr) { dynamic_ptr_cast<To>(ptr); }
LEV_HIDDEN [[nodiscard]] inline details::cast_result_python_t<From, To>
dynamic_ptr_cast(python_ptr<From>&& other) noexcept {
    if (dynamic_ptr_cast<To>(other.get())) {
        return python_ptr{retain_object, py_cast<To>(other.release())};
    } else {
        return nullptr;
    }
}

template <pyobj_type To, pyobj_type From>
requires requires(unmanaged_ptr<From> ptr) { exact_ptr_cast<To>(ptr); }
LEV_HIDDEN [[nodiscard]] inline details::cast_result_python_t<From, To>
exact_ptr_cast(python_ptr<From> const& other) noexcept {
    if (exact_ptr_cast<To>(other.get())) {
        return python_ptr{adopt_object, py_cast<To>(other.get())};
    } else {
        return nullptr;
    }
}

template <pyobj_type To, pyobj_type From>
requires requires(unmanaged_ptr<From> ptr) { exact_ptr_cast<To>(ptr); }
LEV_HIDDEN [[nodiscard]] inline details::cast_result_python_t<From, To>
exact_ptr_cast(python_ptr<From>&& other) noexcept {
    if (exact_ptr_cast<To>(other.get())) {
        return python_ptr{retain_object, py_cast<To>(other.release())};
    } else {
        return nullptr;
    }
}

template <pyobj_type To, castable_to<To> From>
LEV_HIDDEN [[nodiscard]] inline details::cast_result_unmanaged_t<From, To>
static_ptr_cast(unmanaged_ptr<From> other) noexcept {
    return py_cast<To>(other.get());
}

template <pyobj_type To, castable_to<To> From>
requires std::same_as<std::remove_cv_t<To>, std::remove_cv_t<From>>
LEV_HIDDEN [[nodiscard]] inline details::cast_result_unmanaged_t<From, To>
dynamic_ptr_cast(unmanaged_ptr<From> other) noexcept {
    return py_cast<To>(other.get());
}

template <pyobj_type To, pyobj_derived_from<To> From>
requires (!std::same_as<std::remove_cv_t<To>, std::remove_cv_t<From>>)
LEV_HIDDEN [[nodiscard]] inline details::cast_result_unmanaged_t<From, To>
dynamic_ptr_cast(unmanaged_ptr<From> other) noexcept {
    return py_cast<To>(other.get());
}

template <identifiable_pyobj_type To, pyobj_base_of<To> From>
requires requires(From* ptr) {
    requires !std::same_as<std::remove_cv_t<To>, std::remove_cv_t<From>>;
    py_cast<To>(ptr);
}
LEV_HIDDEN [[nodiscard]] inline details::cast_result_unmanaged_t<From, To>
dynamic_ptr_cast(unmanaged_ptr<From> other) noexcept {
    if (other && PyObject_TypeCheck(other.get(), type_object<To>())) {
        return py_cast<To>(other.get());
    }

    return nullptr;
}

template <pyobj_type To, castable_to<To> From>
requires std::same_as<std::remove_cv_t<To>, std::remove_cv_t<From>>
LEV_HIDDEN [[nodiscard]] inline details::cast_result_unmanaged_t<From, To>
exact_ptr_cast(unmanaged_ptr<From> other) noexcept {
    return py_cast<To>(other.get());
}

template <identifiable_pyobj_type To, pyobj_base_of<To> From>
requires requires(From* ptr) {
    requires !std::same_as<std::remove_cv_t<To>, std::remove_cv_t<From>>;
    requires castable_to<To>;
}
LEV_HIDDEN [[nodiscard]] inline details::cast_result_unmanaged_t<From, To>
exact_ptr_cast(unmanaged_ptr<From> other) noexcept {
    if (other && type_object<To>() == Py_TYPE(other)) {
        return py_cast<To>(other.get());
    }

    return nullptr;
}

template <pyobj_type T>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> adopt(
    unmanaged_ptr<T> ptr) noexcept {
    return python_ptr{adopt_object, ptr};
}

template <pyobj_type T>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> adopt(T* ptr) noexcept {
    return python_ptr{adopt_object, ptr};
}

template <pyobj_type T>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> adopt(
    python_ptr<T> const& ptr) noexcept {
    return ptr;
}

template <pyobj_type T>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> steal(
    unmanaged_ptr<T> ptr) noexcept {
    return python_ptr{retain_object, ptr};
}

template <pyobj_type T>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> steal(T* ptr) noexcept {
    return python_ptr{retain_object, ptr};
}

template <pyobj_type T>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> steal(
    python_ptr<T>&& ptr) noexcept {
    return std::move(ptr);
}

template <pyobj_type T>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> borrow(
    unmanaged_ptr<T> ptr) noexcept {
    return unamanage_ptr<T>{ptr};
}

template <pyobj_type T>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> borrow(
    T* ptr) noexcept {
    return unamanage_ptr<T>{ptr};
}

template <pyobj_type T>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> borrow(
    python_ptr<T> const& ptr LEV_LIFETIMEBOUND) noexcept {
    return ptr.get();
}

template <pyobj_type T, pyobj_derived_from<T> U>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> adopt(
    unmanaged_ptr<U> ptr) noexcept {
    return python_ptr{adopt_object, ptr};
}

template <pyobj_type T, pyobj_derived_from<T> U>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> adopt(U* ptr) noexcept {
    return python_ptr{adopt_object, ptr};
}

template <pyobj_type T, pyobj_derived_from<T> U>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> adopt(
    python_ptr<U> const& ptr) noexcept {
    return ptr;
}

template <pyobj_type T, typename U>
requires std::is_nothrow_convertible_v<U, unmanaged_ptr<T>> &&
    (!pyobj_derived_from<U, T>)
LEV_HIDDEN
    [[nodiscard]] inline constexpr python_ptr<T> adopt(U&& val) noexcept {
    return adopt<T>(static_cast<unmanaged_ptr<T>>(val));
}

template <pyobj_type T, pyobj_derived_from<T> U>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> steal(
    unmanaged_ptr<U> ptr) noexcept {
    return python_ptr{retain_object, ptr};
}

template <pyobj_type T, pyobj_derived_from<T> U>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> steal(U* ptr) noexcept {
    return python_ptr{retain_object, ptr};
}

template <pyobj_type T, pyobj_derived_from<T> U>
LEV_HIDDEN [[nodiscard]] inline constexpr python_ptr<T> steal(
    python_ptr<U>&& ptr) noexcept {
    return std::move(ptr);
}

template <pyobj_type T, pyobj_derived_from<T> U>
LEV_HIDDEN [[nodiscard]] inline constexpr unamanage_ptr<T> borrow(
    unmanaged_ptr<U> ptr) noexcept {
    return unamanage_ptr<T>{ptr};
}

template <pyobj_type T, pyobj_derived_from<T> U>
LEV_HIDDEN [[nodiscard]] inline constexpr unamanage_ptr<T> borrow(
    U* ptr) noexcept {
    return unamanage_ptr<T>{ptr};
}

template <pyobj_type T, pyobj_derived_from<T> U>
LEV_HIDDEN [[nodiscard]] inline constexpr unamanage_ptr<T> borrow(
    python_ptr<U> const& ptr LEV_LIFETIMEBOUND) noexcept {
    return ptr.get();
}

template <typename T, typename U>
concept adoptable_as =
    pyobj_type<U> && requires(T&& val) { adopt<U>(std::forward<T>(val)); };

template <typename T, typename U>
concept stealable_as =
    pyobj_type<U> && requires(T&& val) { steal<U>(std::forward<T>(val)); };

template <typename T, typename U>
concept borrowable_as =
    pyobj_type<U> && requires(T&& val) { borrow<U>(std::forward<T>(val)); };

} // namespace lev
