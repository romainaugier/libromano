/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIB_ROMANO_ATOMIC)
#define __LIB_ROMANO_ATOMIC

#include "libromano/common.h"

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
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
typedef enum {
    MemoryOrder_Relax = __ATOMIC_RELAXED,
    MemoryOrder_Consume = __ATOMIC_CONSUME,
    MemoryOrder_Acquire = __ATOMIC_ACQUIRE,
    MemoryOrder_Release = __ATOMIC_RELEASE,
    MemoryOrder_AcqRel = __ATOMIC_ACQ_REL,
    MemoryOrder_SeqCst = __ATOMIC_SEQ_CST,
} MemoryOrder;
#endif /* defined(ROMANO_WIN) */

/*
 * Atomically loads a 32-bit value from *dest
 * dest: pointer to the atomic variable
 * mo: memory order
 * Returns: the loaded value
 */
ROMANO_FORCE_INLINE Atomic32 atomic_load_32(Atomic32* volatile dest,
                                            MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    return InterlockedOr((LONG*)dest, 0);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_load_n(dest, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically loads a 64-bit value from *dest
 * dest: pointer to the atomic variable
 * mo: memory order
 * Returns: the loaded value
 */
ROMANO_FORCE_INLINE Atomic64 atomic_load_64(Atomic64* volatile dest,
                                            MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    return InterlockedOr64((LONG64*)dest, 0);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_load_n(dest, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically stores a 32-bit value into *dest
 * dest: pointer to the atomic variable
 * value: value to store
 * mo: memory order
 */
ROMANO_FORCE_INLINE void atomic_store_32(Atomic32* volatile dest,
                                         Atomic32 value,
                                         MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    InterlockedExchange((LONG*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_store_n(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically stores a 64-bit value into *dest
 * dest: pointer to the atomic variable
 * value: value to store
 * mo: memory order
 */
ROMANO_FORCE_INLINE void atomic_store_64(Atomic64* volatile dest,
                                         Atomic64 value,
                                         MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    InterlockedExchange64((LONG64*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_store_n(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically adds a 32-bit value to *dest
 * dest: pointer to the atomic variable
 * value: value to add
 * mo: memory order
 */
ROMANO_FORCE_INLINE void atomic_add_32(Atomic32* volatile dest,
                                       Atomic32 value,
                                       MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    InterlockedAdd((LONG*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_add_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically adds a 32-bit value to *dest and returns the new value
 * dest: pointer to the atomic variable
 * value: value to add
 * mo: memory order
 * Returns: the new value after the addition
 */
ROMANO_FORCE_INLINE Atomic32 atomic_fetch_add_32(Atomic32* volatile dest,
                                                 Atomic32 value,
                                                 MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    return (Atomic32)InterlockedAdd((LONG*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return (Atomic32)__atomic_add_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
    return 0;
}

/*
 * Atomically adds a 64-bit value to *dest
 * dest: pointer to the atomic variable
 * value: value to add
 * mo: memory order
 */
ROMANO_FORCE_INLINE void atomic_add_64(Atomic64* volatile dest,
                                       Atomic64 value,
                                       MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    InterlockedAdd64((LONG64*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_add_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically adds a 64-bit value to *dest and returns the new value
 * dest: pointer to the atomic variable
 * value: value to add
 * mo: memory order
 * Returns: the new value after the addition
 */
ROMANO_FORCE_INLINE Atomic64 atomic_fetch_add_64(Atomic64* volatile dest,
                                                 Atomic64 value,
                                                 MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    return (Atomic64)InterlockedAdd64((LONG64*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return (Atomic64)__atomic_add_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
    return 0;
}

/*
 * Atomically subtracts a 32-bit value from *dest
 * dest: pointer to the atomic variable
 * value: value to subtract
 * mo: memory order
 */
ROMANO_FORCE_INLINE void atomic_sub_32(Atomic32* volatile dest,
                                       Atomic32 value,
                                       MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    _InlineInterlockedAdd((LONG*)dest, -value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_sub_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically subtracts a 64-bit value from *dest
 * dest: pointer to the atomic variable
 * value: value to subtract
 * mo: memory order
 */
ROMANO_FORCE_INLINE void atomic_sub_64(Atomic64* volatile dest,
                                       Atomic64 value,
                                       MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    _InlineInterlockedAdd64((LONG64*)dest, -value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_sub_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically subtracts a 32-bit value from *dest and returns the new value
 * dest: pointer to the atomic variable
 * value: value to subtract
 * mo: memory order
 * Returns: the new value after the subtraction
 */
ROMANO_FORCE_INLINE Atomic32 atomic_fetch_sub_32(Atomic32* volatile dest,
                                                 Atomic32 value,
                                                 MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    return (Atomic32)InterlockedAdd((LONG*)dest, -value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return (Atomic32)__atomic_sub_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
    return 0;
}

/*
 * Atomically subtracts a 64-bit value from *dest and returns the new value
 * dest: pointer to the atomic variable
 * value: value to subtract
 * mo: memory order
 * Returns: the new value after the subtraction
 */
ROMANO_FORCE_INLINE Atomic64 atomic_fetch_sub_64(Atomic64* volatile dest,
                                                 Atomic64 value,
                                                 MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    return (Atomic64)InterlockedAdd64((LONG64*)dest, -value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return (Atomic64)__atomic_sub_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
    return 0;
}

/*
 * Atomically performs a bitwise AND on *dest with value
 * dest: pointer to the atomic variable
 * value: value to AND with
 * mo: memory order
 */
ROMANO_FORCE_INLINE void atomic_and_32(Atomic32* volatile dest,
                                       Atomic32 value,
                                       MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    InterlockedAnd((LONG*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_and_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically performs a bitwise AND on *dest with value and returns the new value
 * dest: pointer to the atomic variable
 * value: value to AND with
 * mo: memory order
 * Returns: the new value after the AND operation
 */
ROMANO_FORCE_INLINE Atomic32 atomic_fetch_and_32(Atomic32* volatile dest,
                                                 Atomic32 value,
                                                 MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    LONG old = InterlockedAnd((LONG*)dest, value);
    return (Atomic32)(old & value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return (Atomic32)__atomic_and_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
    return 0;
}

/*
 * Atomically performs a bitwise AND on *dest with value (64-bit)
 * dest: pointer to the atomic variable
 * value: value to AND with
 * mo: memory order
 */
ROMANO_FORCE_INLINE void atomic_and_64(Atomic64* volatile dest,
                                       Atomic64 value,
                                       MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    InterlockedAnd64((LONG64*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_and_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically performs a bitwise AND on *dest with value and returns the new value (64-bit)
 * dest: pointer to the atomic variable
 * value: value to AND with
 * mo: memory order
 * Returns: the new value after the AND operation
 */
ROMANO_FORCE_INLINE Atomic64 atomic_fetch_and_64(Atomic64* volatile dest,
                                                 Atomic64 value,
                                                 MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    LONG64 old = InterlockedAnd64((LONG64*)dest, value);
    return (Atomic64)(old & value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return (Atomic64)__atomic_and_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
    return 0;
}

/*
 * Atomically performs a bitwise OR on *dest with value
 * dest: pointer to the atomic variable
 * value: value to OR with
 * mo: memory order
 */
ROMANO_FORCE_INLINE void atomic_or_32(Atomic32* volatile dest,
                                      Atomic32 value,
                                      MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    InterlockedOr((LONG*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_or_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically performs a bitwise OR on *dest with value and returns the new value
 * dest: pointer to the atomic variable
 * value: value to OR with
 * mo: memory order
 * Returns: the new value after the OR operation
 */
ROMANO_FORCE_INLINE Atomic32 atomic_fetch_or_32(Atomic32* volatile dest,
                                                Atomic32 value,
                                                MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    LONG old = InterlockedOr((LONG*)dest, value);
    return (Atomic32)(old | value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return (Atomic32)__atomic_or_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
    return 0;
}

/*
 * Atomically performs a bitwise OR on *dest with value (64-bit)
 * dest: pointer to the atomic variable
 * value: value to OR with
 * mo: memory order
 */
ROMANO_FORCE_INLINE void atomic_or_64(Atomic64* volatile dest,
                                      Atomic64 value,
                                      MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    InterlockedOr64((LONG64*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_or_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically performs a bitwise OR on *dest with value and returns the new value (64-bit)
 * dest: pointer to the atomic variable
 * value: value to OR with
 * mo: memory order
 * Returns: the new value after the OR operation
 */
ROMANO_FORCE_INLINE Atomic64 atomic_fetch_or_64(Atomic64* volatile dest,
                                                Atomic64 value,
                                                MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    LONG64 old = InterlockedOr64((LONG64*)dest, value);
    return (Atomic64)(old | value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return (Atomic64)__atomic_or_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
    return 0;
}

/*
 * Atomically performs a bitwise XOR on *dest with value
 * dest: pointer to the atomic variable
 * value: value to XOR with
 * mo: memory order
 */
ROMANO_FORCE_INLINE void atomic_xor_32(Atomic32* volatile dest,
                                       Atomic32 value,
                                       MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    InterlockedXor((LONG*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_xor_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically performs a bitwise XOR on *dest with value and returns the new value
 * dest: pointer to the atomic variable
 * value: value to XOR with
 * mo: memory order
 * Returns: the new value after the XOR operation
 */
ROMANO_FORCE_INLINE Atomic32 atomic_fetch_xor_32(Atomic32* volatile dest,
                                                 Atomic32 value,
                                                 MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    LONG old = InterlockedXor((LONG*)dest, value);
    return (Atomic32)(old ^ value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return (Atomic32)__atomic_xor_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
    return 0;
}

/*
 * Atomically performs a bitwise XOR on *dest with value (64-bit)
 * dest: pointer to the atomic variable
 * value: value to XOR with
 * mo: memory order
 */
ROMANO_FORCE_INLINE void atomic_xor_64(Atomic64* volatile dest,
                                       Atomic64 value,
                                       MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    InterlockedXor64((LONG64*)dest, value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    __atomic_xor_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically performs a bitwise XOR on *dest with value and returns the new value (64-bit)
 * dest: pointer to the atomic variable
 * value: value to XOR with
 * mo: memory order
 * Returns: the new value after the XOR operation
 */
ROMANO_FORCE_INLINE Atomic64 atomic_fetch_xor_64(Atomic64* volatile dest,
                                                 Atomic64 value,
                                                 MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    LONG64 old = InterlockedXor64((LONG64*)dest, value);
    return (Atomic64)(old ^ value);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return (Atomic64)__atomic_xor_fetch(dest, value, mo);
#endif /* defined(ROMANO_MSVC) */
    return 0;
}

/* All compare exchange functions return a bool if the exchange has been successful */

#if defined(ROMANO_GCC) || defined(ROMANO_CLANG)
/*
 * The failure path of a compare-exchange is a plain load: it cannot use a release or acq_rel
 * order (GCC/Clang reject or silently strengthen it). Derive the strongest valid one.
 */
ROMANO_FORCE_INLINE int atomic__cas_failure_order(MemoryOrder mo)
{
    return mo == MemoryOrder_Release ? __ATOMIC_RELAXED :
           mo == MemoryOrder_AcqRel ? __ATOMIC_ACQUIRE : (int)mo;
}
#endif /* defined(ROMANO_GCC) || defined(ROMANO_CLANG) */

/*
 * Atomically compares *dest with compare and, if equal, stores exchange
 * This is the weak variant and may fail spuriously.
 * dest: pointer to the atomic variable
 * exchange: value to store if comparison succeeds
 * compare: value to compare against
 * mo: memory order
 * Returns: true if the exchange was performed, false otherwise
 */
ROMANO_FORCE_INLINE bool atomic_compare_exchange_weak_32(Atomic32* volatile dest,
                                                         Atomic32 exchange,
                                                         Atomic32 compare,
                                                         MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    return (bool)(_InterlockedCompareExchange((LONG*)dest, exchange, compare) == compare);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_compare_exchange_n(dest, &compare, exchange, true, mo, atomic__cas_failure_order(mo));
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically compares *dest with compare and, if equal, stores exchange
 * This is the strong variant and will not fail spuriously.
 * dest: pointer to the atomic variable
 * exchange: value to store if comparison succeeds
 * compare: value to compare against
 * mo: memory order
 * Returns: true if the exchange was performed, false otherwise
 */
ROMANO_FORCE_INLINE bool atomic_compare_exchange_strong_32(Atomic32* volatile dest,
                                                           Atomic32 exchange,
                                                           Atomic32 compare,
                                                           MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    return (bool)(_InterlockedCompareExchange((LONG*)dest, exchange, compare) == compare);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_compare_exchange_n(dest, &compare, exchange, false, mo, atomic__cas_failure_order(mo));
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically compares *dest with compare and, if equal, stores exchange (64-bit)
 * This is the weak variant and may fail spuriously.
 * dest: pointer to the atomic variable
 * exchange: value to store if comparison succeeds
 * compare: value to compare against
 * mo: memory order
 * Returns: true if the exchange was performed, false otherwise
 */
ROMANO_FORCE_INLINE bool atomic_compare_exchange_weak_64(Atomic64* volatile dest,
                                                         Atomic64 exchange,
                                                         Atomic64 compare,
                                                         MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    return (bool)(_InterlockedCompareExchange64((LONG64*)dest, exchange, compare) == compare);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_compare_exchange_n(dest, &compare, exchange, true, mo, atomic__cas_failure_order(mo));
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically compares *dest with compare and, if equal, stores exchange (64-bit)
 * This is the strong variant and will not fail spuriously.
 * dest: pointer to the atomic variable
 * exchange: value to store if comparison succeeds
 * compare: value to compare against
 * mo: memory order
 * Returns: true if the exchange was performed, false otherwise
 */
ROMANO_FORCE_INLINE bool atomic_compare_exchange_strong_64(Atomic64* volatile dest,
                                                           Atomic64 exchange,
                                                           Atomic64 compare,
                                                           MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    return (bool)(_InterlockedCompareExchange64((LONG64*)dest, exchange, compare) == compare);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __atomic_compare_exchange_n(dest, &compare, exchange, false, mo, atomic__cas_failure_order(mo));
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically exchanges *dest with exchange and returns the previous value
 * dest: pointer to the atomic variable
 * exchange: value to store
 * mo: memory order
 * Returns: the previous value
 */
ROMANO_FORCE_INLINE Atomic32 atomic_exchange_32(Atomic32* volatile dest,
                                                Atomic32 exchange,
                                                MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    return _InterlockedExchange((LONG*)dest, exchange);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    Atomic32 ret;
    __atomic_exchange(dest, &exchange, &ret, mo);
    return ret;
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Atomically exchanges *dest with exchange and returns the previous value (64-bit)
 * dest: pointer to the atomic variable
 * exchange: value to store
 * mo: memory order
 * Returns: the previous value
 */
ROMANO_FORCE_INLINE Atomic64 atomic_exchange_64(Atomic64* volatile dest,
                                                Atomic64 exchange,
                                                MemoryOrder mo)
{
#if defined(ROMANO_MSVC)
    ROMANO_UNUSED(mo);
    return _InterlockedExchange64((LONG64*)dest, exchange);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    Atomic64 ret;
    __atomic_exchange(dest, &exchange, &ret, mo);
    return ret;
#endif /* defined(ROMANO_MSVC) */
}

/*
 * Establishes a memory fence with the given memory order
 * mo: memory order for the fence
 */
ROMANO_FORCE_INLINE void atomic_thread_fence(MemoryOrder mo)
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