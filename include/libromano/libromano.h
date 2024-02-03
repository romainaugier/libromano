/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO)
#define __LIBROMANO

#if defined(_MSC_VER)
#define ROMANO_MSVC
#pragma warning(disable:4711) /* function selected for automatic inline expansion */
#elif defined(__GNUC__)
#define ROMANO_GCC
#elif defined(__clang__)
#define ROMANO_CLANG
#endif /* defined(_MSC_VER) */

#if !defined(ROMANO_VERSION_STR)
#define ROMANO_VERSION_STR "Debug"
#endif /* !defined(ROMANO_VERSION_STR) */

#include <stddef.h>
#include <stdint.h>

#if INTPTR_MAX == INT64_MAX || defined(__x86_64__)
#define ROMANO_X64
#elif INTPTR_MAX == INT32_MAX
#define ROMANO_X86
#endif /* INTPTR_MAX == INT64_MAX || defined(__x86_64__) */

#if defined(_WIN32)
#define ROMANO_WIN
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif /* !defined(WIN32_LEAN_AND_MEAN) */
#if defined(ROMANO_X64)
#define ROMANO_PLATFORM_STR "WIN64"
#else
#define ROMANO_PLATFORM_STR "WIN32"
#endif /* defined(ROMANO_x64) */
#elif defined(__linux__)
#define ROMANO_LINUX
#if defined(ROMANO_X64)
#define ROMANO_PLATFORM_STR "LINUX64"
#else
#define ROMANO_PLATFORM_STR "LINUX32"
#endif /* defined(ROMANO_X64) */
#endif /* defined(_WIN32) */

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
#endif /* defined(ROMANO_MSVC) */

#if defined(ROMANO_WIN)
#define ROMANO_FORCE_INLINE __forceinline
#define ROMANO_DLL_ENTRY
#elif defined(ROMANO_GCC)
#define ROMANO_FORCE_INLINE inline __attribute__((always_inline)) 
#define ROMANO_DLL_ENTRY __attribute__((constructor))
#elif defined(ROMANO_CLANG)
#define ROMANO_FORCE_INLINE __attribute__((always_inline))
#define ROMANO_DLL_ENTRY __attribute__((constructor))
#endif /* defined(ROMANO_WIN) */

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

#if defined(ROMANO_WIN)
#define ROMANO_FUNCTION __FUNCTION__
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
#define ROMANO_FUNCTION __PRETTY_FUNCTION__
#endif /* ROMANO_WIN */

#define CONCAT_(prefix, suffix)     prefix##suffix
#define CONCAT(prefix, suffix)      CONCAT_(prefix, suffix)

#define STATIC_ASSERT(expr)                             \
    struct CONCAT(__outscope_assert_, __COUNTER__)      \
    {                                                   \
        char                                            \
        outscope_assert                                 \
        [2*(expr)-1];                                   \
                                                        \
    } CONCAT(__outscope_assert_, __COUNTER__)

#define ROMANO_NOT_IMPLEMENTED "Function "ROMANO_FUNCTION" not implemented" 

#endif /* !defined(__LIBROMANO) */
