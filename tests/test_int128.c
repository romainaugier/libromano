/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/int128.h"

#include <stdio.h>
#include <stddef.h>

static uint128_t u128(uint64_t high, uint64_t low)
{
    return make_uint128(high, low);
}

static int128_t i128(int64_t high, uint64_t low)
{
    return make_int128(high, low);
}

static int128_t i128_from_i64(int64_t v)
{
    return make_int128((v < 0) ? -1 : 0, (uint64_t)v);
}

static void test_make_and_accessors(void)
{
    uint128_t v;

    v = u128(0xDEADBEEFCAFEBABEULL, 0x0123456789ABCDEFULL);
    TEST_CHECK(uint128_high(v) == 0xDEADBEEFCAFEBABEULL);
    TEST_CHECK(uint128_low(v)  == 0x0123456789ABCDEFULL);

    v = u128(0, 0);
    TEST_CHECK(uint128_high(v) == 0);
    TEST_CHECK(uint128_low(v)  == 0);

    v = u128(~0ULL, ~0ULL);
    TEST_CHECK(uint128_high(v) == ~0ULL);
    TEST_CHECK(uint128_low(v)  == ~0ULL);

    v = u128(0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL);
    TEST_CHECK(uint128_high(v) == 0x0123456789ABCDEFULL);
    TEST_CHECK(uint128_low(v)  == 0xFEDCBA9876543210ULL);

    /* Round-trip a signed value through int128 comparison */
    TEST_CHECK(int128_eq(i128_from_i64(-1), i128(-1, ~0ULL)));
    TEST_CHECK(int128_eq(i128_from_i64(0),  i128(0, 0)));
    TEST_CHECK(int128_eq(i128_from_i64(1),  i128(0, 1)));
}

static void test_unsigned_add(void)
{
    /* No carry */
    TEST_CHECK(uint128_eq(uint128_add(u128(0, 10), u128(0, 20)), u128(0, 30)));

    /* Carry from low into high */
    TEST_CHECK(uint128_eq(uint128_add(u128(0, ~0ULL), u128(0, 1)), u128(1, 0)));
    TEST_CHECK(uint128_eq(uint128_add(u128(0, 0xFFFFFFFFFFFFFFFFULL),
                                 u128(0, 0x0000000000000002ULL)),
                     u128(1, 1)));

    /* Full wrap-around */
    TEST_CHECK(uint128_eq(uint128_add(u128(~0ULL, ~0ULL), u128(0, 1)),
                     u128(0, 0)));
    TEST_CHECK(uint128_eq(uint128_add(u128(~0ULL, ~0ULL), u128(~0ULL, ~0ULL)),
                     u128(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFEULL)));

    /* Commutativity on two multi-limb values */
    TEST_CHECK(uint128_eq(
        uint128_add(u128(0x1111111111111111ULL, 0x2222222222222222ULL),
                    u128(0x3333333333333333ULL, 0x4444444444444444ULL)),
        uint128_add(u128(0x3333333333333333ULL, 0x4444444444444444ULL),
                    u128(0x1111111111111111ULL, 0x2222222222222222ULL))));

    /* Additive identity */
    TEST_CHECK(uint128_eq(
        uint128_add(u128(0xABCDEF0123456789ULL, 0x9876543210FEDCBAULL),
                    u128(0, 0)),
        u128(0xABCDEF0123456789ULL, 0x9876543210FEDCBAULL)));

    /* Purely high-word addition */
    TEST_CHECK(uint128_eq(uint128_add(u128(1, 0), u128(2, 0)), u128(3, 0)));
}

static void test_unsigned_sub(void)
{
    /* No borrow */
    TEST_CHECK(uint128_eq(uint128_sub(u128(0, 20), u128(0, 10)), u128(0, 10)));

    /* Borrow from the high limb */
    TEST_CHECK(uint128_eq(uint128_sub(u128(1, 0), u128(0, 1)), u128(0, ~0ULL)));
    TEST_CHECK(uint128_eq(uint128_sub(u128(1, 1), u128(0, 2)), u128(0, ~0ULL)));

    /* Full wrap-around (underflow) */
    TEST_CHECK(uint128_eq(uint128_sub(u128(0, 0), u128(0, 1)), u128(~0ULL, ~0ULL)));
    TEST_CHECK(uint128_eq(uint128_sub(u128(0, 0), u128(1, 0)),
                     u128(0xFFFFFFFFFFFFFFFFULL, 0)));

    /* x - x = 0 */
    TEST_CHECK(uint128_eq(
        uint128_sub(u128(0xABCDEF0123456789ULL, 0x9876543210FEDCBAULL),
                    u128(0xABCDEF0123456789ULL, 0x9876543210FEDCBAULL)),
        u128(0, 0)));

    /* Subtract zero */
    TEST_CHECK(uint128_eq(
        uint128_sub(u128(0xCAFEBABEDEADBEEFULL, 0x0123456789ABCDEFULL),
                    u128(0, 0)),
        u128(0xCAFEBABEDEADBEEFULL, 0x0123456789ABCDEFULL)));
}

static void test_unsigned_mul(void)
{
    /* Small values */
    TEST_CHECK(uint128_eq(uint128_mul(u128(0, 6), u128(0, 7)), u128(0, 42)));

    /* Multiply by zero and one */
    TEST_CHECK(uint128_eq(
        uint128_mul(u128(0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL), u128(0, 0)),
        u128(0, 0)));
    TEST_CHECK(uint128_eq(
        uint128_mul(u128(0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL), u128(0, 1)),
        u128(0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL)));

    /* 2^64 * 2^64 wraps to 0 (high bit falls off the top) */
    TEST_CHECK(uint128_eq(uint128_mul(u128(1, 0), u128(1, 0)), u128(0, 0)));

    /* 2^64 * 3 = 3 * 2^64 => high = 3, low = 0 */
    TEST_CHECK(uint128_eq(uint128_mul(u128(1, 0), u128(0, 3)), u128(3, 0)));

    /* 64x64 -> 128: (2^64 - 1)^2 = 2^128 - 2^65 + 1 */
    TEST_CHECK(uint128_eq(
        uint128_mul(u128(0, ~0ULL), u128(0, ~0ULL)),
        u128(0xFFFFFFFFFFFFFFFEULL, 0x0000000000000001ULL)));

    /* (2^64 - 1) * 2 = 2^65 - 2 */
    TEST_CHECK(uint128_eq(
        uint128_mul(u128(0, ~0ULL), u128(0, 2)),
        u128(1, 0xFFFFFFFFFFFFFFFEULL)));

    /* (2^64 + 1) * (2^64 + 1) = 2^128 + 2^65 + 1 -> high=2, low=1 after wrap */
    TEST_CHECK(uint128_eq(
        uint128_mul(u128(1, 1), u128(1, 1)),
        u128(2, 1)));
}

static void test_unsigned_divmod(void)
{
    uint128_t a, b;

    /* Small values */
    a = u128(0, 100); b = u128(0, 7);
    TEST_CHECK(uint128_eq(uint128_div(a, b), u128(0, 14)));
    TEST_CHECK(uint128_eq(uint128_mod(a, b), u128(0, 2)));

    /* Exact division */
    a = u128(0, 42); b = u128(0, 42);
    TEST_CHECK(uint128_eq(uint128_div(a, b), u128(0, 1)));
    TEST_CHECK(uint128_eq(uint128_mod(a, b), u128(0, 0)));

    /* Dividend smaller than divisor */
    a = u128(0, 42); b = u128(0, 100);
    TEST_CHECK(uint128_eq(uint128_div(a, b), u128(0, 0)));
    TEST_CHECK(uint128_eq(uint128_mod(a, b), u128(0, 42)));

    /* Division across 64-bit boundary: 2^64 / 2 = 2^63 */
    a = u128(1, 0); b = u128(0, 2);
    TEST_CHECK(uint128_eq(uint128_div(a, b), u128(0, 0x8000000000000000ULL)));
    TEST_CHECK(uint128_eq(uint128_mod(a, b), u128(0, 0)));

    /* (2^64 + 1) / 2 = 2^63 remainder 1 */
    a = u128(1, 1); b = u128(0, 2);
    TEST_CHECK(uint128_eq(uint128_div(a, b), u128(0, 0x8000000000000000ULL)));
    TEST_CHECK(uint128_eq(uint128_mod(a, b), u128(0, 1)));

    /* (2^96) / (2^32) = 2^64 */
    a = u128(0x0000000100000000ULL, 0); /* 2^96 */
    b = u128(0, 0x100000000ULL);        /* 2^32 */
    TEST_CHECK(uint128_eq(uint128_div(a, b), u128(1, 0)));
    TEST_CHECK(uint128_eq(uint128_mod(a, b), u128(0, 0)));

    /* Max / small divisor */
    a = u128(~0ULL, ~0ULL);
    b = u128(0, 2);
    TEST_CHECK(uint128_eq(uint128_div(a, b), u128(0x7FFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL)));
    TEST_CHECK(uint128_eq(uint128_mod(a, b), u128(0, 1)));

    /* x / x = 1, x % x = 0 */
    a = u128(0xFEEDFACECAFEBEEFULL, 0xDEADBEEFDEADBEEFULL);
    TEST_CHECK(uint128_eq(uint128_div(a, a), u128(0, 1)));
    TEST_CHECK(uint128_eq(uint128_mod(a, a), u128(0, 0)));
}

static void test_unsigned_compare(void)
{
    uint128_t big   = u128(1, 0);
    uint128_t small = u128(0, ~0ULL);

    TEST_CHECK(uint128_gt(big, small));
    TEST_CHECK(uint128_ge(big, small));
    TEST_CHECK(uint128_ne(big, small));
    TEST_CHECK(!uint128_eq(big, small));
    TEST_CHECK(!uint128_lt(big, small));
    TEST_CHECK(!uint128_le(big, small));

    TEST_CHECK(uint128_lt(small, big));
    TEST_CHECK(uint128_le(small, big));
    TEST_CHECK(uint128_gt(small, big) == 0);
    TEST_CHECK(uint128_lt(small, big) != 0);

    /* Reflexive */
    TEST_CHECK(uint128_eq(big, big));
    TEST_CHECK(uint128_le(big, big));
    TEST_CHECK(uint128_ge(big, big));
    TEST_CHECK(!uint128_lt(big, big));
    TEST_CHECK(!uint128_gt(big, big));

    /* Zero versus one */
    TEST_CHECK(uint128_lt(u128(0, 0), u128(0, 1)));
    TEST_CHECK(uint128_gt(u128(0, 1), u128(0, 0)));
    TEST_CHECK(uint128_eq(u128(0, 0), u128(0, 0)));

    /* Equal high, different low */
    TEST_CHECK(uint128_lt(u128(5, 1), u128(5, 2)));
    TEST_CHECK(uint128_gt(u128(5, 2), u128(5, 1)));

    /* Different high dominates */
    TEST_CHECK(uint128_lt(u128(1, ~0ULL), u128(2, 0)));
    TEST_CHECK(uint128_gt(u128(2, 0), u128(1, ~0ULL)));

    /* Maximum */
    TEST_CHECK(uint128_gt(u128(~0ULL, ~0ULL), u128(~0ULL, ~0ULL - 1)));
}

static void test_bitwise(void)
{
    uint128_t a = u128(0xF0F0F0F0F0F0F0F0ULL, 0x0F0F0F0F0F0F0F0FULL);
    uint128_t b = u128(0xFFFFFFFFFFFFFFFFULL, 0x0000000000000000ULL);

    TEST_CHECK(uint128_eq(uint128_and(a, b), u128(0xF0F0F0F0F0F0F0F0ULL, 0)));
    TEST_CHECK(uint128_eq(uint128_or(a, b),  u128(0xFFFFFFFFFFFFFFFFULL, 0x0F0F0F0F0F0F0F0FULL)));
    TEST_CHECK(uint128_eq(uint128_xor(a, b), u128(0x0F0F0F0F0F0F0F0FULL, 0x0F0F0F0F0F0F0F0FULL)));
    TEST_CHECK(uint128_eq(uint128_not(a),    u128(0x0F0F0F0F0F0F0F0FULL, 0xF0F0F0F0F0F0F0F0ULL)));

    /* Identity laws */
    TEST_CHECK(uint128_eq(uint128_and(a, a), a));
    TEST_CHECK(uint128_eq(uint128_or(a, a),  a));
    TEST_CHECK(uint128_eq(uint128_xor(a, a), u128(0, 0)));
    TEST_CHECK(uint128_eq(uint128_and(a, u128(0, 0)), u128(0, 0)));
    TEST_CHECK(uint128_eq(uint128_or(a,  u128(0, 0)), a));
    TEST_CHECK(uint128_eq(uint128_xor(a, u128(0, 0)), a));
    TEST_CHECK(uint128_eq(uint128_and(a, u128(~0ULL, ~0ULL)), a));
    TEST_CHECK(uint128_eq(uint128_or(a,  u128(~0ULL, ~0ULL)), u128(~0ULL, ~0ULL)));

    /* Involution: ~~a == a */
    TEST_CHECK(uint128_eq(uint128_not(uint128_not(a)), a));

    /* De Morgan */
    TEST_CHECK(uint128_eq(uint128_not(uint128_and(a, b)),
                     uint128_or(uint128_not(a), uint128_not(b))));
    TEST_CHECK(uint128_eq(uint128_not(uint128_or(a, b)),
                     uint128_and(uint128_not(a), uint128_not(b))));
}

static void test_unsigned_negate(void)
{
    TEST_CHECK(uint128_eq(uint128_neg(u128(0, 0)), u128(0, 0)));
    TEST_CHECK(uint128_eq(uint128_neg(u128(0, 1)), u128(~0ULL, ~0ULL)));
    TEST_CHECK(uint128_eq(uint128_neg(u128(0, 5)), u128(~0ULL, ~0ULL - 4)));
    TEST_CHECK(uint128_eq(uint128_neg(u128(~0ULL, ~0ULL)), u128(0, 1)));

    /* x + (-x) == 0 */
    TEST_CHECK(uint128_eq(
        uint128_add(u128(0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL),
                    uint128_neg(u128(0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL))),
        u128(0, 0)));

    /* Double negation */
    TEST_CHECK(uint128_eq(uint128_neg(uint128_neg(u128(0xABCDEF, 0x12345))),
                     u128(0xABCDEF, 0x12345)));
}

static void test_unsigned_shifts(void)
{
    uint128_t v = u128(0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL);

    /* Shift by zero returns same value */
    TEST_CHECK(uint128_eq(uint128_shl(v, 0), v));
    TEST_CHECK(uint128_eq(uint128_shr(v, 0), v));

    /* Negative shift count returns same value */
    TEST_CHECK(uint128_eq(uint128_shl(v, -1), v));
    TEST_CHECK(uint128_eq(uint128_shr(v, -1), v));

    /* Shift by >= 128 returns 0 */
    TEST_CHECK(uint128_eq(uint128_shl(v, 128), u128(0, 0)));
    TEST_CHECK(uint128_eq(uint128_shr(v, 128), u128(0, 0)));
    TEST_CHECK(uint128_eq(uint128_shl(v, 200), u128(0, 0)));
    TEST_CHECK(uint128_eq(uint128_shr(v, 200), u128(0, 0)));

    /* Shift by 64 swaps halves */
    TEST_CHECK(uint128_eq(uint128_shl(v, 64), u128(0xFEDCBA9876543210ULL, 0)));
    TEST_CHECK(uint128_eq(uint128_shr(v, 64), u128(0, 0x0123456789ABCDEFULL)));

    /* Simple small shifts */
    TEST_CHECK(uint128_eq(uint128_shl(u128(0, 1), 1), u128(0, 2)));
    TEST_CHECK(uint128_eq(uint128_shl(u128(0, 1), 4), u128(0, 16)));
    TEST_CHECK(uint128_eq(uint128_shl(u128(0, 1), 63), u128(0, 0x8000000000000000ULL)));
    TEST_CHECK(uint128_eq(uint128_shl(u128(0, 1), 64), u128(1, 0)));
    TEST_CHECK(uint128_eq(uint128_shl(u128(0, 1), 65), u128(2, 0)));
    TEST_CHECK(uint128_eq(uint128_shl(u128(0, 1), 127), u128(0x8000000000000000ULL, 0)));

    TEST_CHECK(uint128_eq(uint128_shr(u128(0, 2), 1), u128(0, 1)));
    TEST_CHECK(uint128_eq(uint128_shr(u128(1, 0), 1), u128(0, 0x8000000000000000ULL)));
    TEST_CHECK(uint128_eq(uint128_shr(u128(1, 0), 64), u128(0, 1)));
    TEST_CHECK(uint128_eq(uint128_shr(u128(2, 0), 65), u128(0, 1)));
    TEST_CHECK(uint128_eq(uint128_shr(u128(0x8000000000000000ULL, 0), 127), u128(0, 1)));

    /* Cross-boundary carry-in on left shift */
    TEST_CHECK(uint128_eq(uint128_shl(u128(0, 0x8000000000000000ULL), 1), u128(1, 0)));
    TEST_CHECK(uint128_eq(uint128_shl(u128(0, 0xC000000000000000ULL), 1),
                     u128(1, 0x8000000000000000ULL)));

    /* Cross-boundary carry-out on right shift */
    TEST_CHECK(uint128_eq(uint128_shr(u128(1, 0), 1), u128(0, 0x8000000000000000ULL)));
    TEST_CHECK(uint128_eq(uint128_shr(u128(0, ~0ULL), 63), u128(0, 1)));

    /* Shift by 63 boundary */
    TEST_CHECK(uint128_eq(uint128_shr(u128(1, 0), 63), u128(0, 2)));
    TEST_CHECK(uint128_eq(uint128_shl(u128(0, 1), 63), u128(0, 0x8000000000000000ULL)));

    /* Round trip for shiftable values */
    TEST_CHECK(uint128_eq(uint128_shr(uint128_shl(u128(0, 0x1234), 40), 40),
                     u128(0, 0x1234)));
}

static void test_signed_arith(void)
{
    int128_t a, b, r;

    /* Addition */
    TEST_CHECK(int128_eq(int128_add(i128_from_i64(10), i128_from_i64(20)),
                    i128_from_i64(30)));
    TEST_CHECK(int128_eq(int128_add(i128_from_i64(-1), i128_from_i64(1)),
                    i128_from_i64(0)));
    TEST_CHECK(int128_eq(int128_add(i128_from_i64(-5), i128_from_i64(-7)),
                    i128_from_i64(-12)));

    /* Subtraction */
    TEST_CHECK(int128_eq(int128_sub(i128_from_i64(5), i128_from_i64(3)),
                    i128_from_i64(2)));
    TEST_CHECK(int128_eq(int128_sub(i128_from_i64(0), i128_from_i64(1)),
                    i128_from_i64(-1)));
    TEST_CHECK(int128_eq(int128_sub(i128_from_i64(-5), i128_from_i64(-3)),
                    i128_from_i64(-2)));

    /* Negation */
    TEST_CHECK(int128_eq(int128_neg(i128_from_i64(0)),  i128_from_i64(0)));
    TEST_CHECK(int128_eq(int128_neg(i128_from_i64(1)),  i128_from_i64(-1)));
    TEST_CHECK(int128_eq(int128_neg(i128_from_i64(-1)), i128_from_i64(1)));
    TEST_CHECK(int128_eq(int128_neg(i128_from_i64(42)), i128_from_i64(-42)));

    /* Multiplication */
    TEST_CHECK(int128_eq(int128_mul(i128_from_i64(6), i128_from_i64(7)),
                    i128_from_i64(42)));
    TEST_CHECK(int128_eq(int128_mul(i128_from_i64(-1), i128_from_i64(5)),
                    i128_from_i64(-5)));
    TEST_CHECK(int128_eq(int128_mul(i128_from_i64(-1), i128_from_i64(-1)),
                    i128_from_i64(1)));
    TEST_CHECK(int128_eq(int128_mul(i128_from_i64(0), i128_from_i64(12345)),
                    i128_from_i64(0)));

    /* Division (truncates toward zero, matching C's /) */
    TEST_CHECK(int128_eq(int128_div(i128_from_i64(100), i128_from_i64(7)),
                    i128_from_i64(14)));
    TEST_CHECK(int128_eq(int128_div(i128_from_i64(-100), i128_from_i64(7)),
                    i128_from_i64(-14)));
    TEST_CHECK(int128_eq(int128_div(i128_from_i64(100), i128_from_i64(-7)),
                    i128_from_i64(-14)));
    TEST_CHECK(int128_eq(int128_div(i128_from_i64(-100), i128_from_i64(-7)),
                    i128_from_i64(14)));
    TEST_CHECK(int128_eq(int128_div(i128_from_i64(0), i128_from_i64(1)),
                    i128_from_i64(0)));

    /* Modulo (sign of result follows dividend) */
    TEST_CHECK(int128_eq(int128_mod(i128_from_i64(100), i128_from_i64(7)),
                    i128_from_i64(2)));
    TEST_CHECK(int128_eq(int128_mod(i128_from_i64(-100), i128_from_i64(7)),
                    i128_from_i64(-2)));
    TEST_CHECK(int128_eq(int128_mod(i128_from_i64(100), i128_from_i64(-7)),
                    i128_from_i64(2)));
    TEST_CHECK(int128_eq(int128_mod(i128_from_i64(-100), i128_from_i64(-7)),
                    i128_from_i64(-2)));

    /* Mixed large value: -(2^64) + 5 */
    a = i128(-1, 5);
    b = i128_from_i64(5);
    r = int128_add(a, b);
    TEST_CHECK(int128_eq(r, i128(-1, 10)));
}

static void test_signed_compare(void)
{
    int128_t pos = i128_from_i64(1);
    int128_t zero = i128_from_i64(0);
    int128_t neg = i128_from_i64(-1);
    int128_t neg2 = i128_from_i64(-2);

    TEST_CHECK(int128_lt(neg, pos));
    TEST_CHECK(int128_le(neg, pos));
    TEST_CHECK(int128_ne(neg, pos));
    TEST_CHECK(!int128_eq(neg, pos));
    TEST_CHECK(int128_gt(pos, neg));
    TEST_CHECK(int128_ge(pos, neg));

    TEST_CHECK(int128_lt(neg, zero));
    TEST_CHECK(int128_gt(zero, neg));
    TEST_CHECK(int128_eq(zero, zero));

    TEST_CHECK(int128_lt(neg2, neg));
    TEST_CHECK(int128_gt(neg, neg2));

    /* Reflexive */
    TEST_CHECK(int128_eq(neg, neg));
    TEST_CHECK(int128_le(neg, neg));
    TEST_CHECK(int128_ge(neg, neg));
    TEST_CHECK(!int128_lt(neg, neg));
    TEST_CHECK(!int128_gt(neg, neg));

    /* -2^64 (high = -1, low = 0) is still negative and less than -1 */
    TEST_CHECK(int128_lt(i128(-1, 0), i128(-1, ~0ULL)));
}

static void test_signed_shifts(void)
{
    int128_t v;

    /* Left shift */
    v = i128_from_i64(1);
    TEST_CHECK(int128_eq(int128_shl(v, 4),  i128_from_i64(16)));
    TEST_CHECK(int128_eq(int128_shl(v, 64), i128(1, 0)));

    /* Left shift by 0 */
    v = i128_from_i64(-1);
    TEST_CHECK(int128_eq(int128_shl(v, 0), v));

    /* Arithmetic right shift on positive */
    v = i128(0, 0x8000000000000000ULL);
    TEST_CHECK(int128_eq(int128_shr(v, 1), i128(0, 0x4000000000000000ULL)));
    TEST_CHECK(int128_eq(int128_shr(v, 63), i128_from_i64(1)));
    TEST_CHECK(int128_eq(int128_shr(v, 64), i128_from_i64(0)));

    /* Arithmetic right shift on -1 stays -1 */
    v = i128_from_i64(-1);
    TEST_CHECK(int128_eq(int128_shr(v, 1),   v));
    TEST_CHECK(int128_eq(int128_shr(v, 63),  v));
    TEST_CHECK(int128_eq(int128_shr(v, 64),  v));
    TEST_CHECK(int128_eq(int128_shr(v, 127), v));

    /* -4 >> 1 = -2 */
    v = i128_from_i64(-4);
    TEST_CHECK(int128_eq(int128_shr(v, 1), i128_from_i64(-2)));

    /* -2^64 >> 1 = -2^63 */
    v = i128(-1, 0);
    TEST_CHECK(int128_eq(int128_shr(v, 1), i128(-1, 0x8000000000000000ULL)));

    /* Shift by >= 128 */
    TEST_CHECK(int128_eq(int128_shr(i128(-1, 0), 128), i128_from_i64(-1)));
    TEST_CHECK(int128_eq(int128_shr(i128(0, 1),  128), i128_from_i64(0)));
    TEST_CHECK(int128_eq(int128_shl(i128(0, 1),  128), i128_from_i64(0)));

    /* Shift by 0 returns input */
    v = i128(-1, 0x123456789ABCDEF0ULL);
    TEST_CHECK(int128_eq(int128_shl(v, 0), v));
    TEST_CHECK(int128_eq(int128_shr(v, 0), v));
}

static void test_print(void)
{
    printf("    uint128(0xDEADBEEFCAFEBABE, 0x0123456789ABCDEF) = ");
    print_uint128(u128(0xDEADBEEFCAFEBABEULL, 0x0123456789ABCDEFULL));
    printf("\n");

    printf("    uint128(0, 0)                                       = ");
    print_uint128(u128(0, 0));
    printf("\n");

    printf("    uint128(~0, ~0)                                     = ");
    print_uint128(u128(~0ULL, ~0ULL));
    printf("\n");

    printf("    int128(-1)                                          = ");
    print_int128(i128_from_i64(-1));
    printf("\n");

    printf("    int128(1)                                           = ");
    print_int128(i128_from_i64(1));
    printf("\n");

    printf("    int128(0xDEADBEEF, 0xCAFEBABE01234567)              = ");
    print_int128(i128(0xDEADBEEF, 0xCAFEBABE01234567ULL));
    printf("\n");

    TEST_CHECK(1);
}

#if defined(__SIZEOF_INT128__)

__extension__ typedef unsigned __int128 native_u128;
__extension__ typedef __int128 native_i128;

static uint64_t fuzz_u64_word(FuzzSource* source)
{
    switch(fuzz_range(source, 0, 3))
    {
        case 0: return 0;
        case 1: return UINT64_MAX;
        default: return fuzz_u64_special(source);
    }
}

static void u128_parts(uint128_t x, uint64_t* low, uint64_t* high)
{
    uint64_t parts[2];

    memcpy(parts, &x, sizeof(parts));
    *low = parts[0];
    *high = parts[1];
}

static void i128_parts(int128_t x, uint64_t* low, uint64_t* high)
{
    uint64_t parts[2];

    memcpy(parts, &x, sizeof(parts));
    *low = parts[0];
    *high = parts[1];
}

static bool u128_matches(uint128_t x, native_u128 expected)
{
    uint64_t low, high;
    u128_parts(x, &low, &high);
    return low == (uint64_t)expected && high == (uint64_t)(expected >> 64);
}

static bool i128_matches(int128_t x, native_i128 expected)
{
    uint64_t low, high;
    i128_parts(x, &low, &high);
    return low == (uint64_t)expected && high == (uint64_t)((native_u128)expected >> 64);
}

static bool property_unsigned_matches_native(FuzzSource* source, void* user_data)
{
    const uint64_t ah = fuzz_u64_word(source), al = fuzz_u64_word(source);
    const uint64_t bh = fuzz_one_in(source, 3) ? 0 : fuzz_u64_word(source), bl = fuzz_u64_word(source);
    const int shift = (int)fuzz_range(source, 0, 127);
    const uint128_t a = make_uint128(ah, al);
    const uint128_t b = make_uint128(bh, bl);
    const native_u128 na = ((native_u128)ah << 64) | al;
    const native_u128 nb = ((native_u128)bh << 64) | bl;

    ROMANO_UNUSED(user_data);

    TEST_FUZZ_CHECK(uint128_low(a) == al && uint128_high(a) == ah);
    TEST_FUZZ_CHECK(u128_matches(uint128_add(a, b), na + nb));
    TEST_FUZZ_CHECK(u128_matches(uint128_sub(a, b), na - nb));
    TEST_FUZZ_CHECK_MSG(u128_matches(uint128_mul(a, b), na * nb), "mul %016llx%016llx * %016llx%016llx",
                        (unsigned long long)ah, (unsigned long long)al, (unsigned long long)bh, (unsigned long long)bl);
    TEST_FUZZ_CHECK(u128_matches(uint128_and(a, b), na & nb));
    TEST_FUZZ_CHECK(u128_matches(uint128_or(a, b), na | nb));
    TEST_FUZZ_CHECK(u128_matches(uint128_xor(a, b), na ^ nb));
    TEST_FUZZ_CHECK(u128_matches(uint128_not(a), ~na));
    TEST_FUZZ_CHECK(u128_matches(uint128_neg(a), -na));
    TEST_FUZZ_CHECK_MSG(u128_matches(uint128_shl(a, shift), na << shift), "shl %d", shift);
    TEST_FUZZ_CHECK_MSG(u128_matches(uint128_shr(a, shift), na >> shift), "shr %d", shift);

    TEST_FUZZ_CHECK(uint128_eq(a, b) == (na == nb));
    TEST_FUZZ_CHECK(uint128_ne(a, b) == (na != nb));
    TEST_FUZZ_CHECK(uint128_lt(a, b) == (na < nb));
    TEST_FUZZ_CHECK(uint128_le(a, b) == (na <= nb));
    TEST_FUZZ_CHECK(uint128_gt(a, b) == (na > nb));
    TEST_FUZZ_CHECK(uint128_ge(a, b) == (na >= nb));
    TEST_FUZZ_CHECK(uint128_eq(a, a) && uint128_le(a, a) && uint128_ge(a, a));

    if(nb != 0)
    {
        TEST_FUZZ_CHECK_MSG(u128_matches(uint128_div(a, b), na / nb), "div %016llx%016llx / %016llx%016llx",
                            (unsigned long long)ah, (unsigned long long)al, (unsigned long long)bh, (unsigned long long)bl);
        TEST_FUZZ_CHECK(u128_matches(uint128_mod(a, b), na % nb));
    }

    return true;
}

static bool property_signed_matches_native(FuzzSource* source, void* user_data)
{
    const int64_t ah = (int64_t)fuzz_u64_word(source);
    const uint64_t al = fuzz_u64_word(source);
    const int64_t bh = fuzz_one_in(source, 3) ? (fuzz_bool(source) ? 0 : -1) : (int64_t)fuzz_u64_word(source);
    const uint64_t bl = fuzz_u64_word(source);
    const int shift = (int)fuzz_range(source, 0, 127);
    const int128_t a = make_int128(ah, al);
    const int128_t b = make_int128(bh, bl);
    const native_i128 na = (native_i128)(((native_u128)(uint64_t)ah << 64) | al);
    const native_i128 nb = (native_i128)(((native_u128)(uint64_t)bh << 64) | bl);
    const native_i128 min = (native_i128)((native_u128)1 << 127);

    ROMANO_UNUSED(user_data);

    TEST_FUZZ_CHECK(i128_matches(int128_add(a, b), (native_i128)((native_u128)na + (native_u128)nb)));
    TEST_FUZZ_CHECK(i128_matches(int128_sub(a, b), (native_i128)((native_u128)na - (native_u128)nb)));
    TEST_FUZZ_CHECK(i128_matches(int128_mul(a, b), (native_i128)((native_u128)na * (native_u128)nb)));
    TEST_FUZZ_CHECK(i128_matches(int128_neg(a), (native_i128)(0 - (native_u128)na)));
    TEST_FUZZ_CHECK_MSG(i128_matches(int128_shl(a, shift), (native_i128)((native_u128)na << shift)), "shl %d", shift);
    TEST_FUZZ_CHECK_MSG(i128_matches(int128_shr(a, shift), na >> shift), "shr %d of %016llx%016llx", shift,
                        (unsigned long long)ah, (unsigned long long)al);

    TEST_FUZZ_CHECK(int128_eq(a, b) == (na == nb));
    TEST_FUZZ_CHECK(int128_ne(a, b) == (na != nb));
    TEST_FUZZ_CHECK_MSG(int128_lt(a, b) == (na < nb), "lt %016llx%016llx < %016llx%016llx",
                        (unsigned long long)ah, (unsigned long long)al, (unsigned long long)bh, (unsigned long long)bl);
    TEST_FUZZ_CHECK(int128_le(a, b) == (na <= nb));
    TEST_FUZZ_CHECK(int128_gt(a, b) == (na > nb));
    TEST_FUZZ_CHECK(int128_ge(a, b) == (na >= nb));

    if(nb != 0 && !(na == min && nb == -1))
    {
        TEST_FUZZ_CHECK_MSG(i128_matches(int128_div(a, b), na / nb), "div %016llx%016llx / %016llx%016llx",
                            (unsigned long long)ah, (unsigned long long)al, (unsigned long long)bh, (unsigned long long)bl);
        TEST_FUZZ_CHECK(i128_matches(int128_mod(a, b), na % nb));
    }

    return true;
}

static void test_fuzz_matches_native(void)
{
    test_fuzz_property("uint128_vs_native", 50000, property_unsigned_matches_native, NULL);
    test_fuzz_property("int128_vs_native", 50000, property_signed_matches_native, NULL);
}

#define NATIVE_TESTS TEST(test_fuzz_matches_native),
#else
#define NATIVE_TESTS
#endif /* defined(__SIZEOF_INT128__) */

TEST_MAIN(
    TEST(test_make_and_accessors),
    TEST(test_unsigned_add),
    TEST(test_unsigned_sub),
    TEST(test_unsigned_mul),
    TEST(test_unsigned_divmod),
    TEST(test_unsigned_compare),
    TEST(test_signed_compare),
    TEST(test_bitwise),
    TEST(test_unsigned_negate),
    TEST(test_unsigned_shifts),
    TEST(test_signed_arith),
    TEST(test_signed_shifts),
    TEST(test_print),
    NATIVE_TESTS
)
