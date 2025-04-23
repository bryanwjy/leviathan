// Copyright 2025, Bryan Wong
#pragma once

#ifdef LEVIATHAN_LIBRARY

#  define LEV_API __attribute__((__visibility__("default")))

#  define LEV_HIDDEN __attribute__((__visibility__("hidden")))

#  if __has_attribute(__exclude_from_explicit_instantiation__)
#    define LEV_HIDE_INSTANTIATION               \
        __attribute__((__visibility__("hidden"), \
            __exclude_from_explicit_instantiation__))
#  else
#    define LEV_HIDE_INSTANTIATION \
        __attribute__((__visibility__("hidden"), __always_inline__))
#  endif

#else

#  define LEV_HIDDEN __attribute__((__visibility__("hidden")))
#  define LEV_API LEV_HIDDEN
#  define LEV_HIDE_INSTANTIATION LEV_HIDDEN

#endif // LEVIATHAN_LIBRARY

#ifdef _MSC_VER
#  define LEV_MSVC msvc::
#else
#  define LEV_MSVC
#endif

#if __has_cpp_attribute(msvc::no_unique_address)
#  define LEV_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
#  define LEV_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#  error "Compilation will result in invalid ABI"
#endif

#if __has_cpp_attribute(msvc::lifetimebound)
#  define LEV_LIFETIMEBOUND [[msvc::lifetimebound]]
#elif __has_cpp_attribute( \
    clang::lifetimebound) /* LEV_HAS_CPP_ATTRIBUTE(msvc::lifetimebound) */
#  define LEV_LIFETIMEBOUND [[clang::lifetimebound]]
#elif __has_cpp_attribute( \
    gnu::lifetimebound) /* LEV_HAS_CPP_ATTRIBUTE(msvc::lifetimebound) */
#  define LEV_LIFETIMEBOUND [[gnu::lifetimebound]]
#elif __has_attribute( \
    __lifetimebound__) /* LEV_HAS_CPP_ATTRIBUTE(msvc::lifetimebound) */
#  define LEV_LIFETIMEBOUND __attribute__((__lifetimebound__))
#else
#  define LEV_LIFETIMEBOUND
#endif /* LEV_HAS_CPP_ATTRIBUTE(msvc::lifetimebound) */

// TODO: Look into integrating with UTL's configuration library
#if __has_cpp_attribute(gnu::pure)
#  define LEV_PURE [[gnu::pure]]
#elif __has_cpp_attribute(clang::pure)
#  define LEV_PURE [[clang::pure]]
#elif __has_cpp_attribute(msvc::pure)
#  define LEV_PURE [[msvc::pure]]
#else
#  define LEV_PURE
#endif

#if __has_cpp_attribute(gnu::const)
#  define LEV_CONST [[gnu::const]]
#elif __has_cpp_attribute(clang::const)
#  define LEV_CONST [[clang::const]]
#elif __has_cpp_attribute(msvc::const)
#  define LEV_CONST [[msvc::const]]
#else
#  define LEV_CONST
#endif

#if __has_cpp_attribute(clang::reinitializes)
#  define LEV_REINITIALIZES [[clang::reinitializes]]
#else
#  define LEV_REINITIALIZES
#endif

#if __has_cpp_attribute(gnu::always_inline)
#  define LEV_ALWAYS_INLINE [[gnu::always_inline]]
#else
#  define LEV_ALWAYS_INLINE __forceinline
#endif

#ifdef __GNUC__ // GCC 4.8+, Clang, Intel and other compilers compatible with
                // GCC (-std=c++0x or above)
#  define LEV_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER) // MSVC
#  define LEV_UNREACHABLE() __assume(false)
#else // ???
#  error "No Unreachable"
#endif

#define LEV_THROW(...) throw __VA_ARGS__
#define LEV_TRY try
#define LEV_CATCH(...) catch (__VA_ARGS__)
#define LEV_RETHROW() throw

#ifdef _LIBCPP_ABI_NAMESPACE
#  define LEV_STD_ABI_NAMESPACE_BEGIN inline namespace _LIBCPP_ABI_NAMESPACE {
#  define LEV_STD_ABI_NAMESPACE_END }
#elif defined(_GLIBCXX_BEGIN_NAMESPACE_VERSION)
#  define LEV_STD_ABI_NAMESPACE_BEGIN _GLIBCXX_BEGIN_NAMESPACE_VERSION
#  define LEV_STD_ABI_NAMESPACE_END _GLIBCXX_END_NAMESPACE_VERSION
#endif

#ifndef LEV_STD_ABI_NAMESPACE_BEGIN
#  define LEV_STD_ABI_NAMESPACE_BEGIN
#  define LEV_STD_ABI_NAMESPACE_END
#endif

#ifndef LEV_DISABLE_UNSAFE_API_WARNINGS
#  define LEV_UNSAFE_API                                                    \
      [[deprecated("This API relies on internal Python implementation and " \
                   "may break depending on Python configuration")]]
#else
#  define LEV_UNSAFE_API
#endif

/* extern C++ for MSVC > C++20, no effect anywhere else */
#define LEV_STD_NAMESPACE_BEGIN                                              \
    LEV_EXTERN_CXX_BEGIN namespace LEV_ATTRIBUTE(TYPE_VISIBILITY("default")) \
        std {                                                                \
        LEV_STD_ABI_NAMESPACE_BEGIN

#define LEV_STD_NAMESPACE_END \
    LEV_STD_ABI_NAMESPACE_END \
    }                         \
    LEV_EXTERN_CXX_END

#define __LTL ::ltl::
#define __LEV ::lev::

#ifdef __cpp_contracts >= 202502L
#  define LEV_CONTRACT_PRE(...) pre(__VA_ARGS__)
#  define LEV_CONTRACT_POST(...) post(__VA_ARGS__)
#  define LEV_CONTRACT_ASSERT(...) contract_assert(__VA_ARGS__)

#  define LEV_SUPPORTS_CONTRACTS 1
#else
#  define LEV_CONTRACT_PRE(...)
#  define LEV_CONTRACT_POST(...)
#  define LEV_CONTRACT_ASSERT(...) LEV_ASSERT(__VA_ARGS__)
#endif

#if !LEV_SUPPORTS_CONTRACTS && !defined(NDEBUG)
#  include <cassert>
#  define LEV_ASSERT(...) assert(__VA_ARGS__)
#else
#  define LEV_ASSERT(...)                                                   \
      static_assert(static_cast<decltype(static_cast<bool>(__VA_ARGS__))*>( \
                        0) == nullptr,                                      \
          "Invalid assert expression");
#endif

#ifndef __cpp_lib_start_lifetime_as
// warning suppression
#  define __cpp_lib_start_lifetime_as 0
#endif

#define LEV_PYTHON_VERSION_GE(MAJOR, MINOR, PATCH) \
    PY_MAJOR_VERSION > MAJOR ||                    \
        (PY_MAJOR_VERSION == MAJOR &&              \
            (PY_MINOR_VERSION > MINOR ||           \
                (PY_MINOR_VERSION == MINOR && PY_PATCH_VERSION >= PATCH)))

#define LEV_PYTHON_VERSION_GT(MAJOR, MINOR, PATCH) \
    PY_MAJOR_VERSION > MAJOR ||                    \
        (PY_MAJOR_VERSION == MAJOR &&              \
            (PY_MINOR_VERSION > MINOR ||           \
                (PY_MINOR_VERSION == MINOR && PY_PATCH_VERSION > PATCH)))

#define LEV_PYTHON_VERSION_LT(MAJOR, MINOR, PATCH) \
    !(LEV_PYTHON_VERSION_GE(MAJOR, MINOR, PATCH))

#define LEV_PYTHON_VERSION_LE(MAJOR, MINOR, PATCH) \
    !LEV_PYTHON_VERSION_GT(MAJOR, MINOR, PATCH)
