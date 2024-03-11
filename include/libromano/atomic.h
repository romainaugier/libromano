/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/libromano.h"
#include "libromano/bool.h"

#if defined(ROMANO_WIN)
#include "Windows.h"
#endif /* defined(ROMANO_WIN) */

typedef int32_t atomic32_t;
typedef int64_t atomic64_t;

static ROMANO_FORCE_INLINE atomic32_t atomic_load_32(atomic32_t* volatile dest)
{
#if defined(ROMANO_MSVC)
    return InterlockedOr((LONG*)dest, 0);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_load_n(dest, __ATOMIC_RELAXED);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE atomic64_t atomic_load_64(atomic64_t* volatile dest)
{
#if defined(ROMANO_MSVC)
    return InterlockedOr64((LONG64*)dest, 0);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_load_n(dest, __ATOMIC_RELAXED);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_store_32(atomic32_t* volatile dest, atomic32_t value)
{
#if defined(ROMANO_MSVC)
    InterlockedExchange((LONG*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_store_n(dest, value, __ATOMIC_RELAXED);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_store_64(atomic64_t* volatile dest, atomic64_t value)
{
#if defined(ROMANO_MSVC)
    InterlockedExchange64((LONG64*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_store_n(dest, value, __ATOMIC_RELAXED);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_add_32(atomic32_t* volatile dest, atomic32_t value)
{
#if defined(ROMANO_MSVC)
    InterlockedAdd((LONG*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __sync_add_and_fetch(dest, value);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_add_64(atomic64_t* volatile dest, atomic64_t value)
{
#if defined(ROMANO_MSVC)
    InterlockedAdd64((LONG64*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __sync_add_and_fetch(dest, value);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_sub_32(atomic32_t* volatile dest, atomic32_t value)
{
#if defined(ROMANO_MSVC)
    _InlineInterlockedAdd((LONG*)dest, -value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __sync_sub_and_fetch(dest, value);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_sub_64(atomic64_t* volatile dest, atomic64_t value)
{
#if defined(ROMANO_MSVC)
    _InlineInterlockedAdd64((LONG64*)dest, -value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __sync_sub_and_fetch(dest, value);
#endif /* defined(ROMANO_MSVC) */
}

/* All compare exchange functions return a bool if the exchange has been successful */

static ROMANO_FORCE_INLINE bool atomic_compare_exchange_32(atomic32_t* volatile dest, atomic32_t exchange, atomic32_t compare)
{
#if defined(ROMANO_MSVC)
    return (bool)(_InterlockedCompareExchange((LONG*)dest, exchange, compare) == compare);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __sync_bool_compare_and_swap(dest, compare, exchange);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE bool atomic_compare_exchange_64(atomic64_t* volatile dest, atomic64_t exchange, atomic64_t compare)
{
#if defined(ROMANO_MSVC)
    return (bool)(_InterlockedCompareExchange64((LONG64*)dest, exchange, compare) == compare);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __sync_bool_compare_and_swap(dest, compare, exchange);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE atomic32_t atomic_exchange_32(atomic32_t* volatile dest, atomic32_t exchange)
{
#if defined(ROMANO_MSVC)
    return _InterlockedExchange((LONG*)dest, exchange);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __sync_lock_test_and_set(dest, exchange);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE atomic64_t atomic_exchange_64(atomic64_t* volatile dest, atomic64_t exchange)
{
#if defined(ROMANO_MSVC)
    return _InterlockedExchange64((LONG64*)dest, exchange);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __sync_lock_test_and_set(dest, exchange);
#endif /* defined(ROMANO_MSVC) */
}