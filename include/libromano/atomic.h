/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/libromano.h"

#if defined(ROMANO_WIN)
#include "Windows.h"
#endif /* defined(ROMANO_WIN) */

typedef long atomic32_t;
typedef long long atomic64_t;

ROMANO_FORCE_INLINE void atomic_add32(atomic32_t* volatile dest, atomic32_t value)
{
#if defined(ROMANO_WIN)
    InterlockedAdd(dest, value);
#elif defined(ROMANO_LINUX)
    __sync_add_and_fetch(dest, value);
#endif /* defined(ROMANO_WIN) */
}

ROMANO_FORCE_INLINE void atomic_add64(atomic64_t* volatile dest, atomic64_t value)
{
#if defined(ROMANO_WIN)
    InterlockedAdd64(dest, value);
#elif defined(ROMANO_LINUX)
    __sync_add_and_fetch(dest, value);
#endif /* defined(ROMANO_WIN) */
}

ROMANO_FORCE_INLINE atomic32_t atomic_compare_exchange32(atomic32_t* volatile dest, atomic32_t exchange, atomic32_t compare)
{
#if defined(ROMANO_WIN)
    return InterlockedCompareExchange(dest, exchange, compare);
#elif defined(ROMANO_LINUX)
    return __sync_val_compare_and_swap(dest, compare, exchange);
#endif /* defined(ROMANO_WIN) */
}

ROMANO_FORCE_INLINE atomic64_t atomic_compare_exchange64(atomic64_t* volatile dest, atomic64_t exchange, atomic64_t compare)
{
#if defined(ROMANO_WIN)
    return InterlockedCompareExchange64(dest, exchange, compare);
#elif defined(ROMANO_LINUX)
    return __sync_val_compare_and_swap(dest, compare, exchange);
#endif /* defined(ROMANO_WIN) */
}