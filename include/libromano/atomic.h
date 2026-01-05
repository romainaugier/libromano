/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIB_ROMANO_ATOMIC)
#define __LIB_ROMANO_ATOMIC

#include "libromano/common.h"

#include <intrin.h>

#if defined(ROMANO_WIN)
#include "Windows.h"
#endif /* defined(ROMANO_WIN) */

ROMANO_CPP_ENTER

typedef int32_t Atomic32;
typedef int64_t Atomic64;

#if defined(ROMANO_WIN)
typedef enum {
    MemoryOrder_Relax,
    MemoryOrder_Consume,
    MemoryOrder_Acquire,
    MemoryOrder_Release,
    MemoryOrder_AcqRel,
    MemoryOrder_SeqCst,
} MemoryOrder;
#elif defined(ROMANO_LINUX)
typedef enum {
    MemoryOrder_Relax = __ATOMIC_RELAXED,
    MemoryOrder_Consume = __ATOMIC_CONSUME,
    MemoryOrder_Acquire = __ATOMIC_ACQUIRE,
    MemoryOrder_Release = __ATOMIC_RELEASE,
    MemoryOrder_AcqRel = __ATOMIC_ACQ_REL,
    MemoryOrder_SeqCst = __ATOMIC_SEQ_CST,
} MemoryOrder;
#endif /* defined(ROMANO_WIN) */

static ROMANO_FORCE_INLINE Atomic32 atomic_load_32(Atomic32* volatile dest,
                                                   MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    return InterlockedOr((LONG*)dest, 0);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_load_n(dest, mo);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE Atomic64 atomic_load_64(Atomic64* volatile dest,
                                                   MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    return InterlockedOr64((LONG64*)dest, 0);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_load_n(dest, mo);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_store_32(Atomic32* volatile dest,
                                                Atomic32 value,
                                                MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    InterlockedExchange((LONG*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_store_n(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_store_64(Atomic64* volatile dest,
                                                Atomic64 value,
                                                MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    InterlockedExchange64((LONG64*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_store_n(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_add_32(Atomic32* volatile dest,
                                              Atomic32 value,
                                              MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    InterlockedAdd((LONG*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_add_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_add_64(Atomic64* volatile dest,
                                              Atomic64 value,
                                              MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    InterlockedAdd64((LONG64*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_add_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_sub_32(Atomic32* volatile dest, Atomic32 value,
                                              MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    _InlineInterlockedAdd((LONG*)dest, -value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_sub_fetch(dest, value);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_sub_64(Atomic64* volatile dest, Atomic64 value,
                                              MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    _InlineInterlockedAdd64((LONG64*)dest, -value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_fetch_sub(dest, value);
#endif /* defined(ROMANO_MSVC) */
}

/* All compare exchange functions return a bool if the exchange has been successful */

static ROMANO_FORCE_INLINE bool atomic_compare_exchange_weak_32(Atomic32* volatile dest,
                                                                Atomic32 exchange,
                                                                Atomic32 compare,
                                                                MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    return (bool)(_InterlockedCompareExchange((LONG*)dest, exchange, compare) == compare);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_compare_exchange_n(dest, &compare, exchange, true, mo, mo);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE bool atomic_compare_exchange_strong_32(Atomic32* volatile dest,
                                                                  Atomic32 exchange,
                                                                  Atomic32 compare,
                                                                  MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    return (bool)(_InterlockedCompareExchange((LONG*)dest, exchange, compare) == compare);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_compare_exchange_n(dest, &compare, exchange, false, mo, mo);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE bool atomic_compare_exchange_weak_64(Atomic64* volatile dest,
                                                                Atomic64 exchange,
                                                                Atomic64 compare,
                                                                MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    return (bool)(_InterlockedCompareExchange64((LONG64*)dest, exchange, compare) == compare);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_compare_exchange_n(dest, &compare, exchange, false, mo, mo);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE bool atomic_compare_exchange_strong_64(Atomic64* volatile dest,
                                                                  Atomic64 exchange,
                                                                  Atomic64 compare,
                                                                  MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    return (bool)(_InterlockedCompareExchange64((LONG64*)dest, exchange, compare) == compare);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_compare_exchange_n(dest, &compare, exchange, false, mo, mo);
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE Atomic32 atomic_exchange_32(Atomic32* volatile dest,
                                                       Atomic32 exchange,
                                                       MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    return _InterlockedExchange((LONG*)dest, exchange);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    Atomic32 ret;
    __atomic_exchange(dest, &exchange, &ret, mo);
    return ret;
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE Atomic64 atomic_exchange_64(Atomic64* volatile dest,
                                                       Atomic64 exchange,
                                                       MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    return _InterlockedExchange64((LONG64*)dest, exchange);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    Atomic64 ret;
    __atomic_exchange(dest, &exchange, &ret, mo);
    return ret;
#endif /* defined(ROMANO_MSVC) */
}

static ROMANO_FORCE_INLINE void atomic_thread_fence(MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    switch(mo)
    {
        case MemoryOrder_Relax:
            /* no-op */
            break;
        case MemoryOrder_Consume:
        case MemoryOrder_Acquire:
            _ReadBarrier();
            break;
        case MemoryOrder_Release:
            _WriteBarrier();
            break;
        case MemoryOrder_AcqRel:
        case MemoryOrder_SeqCst:
            _ReadWriteBarrier();
            break;
    }
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_thread_fence(mo);
#endif /* defined(ROMANO_MSVC) */
}

ROMANO_CPP_END

#endif /* !defined(__LIB_ROMANO_ATOMIC) */
