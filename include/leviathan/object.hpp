// Copyright 2025, Bryan Wong
#pragma once

#include "leviathan/instantiation.hpp"

#include <Python.h>

#include <concepts>

namespace lev {

class LEV_API object_root : public PyObject {
protected:
    object_root(object_root const&) = delete;
    object_root& operator=(object_root const&) = delete;

    LEV_HIDDEN inline object_root(python_ptr<PyTypeObject> type) noexcept
        : PyObject{} {
        PyObject_Init(static_cast<PyObject*>(this), type.release());
    }

    LEV_HIDDEN inline constexpr ~object_root() noexcept = default;
};

template <typename T, typename Instantiator = vector_instantiation>
class LEV_API basic_object;

template <typename T>
class LEV_API basic_object<T, basic_instantiation> : public object_root {
public:
    using instantiation_concept = basic_instantiation;

    LEV_HIDE_INSTANTIATION inline basic_object() noexcept
        : object_root{adaptor_traits<T>::type_object(adopt_object)} {}

    template <typename... Args>
    requires std::is_constructible_v<T, Args>
    LEV_HIDE_INSTANTIATION inline basic_object(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, Args>)
        : object_root{adaptor_traits<T>::type_object(adopt_object)}
        , storage_{std::in_place, std::forward<Args>(args)...} {}

    LEV_HIDE_INSTANTIATION inline constexpr bool initialized() const noexcept {
        return storage_.has_value();
    }

    LEV_HIDE_INSTANTIATION inline constexpr operator T const&() const noexcept {
        return *storage_;
    }

    LEV_HIDE_INSTANTIATION inline constexpr operator T&() noexcept {
        return *storage_;
    }

    LEV_HIDE_INSTANTIATION inline constexpr
    operator T const&&() const&& noexcept {
        return std::move(*storage_);
    }

    LEV_HIDE_INSTANTIATION inline constexpr operator T&&() && noexcept {
        return std::move(*storage_);
    }

protected:
    LEV_HIDE_INSTANTIATION constexpr ~basic_object() noexcept = default;

    LEV_HIDE_INSTANTIATION T const& self() const noexcept { return *storage_; }

    LEV_HIDE_INSTANTIATION T& self() noexcept { return *storage_; }

    template <typename... Args>
    requires std::is_constructible_v<T, Args>
    LEV_HIDE_INSTANTIATION void initialize(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, Args>) {
        LEV_ASSERT(!storage_);
        storage_.emplace(std::forward<Args>(args)...);
    }

private:
    std::optional<T> storage_;
};

template <typename T, instantiation_concept I>
class LEV_API basic_object<T, I> : public object_root {
    using traits = adaptor_traits<T>;

public:
    using instantiation_concept = I;

    template <typename... Args>
    requires std::constructible_from<T, Args...> &&
                 std::is_nothrow_constructible_v<T, Args...>
    LEV_HIDE_INSTANTIATION explicit(is_explicit_constructible_v<T,
        Args...>) inline constexpr basic_object(Args&&... args) noexcept
        : object_root{traits::type_object(adopt_object)}
        , object_{args...} {}

    template <typename... Args>
    requires std::constructible_from<T, Args...>
    LEV_HIDE_INSTANTIATION explicit(is_explicit_constructible_v<T, Args...>)
        basic_object(Args&&... args) try
        : object_root{traits::type_object(adopt_object)}
        , object_{args...} {
    } catch (std::exception const& error) {
        // TODO
    } catch (...) {
        // TODO
    }

    LEV_HIDE_INSTANTIATION inline constexpr operator T const&() const noexcept {
        return object_;
    }

    LEV_HIDE_INSTANTIATION inline constexpr operator T&() noexcept {
        return object_;
    }

    LEV_HIDE_INSTANTIATION inline constexpr
    operator T const&&() const&& noexcept {
        return std::move(object_);
    }

    LEV_HIDE_INSTANTIATION inline constexpr operator T&&() && noexcept {
        return std::move(object_);
    }

protected:
    LEV_HIDE_INSTANTIATION T const& self() const noexcept { return object_; }

    LEV_HIDE_INSTANTIATION T& self() noexcept { return object_; }

    LEV_HIDE_INSTANTIATION constexpr ~basic_object() noexcept = default;

private:
    T object_;
};

template <typename T, instantiation_concept I>
requires (std::is_void_v<T>)
class LEV_API basic_object<T, I> : public object_root {
public:
    using instantiation_concept = I;

    LEV_HIDE_INSTANTIATION inline constexpr basic_object(
        python_ptr<PyObject> ptr) noexcept
        : object_root{std::move(ptr)} {}

protected:
    LEV_HIDE_INSTANTIATION constexpr ~basic_object() noexcept = default;
};

} // namespace lev
