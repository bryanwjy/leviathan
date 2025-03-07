// Copyright 2025, Bryan Wong
#pragma once

#include <Python.h>

#include <compare>
#include <leviathan/object_traits.hpp>
#include <leviathan/utility.hpp>

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
class LEV_API non_owning_ptr<Obj>;

template <pyobj_type Obj>
class LEV_API python_ptr<Obj> : pointer_comparable<python_ptr<Obj>> {
public:
    using element_type = Obj;

    constexpr python_ptr(retain_t, decltype(nullptr) ptr) noexcept = delete;
    constexpr python_ptr(adopt_t, decltype(nullptr) ptr) noexcept = delete;
    LEV_HIDE_INSTANTIATION constexpr python_ptr() noexcept : ptr_{nullptr} {}
    LEV_HIDE_INSTANTIATION constexpr python_ptr(decltype(nullptr)) noexcept
        : python_ptr{} {}
    LEV_HIDE_INSTANTIATION constexpr explicit python_ptr(Obj* ptr) noexcept
        : python_ptr{retain_object, ptr} {}
    LEV_HIDE_INSTANTIATION constexpr python_ptr(retain_t, Obj* ptr) noexcept
        : ptr_{ptr} {}
    LEV_HIDE_INSTANTIATION constexpr python_ptr(adopt_t, Obj* ptr) noexcept
        : ptr_{Py_NewRef(as_pyobject(ptr))} {}

    LEV_HIDE_INSTANTIATION constexpr explicit python_ptr(
        non_owning_ptr<Obj> ptr) noexcept
        : python_ptr{retain_object, ptr} {}
    LEV_HIDE_INSTANTIATION constexpr python_ptr(
        retain_t, non_owning_ptr<Obj> ptr) noexcept
        : ptr_{ptr} {}
    LEV_HIDE_INSTANTIATION constexpr python_ptr(
        adopt_t, non_owning_ptr<Obj> ptr) noexcept
        : ptr_{Py_NewRef(as_pyobject(ptr))} {}

    LEV_HIDE_INSTANTIATION constexpr python_ptr(
        python_ptr const& other) noexcept
        : ptr_{Py_NewRef(as_pyobject(other.ptr_))} {}
    LEV_HIDE_INSTANTIATION constexpr python_ptr& operator=(
        python_ptr const& other) noexcept {
        if (this != &other) {
            reset();
            Py_XINCREF(as_pyobject(other.ptr_));
            ptr_ = other.ptr_;
        }

        return *this;
    }

    LEV_HIDE_INSTANTIATION constexpr python_ptr(python_ptr&& other) noexcept
        : ptr_(exchange(other.ptr_, nullptr)) {}
    LEV_HIDE_INSTANTIATION constexpr python_ptr& operator=(
        python_ptr&& other) noexcept
        : ptr_(exchange(other.ptr_, nullptr)) {}

    template <pyobj_type U>
    requires std::convertible_to<U*, T*>
    LEV_HIDE_INSTANTIATION constexpr python_ptr(
        python_ptr<U> const& other) noexcept
        : ptr_{py_cast<T>(Py_NewRef(as_pyobject(other.ptr_)))} {}

    template <pyobj_type U>
    requires std::convertible_to<U*, T*>
    LEV_HIDE_INSTANTIATION constexpr python_ptr& operator=(
        python_ptr<U> const& other) noexcept {
        reset();
        Py_XINCREF(as_pyobject(other.ptr_));
        ptr_ = other.ptr_;

        return *this;
    }

    template <pyobj_type U>
    requires (aliasable_subobject_of<T, U> && !std::convertible_to<U*, T*>)
    LEV_HIDE_INSTANTIATION constexpr python_ptr(
        python_ptr<U> const& other) noexcept
        : ptr_{py_cast<T>(Py_NewRef(as_pyobject(other.ptr_)))} {}

    template <pyobj_type U>
    requires (aliasable_subobject_of<T, U> && !std::convertible_to<U*, T*>)
    LEV_HIDE_INSTANTIATION constexpr python_ptr& operator=(
        python_ptr<U> const& other) noexcept {
        reset();
        Py_XINCREF(as_pyobject(other.ptr_));
        ptr_ = py_cast<T>(other.ptr_);

        return *this;
    }

    template <pyobj_type U>
    requires std::convertible_to<U*, T*>
    LEV_HIDE_INSTANTIATION constexpr python_ptr(python_ptr<U>&& other) noexcept
        : ptr_(exchange(other.ptr_, nullptr)) {}

    template <pyobj_type U>
    requires std::convertible_to<U*, T*>
    LEV_HIDE_INSTANTIATION constexpr python_ptr& operator=(
        python_ptr<U>&& other) noexcept {
        reset();
        ptr_ = exchange(other.ptr_, nullptr);
        return *this;
    }

    template <pyobj_type U>
    requires (aliasable_subobject_of<T, U> && !std::convertible_to<U*, T*>)
    LEV_HIDE_INSTANTIATION constexpr python_ptr(python_ptr<U>&& other) noexcept
        : ptr_(py_cast<T>(exchange(other.ptr_, nullptr))) {}

    template <pyobj_type U>
    requires (aliasable_subobject_of<T, U> && !std::convertible_to<U*, T*>)
    LEV_HIDE_INSTANTIATION constexpr python_ptr& operator=(
        python_ptr<U>&& other) noexcept {
        reset();
        ptr_ = py_cast<T>(exchange(other.ptr_, nullptr));
        return *this;
    }

    LEV_HIDE_INSTANTIATION constexpr Obj* operator->() const noexcept {
        return ptr_;
    }

    LEV_HIDE_INSTANTIATION constexpr Obj& operator*() const noexcept {
        return *ptr_;
    }

    LEV_HIDE_INSTANTIATION constexpr non_owning_ptr<Obj> get() const noexcept {
        return ptr_;
    }

    LEV_HIDE_INSTANTIATION non_owning_ptr<Obj> release() noexcept {
        return exchange(ptr_, nullptr);
    }

    LEV_HIDE_INSTANTIATION constexpr void reset() noexcept {
        Py_XDECREF(as_pyobject(exchange(ptr_, nullptr)));
    }

    LEV_HIDE_INSTANTIATION void reset(retain_t, decltype(nullptr)) = delete;
    LEV_HIDE_INSTANTIATION void reset(adopt_t, decltype(nullptr)) = delete;

    LEV_HIDE_INSTANTIATION constexpr void reset(retain_t, Obj* ptr) noexcept {
        Py_XDECREF(as_pyobject(exchange(ptr_, ptr)));
    }

    LEV_HIDE_INSTANTIATION constexpr void reset(adopt_t, Obj* ptr) noexcept {
        Py_INCREF(as_pyobject(ptr));
        Py_XDECREF(as_pyobject(exchange(ptr_, ptr)));
    }

    LEV_HIDE_INSTANTIATION constexpr void swap(python_ptr& other) noexcept {
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

template <typename To, typename From>
LEF_HIDDEN python_ptr<To> static_ptr_cast(
    python_ptr<From> const& other) noexcept {
    return python_ptr{adopt_object, py_cast<To>(other.get())};
}

template <typename To, typename From>
LEF_HIDDEN python_ptr<To> static_ptr_cast(python_ptr<From>&& other) noexcept {
    return python_ptr{retain_object, py_cast<To>(other.release())};
}

template <typename To, typename From>
LEF_HIDDEN python_ptr<To> dynamic_ptr_cast(
    python_ptr<From> const& other) noexcept {
    if (dynamic_ptr_cast<To>(other.get())) {
        return python_ptr{adopt_object, py_cast<To>(other.get())};
    } else {
        return python_ptr<To>{};
    }
}

template <typename To, typename From>
LEF_HIDDEN python_ptr<To> dynamic_ptr_cast(python_ptr<From>&& other) noexcept {
    if (dynamic_ptr_cast<To>(other.get())) {
        return python_ptr{retain_object, py_cast<To>(other.release())};
    } else {
        return nullptr;
    }
}

template <typename To, typename From>
LEF_HIDDEN python_ptr<To> exact_ptr_cast(
    python_ptr<From> const& other) noexcept {
    if (exact_ptr_cast<To>(other.get())) {
        return python_ptr{adopt_object, py_cast<To>(other.get())};
    } else {
        return nullptr;
    }
}

template <typename To, typename From>
LEF_HIDDEN python_ptr<To> exact_ptr_cast(python_ptr<From>&& other) noexcept {
    if (exact_ptr_cast<To>(other.get())) {
        return python_ptr{retain_object, py_cast<To>(other.release())};
    } else {
        return nullptr;
    }
}

template <typename Obj>
class LEV_API non_owning_ptr<Obj> : pointer_comparable<non_owning_ptr<Obj>> {

public:
    using element_type = Obj;

    LEV_HIDE_INSTANTIATION constexpr non_owning_ptr() noexcept
        : ptr_{nullptr} {}
    LEV_HIDE_INSTANTIATION constexpr non_owning_ptr(decltype(nullptr)) noexcept
        : non_owning_ptr{} {}
    LEV_HIDE_INSTANTIATION constexpr non_owning_ptr(Obj* ptr) noexcept
        : ptr_{ptr} {}

    LEV_HIDE_INSTANTIATION constexpr non_owning_ptr(
        non_owning_ptr const& other) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr non_owning_ptr& operator=(
        non_owning_ptr const& other) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr non_owning_ptr(
        non_owning_ptr&& other) noexcept = default;
    LEV_HIDE_INSTANTIATION constexpr non_owning_ptr& operator=(
        non_owning_ptr&& other) noexcept = default;

    LEV_HIDE_INSTANTIATION constexpr ~non_owning_ptr() noexcept = default;

    template <typename U>
    requires std::is_convertible_v<U*, T*>
    LEV_HIDE_INSTANTIATION constexpr non_owning_ptr(
        non_owning_ptr<U> const& other) noexcept
        : ptr_{py_cast<T>(other.get())} {}

    template <typename U>
    requires std::is_convertible_v<U*, T*>
    LEV_HIDE_INSTANTIATION constexpr non_owning_ptr& operator=(
        non_owning_ptr const& other) noexcept {
        ptr_ = other.get();
        return *this;
    }

    template <typename U>
    requires (aliasable_subobject_of<T, U> && !std::convertible_to<U*, T*>)
    LEV_HIDE_INSTANTIATION constexpr non_owning_ptr(
        non_owning_ptr<U> const& other) noexcept
        : ptr_{py_cast<T>(other.ptr_)} {}

    template <typename U>
    requires (aliasable_subobject_of<T, U> && !std::convertible_to<U*, T*>)
    LEV_HIDE_INSTANTIATION constexpr non_owning_ptr& operator=(
        non_owning_ptr<U> const& other) noexcept {
        ptr_ = py_cast<T>(other.ptr_);

        return *this;
    }

    LEV_HIDE_INSTANTIATION constexpr operator T*() const noexcept {
        return ptr_;
    }

    template <typename U>
    requires std::is_convertible_v<T*, U*>
    LEV_HIDE_INSTANTIATION constexpr operator U*() const noexcept {
        return py_cast<U>(ptr_);
    }

    template <aliasable_subobject_of<T> U>
    requires (!std::is_convertible_v<T*, U*>)
    LEV_HIDE_INSTANTIATION constexpr operator U*() const noexcept {
        return py_cast<U>(ptr_);
    }

    template <typename U>
    requires requires {
        requires (
            !std::is_convertible_v<T*, U*> && !aliasable_subobject_of<U, T>);
        py_cast<U>(ptr_);
    }
    LEV_HIDE_INSTANTIATION constexpr explicit operator U*() const noexcept {
        return py_cast<U>(ptr_);
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

template <pyobj_type T>
LEF_HIDDEN python_ptr<T> adopt(non_owning_ptr<T> ptr) noexcept {
    return python_ptr{adopt_object, ptr.get()};
}

template <pyobj_type T>
LEF_HIDDEN python_ptr<T> adopt(T* ptr) noexcept {
    return python_ptr{adopt_object, ptr};
}

template <pyobj_type T>
LEF_HIDDEN python_ptr<T> adopt(python_ptr<T> const& ptr) noexcept {
    return ptr;
}

template <typename To, typename From>
LEF_HIDDEN non_owning_ptr<To> static_ptr_cast(
    non_owning_ptr<From> other) noexcept {
    return py_cast<To>(other.get());
}

template <typename To, typename From>
LEF_HIDDEN non_owning_ptr<To> dynamic_ptr_cast(
    non_owning_ptr<From> other) noexcept {
    if (dynamic_ptr_cast<To>(other.get())) {
        return py_cast<To>(other.get());
    } else {
        return nullptr;
    }
}

template <typename To, typename From>
LEF_HIDDEN non_owning_ptr<To> exact_ptr_cast(
    non_owning_ptr<From> other) noexcept {
    if (exact_ptr_cast<To>(other.get())) {
        return py_cast<To>(other.get());
    } else {
        return nullptr;
    }
}

} // namespace lev
