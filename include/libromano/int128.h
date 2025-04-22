/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/* https://www.codeproject.com/Tips/784635/UInt-Bit-Operations */

#pragma once

#if !defined(__LIBROMANO_INT128)
#define __LIBROMANO_128

#include "libromano/libromano.h"

#include <immintrin.h>

ROMANO_CPP_ENTER

typedef __m128i int128_t;
typedef __m128i uint128_t;

ROMANO_FORCE_INLINE void print_uint128(uint128_t x) 
{
    const uint64_t* parts = (const uint64_t*)&x;
    printf("0x%016zX%016zX", parts[1], parts[0]);
}

ROMANO_FORCE_INLINE void print_int128(int128_t x) 
{
    const uint64_t* parts = (const uint64_t*)&x;
    printf("%s0x%016zX%016zX", 
           (parts[1] & 0x8000000000000000) ? "-" : "", 
           parts[1] & 0x7FFFFFFFFFFFFFFF, 
           parts[0]);
}

ROMANO_FORCE_INLINE uint128_t make_uint128(uint64_t high, uint64_t low) 
{
    return _mm_set_epi64x(high, low);
}

ROMANO_FORCE_INLINE int128_t make_int128(int64_t high, uint64_t low) 
{
    return _mm_set_epi64x(high, low);
}

ROMANO_FORCE_INLINE uint64_t uint128_low(uint128_t x) 
{
    return _mm_extract_epi64(x, 0);
}

ROMANO_FORCE_INLINE uint64_t uint128_high(uint128_t x) 
{
    return _mm_extract_epi64(x, 1);
}

ROMANO_FORCE_INLINE uint128_t uint128_add(uint128_t a, uint128_t b) 
{
    uint128_t sum = _mm_add_epi64(a, b);
    
    const uint64_t a_low = uint128_low(a);
    const uint64_t b_low = uint128_low(b);
    const uint64_t sum_low = uint128_low(sum);
    
    if(sum_low < a_low || sum_low < b_low) 
    {
        const uint128_t carry = _mm_set_epi64x(1, 0);
        sum = _mm_add_epi64(sum, carry);
    }
    
    return sum;
}

ROMANO_FORCE_INLINE int128_t int128_add(int128_t a, int128_t b) 
{
    return uint128_add(a, b);
}

ROMANO_FORCE_INLINE uint128_t uint128_sub(uint128_t a, uint128_t b) 
{
    uint128_t diff = _mm_sub_epi64(a, b);
    
    const uint64_t a_low = uint128_low(a);
    const uint64_t b_low = uint128_low(b);
    
    if(a_low < b_low) 
    {
        uint128_t borrow = _mm_set_epi64x(1, 0);
        diff = _mm_sub_epi64(diff, borrow);
    }
    
    return diff;
}

ROMANO_FORCE_INLINE int128_t int128_sub(int128_t a, int128_t b) 
{
    return uint128_sub(a, b);
}

ROMANO_FORCE_INLINE int uint128_eq(uint128_t a, uint128_t b) 
{
    __m128i cmp = _mm_cmpeq_epi64(a, b);
    return _mm_movemask_epi8(cmp) == 0xFFFF;
}

ROMANO_FORCE_INLINE int uint128_lt(uint128_t a, uint128_t b) 
{
    const uint64_t a_hi = uint128_high(a);
    const uint64_t b_hi = uint128_high(b);

    if(a_hi != b_hi) 
    {
        return a_hi < b_hi;
    }

    return uint128_low(a) < uint128_low(b);
}

ROMANO_FORCE_INLINE int uint128_gt(uint128_t a, uint128_t b) 
{
    return uint128_lt(b, a);
}

ROMANO_FORCE_INLINE uint128_t uint128_and(uint128_t a, uint128_t b) 
{
    return _mm_and_si128(a, b);
}

ROMANO_FORCE_INLINE uint128_t uint128_or(uint128_t a, uint128_t b) 
{
    return _mm_or_si128(a, b);
}

ROMANO_FORCE_INLINE uint128_t uint128_xor(uint128_t a, uint128_t b) 
{
    return _mm_xor_si128(a, b);
}

ROMANO_FORCE_INLINE uint128_t uint128_not(uint128_t a) 
{
    return _mm_xor_si128(a, _mm_set1_epi8(0xFF));
}

ROMANO_FORCE_INLINE uint128_t uint128_shl(uint128_t a, int count)
{
    if(count >= 64) 
    {
        if(count >= 128)
        {
            return _mm_setzero_si128();
        }

        return _mm_slli_si128(a, 8);
    } 
    else if (count > 0) 
    {
        uint128_t high_shifted = _mm_slli_epi64(a, count);
        uint128_t cross_boundary = _mm_srli_epi64(a, 64 - count);
        cross_boundary = _mm_slli_si128(cross_boundary, 8);
        return _mm_or_si128(high_shifted, cross_boundary);
    }

    return a;
}

ROMANO_FORCE_INLINE uint128_t uint128_shr(uint128_t a, int count)
{
    if(count >= 64) 
    {
        if(count >= 128) 
        {
            return _mm_setzero_si128();
        }

        a = _mm_srli_si128(a, 8);

        count -= 64;
    }
    
    if(count > 0) 
    {
        uint128_t low_shifted = _mm_srli_epi64(a, count);
        uint128_t cross_boundary = _mm_slli_epi64(a, 64 - count);
        cross_boundary = _mm_srli_si128(cross_boundary, 8);
        return _mm_or_si128(low_shifted, cross_boundary);
    }

    return a;
}

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_INT128) */