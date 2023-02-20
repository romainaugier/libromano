// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#pragma once

#if !defined(__LIBROMANO)
#define __LIBROMANO

#if defined(_MSC_VER)
#define ROMANO_MSVC
#elif defined(__GNUC__)
#define ROMANO_GCC
#elif defined(__clang__)
#define ROMANO_CLANG
#endif

#if !defined(ROMANO_VERSION_STR)
#define ROMANO_VERSION_STR "Debug"
#endif

#include <stdint.h>

#if INTPTR_MAX == INT64_MAX || defined(__x86_64__)
#define ROMANO_X64
#elif INTPTR_MAX == INT32_MAX
#define ROMANO_X86
#endif

#if !defined(ROMANO_PLATFORM_STR)
#if defined(_WIN32)
#define ROMANO_WIN
#if defined(ROMANO_X64)
#define ROMANO_PLATFORM_STR "WIN64"
#else
#define ROMANO_PLATFORM_STR "WIN32"
#endif
#elif defined(__linux__)
#define ROMANO_LINUX
#if defined(ROMANO_X64)
#define ROMANO_PLATFORM_STR "LINUX64"
#else
#define ROMANO_PLATFORM_STR "LINUX32"
#endif
#endif
#endif

#if defined(ROMANO_MSVC)
#define ROMANO_EXPORT __declspec(dllexport)
#define ROMANO_IMPORT __declspec(dllimport)
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
#define ROMANO_EXPORT __attribute__((visibility(default)))
#define ROMANO_IMPORT
#endif

#if defined(ROMANO_BUILD_SHARED)
#define ROMANO_API ROMANO_EXPORT
#else
#define ROMANO_API ROMANO_IMPORT
#endif

#endif // __LIBROMANO