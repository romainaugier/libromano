/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/* https://www.codeproject.com/Tips/784635/UInt-Bit-Operations */

#pragma once

#if !defined(__LIBROMANO_INT128)
#define __LIBROMANO_128

#include "libromano/bit.h"

ROMANO_CPP_ENTER

struct _int128
{
    int64_t high;
    int64_t low;
};

typedef struct _int128 int128_t;

ROMANO_FORCE_INLINE int128_t int128_make(const int64_t low, const int64_t high)
{
    int128_t res;
    res.low = low;
    res.high = high;
    return res;
}

ROMANO_FORCE_INLINE int128_t int128_and(const int128_t a, const int128_t b)
{
    int128_t res;
    res.low = a.low & b.low;
    res.high = a.high & b.high;
    return res;
}

ROMANO_FORCE_INLINE int128_t int128_or(const int128_t a, const int128_t b)
{
    int128_t res;
    res.low = a.low | b.low;
    res.high = a.high | b.high;
    return res;
}

ROMANO_FORCE_INLINE int128_t int128_xor(const int128_t a, const int128_t b)
{
    int128_t res;
    res.low = a.low ^ b.low;
    res.high = a.high ^ b.high;
    return res;
}

ROMANO_FORCE_INLINE int128_t int128_sl(int128_t x, uint32_t shift)
{
    int128_t res;
    int64_t m1, m2;

    shift &= 127;

    m1 = ((((shift + 127) | shift) & 64) >> 6) - 1LLU;
    m2 = (shift >> 6) - 1LLU;
    shift &= 63;
    res.high = (x.low << shift) & (~m2);
    res.low = (x.low << shift) & m2;
    res.high |= ((x.high << shift) | ((x.low >> (64 - shift)) & m1)) & m2;

    return res;
}

ROMANO_FORCE_INLINE int128_t int128_sr(int128_t x, uint32_t shift)
{
    int128_t res;
    int64_t m1, m2;

    shift &= 127;

    m1 = ((((shift + 127) | shift) & 64) >> 6) - 1LLU;
    m2 = (shift >> 6) - 1LLU;
    shift &= 63;
    res.low = (x.high >> shift) & (~m2);
    res.high = (x.high >> shift) & m2;
    res.low |= ((x.low >> shift) | ((x.high << (64 - shift)) & m1)) & m2;

    return res;
} 

ROMANO_FORCE_INLINE int128_t int128_not(int128_t a)
{
    a.low = ~a.low;
    a.high = ~a.high;
    return a;
}

ROMANO_FORCE_INLINE uint64_t int128_popcnt(int128_t a)
{
    return popcount_u64(a.low) + popcount_u64(a.high);
}

ROMANO_FORCE_INLINE int128_t int128_add(int128_t a, int128_t b)
{
    int128_t res;

    ROMANO_NOT_IMPLEMENTED;

    return res;
}

ROMANO_FORCE_INLINE int128_t int128_add_int64(int128_t a, int64_t b)
{
    int128_t res;

    ROMANO_NOT_IMPLEMENTED;

    return res;
}

ROMANO_FORCE_INLINE int128_t int128_mul(int128_t a, int128_t b)
{
    int128_t res;

    ROMANO_NOT_IMPLEMENTED;

    return res;
}

ROMANO_FORCE_INLINE int128_t int128_mul_int64(int128_t a, int64_t b)
{
    int128_t res;

    ROMANO_NOT_IMPLEMENTED;

    return res;
}

ROMANO_FORCE_INLINE int128_t int128_sub(int128_t a, int128_t b)
{
    int128_t res;

    ROMANO_NOT_IMPLEMENTED;

    return res;
}

ROMANO_FORCE_INLINE int128_t int128_sub_int64(int128_t a, int64_t b)
{
    int128_t res;

    ROMANO_NOT_IMPLEMENTED;

    return res;
}

ROMANO_FORCE_INLINE int128_t int128_div(int128_t a, int128_t b)
{
    int128_t res;

    ROMANO_NOT_IMPLEMENTED;

    return res;
}

ROMANO_FORCE_INLINE int128_t int128_div_int64(int128_t a, int64_t b)
{
    int128_t res;

    ROMANO_NOT_IMPLEMENTED;

    return res;
}

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_INT128) */