// Copyright 2025, Bryan Wong

#include <Python.h>

#include <utility>

namespace lev {
namespace py {

namespace details::crtical_section {
inline bool is_supported_v = requires(PyObject* ptr) { ptr->ob_mutex; };
} // namespace details::crtical_section

class crtical_section {
public:
    crtical_section(crtical_section const&) = delete;
    crtical_section& operator=(crtical_section const&) = delete;
    template <pyobj_type T>
    LEV_HIDE_INSTANTIATION explicit inline constexpr crtical_section(
        unmanaged_ptr<T> object) noexcept {}
    template <pyobj_type T1, pyobj_type T2>
    LEV_HIDE_INSTANTIATION explicit inline constexpr crtical_section(
        unmanaged_ptr<T1> obj1, unmanaged_ptr<T1> obj2) noexcept {}

#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 13
    template <pyobj_type T>
    explicit inline constexpr crtical_section(unmanaged_ptr<T> object) noexcept
    requires details::is_supported_v
        : section{.one = {}}
        , type_{section_type::one} {
        LEV_ASSERT(object != nullptr);
        PyCriticalSection_Begin(&section_.one, object);
    }

    template <pyobj_type T1, pyobj_type T2>
    explicit inline constexpr crtical_section(
        unmanaged_ptr<T1> obj1, unmanaged_ptr<T2> obj2) noexcept
    requires details::is_supported_v
        : section{.two = {}}
        , type_{section_type::two} {
        LEV_ASSERT(obj1 != nullptr);
        if (obj2 == nullptr || obj2 == obj1) {
            section_.one = {};
            type_ = section_type::one;
            PyCriticalSection_Begin(&section_.one, obj1);
        } else if (reinterpret_cast<uintptr_t>(obj2) <
            reinterpret_cast<uintptr_t>(obj1)) {
            obj1.swap(obj2);
            PyCriticalSection2_Begin(&section_.two, obj1, obj2);
        }
    }

    LEV_HIDE_INSTANTIATION inline constexpr ~crtical_section() noexcept {
        if (type_ == section_type::one) {
            PyCriticalSection_End(&section_.one);
        } else {
            PyCriticalSection2_End(&section_.two);
        }
    }

private:
    union {
        PyCriticalSection one;
        PyCriticalSection2 two;
    } section_;
    enum class section_type : unsigned char {
        one,
        two
    } type_;
#endif
};

} // namespace py
} // namespace lev
