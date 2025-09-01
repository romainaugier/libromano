/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_BIT)
#define __LIBROMANO_BIT

#include "libromano/common.h"

ROMANO_CPP_ENTER

#define BIT(bit) (size_t)1ULL << bit
#define BIT32(bit) ((uint32_t)1UL << (bit))
#define HAS_BIT32(i, b) ((i) & BIT32((b)))
#define SET_BIT32(i, b) ((i) |= BIT32((b)))
#define UNSET_BIT32(i, b) ((i) &= ~BIT32((b)))
#define TOGGLE_BIT32(i, b) ((i) ^= BIT32((b)))

#define BIT64(bit) ((uint64_t)1ULL << (bit))
#define HAS_BIT64(i, b) ((i) & BIT64((b)))
#define SET_BIT64(i, b) ((i) |= BIT64((b)))
#define UNSET_BIT64(i, b) ((i) &= ~BIT64((b)))
#define TOGGLE_BIT64(i, b) ((i) ^= BIT64((b)))

ROMANO_FORCE_INLINE uint32_t round_u32_to_next_pow2(uint32_t x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x++;
}

ROMANO_FORCE_INLINE uint64_t round_u64_to_next_pow2(uint64_t x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x++;
}

#if defined(ROMANO_WIN)
#include <intrin.h>
#else
#include <immintrin.h>
#endif /* defined(ROMANO_WIN) */

ROMANO_FORCE_INLINE uint64_t popcount_u32(const uint32_t x)
{
#if defined(ROMANO_MSVC)
    return __popcnt(x);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __builtin_popcountl(x);
#else
    /* A good compiler will replace it with popcnt instruction if available */
    uint32_t v = x;
    v = v - ((v >> 1) & (uint32_t)~(uint32_t)0/3);
    v = (v & (uint32_t)~(uint32_t)0/15*3) + ((v >> 2) & (uint32_t)~(uint32_t)0/15*3);
    v = (v + (v >> 4)) & (uint32_t)~(uint32_t)0/255*15;
    return (uint32_t)(v * ((uint32_t)~(uint32_t)0/255)) >> (sizeof(uint32_t) - 1) * 8;
#endif /* defined(ROMANO_WIN) */
}

ROMANO_FORCE_INLINE uint64_t popcount_u64(const uint64_t x)
{
#if defined(ROMANO_MSVC)
    return __popcnt64(x);
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    return __builtin_popcountll(x);
#else
    /* A good compiler will replace it with popcnt instruction if available */
    uint64_t v = x;
    v = v - ((v >> 1) & (uint64_t)~(uint64_t)0/3);
    v = (v & (uint64_t)~(uint64_t)0/15*3) + ((v >> 2) & (uint64_t)~(uint64_t)0/15*3);
    v = (v + (v >> 4)) & (uint64_t)~(uint64_t)0/255*15;
    return (uint64_t)(v * ((uint64_t)~(uint64_t)0/255)) >> (sizeof(uint64_t) - 1) * 8;
#endif /* defined(ROMANO_WIN) */
}

ROMANO_FORCE_INLINE uint32_t clz_u64(const uint64_t x)
{
#if defined(ROMANO_MSVC)
    unsigned long trailing_zero = 0;

    if(_BitScanReverse64(&trailing_zero, x))
    {
        return 63UL - trailing_zero;
    }

    return 32UL;
#elif defined(ROMANO_GCC)
    return __builtin_clzll(x);
#endif /* defined(ROMANO_MSVC) */
}

ROMANO_FORCE_INLINE uint32_t ctz_u64(const uint64_t x)
{
#if defined(ROMANO_MSVC)
    unsigned long trailing_zero = 0UL;

    if(_BitScanForward64(&trailing_zero, x))
    {
        return trailing_zero;
    }

    return 63UL;
#elif defined(ROMANO_GCC)
    return __builtin_ctzll(x);
#endif /* defined(ROMANO_MSVC) */
}

ROMANO_FORCE_INLINE uint32_t pext_u32(const uint32_t x, const uint32_t y)
{
    return _pext_u32(x, y);
}

ROMANO_FORCE_INLINE uint64_t pext_u64(const uint64_t x, const uint64_t y)
{
    return _pext_u64(x, y);
}

ROMANO_FORCE_INLINE uint8_t abs_u8(const int8_t x)
{
    const uint8_t mask = x >> 7U;
    return (uint8_t)((mask ^ x) - mask);
}

ROMANO_FORCE_INLINE uint16_t abs_u16(const int16_t x)
{
    const uint16_t mask = x >> 15U;
    return (uint16_t)((mask ^ x) - mask);
}

ROMANO_FORCE_INLINE uint32_t abs_u32(const int32_t x)
{
    const uint32_t mask = x >> 31U;
    return (uint32_t)((mask ^ x) - mask);
}

ROMANO_FORCE_INLINE uint64_t abs_u64(const int64_t x)
{
    const uint64_t mask = x >> 63U;
    return (uint64_t)((mask ^ x) - mask);
}

ROMANO_FORCE_INLINE uint64_t lsb_u64(const uint64_t x)
{
    return x & -x;
}

ROMANO_FORCE_INLINE uint64_t clsb_u64(const uint64_t x)
{
    return x & (x - 1ULL);
}

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_BIT) */
