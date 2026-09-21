/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/* https://www.codeproject.com/Tips/784635/UInt-Bit-Operations */

#pragma once

#if !defined(__LIBROMANO_INT128)
#define __LIBROMANO_INT128

#include "libromano/common.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if defined(ROMANO_X86_64)
#include <immintrin.h>
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif

/* On aarch64 with clang/gcc, __SIZEOF_INT128__ is defined, so we use the */
/* native 128-bit integer type via the compiler GNU extension. */
/* Define ROMANO_INT128_NO_NATIVE to use the fallback implementation (mostly for testing) */
#if defined(__SIZEOF_INT128__) && !defined(ROMANO_INT128_NO_NATIVE)
#define ROMANO_USE_NATIVE_INT128
#endif

/* MSVC on x86_64 does not have __int128, so we use MSVC intrinsics. */
#if defined(_MSC_VER) && defined(ROMANO_X86_64) && !defined(ROMANO_USE_NATIVE_INT128)
#define ROMANO_USE_MSVC_INT128
#endif

ROMANO_CPP_ENTER

#if defined(ROMANO_USE_NATIVE_INT128)
#if defined(ROMANO_GCC)
__extension__ typedef __int128 int128_t;
__extension__ typedef unsigned __int128 uint128_t;
#else
typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;
#endif
#elif defined(ROMANO_USE_MSVC_INT128)
typedef struct { uint64_t low; uint64_t high; } uint128_t;
typedef struct { uint64_t low; int64_t high; } int128_t;
#elif defined(ROMANO_X86_64)
typedef __m128i int128_t;
typedef __m128i uint128_t;
#else
#error "No int128 implementation available for this platform"
#endif

ROMANO_FORCE_INLINE void int128_print_parts(uint64_t high, uint64_t low)
{
    const int negative = (high >> 63) != 0;

    if(negative)
    {
        low = ~low + 1;
        high = ~high + (low == 0);
    }

    printf("%s0x%016llX%016llX", negative ? "-" : "", (unsigned long long)high, (unsigned long long)low);
}

/* Native __int128 implementation (aarch64 / clang / gcc) */
#if defined(ROMANO_USE_NATIVE_INT128)

ROMANO_FORCE_INLINE void print_uint128(uint128_t x)
{
    const uint64_t low = (uint64_t)x;
    const uint64_t high = (uint64_t)(x >> 64);

    printf("0x%016llX%016llX",
           (unsigned long long)high,
           (unsigned long long)low);
}

ROMANO_FORCE_INLINE void print_int128(int128_t x)
{
    const uint128_t ux = (uint128_t)x;

    int128_print_parts((uint64_t)(ux >> 64), (uint64_t)ux);
}

ROMANO_FORCE_INLINE uint128_t make_uint128(uint64_t high, uint64_t low)
{
    return ((uint128_t)high << 64) | (uint128_t)low;
}

ROMANO_FORCE_INLINE int128_t make_int128(int64_t high, uint64_t low)
{
    return (int128_t)(((uint128_t)(uint64_t)high << 64) | (uint128_t)low);
}

ROMANO_FORCE_INLINE uint64_t uint128_low(uint128_t x)
{
    return (uint64_t)x;
}

ROMANO_FORCE_INLINE uint64_t uint128_high(uint128_t x)
{
    return (uint64_t)(x >> 64);
}

ROMANO_FORCE_INLINE uint128_t uint128_add(uint128_t a, uint128_t b) { return a + b; }
ROMANO_FORCE_INLINE int128_t int128_add(int128_t a, int128_t b) { return (int128_t)((uint128_t)a + (uint128_t)b); }
ROMANO_FORCE_INLINE uint128_t uint128_sub(uint128_t a, uint128_t b) { return a - b; }
ROMANO_FORCE_INLINE int128_t int128_sub(int128_t a, int128_t b) { return (int128_t)((uint128_t)a - (uint128_t)b); }
ROMANO_FORCE_INLINE uint128_t uint128_mul(uint128_t a, uint128_t b) { return a * b; }
ROMANO_FORCE_INLINE int128_t int128_mul(int128_t a, int128_t b) { return (int128_t)((uint128_t)a * (uint128_t)b); }
ROMANO_FORCE_INLINE uint128_t uint128_div(uint128_t a, uint128_t b) { return a / b; }
ROMANO_FORCE_INLINE uint128_t uint128_mod(uint128_t a, uint128_t b) { return a % b; }
ROMANO_FORCE_INLINE int128_t int128_div(int128_t a, int128_t b) { return a / b; }
ROMANO_FORCE_INLINE int128_t int128_mod(int128_t a, int128_t b) { return a % b; }

ROMANO_FORCE_INLINE int uint128_eq(uint128_t a, uint128_t b) { return a == b; }
ROMANO_FORCE_INLINE int uint128_ne(uint128_t a, uint128_t b) { return a != b; }
ROMANO_FORCE_INLINE int uint128_lt(uint128_t a, uint128_t b) { return a < b; }
ROMANO_FORCE_INLINE int uint128_le(uint128_t a, uint128_t b) { return a <= b; }
ROMANO_FORCE_INLINE int uint128_gt(uint128_t a, uint128_t b) { return a > b; }
ROMANO_FORCE_INLINE int uint128_ge(uint128_t a, uint128_t b) { return a >= b; }

ROMANO_FORCE_INLINE int int128_eq(int128_t a, int128_t b) { return a == b; }
ROMANO_FORCE_INLINE int int128_ne(int128_t a, int128_t b) { return a != b; }
ROMANO_FORCE_INLINE int int128_lt(int128_t a, int128_t b) { return a < b; }
ROMANO_FORCE_INLINE int int128_le(int128_t a, int128_t b) { return a <= b; }
ROMANO_FORCE_INLINE int int128_gt(int128_t a, int128_t b) { return a > b; }
ROMANO_FORCE_INLINE int int128_ge(int128_t a, int128_t b) { return a >= b; }

ROMANO_FORCE_INLINE uint128_t uint128_and(uint128_t a, uint128_t b) { return a & b; }
ROMANO_FORCE_INLINE uint128_t uint128_or(uint128_t a, uint128_t b) { return a | b; }
ROMANO_FORCE_INLINE uint128_t uint128_xor(uint128_t a, uint128_t b) { return a ^ b; }
ROMANO_FORCE_INLINE uint128_t uint128_not(uint128_t a) { return ~a; }
ROMANO_FORCE_INLINE uint128_t uint128_neg(uint128_t a) { return -a; }
ROMANO_FORCE_INLINE int128_t int128_neg(int128_t a) { return (int128_t)(0 - (uint128_t)a); }

ROMANO_FORCE_INLINE uint128_t uint128_shl(uint128_t a, int count)
{
    if(count >= 128) return (uint128_t)0;
    if(count <= 0) return a;
    return a << count;
}

ROMANO_FORCE_INLINE uint128_t uint128_shr(uint128_t a, int count)
{
    if(count >= 128) return (uint128_t)0;
    if(count <= 0) return a;
    return a >> count;
}

ROMANO_FORCE_INLINE int128_t int128_shl(int128_t a, int count)
{
    if(count >= 128) return (int128_t)0;
    if(count <= 0) return a;
    return (int128_t)((uint128_t)a << count);
}

ROMANO_FORCE_INLINE int128_t int128_shr(int128_t a, int count)
{
    if(count >= 128) return (a < 0) ? (int128_t)-1 : (int128_t)0;
    if(count <= 0) return a;
    return a >> count;
}

/* MSVC x64 implementation using intrinsics */
#elif defined(ROMANO_USE_MSVC_INT128)

ROMANO_FORCE_INLINE void print_uint128(uint128_t x)
{
    printf("0x%016llX%016llX", (unsigned long long)x.high, (unsigned long long)x.low);
}

ROMANO_FORCE_INLINE void print_int128(int128_t x)
{
    int128_print_parts((uint64_t)x.high, x.low);
}

ROMANO_FORCE_INLINE uint128_t make_uint128(uint64_t high, uint64_t low)
{
    uint128_t r = { low, high };
    return r;
}

ROMANO_FORCE_INLINE int128_t make_int128(int64_t high, uint64_t low)
{
    int128_t r = { low, high };
    return r;
}

ROMANO_FORCE_INLINE uint64_t uint128_low(uint128_t x) { return x.low; }
ROMANO_FORCE_INLINE uint64_t uint128_high(uint128_t x) { return x.high; }

ROMANO_FORCE_INLINE uint128_t uint128_add(uint128_t a, uint128_t b)
{
    uint128_t r;
    unsigned char carry = _addcarry_u64(0, a.low, b.low, &r.low);
    _addcarry_u64(carry, a.high, b.high, &r.high);
    return r;
}

ROMANO_FORCE_INLINE int128_t int128_add(int128_t a, int128_t b)
{
    uint128_t ua = { a.low, (uint64_t)a.high };
    uint128_t ub = { b.low, (uint64_t)b.high };
    uint128_t ur = uint128_add(ua, ub);
    int128_t r = { ur.low, (int64_t)ur.high };
    return r;
}

ROMANO_FORCE_INLINE uint128_t uint128_sub(uint128_t a, uint128_t b)
{
    uint128_t r;
    unsigned char borrow = _subborrow_u64(0, a.low, b.low, &r.low);
    _subborrow_u64(borrow, a.high, b.high, &r.high);
    return r;
}

ROMANO_FORCE_INLINE int128_t int128_sub(int128_t a, int128_t b)
{
    uint128_t ua = { a.low, (uint64_t)a.high };
    uint128_t ub = { b.low, (uint64_t)b.high };
    uint128_t ur = uint128_sub(ua, ub);
    int128_t r = { ur.low, (int64_t)ur.high };
    return r;
}

ROMANO_FORCE_INLINE uint128_t uint128_mul(uint128_t a, uint128_t b)
{
    uint64_t high;
    uint64_t low = _umul128(a.low, b.low, &high);
    high += a.high * b.low + a.low * b.high;
    uint128_t r = { low, high };
    return r;
}

ROMANO_FORCE_INLINE int128_t int128_mul(int128_t a, int128_t b)
{
    uint128_t ua = { a.low, (uint64_t)a.high };
    uint128_t ub = { b.low, (uint64_t)b.high };
    uint128_t ur = uint128_mul(ua, ub);
    int128_t r = { ur.low, (int64_t)ur.high };
    return r;
}

ROMANO_FORCE_INLINE int uint128_eq(uint128_t a, uint128_t b)
{
    return a.low == b.low && a.high == b.high;
}

ROMANO_FORCE_INLINE int uint128_ne(uint128_t a, uint128_t b)
{
    return !uint128_eq(a, b);
}

ROMANO_FORCE_INLINE int uint128_lt(uint128_t a, uint128_t b)
{
    if(a.high != b.high)
        return a.high < b.high;

    return a.low < b.low;
}

ROMANO_FORCE_INLINE int uint128_gt(uint128_t a, uint128_t b)
{
    return uint128_lt(b, a);
}

ROMANO_FORCE_INLINE int uint128_le(uint128_t a, uint128_t b)
{
    return !uint128_gt(a, b);
}

ROMANO_FORCE_INLINE int uint128_ge(uint128_t a, uint128_t b)
{
    return !uint128_lt(a, b);
}

ROMANO_FORCE_INLINE int int128_eq(int128_t a, int128_t b)
{
    return a.low == b.low && a.high == b.high;
}

ROMANO_FORCE_INLINE int int128_ne(int128_t a, int128_t b)
{
    return !int128_eq(a, b);
}

ROMANO_FORCE_INLINE int int128_lt(int128_t a, int128_t b)
{
    if(a.high != b.high)
        return a.high < b.high;

    return a.low < b.low;
}

ROMANO_FORCE_INLINE int int128_gt(int128_t a, int128_t b)
{
    return int128_lt(b, a);
}

ROMANO_FORCE_INLINE int int128_le(int128_t a, int128_t b)
{
    return !int128_gt(a, b);
}

ROMANO_FORCE_INLINE int int128_ge(int128_t a, int128_t b)
{
    return !int128_lt(a, b);
}

ROMANO_FORCE_INLINE uint128_t uint128_and(uint128_t a, uint128_t b)
{
    uint128_t r = { a.low & b.low, a.high & b.high };
    return r;
}

ROMANO_FORCE_INLINE uint128_t uint128_or(uint128_t a, uint128_t b)
{
    uint128_t r = { a.low | b.low, a.high | b.high };
    return r;
}

ROMANO_FORCE_INLINE uint128_t uint128_xor(uint128_t a, uint128_t b)
{
    uint128_t r = { a.low ^ b.low, a.high ^ b.high };
    return r;
}

ROMANO_FORCE_INLINE uint128_t uint128_not(uint128_t a)
{
    uint128_t r = { ~a.low, ~a.high };
    return r;
}

ROMANO_FORCE_INLINE uint128_t uint128_neg(uint128_t a)
{
    return uint128_sub(make_uint128(0, 0), a);
}

ROMANO_FORCE_INLINE int128_t int128_neg(int128_t a)
{
    uint128_t ua = { a.low, (uint64_t)a.high };
    uint128_t ur = uint128_neg(ua);
    int128_t r = { ur.low, (int64_t)ur.high };
    return r;
}

ROMANO_FORCE_INLINE uint128_t uint128_shl(uint128_t a, int count)
{
    if(count >= 128)
        return make_uint128(0, 0);

    if(count <= 0)
        return a;

    uint128_t r;

    if(count >= 64)
    {
        r.high = a.low << (count - 64);
        r.low = 0;
    }
    else
    {
        r.high = (a.high << count) | (a.low >> (64 - count));
        r.low = a.low << count;
    }

    return r;
}

ROMANO_FORCE_INLINE uint128_t uint128_shr(uint128_t a, int count)
{
    if(count >= 128)
        return make_uint128(0, 0);

    if(count <= 0)
        return a;

    uint128_t r;

    if(count >= 64)
    {
        r.low = a.high >> (count - 64);
        r.high = 0;
    }
    else
    {
        r.low = (a.low >> count) | (a.high << (64 - count));
        r.high = a.high >> count;
    }

    return r;
}

ROMANO_FORCE_INLINE int128_t int128_shl(int128_t a, int count)
{
    uint128_t ua = { a.low, (uint64_t)a.high };
    uint128_t ur = uint128_shl(ua, count);
    int128_t r = { ur.low, (int64_t)ur.high };
    return r;
}

ROMANO_FORCE_INLINE int128_t int128_shr(int128_t a, int count)
{
    if(count >= 128)
        return (a.high < 0) ? make_int128(-1, ~0ULL) : make_int128(0, 0);

    if(count <= 0)
        return a;

    int128_t r;

    if(count >= 64)
    {
        r.low = (uint64_t)(a.high >> (count - 64));
        r.high = a.high < 0 ? -1 : 0;
    }
    else
    {
        r.low = (a.low >> count) | ((uint64_t)a.high << (64 - count));
        r.high = a.high >> count;
    }

    return r;
}

/* Generic binary long division that works on all MSVC versions */
ROMANO_FORCE_INLINE uint128_t uint128_div(uint128_t a, uint128_t b)
{
    if(b.high == 0 && b.low == 0)
        return make_uint128(0, 0); // division by zero

    uint128_t quotient = make_uint128(0, 0);
    uint128_t remainder = make_uint128(0, 0);

    for(int i = 127; i >= 0; i--)
    {
        remainder = uint128_shl(remainder, 1);

        if(i >= 64)
            remainder.low |= (a.high >> (i - 64)) & 1;
        else
            remainder.low |= (a.low >> i) & 1;

        if(uint128_ge(remainder, b))
        {
            remainder = uint128_sub(remainder, b);

            if(i >= 64)
                quotient.high |= (1ULL << (i - 64));
            else
                quotient.low |= (1ULL << i);
        }
    }

    return quotient;
}

ROMANO_FORCE_INLINE uint128_t uint128_mod(uint128_t a, uint128_t b)
{
    if(b.high == 0 && b.low == 0)
        return make_uint128(0, 0);

    uint128_t remainder = make_uint128(0, 0);

    for(int i = 127; i >= 0; i--)
    {
        remainder = uint128_shl(remainder, 1);

        if(i >= 64)
            remainder.low |= (a.high >> (i - 64)) & 1;
        else
            remainder.low |= (a.low >> i) & 1;

        if(uint128_ge(remainder, b))
            remainder = uint128_sub(remainder, b);
    }

    return remainder;
}

ROMANO_FORCE_INLINE int128_t int128_div(int128_t a, int128_t b)
{
    int sign = 1;

    uint128_t ua = { a.low, (uint64_t)a.high };
    uint128_t ub = { b.low, (uint64_t)b.high };

    if(a.high < 0)
    { 
        ua = uint128_neg(ua);
        sign = -sign;
    }

    if(b.high < 0)
    { 
        ub = uint128_neg(ub);
        sign = -sign;
    }

    uint128_t uq = uint128_div(ua, ub);

    if(sign < 0)
        uq = uint128_neg(uq);

    int128_t r = { uq.low, (int64_t)uq.high };

    return r;
}

ROMANO_FORCE_INLINE int128_t int128_mod(int128_t a, int128_t b)
{
    int sign = 1;

    uint128_t ua = { a.low, (uint64_t)a.high };
    uint128_t ub = { b.low, (uint64_t)b.high };

    if(a.high < 0)
    { 
        ua = uint128_neg(ua);
        sign = -sign;
    }

    if(b.high < 0)
        ub = uint128_neg(ub);

    uint128_t ur = uint128_mod(ua, ub);

    if(sign < 0)
        ur = uint128_neg(ur);

    int128_t r = { ur.low, (int64_t)ur.high };

    return r;
}

/* SSE2 fallback (x86_64 without native 128 and without MSVC) */

#elif defined(ROMANO_X86_64)

ROMANO_FORCE_INLINE void print_uint128(uint128_t x)
{
    const uint64_t* parts = (const uint64_t*)&x;
    printf("0x%016zX%016zX", parts[1], parts[0]);
}

ROMANO_FORCE_INLINE void print_int128(int128_t x)
{
    uint64_t parts[2];

    memcpy(parts, &x, sizeof(parts));
    int128_print_parts(parts[1], parts[0]);
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

ROMANO_FORCE_INLINE int uint128_ne(uint128_t a, uint128_t b) { return !uint128_eq(a, b); }

ROMANO_FORCE_INLINE int uint128_lt(uint128_t a, uint128_t b)
{
    const uint64_t a_hi = uint128_high(a);
    const uint64_t b_hi = uint128_high(b);

    if(a_hi != b_hi)
        return a_hi < b_hi;

    return uint128_low(a) < uint128_low(b);
}

ROMANO_FORCE_INLINE int uint128_gt(uint128_t a, uint128_t b)
{
    return uint128_lt(b, a);
}

ROMANO_FORCE_INLINE int uint128_le(uint128_t a, uint128_t b) { return !uint128_gt(a, b); }
ROMANO_FORCE_INLINE int uint128_ge(uint128_t a, uint128_t b) { return !uint128_lt(a, b); }

ROMANO_FORCE_INLINE int int128_eq(int128_t a, int128_t b) { return uint128_eq(a, b); }
ROMANO_FORCE_INLINE int int128_ne(int128_t a, int128_t b) { return uint128_ne(a, b); }

ROMANO_FORCE_INLINE int int128_lt(int128_t a, int128_t b)
{
    uint64_t a_hi = uint128_high(a);
    uint64_t b_hi = uint128_high(b);

    if(a_hi != b_hi)
        return (int64_t)a_hi < (int64_t)b_hi;

    return uint128_low(a) < uint128_low(b);
}

ROMANO_FORCE_INLINE int int128_gt(int128_t a, int128_t b) { return int128_lt(b, a); }
ROMANO_FORCE_INLINE int int128_le(int128_t a, int128_t b) { return !int128_gt(a, b); }
ROMANO_FORCE_INLINE int int128_ge(int128_t a, int128_t b) { return !int128_lt(a, b); }

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
    return _mm_xor_si128(a, _mm_set1_epi8((char)-1));
}

ROMANO_FORCE_INLINE uint128_t uint128_neg(uint128_t a)
{
    return uint128_sub(_mm_setzero_si128(), a);
}

ROMANO_FORCE_INLINE int128_t int128_neg(int128_t a)
{
    return uint128_neg(a);
}

ROMANO_FORCE_INLINE uint128_t uint128_shl(uint128_t a, int count)
{
    if(count >= 64)
    {
        if(count >= 128)
            return _mm_setzero_si128();

        return _mm_slli_epi64(_mm_slli_si128(a, 8), count - 64);
    }
    else if(count > 0)
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
            return _mm_setzero_si128();

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

ROMANO_FORCE_INLINE int128_t int128_shl(int128_t a, int count)
{
    return uint128_shl(a, count);
}

ROMANO_FORCE_INLINE int128_t int128_shr(int128_t a, int count)
{
    if(count >= 128)
        return (int128_lt(a, _mm_setzero_si128())) ? _mm_set1_epi8((char)-1) : _mm_setzero_si128();

    if(count <= 0)
        return a;

    int is_negative = ((int64_t)uint128_high(a)) < 0;
    uint128_t shifted = uint128_shr(a, count);

    if(is_negative)
    {
        if(count >= 64)
        {
            uint64_t low = uint128_low(shifted);
            uint64_t k = count - 64;

            if(k > 0)
                low |= (~0ULL << (64 - k));

            shifted = make_uint128(~0ULL, low);
        }
        else
        {
            uint64_t high = uint128_high(shifted);
            high |= (~0ULL << (64 - count));
            shifted = make_uint128(high, uint128_low(shifted));
        }
    }

    return shifted;
}

/* 64x64 -> 128 multiplication using 32-bit parts */
ROMANO_FORCE_INLINE uint128_t uint128_mul(uint128_t a, uint128_t b)
{
    uint64_t a_low = uint128_low(a);
    uint64_t a_high = uint128_high(a);
    uint64_t b_low = uint128_low(b);
    uint64_t b_high = uint128_high(b);

    uint64_t a_lo = a_low & 0xFFFFFFFF;
    uint64_t a_hi = a_low >> 32;
    uint64_t b_lo = b_low & 0xFFFFFFFF;
    uint64_t b_hi = b_low >> 32;

    uint64_t p0 = a_lo * b_lo;
    uint64_t p1 = a_lo * b_hi;
    uint64_t p2 = a_hi * b_lo;
    uint64_t p3 = a_hi * b_hi;

    uint64_t mid = p1 + (p0 >> 32) + (p2 & 0xFFFFFFFF);
    uint64_t low = (p0 & 0xFFFFFFFF) | (mid << 32);
    uint64_t high = p3 + (p2 >> 32) + (mid >> 32);
    high += a_high * b_low + a_low * b_high;

    return make_uint128(high, low);
}

ROMANO_FORCE_INLINE int128_t int128_mul(int128_t a, int128_t b)
{
    uint128_t ua = { uint128_low(a), uint128_high(a) };
    uint128_t ub = { uint128_low(b), uint128_high(b) };
    uint128_t ur = uint128_mul(ua, ub);

    return make_int128((int64_t)uint128_high(ur), uint128_low(ur));
}

/* Generic binary long division for SSE2 */
ROMANO_FORCE_INLINE uint128_t uint128_div(uint128_t a, uint128_t b)
{
    if(uint128_eq(b, _mm_setzero_si128()))
        return _mm_setzero_si128();

    uint128_t quotient = _mm_setzero_si128();
    uint128_t remainder = _mm_setzero_si128();

    for(int i = 127; i >= 0; i--)
    {
        remainder = uint128_shl(remainder, 1);
        uint64_t bit;

        if(i >= 64)
            bit = (uint128_high(a) >> (i - 64)) & 1;
        else
            bit = (uint128_low(a) >> i) & 1;

        remainder = _mm_or_si128(remainder, make_uint128(0, bit));

        if(uint128_ge(remainder, b))
        {
            remainder = uint128_sub(remainder, b);

            if(i >= 64)
                quotient = _mm_or_si128(quotient, make_uint128(1ULL << (i - 64), 0));
            else
                quotient = _mm_or_si128(quotient, make_uint128(0, 1ULL << i));
        }
    }

    return quotient;
}

ROMANO_FORCE_INLINE uint128_t uint128_mod(uint128_t a, uint128_t b)
{
    if(uint128_eq(b, _mm_setzero_si128()))
        return _mm_setzero_si128();

    uint128_t remainder = _mm_setzero_si128();

    for(int i = 127; i >= 0; i--)
    {
        remainder = uint128_shl(remainder, 1);
        uint64_t bit;

        if(i >= 64)
            bit = (uint128_high(a) >> (i - 64)) & 1;
        else
            bit = (uint128_low(a) >> i) & 1;

        remainder = _mm_or_si128(remainder, make_uint128(0, bit));

        if(uint128_ge(remainder, b))
            remainder = uint128_sub(remainder, b);
    }

    return remainder;
}

ROMANO_FORCE_INLINE int128_t int128_div(int128_t a, int128_t b)
{
    int sign = 1;

    uint128_t ua = a;
    uint128_t ub = b;

    if((int64_t)uint128_high(a) < 0)
    {   
        ua = uint128_neg(ua);
        sign = -sign;
    }

    if((int64_t)uint128_high(b) < 0)
    { 
        ub = uint128_neg(ub);
        sign = -sign;
    }

    uint128_t uq = uint128_div(ua, ub);

    if(sign < 0)
        uq = uint128_neg(uq);

    return uq;
}

ROMANO_FORCE_INLINE int128_t int128_mod(int128_t a, int128_t b)
{
    int sign = 1;

    uint128_t ua = a;
    uint128_t ub = b;

    if((int64_t)uint128_high(a) < 0)
    { 
        ua = uint128_neg(ua);
        sign = -sign;
    }

    if((int64_t)uint128_high(b) < 0)
        ub = uint128_neg(ub);

    uint128_t ur = uint128_mod(ua, ub);

    if(sign < 0)
        ur = uint128_neg(ur);

    return ur;
}

#endif /* defined(ROMANO_USE_NATIVE_INT128) */

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_INT128) */