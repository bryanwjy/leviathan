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

#ifndef NDEBUG
#  include <cassert>
#  define LEV_ASSERT(...) assert(__VA_ARGS__)
#else
#  define LEV_ASSERT(...)                                                   \
      static_assert(static_cast<decltype(static_cast<bool>(__VA_ARGS__))*>( \
                        0) == nullptr,                                      \
          "Invalid assert expression");
#endif

#ifdef _MSC_VER
#  define LEV_MSVC msvc::
#else
#  define LEV_MSVC
#endif

#if __has_cpp_attribute(msvc::lifetimebound)
#  define LEV_LIFETIMEBOUND [[msvc::lifetimebound]]
#elif __has_cpp_attribute( \
    clang::lifetimebound) /* UTL_HAS_CPP_ATTRIBUTE(msvc::lifetimebound) */
#  define LEV_LIFETIMEBOUND [[clang::lifetimebound]]
#elif __has_cpp_attribute( \
    gnu::lifetimebound) /* UTL_HAS_CPP_ATTRIBUTE(msvc::lifetimebound) */
#  define LEV_LIFETIMEBOUND [[gnu::lifetimebound]]
#elif __has_attribute( \
    __lifetimebound__) /* UTL_HAS_CPP_ATTRIBUTE(msvc::lifetimebound) */
#  define LEV_LIFETIMEBOUND __attribute__((__lifetimebound__))
#else
#  define LEV_LIFETIMEBOUND
#endif /* UTL_HAS_CPP_ATTRIBUTE(msvc::lifetimebound) */

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
