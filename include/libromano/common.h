/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO)
#define __LIBROMANO

/* https://github.com/cpredef/predef/blob/master/Compilers.md */
#if defined(_MSC_VER)
#define ROMANO_MSVC
#elif defined(__GNUC__)
#define ROMANO_GCC
#elif defined(__clang__)
#define ROMANO_CLANG
#elif defined(__EMSCRIPTEN__)
#define ROMANO_EMSCRIPTEN
#elif defined(__INTEL_COMPILER) || defined(__ICC)
#define ROMANO_ICC
#elif defined(__MINGW32__)
#define ROMANO_MINGW32
#elif defined(__MINGW64__)
#define ROMANO_MINGW64
#else
#error "Unknown compiler"
#endif /* defined(_MSC_VER) */

#if !defined(ROMANO_VERSION_STR)
#define ROMANO_VERSION_STR "Debug"
#endif /* !defined(ROMANO_VERSION_STR) */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=msvc-170 */
/* https://github.com/cpredef/predef/blob/master/Architectures.md */
#if INTPTR_MAX == INT64_MAX || defined(__x86_64__) || defined(_M_AMD64)
#define ROMANO_X64
#define ROMANO_SIZEOF_PTR 8
#elif INTPTR_MAX == INT32_MAX
#define ROMANO_X86
#define ROMANO_SIZEOF_PTR 4
#elif defined(__arm__)
#define ROMANO_AARCH32
#define ROMANO_SIZEOF_PTR 4
#elif defined(__aarch64__)
#define ROMANO_AARCH64
#define ROMANO_SIZEOF_PTR 8
#endif /* INTPTR_MAX == INT64_MAX || defined(__x86_64__) || defined(_M_AMD64) */

/* https://sourceforge.net/p/predef/wiki/OperatingSystems */
#if defined(_WIN32)
#define ROMANO_WIN
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif /* !defined(WIN32_LEAN_AND_MEAN) */
#if defined(ROMANO_X64)
#define ROMANO_PLATFORM_STR "WIN_X86_64"
#elif defined(ROMANO_X86)
#define ROMANO_PLATFORM_STR "WIN_X86"
#elif defined(ROMANO_AARCH32)
#define ROMANO_PLATFORM_STR "WIN_AARCH32"
#elif defined(ROMANO_AARCH64)
#define ROMANO_PLATFORM_STR "WIN_AARCH64"
#else
#error "Unknown platform"
#endif /* defined(ROMANO_X64) */
#elif defined(__linux__)
#define ROMANO_LINUX
#if defined(ROMANO_X64)
#define ROMANO_PLATFORM_STR "LINUX_X86_64"
#elif defined(ROMANO_X86)
#define ROMANO_PLATFORM_STR "LINUX_X86"
#elif defined(ROMANO_AARCH32)
#define ROMANO_PLATFORM_STR "LINUX_AARCH32"
#elif defined(ROMANO_AARCH64)
#define ROMANO_PLATFORM_STR "LINUX_AARCH64"
#else
#error "Unknown platform"
#endif /* defined(ROMANO_X64) */
#elif defined(__APPLE__)
#define ROMANO_APPLE
#if defined(ROMANO_X64)
#define ROMANO_PLATFORM_STR "APPLE_X86_64"
#elif defined(ROMANO_X86)
#define ROMANO_PLATFORM_STR "APPLE_X86"
#elif defined(ROMANO_AARCH32)
#define ROMANO_PLATFORM_STR "APPLE_AARCH32"
#elif defined(ROMANO_AARCH64)
#define ROMANO_PLATFORM_STR "APPLE_AARCH64"
#else
#error "Unknown platform"
#endif /* defined(ROMANO_X64) */
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)
#define ROMANO_BSD
#if defined(ROMANO_X64)
#define ROMANO_PLATFORM_STR "BSD_X86_64"
#elif defined(ROMANO_X86)
#define ROMANO_PLATFORM_STR "BSD_X86"
#else 
#error "Unknown platform"
#endif /* defined(ROMANO_X64) */
#else
#error "Unknown platform"
#endif /* defined(_WIN32) */

#define ROMANO_BYTE_ORDER_UNDEFINED 0
#define ROMANO_BYTE_ORDER_LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define ROMANO_BYTE_ORDER_BIG_ENDIAN __ORDER_BIG_ENDIAN__

#define ROMANO_BYTE_ORDER __BYTE_ORDER__

#if defined(ROMANO_MSVC)
#define ROMANO_RESTRICT __restrict
#else
#define ROMANO_RESTRICT restrict
#endif /* defined(ROMANO_MSVC) */

#if defined(ROMANO_WIN)
#if defined(ROMANO_MSVC)
#define ROMANO_EXPORT __declspec(dllexport)
#define ROMANO_IMPORT __declspec(dllimport)
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
#define ROMANO_EXPORT __attribute__((dllexport))
#define ROMANO_IMPORT __attribute__((dllimport))
#endif /* defined(ROMANO_MSVC) */
#elif defined(ROMANO_LINUX)
#define ROMANO_EXPORT __attribute__((visibility("default")))
#define ROMANO_IMPORT
#endif /* defined(ROMANO_WIN) */

#if defined(ROMANO_MSVC)
#define ROMANO_FORCE_INLINE __forceinline
#define ROMANO_LIB_ENTRY
#define ROMANO_LIB_EXIT
#elif defined(ROMANO_GCC)
#define ROMANO_FORCE_INLINE inline __attribute__((always_inline)) 
#define ROMANO_LIB_ENTRY __attribute__((constructor))
#define ROMANO_LIB_EXIT __attribute__((destructor))
#elif defined(ROMANO_CLANG)
#define ROMANO_FORCE_INLINE __attribute__((always_inline))
#define ROMANO_LIB_ENTRY __attribute__((constructor))
#define ROMANO_LIB_EXIT __attribute__((destructor))
#endif /* defined(ROMANO_MSVC) */

#if defined(ROMANO_GCC) || defined(ROMANO_CLANG)
#define	ROMANO_LIKELY(x)	__builtin_expect((x) != 0, 1)
#define	ROMANO_UNLIKELY(x)	__builtin_expect((x) != 0, 0)
#else
#define	ROMANO_LIKELY(x) (x)
#define	ROMANO_UNLIKELY(x) (x)
#endif /* defined(ROMANO_GCC) || defined(ROMANO_CLANG) */

#if defined(ROMANO_BUILD_SHARED)
#define ROMANO_API ROMANO_EXPORT
#else
#define ROMANO_API ROMANO_IMPORT
#endif /* defined(ROMANO_BUILD_SHARED) */

#if defined __cplusplus
#define ROMANO_CPP_ENTER extern "C" {
#define ROMANO_CPP_END }
#else
#define ROMANO_CPP_ENTER
#define ROMANO_CPP_END
#endif /* DEFINED __cplusplus */

#if !defined NULL
#define NULL (void*)0
#endif /* !defined NULL */

#if defined(ROMANO_MSVC)
#define ROMANO_NO_VECTORIZATION __pragma(loop(no_vector))
#else
#define ROMANO_NO_VECTORIZATION
#endif /* defined(ROMANO_MSVC) */

#if defined(ROMANO_WIN)
#define ROMANO_FUNCTION __FUNCTION__
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
#define ROMANO_FUNCTION __PRETTY_FUNCTION__
#endif /* ROMANO_WIN */

#define ROMANO_STATIC_FUNCTION static

#define CONCAT_(prefix, suffix)     prefix##suffix
#define CONCAT(prefix, suffix)      CONCAT_(prefix, suffix)

#define ROMANO_ASSERT(expr, message) if(!(expr)) { fprintf(stderr, "Assertion failed in file %s at line %d: %s", __FILE__, __LINE__, message); abort(); }

#define ROMANO_STATIC_ASSERT(expr)                      \
    struct CONCAT(__outscope_assert_, __COUNTER__)      \
    {                                                   \
        char                                            \
        outscope_assert                                 \
        [2*(expr)-1];                                   \
                                                        \
    } CONCAT(__outscope_assert_, __COUNTER__)

#define ROMANO_NOT_IMPLEMENTED fprintf(stderr, "Called function " ROMANO_FUNCTION " that is not implemented (%s:%d)", __FILE__, __LINE__); exit(1)

#if defined(ROMANO_MSVC)
#define ROMANO_PACKED_STRUCT(__struct__) __pragma(pack(push, 1)) __struct__ __pragma(pack(pop))
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
#define ROMANO_PACKED_STRUCT(__struct__) __struct__ __attribute__((__packed__))
#else
#define ROMANO_PACKED_STRUCT(__struct__) __struct__
#endif /* defined(ROMANO_MSVC) */

#if defined(ROMANO_CLANG)
#define dump_struct(s) __builtin_dump_struct(s, printf)
#else
#define dump_struct(s) 
#endif /* defined(ROMANO_CLANG) */

#if defined(DEBUG_BUILD)
#define ROMANO_DEBUG 1
#else
#define ROMANO_DEBUG 0
#endif /* defined(DEBUG_BUILD) */

#endif /* !defined(__LIBROMANO) */
