/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/int128.h"
#include "libromano/logger.h"

#include <stdio.h>
#include <stddef.h>

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

static void test_check(int condition, const char* expression, int line)
{
    g_tests_run++;

    if(condition)
    {
        g_tests_passed++;
    }
    else
    {
        g_tests_failed++;
        printf("    [FAIL] line %d: %s\n", line, expression);
    }
}

#define CHECK(expr) test_check((expr) ? 1 : 0, #expr, __LINE__)

static uint128_t u128(uint64_t high, uint64_t low)
{
    return make_uint128(high, low);
}

static int128_t i128(int64_t high, uint64_t low)
{
    return make_int128(high, low);
}

/* Convenient int128 constants built from a signed 64-bit value */
static int128_t i128_from_i64(int64_t v)
{
    return make_int128((v < 0) ? -1 : 0, (uint64_t)v);
}

/* Construction and accessors */

static void test_make_and_accessors(void)
{
    uint128_t v;

    v = u128(0xDEADBEEFCAFEBABEULL, 0x0123456789ABCDEFULL);
    CHECK(uint128_high(v) == 0xDEADBEEFCAFEBABEULL);
    CHECK(uint128_low(v)  == 0x0123456789ABCDEFULL);

    v = u128(0, 0);
    CHECK(uint128_high(v) == 0);
    CHECK(uint128_low(v)  == 0);

    v = u128(~0ULL, ~0ULL);
    CHECK(uint128_high(v) == ~0ULL);
    CHECK(uint128_low(v)  == ~0ULL);

    v = u128(0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL);
    CHECK(uint128_high(v) == 0x0123456789ABCDEFULL);
    CHECK(uint128_low(v)  == 0xFEDCBA9876543210ULL);

    /* Round-trip a signed value through int128 comparison */
    CHECK(int128_eq(i128_from_i64(-1), i128(-1, ~0ULL)));
    CHECK(int128_eq(i128_from_i64(0),  i128(0, 0)));
    CHECK(int128_eq(i128_from_i64(1),  i128(0, 1)));
}

/* Unsigned addition */

static void test_unsigned_add(void)
{
    /* No carry */
    CHECK(uint128_eq(uint128_add(u128(0, 10), u128(0, 20)), u128(0, 30)));

    /* Carry from low into high */
    CHECK(uint128_eq(uint128_add(u128(0, ~0ULL), u128(0, 1)), u128(1, 0)));
    CHECK(uint128_eq(uint128_add(u128(0, 0xFFFFFFFFFFFFFFFFULL),
                                 u128(0, 0x0000000000000002ULL)),
                     u128(1, 1)));

    /* Full wrap-around */
    CHECK(uint128_eq(uint128_add(u128(~0ULL, ~0ULL), u128(0, 1)),
                     u128(0, 0)));
    CHECK(uint128_eq(uint128_add(u128(~0ULL, ~0ULL), u128(~0ULL, ~0ULL)),
                     u128(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFEULL)));

    /* Commutativity on two multi-limb values */
    CHECK(uint128_eq(
        uint128_add(u128(0x1111111111111111ULL, 0x2222222222222222ULL),
                    u128(0x3333333333333333ULL, 0x4444444444444444ULL)),
        uint128_add(u128(0x3333333333333333ULL, 0x4444444444444444ULL),
                    u128(0x1111111111111111ULL, 0x2222222222222222ULL))));

    /* Additive identity */
    CHECK(uint128_eq(
        uint128_add(u128(0xABCDEF0123456789ULL, 0x9876543210FEDCBAULL),
                    u128(0, 0)),
        u128(0xABCDEF0123456789ULL, 0x9876543210FEDCBAULL)));

    /* Purely high-word addition */
    CHECK(uint128_eq(uint128_add(u128(1, 0), u128(2, 0)), u128(3, 0)));
}

/* Unsigned subtraction */

static void test_unsigned_sub(void)
{
    /* No borrow */
    CHECK(uint128_eq(uint128_sub(u128(0, 20), u128(0, 10)), u128(0, 10)));

    /* Borrow from the high limb */
    CHECK(uint128_eq(uint128_sub(u128(1, 0), u128(0, 1)), u128(0, ~0ULL)));
    CHECK(uint128_eq(uint128_sub(u128(1, 1), u128(0, 2)), u128(0, ~0ULL)));

    /* Full wrap-around (underflow) */
    CHECK(uint128_eq(uint128_sub(u128(0, 0), u128(0, 1)), u128(~0ULL, ~0ULL)));
    CHECK(uint128_eq(uint128_sub(u128(0, 0), u128(1, 0)),
                     u128(0xFFFFFFFFFFFFFFFFULL, 0)));

    /* x - x = 0 */
    CHECK(uint128_eq(
        uint128_sub(u128(0xABCDEF0123456789ULL, 0x9876543210FEDCBAULL),
                    u128(0xABCDEF0123456789ULL, 0x9876543210FEDCBAULL)),
        u128(0, 0)));

    /* Subtract zero */
    CHECK(uint128_eq(
        uint128_sub(u128(0xCAFEBABEDEADBEEFULL, 0x0123456789ABCDEFULL),
                    u128(0, 0)),
        u128(0xCAFEBABEDEADBEEFULL, 0x0123456789ABCDEFULL)));
}

/* Unsigned multiplication */

static void test_unsigned_mul(void)
{
    /* Small values */
    CHECK(uint128_eq(uint128_mul(u128(0, 6), u128(0, 7)), u128(0, 42)));

    /* Multiply by zero and one */
    CHECK(uint128_eq(
        uint128_mul(u128(0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL), u128(0, 0)),
        u128(0, 0)));
    CHECK(uint128_eq(
        uint128_mul(u128(0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL), u128(0, 1)),
        u128(0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL)));

    /* 2^64 * 2^64 wraps to 0 (high bit falls off the top) */
    CHECK(uint128_eq(uint128_mul(u128(1, 0), u128(1, 0)), u128(0, 0)));

    /* 2^64 * 3 = 3 * 2^64 => high = 3, low = 0 */
    CHECK(uint128_eq(uint128_mul(u128(1, 0), u128(0, 3)), u128(3, 0)));

    /* 64x64 -> 128: (2^64 - 1)^2 = 2^128 - 2^65 + 1 */
    CHECK(uint128_eq(
        uint128_mul(u128(0, ~0ULL), u128(0, ~0ULL)),
        u128(0xFFFFFFFFFFFFFFFEULL, 0x0000000000000001ULL)));

    /* (2^64 - 1) * 2 = 2^65 - 2 */
    CHECK(uint128_eq(
        uint128_mul(u128(0, ~0ULL), u128(0, 2)),
        u128(1, 0xFFFFFFFFFFFFFFFEULL)));

    /* (2^64 + 1) * (2^64 + 1) = 2^128 + 2^65 + 1 -> high=2, low=1 after wrap */
    CHECK(uint128_eq(
        uint128_mul(u128(1, 1), u128(1, 1)),
        u128(2, 1)));
}

/* Unsigned division and modulo */

static void test_unsigned_divmod(void)
{
    uint128_t a, b;

    /* Small values */
    a = u128(0, 100); b = u128(0, 7);
    CHECK(uint128_eq(uint128_div(a, b), u128(0, 14)));
    CHECK(uint128_eq(uint128_mod(a, b), u128(0, 2)));

    /* Exact division */
    a = u128(0, 42); b = u128(0, 42);
    CHECK(uint128_eq(uint128_div(a, b), u128(0, 1)));
    CHECK(uint128_eq(uint128_mod(a, b), u128(0, 0)));

    /* Dividend smaller than divisor */
    a = u128(0, 42); b = u128(0, 100);
    CHECK(uint128_eq(uint128_div(a, b), u128(0, 0)));
    CHECK(uint128_eq(uint128_mod(a, b), u128(0, 42)));

    /* Division across 64-bit boundary: 2^64 / 2 = 2^63 */
    a = u128(1, 0); b = u128(0, 2);
    CHECK(uint128_eq(uint128_div(a, b), u128(0, 0x8000000000000000ULL)));
    CHECK(uint128_eq(uint128_mod(a, b), u128(0, 0)));

    /* (2^64 + 1) / 2 = 2^63 remainder 1 */
    a = u128(1, 1); b = u128(0, 2);
    CHECK(uint128_eq(uint128_div(a, b), u128(0, 0x8000000000000000ULL)));
    CHECK(uint128_eq(uint128_mod(a, b), u128(0, 1)));

    /* (2^96) / (2^32) = 2^64 */
    a = u128(0x0000000100000000ULL, 0); /* 2^96 */
    b = u128(0, 0x100000000ULL);        /* 2^32 */
    CHECK(uint128_eq(uint128_div(a, b), u128(1, 0)));
    CHECK(uint128_eq(uint128_mod(a, b), u128(0, 0)));

    /* Max / small divisor */
    a = u128(~0ULL, ~0ULL);
    b = u128(0, 2);
    CHECK(uint128_eq(uint128_div(a, b), u128(0x7FFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL)));
    CHECK(uint128_eq(uint128_mod(a, b), u128(0, 1)));

    /* x / x = 1, x % x = 0 */
    a = u128(0xFEEDFACECAFEBEEFULL, 0xDEADBEEFDEADBEEFULL);
    CHECK(uint128_eq(uint128_div(a, a), u128(0, 1)));
    CHECK(uint128_eq(uint128_mod(a, a), u128(0, 0)));
}

/* Unsigned comparisons */

static void test_unsigned_compare(void)
{
    uint128_t big   = u128(1, 0);
    uint128_t small = u128(0, ~0ULL);

    CHECK(uint128_gt(big, small));
    CHECK(uint128_ge(big, small));
    CHECK(uint128_ne(big, small));
    CHECK(!uint128_eq(big, small));
    CHECK(!uint128_lt(big, small));
    CHECK(!uint128_le(big, small));

    CHECK(uint128_lt(small, big));
    CHECK(uint128_le(small, big));
    CHECK(uint128_gt(small, big) == 0);
    CHECK(uint128_lt(small, big) != 0);

    /* Reflexive */
    CHECK(uint128_eq(big, big));
    CHECK(uint128_le(big, big));
    CHECK(uint128_ge(big, big));
    CHECK(!uint128_lt(big, big));
    CHECK(!uint128_gt(big, big));

    /* Zero versus one */
    CHECK(uint128_lt(u128(0, 0), u128(0, 1)));
    CHECK(uint128_gt(u128(0, 1), u128(0, 0)));
    CHECK(uint128_eq(u128(0, 0), u128(0, 0)));

    /* Equal high, different low */
    CHECK(uint128_lt(u128(5, 1), u128(5, 2)));
    CHECK(uint128_gt(u128(5, 2), u128(5, 1)));

    /* Different high dominates */
    CHECK(uint128_lt(u128(1, ~0ULL), u128(2, 0)));
    CHECK(uint128_gt(u128(2, 0), u128(1, ~0ULL)));

    /* Maximum */
    CHECK(uint128_gt(u128(~0ULL, ~0ULL), u128(~0ULL, ~0ULL - 1)));
}

/* Bitwise operations */

static void test_bitwise(void)
{
    uint128_t a = u128(0xF0F0F0F0F0F0F0F0ULL, 0x0F0F0F0F0F0F0F0FULL);
    uint128_t b = u128(0xFFFFFFFFFFFFFFFFULL, 0x0000000000000000ULL);

    CHECK(uint128_eq(uint128_and(a, b), u128(0xF0F0F0F0F0F0F0F0ULL, 0)));
    CHECK(uint128_eq(uint128_or(a, b),  u128(0xFFFFFFFFFFFFFFFFULL, 0x0F0F0F0F0F0F0F0FULL)));
    CHECK(uint128_eq(uint128_xor(a, b), u128(0x0F0F0F0F0F0F0F0FULL, 0x0F0F0F0F0F0F0F0FULL)));
    CHECK(uint128_eq(uint128_not(a),    u128(0x0F0F0F0F0F0F0F0FULL, 0xF0F0F0F0F0F0F0F0ULL)));

    /* Identity laws */
    CHECK(uint128_eq(uint128_and(a, a), a));
    CHECK(uint128_eq(uint128_or(a, a),  a));
    CHECK(uint128_eq(uint128_xor(a, a), u128(0, 0)));
    CHECK(uint128_eq(uint128_and(a, u128(0, 0)), u128(0, 0)));
    CHECK(uint128_eq(uint128_or(a,  u128(0, 0)), a));
    CHECK(uint128_eq(uint128_xor(a, u128(0, 0)), a));
    CHECK(uint128_eq(uint128_and(a, u128(~0ULL, ~0ULL)), a));
    CHECK(uint128_eq(uint128_or(a,  u128(~0ULL, ~0ULL)), u128(~0ULL, ~0ULL)));

    /* Involution: ~~a == a */
    CHECK(uint128_eq(uint128_not(uint128_not(a)), a));

    /* De Morgan */
    CHECK(uint128_eq(uint128_not(uint128_and(a, b)),
                     uint128_or(uint128_not(a), uint128_not(b))));
    CHECK(uint128_eq(uint128_not(uint128_or(a, b)),
                     uint128_and(uint128_not(a), uint128_not(b))));
}

/* Unsigned negation */

static void test_unsigned_negate(void)
{
    CHECK(uint128_eq(uint128_neg(u128(0, 0)), u128(0, 0)));
    CHECK(uint128_eq(uint128_neg(u128(0, 1)), u128(~0ULL, ~0ULL)));
    CHECK(uint128_eq(uint128_neg(u128(0, 5)), u128(~0ULL, ~0ULL - 4)));
    CHECK(uint128_eq(uint128_neg(u128(~0ULL, ~0ULL)), u128(0, 1)));

    /* x + (-x) == 0 */
    CHECK(uint128_eq(
        uint128_add(u128(0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL),
                    uint128_neg(u128(0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL))),
        u128(0, 0)));

    /* Double negation */
    CHECK(uint128_eq(uint128_neg(uint128_neg(u128(0xABCDEF, 0x12345))),
                     u128(0xABCDEF, 0x12345)));
}

/* Unsigned shifts */

static void test_unsigned_shifts(void)
{
    uint128_t v = u128(0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL);

    /* Shift by zero returns same value */
    CHECK(uint128_eq(uint128_shl(v, 0), v));
    CHECK(uint128_eq(uint128_shr(v, 0), v));

    /* Negative shift count returns same value */
    CHECK(uint128_eq(uint128_shl(v, -1), v));
    CHECK(uint128_eq(uint128_shr(v, -1), v));

    /* Shift by >= 128 returns 0 */
    CHECK(uint128_eq(uint128_shl(v, 128), u128(0, 0)));
    CHECK(uint128_eq(uint128_shr(v, 128), u128(0, 0)));
    CHECK(uint128_eq(uint128_shl(v, 200), u128(0, 0)));
    CHECK(uint128_eq(uint128_shr(v, 200), u128(0, 0)));

    /* Shift by 64 swaps halves */
    CHECK(uint128_eq(uint128_shl(v, 64), u128(0xFEDCBA9876543210ULL, 0)));
    CHECK(uint128_eq(uint128_shr(v, 64), u128(0, 0x0123456789ABCDEFULL)));

    /* Simple small shifts */
    CHECK(uint128_eq(uint128_shl(u128(0, 1), 1), u128(0, 2)));
    CHECK(uint128_eq(uint128_shl(u128(0, 1), 4), u128(0, 16)));
    CHECK(uint128_eq(uint128_shl(u128(0, 1), 63), u128(0, 0x8000000000000000ULL)));
    CHECK(uint128_eq(uint128_shl(u128(0, 1), 64), u128(1, 0)));
    CHECK(uint128_eq(uint128_shl(u128(0, 1), 65), u128(2, 0)));
    CHECK(uint128_eq(uint128_shl(u128(0, 1), 127), u128(0x8000000000000000ULL, 0)));

    CHECK(uint128_eq(uint128_shr(u128(0, 2), 1), u128(0, 1)));
    CHECK(uint128_eq(uint128_shr(u128(1, 0), 1), u128(0, 0x8000000000000000ULL)));
    CHECK(uint128_eq(uint128_shr(u128(1, 0), 64), u128(0, 1)));
    CHECK(uint128_eq(uint128_shr(u128(2, 0), 65), u128(0, 1)));
    CHECK(uint128_eq(uint128_shr(u128(0x8000000000000000ULL, 0), 127), u128(0, 1)));

    /* Cross-boundary carry-in on left shift */
    CHECK(uint128_eq(uint128_shl(u128(0, 0x8000000000000000ULL), 1), u128(1, 0)));
    CHECK(uint128_eq(uint128_shl(u128(0, 0xC000000000000000ULL), 1),
                     u128(1, 0x8000000000000000ULL)));

    /* Cross-boundary carry-out on right shift */
    CHECK(uint128_eq(uint128_shr(u128(1, 0), 1), u128(0, 0x8000000000000000ULL)));
    CHECK(uint128_eq(uint128_shr(u128(0, ~0ULL), 63), u128(0, 1)));

    /* Shift by 63 boundary */
    CHECK(uint128_eq(uint128_shr(u128(1, 0), 63), u128(0, 2)));
    CHECK(uint128_eq(uint128_shl(u128(0, 1), 63), u128(0, 0x8000000000000000ULL)));

    /* Round trip for shiftable values */
    CHECK(uint128_eq(uint128_shr(uint128_shl(u128(0, 0x1234), 40), 40),
                     u128(0, 0x1234)));
}

/* Signed arithmetic */

static void test_signed_arith(void)
{
    int128_t a, b, r;

    /* Addition */
    CHECK(int128_eq(int128_add(i128_from_i64(10), i128_from_i64(20)),
                    i128_from_i64(30)));
    CHECK(int128_eq(int128_add(i128_from_i64(-1), i128_from_i64(1)),
                    i128_from_i64(0)));
    CHECK(int128_eq(int128_add(i128_from_i64(-5), i128_from_i64(-7)),
                    i128_from_i64(-12)));

    /* Subtraction */
    CHECK(int128_eq(int128_sub(i128_from_i64(5), i128_from_i64(3)),
                    i128_from_i64(2)));
    CHECK(int128_eq(int128_sub(i128_from_i64(0), i128_from_i64(1)),
                    i128_from_i64(-1)));
    CHECK(int128_eq(int128_sub(i128_from_i64(-5), i128_from_i64(-3)),
                    i128_from_i64(-2)));

    /* Negation */
    CHECK(int128_eq(int128_neg(i128_from_i64(0)),  i128_from_i64(0)));
    CHECK(int128_eq(int128_neg(i128_from_i64(1)),  i128_from_i64(-1)));
    CHECK(int128_eq(int128_neg(i128_from_i64(-1)), i128_from_i64(1)));
    CHECK(int128_eq(int128_neg(i128_from_i64(42)), i128_from_i64(-42)));

    /* Multiplication */
    CHECK(int128_eq(int128_mul(i128_from_i64(6), i128_from_i64(7)),
                    i128_from_i64(42)));
    CHECK(int128_eq(int128_mul(i128_from_i64(-1), i128_from_i64(5)),
                    i128_from_i64(-5)));
    CHECK(int128_eq(int128_mul(i128_from_i64(-1), i128_from_i64(-1)),
                    i128_from_i64(1)));
    CHECK(int128_eq(int128_mul(i128_from_i64(0), i128_from_i64(12345)),
                    i128_from_i64(0)));

    /* Division (truncates toward zero, matching C's /) */
    CHECK(int128_eq(int128_div(i128_from_i64(100), i128_from_i64(7)),
                    i128_from_i64(14)));
    CHECK(int128_eq(int128_div(i128_from_i64(-100), i128_from_i64(7)),
                    i128_from_i64(-14)));
    CHECK(int128_eq(int128_div(i128_from_i64(100), i128_from_i64(-7)),
                    i128_from_i64(-14)));
    CHECK(int128_eq(int128_div(i128_from_i64(-100), i128_from_i64(-7)),
                    i128_from_i64(14)));
    CHECK(int128_eq(int128_div(i128_from_i64(0), i128_from_i64(1)),
                    i128_from_i64(0)));

    /* Modulo (sign of result follows dividend) */
    CHECK(int128_eq(int128_mod(i128_from_i64(100), i128_from_i64(7)),
                    i128_from_i64(2)));
    CHECK(int128_eq(int128_mod(i128_from_i64(-100), i128_from_i64(7)),
                    i128_from_i64(-2)));
    CHECK(int128_eq(int128_mod(i128_from_i64(100), i128_from_i64(-7)),
                    i128_from_i64(2)));
    CHECK(int128_eq(int128_mod(i128_from_i64(-100), i128_from_i64(-7)),
                    i128_from_i64(-2)));

    /* Mixed large value: -(2^64) + 5 */
    a = i128(-1, 5);
    b = i128_from_i64(5);
    r = int128_add(a, b);
    CHECK(int128_eq(r, i128(-1, 10)));
}

/* Signed comparisons */

static void test_signed_compare(void)
{
    int128_t pos = i128_from_i64(1);
    int128_t zero = i128_from_i64(0);
    int128_t neg = i128_from_i64(-1);
    int128_t neg2 = i128_from_i64(-2);

    CHECK(int128_lt(neg, pos));
    CHECK(int128_le(neg, pos));
    CHECK(int128_ne(neg, pos));
    CHECK(!int128_eq(neg, pos));
    CHECK(int128_gt(pos, neg));
    CHECK(int128_ge(pos, neg));

    CHECK(int128_lt(neg, zero));
    CHECK(int128_gt(zero, neg));
    CHECK(int128_eq(zero, zero));

    CHECK(int128_lt(neg2, neg));
    CHECK(int128_gt(neg, neg2));

    /* Reflexive */
    CHECK(int128_eq(neg, neg));
    CHECK(int128_le(neg, neg));
    CHECK(int128_ge(neg, neg));
    CHECK(!int128_lt(neg, neg));
    CHECK(!int128_gt(neg, neg));

    /* -2^64 (high = -1, low = 0) is still negative and less than -1 */
    CHECK(int128_lt(i128(-1, 0), i128(-1, ~0ULL)));
}

/* Signed shifts */

static void test_signed_shifts(void)
{
    int128_t v;

    /* Left shift */
    v = i128_from_i64(1);
    CHECK(int128_eq(int128_shl(v, 4),  i128_from_i64(16)));
    CHECK(int128_eq(int128_shl(v, 64), i128(1, 0)));

    /* Left shift by 0 */
    v = i128_from_i64(-1);
    CHECK(int128_eq(int128_shl(v, 0), v));

    /* Arithmetic right shift on positive */
    v = i128(0, 0x8000000000000000ULL);
    CHECK(int128_eq(int128_shr(v, 1), i128(0, 0x4000000000000000ULL)));
    CHECK(int128_eq(int128_shr(v, 63), i128_from_i64(1)));
    CHECK(int128_eq(int128_shr(v, 64), i128_from_i64(0)));

    /* Arithmetic right shift on -1 stays -1 */
    v = i128_from_i64(-1);
    CHECK(int128_eq(int128_shr(v, 1),   v));
    CHECK(int128_eq(int128_shr(v, 63),  v));
    CHECK(int128_eq(int128_shr(v, 64),  v));
    CHECK(int128_eq(int128_shr(v, 127), v));

    /* -4 >> 1 = -2 */
    v = i128_from_i64(-4);
    CHECK(int128_eq(int128_shr(v, 1), i128_from_i64(-2)));

    /* -2^64 >> 1 = -2^63 */
    v = i128(-1, 0);
    CHECK(int128_eq(int128_shr(v, 1), i128(-1, 0x8000000000000000ULL)));

    /* Shift by >= 128 */
    CHECK(int128_eq(int128_shr(i128(-1, 0), 128), i128_from_i64(-1)));
    CHECK(int128_eq(int128_shr(i128(0, 1),  128), i128_from_i64(0)));
    CHECK(int128_eq(int128_shl(i128(0, 1),  128), i128_from_i64(0)));

    /* Shift by 0 returns input */
    v = i128(-1, 0x123456789ABCDEF0ULL);
    CHECK(int128_eq(int128_shl(v, 0), v));
    CHECK(int128_eq(int128_shr(v, 0), v));
}

/* Print helpers */

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

    CHECK(1);
}

/* Entry point */

typedef void (*test_fn_t)(void);

typedef struct
{
    const char* name;
    test_fn_t   fn;
} test_case_t;

int main(void)
{
    logger_init();
    logger_set_level(LogLevel_Debug);

    static const test_case_t tests[] =
    {
        { "make_and_accessors", test_make_and_accessors },
        { "unsigned_add",       test_unsigned_add       },
        { "unsigned_sub",       test_unsigned_sub       },
        { "unsigned_mul",       test_unsigned_mul       },
        { "unsigned_divmod",    test_unsigned_divmod    },
        { "unsigned_compare",   test_unsigned_compare   },
        { "signed_compare",     test_signed_compare     },
        { "bitwise",            test_bitwise            },
        { "unsigned_negate",    test_unsigned_negate    },
        { "unsigned_shifts",    test_unsigned_shifts    },
        { "signed_arith",       test_signed_arith       },
        { "signed_shifts",      test_signed_shifts      },
        { "print_smoke",        test_print        },
    };

    const size_t count = sizeof(tests) / sizeof(tests[0]);
    size_t i;

    logger_log_info("Running int128 test suite...");
    logger_log_info("--------------------------------------------------");

    for(i = 0; i < count; i++)
    {
        logger_log_info("[ %s ]", tests[i].name);
        tests[i].fn();
    }

    logger_log_info("--------------------------------------------------");
    logger_log_info("Tests run:    %d", g_tests_run);
    logger_log_info("Tests passed: %d", g_tests_passed);
    logger_log_info("Tests failed: %d", g_tests_failed);

    if(g_tests_failed == 0)
    {
        logger_log_info("Result:       PASS");
        return 0;
    }

    logger_log_info("Result:       FAIL");

    logger_release();

    return 1;
}