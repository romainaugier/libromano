/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/libromano.h"

#if defined(ROMANO_WIN)
#include "Windows.h"
#endif /* defined(ROMANO_WIN) */

typedef int32_t atomic32_t;
typedef int64_t atomic64_t;

static ROMANO_FORCE_INLINE atomic32_t atomic_load_32(atomic32_t* volatile dest)
{
#if defined(ROMANO_MSVC)
    return *(dest);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_load_n(dest, __ATOMIC_RELAXED);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE atomic64_t atomic_load_64(atomic64_t* volatile dest)
{
#if defined(ROMANO_MSVC)
    return *(dest);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_load_n(dest, __ATOMIC_RELAXED);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_store_32(atomic32_t* volatile dest, atomic32_t value)
{
#if defined(ROMANO_MSVC)
    InterlockedExchange(dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_store_n(dest, value, __ATOMIC_RELAXED);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_store_64(atomic64_t* volatile dest, atomic64_t value)
{
#if defined(ROMANO_MSVC)
    InterlockedExchange64(dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_store_n(dest, value, __ATOMIC_RELAXED);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_add_32(atomic32_t* volatile dest, atomic32_t value)
{
#if defined(ROMANO_MSVC)
    InterlockedAdd(dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __sync_add_and_fetch(dest, value);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_add_64(atomic64_t* volatile dest, atomic64_t value)
{
#if defined(ROMANO_MSVC)
    InterlockedAdd64(dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __sync_add_and_fetch(dest, value);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_sub_32(atomic64_t* volatile dest, atomic32_t value)
{
#if defined(ROMANO_MSVC)
    InterlockedAdd(dest, -value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __sync_sub_and_fetch(dest, value);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_sub_64(atomic64_t* volatile dest, atomic64_t value)
{
#if defined(ROMANO_MSVC)
    InterlockedAdd64(dest, -value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __sync_sub_and_fetch(dest, value);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE atomic32_t atomic_compare_exchange_32(atomic32_t* volatile dest, atomic32_t exchange, atomic32_t compare)
{
#if defined(ROMANO_MSVC)
    return InterlockedCompareExchange(dest, exchange, compare);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __sync_val_compare_and_swap(dest, compare, exchange);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE atomic64_t atomic_compare_exchange_64(atomic64_t* volatile dest, atomic64_t exchange, atomic64_t compare)
{
#if defined(ROMANO_MSVC)
    return InterlockedCompareExchange64(dest, exchange, compare);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __sync_val_compare_and_swap(dest, compare, exchange);
#endif /* defined(ROMANO_MSVC) */
}