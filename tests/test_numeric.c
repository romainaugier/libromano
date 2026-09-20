/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/numeric.h"

#define EXPECT(cond) TEST_CHECK(cond)

#define EXPECT_EQ_U(T, actual, expected)                                        \
    do {                                                                        \
        const T a_ = (T)(actual);                                               \
        const T e_ = (T)(expected);                                             \
        test_check_uint((u64)a_, (u64)e_, __FILE__, __LINE__, #actual);         \
    } while(0)

#define EXPECT_EQ_I(T, actual, expected)                                        \
    do {                                                                        \
        const T a_ = (T)(actual);                                               \
        const T e_ = (T)(expected);                                             \
        test_check_int((i64)a_, (i64)e_, __FILE__, __LINE__, #actual);          \
    } while(0)

/* S is U (unsigned) or I (signed): selects how mismatches are printed. */
#define EQ(S, T, actual, expected) EXPECT_EQ_##S(T, actual, expected)

/* Per-function check helpers. Arguments are cast to T (shift amounts to u32) */

/* Binary operations */
#define WRAP2(S, T, op, a, b, exp) \
    EQ(S, T, T##_wrapping_##op((T)(a), (T)(b)), exp)

#define SAT2(S, T, op, a, b, exp) \
    EQ(S, T, T##_saturating_##op((T)(a), (T)(b)), exp)

#define OVF2(S, T, op, a, b, exp, ovf)                                 \
    do {                                                               \
        bool f_ = !(ovf);                                              \
        EQ(S, T, T##_overflowing_##op((T)(a), (T)(b), &f_), exp);      \
        EXPECT(f_ == (ovf));                                           \
    } while(0)

#define CHK2(S, T, op, a, b, ok, exp)                                  \
    do {                                                               \
        T o_ = (T)0;                                                   \
        const bool r_ = T##_checked_##op((T)(a), (T)(b), &o_);         \
        EXPECT(r_ == (ok));                                            \
        if(ok)                                                         \
            EQ(S, T, o_, exp);                                         \
    } while(0)

/* Unary operations */
#define WRAP1(S, T, op, a, exp) \
    EQ(S, T, T##_wrapping_##op((T)(a)), exp)

#define SAT1(S, T, op, a, exp) \
    EQ(S, T, T##_saturating_##op((T)(a)), exp)

#define OVF1(S, T, op, a, exp, ovf)                                    \
    do {                                                               \
        bool f_ = !(ovf);                                              \
        EQ(S, T, T##_overflowing_##op((T)(a), &f_), exp);              \
        EXPECT(f_ == (ovf));                                           \
    } while(0)

#define CHK1(S, T, op, a, ok, exp)                                     \
    do {                                                               \
        T o_ = (T)0;                                                   \
        const bool r_ = T##_checked_##op((T)(a), &o_);                 \
        EXPECT(r_ == (ok));                                            \
        if(ok)                                                         \
            EQ(S, T, o_, exp);                                         \
    } while(0)

/* Shift operations */
#define WRAPS(S, T, op, a, s, exp) \
    EQ(S, T, T##_wrapping_##op((T)(a), (u32)(s)), exp)

#define OVFS(S, T, op, a, s, exp, ovf)                                 \
    do {                                                               \
        bool f_ = !(ovf);                                              \
        EQ(S, T, T##_overflowing_##op((T)(a), (u32)(s), &f_), exp);    \
        EXPECT(f_ == (ovf));                                           \
    } while(0)

#define CHKS(S, T, op, a, s, ok, exp)                                  \
    do {                                                               \
        T o_ = (T)0;                                                   \
        const bool r_ = T##_checked_##op((T)(a), (u32)(s), &o_);       \
        EXPECT(r_ == (ok));                                            \
        if(ok)                                                         \
            EQ(S, T, o_, exp);                                         \
    } while(0)

/* checked div/rem by zero must fail and leave *out untouched */
#define CHK_DIV_ZERO_UNTOUCHED(S, T, op, a)                            \
    do {                                                               \
        T o_ = (T)42;                                                  \
        EXPECT(!T##_checked_##op((T)(a), (T)0, &o_));                  \
        EQ(S, T, o_, 42);                                              \
    } while(0)

/* Tests for unsigned */

/*
 * HALF = 2^(BITS/2): HALF * HALF == 2^BITS, the smallest square that overflows.
 * HIGH = 2^(BITS-1): only the top bit set.
 */
#define TEST_UNSIGNED(T, BITS, MAX)                                            \
static void test_##T(void)                                                     \
{                                                                              \
    const T HALF = (T)((T)1 << ((BITS) / 2));                                  \
    const T HIGH = (T)((MAX) - (MAX) / 2);                                     \
                                                                               \
    /* ---- add ---- */                                                        \
    WRAP2(U, T, add, 1, 2, 3);                                                 \
    WRAP2(U, T, add, 0, 0, 0);                                                 \
    WRAP2(U, T, add, MAX, 1, 0);                                               \
    WRAP2(U, T, add, MAX, MAX, MAX - 1);                                       \
    WRAP2(U, T, add, HIGH, HIGH, 0);                                           \
                                                                               \
    OVF2(U, T, add, 1, 2, 3, false);                                           \
    OVF2(U, T, add, MAX - 1, 1, MAX, false);                                   \
    OVF2(U, T, add, MAX, 0, MAX, false);                                       \
    OVF2(U, T, add, MAX, 1, 0, true);                                          \
    OVF2(U, T, add, MAX, MAX, MAX - 1, true);                                  \
    OVF2(U, T, add, HIGH, HIGH, 0, true);                                      \
                                                                               \
    SAT2(U, T, add, 1, 2, 3);                                                  \
    SAT2(U, T, add, MAX - 1, 1, MAX);                                          \
    SAT2(U, T, add, MAX, 1, MAX);                                              \
    SAT2(U, T, add, MAX, MAX, MAX);                                            \
    SAT2(U, T, add, HIGH, HIGH, MAX);                                          \
    SAT2(U, T, add, 0, 0, 0);                                                  \
                                                                               \
    CHK2(U, T, add, 1, 2, true, 3);                                            \
    CHK2(U, T, add, MAX - 1, 1, true, MAX);                                    \
    CHK2(U, T, add, MAX, 1, false, 0);                                         \
    CHK2(U, T, add, MAX, MAX, false, 0);                                       \
                                                                               \
    /* ---- sub ---- */                                                        \
    WRAP2(U, T, sub, 5, 3, 2);                                                 \
    WRAP2(U, T, sub, 0, 1, MAX);                                               \
    WRAP2(U, T, sub, 0, MAX, 1);                                               \
    WRAP2(U, T, sub, MAX, MAX, 0);                                             \
                                                                               \
    OVF2(U, T, sub, 5, 3, 2, false);                                           \
    OVF2(U, T, sub, 1, 1, 0, false);                                           \
    OVF2(U, T, sub, MAX, MAX, 0, false);                                       \
    OVF2(U, T, sub, 0, 1, MAX, true);                                          \
    OVF2(U, T, sub, 1, MAX, 2, true);                                          \
                                                                               \
    SAT2(U, T, sub, 5, 3, 2);                                                  \
    SAT2(U, T, sub, 1, 1, 0);                                                  \
    SAT2(U, T, sub, 0, 1, 0);                                                  \
    SAT2(U, T, sub, 1, MAX, 0);                                                \
    SAT2(U, T, sub, MAX, 0, MAX);                                              \
                                                                               \
    CHK2(U, T, sub, 5, 3, true, 2);                                            \
    CHK2(U, T, sub, MAX, MAX, true, 0);                                        \
    CHK2(U, T, sub, 0, 1, false, 0);                                           \
    CHK2(U, T, sub, MAX - 1, MAX, false, 0);                                   \
                                                                               \
    /* ---- mul ---- */                                                        \
    WRAP2(U, T, mul, 6, 7, 42);                                                \
    WRAP2(U, T, mul, 0, MAX, 0);                                               \
    WRAP2(U, T, mul, MAX, 2, MAX - 1);                                         \
    WRAP2(U, T, mul, MAX, MAX, 1);                                             \
    WRAP2(U, T, mul, HALF, HALF, 0);                                           \
                                                                               \
    OVF2(U, T, mul, 6, 7, 42, false);                                          \
    OVF2(U, T, mul, MAX, 1, MAX, false);                                       \
    OVF2(U, T, mul, 0, MAX, 0, false);                                         \
    OVF2(U, T, mul, HALF, HALF - 1, MAX - HALF + 1, false);                    \
    OVF2(U, T, mul, HALF, HALF, 0, true);                                      \
    OVF2(U, T, mul, MAX, 2, MAX - 1, true);                                    \
    OVF2(U, T, mul, HIGH, 2, 0, true);                                         \
                                                                               \
    SAT2(U, T, mul, 6, 7, 42);                                                 \
    SAT2(U, T, mul, MAX, 1, MAX);                                              \
    SAT2(U, T, mul, 0, MAX, 0);                                                \
    SAT2(U, T, mul, HALF, HALF, MAX);                                          \
    SAT2(U, T, mul, MAX, 2, MAX);                                              \
    SAT2(U, T, mul, MAX, MAX, MAX);                                            \
                                                                               \
    CHK2(U, T, mul, 6, 7, true, 42);                                           \
    CHK2(U, T, mul, MAX, 1, true, MAX);                                        \
    CHK2(U, T, mul, HALF, HALF - 1, true, MAX - HALF + 1);                     \
    CHK2(U, T, mul, HALF, HALF, false, 0);                                     \
    CHK2(U, T, mul, MAX, 2, false, 0);                                         \
                                                                               \
    /* ---- div ---- */                                                        \
    WRAP2(U, T, div, 7, 2, 3);                                                 \
    WRAP2(U, T, div, 0, 5, 0);                                                 \
    WRAP2(U, T, div, MAX, 1, MAX);                                             \
    WRAP2(U, T, div, MAX, MAX, 1);                                             \
    WRAP2(U, T, div, 5, 7, 0);                                                 \
                                                                               \
    OVF2(U, T, div, 7, 2, 3, false);                                           \
    OVF2(U, T, div, MAX, 2, MAX / 2, false);                                   \
                                                                               \
    SAT2(U, T, div, 7, 2, 3);                                                  \
    SAT2(U, T, div, MAX, 1, MAX);                                              \
                                                                               \
    CHK2(U, T, div, 7, 2, true, 3);                                            \
    CHK2(U, T, div, MAX, MAX, true, 1);                                        \
    CHK2(U, T, div, 0, 0, false, 0);                                           \
    CHK2(U, T, div, MAX, 0, false, 0);                                         \
    CHK_DIV_ZERO_UNTOUCHED(U, T, div, 1);                                      \
                                                                               \
    /* ---- inc ---- */                                                        \
    WRAP1(U, T, inc, 1, 2);                                                    \
    WRAP1(U, T, inc, 0, 1);                                                    \
    WRAP1(U, T, inc, MAX, 0);                                                  \
    WRAP1(U, T, inc, HIGH, HIGH + 1);                                          \
                                                                               \
    OVF1(U, T, inc, 1, 2, false);                                              \
    OVF1(U, T, inc, MAX - 1, MAX, false);                                      \
    OVF1(U, T, inc, MAX, 0, true);                                             \
    OVF1(U, T, inc, HIGH, HIGH + 1, false);                                    \
                                                                               \
    SAT1(U, T, inc, 1, 2);                                                     \
    SAT1(U, T, inc, MAX - 1, MAX);                                             \
    SAT1(U, T, inc, MAX, MAX);                                                 \
    SAT1(U, T, inc, MAX, MAX);                                                 \
    SAT1(U, T, inc, HIGH, HIGH + 1);                                           \
    SAT1(U, T, inc, 0, 1);                                                     \
                                                                               \
    CHK1(U, T, inc, 1, true, 2);                                               \
    CHK1(U, T, inc, MAX - 1, true, MAX);                                       \
    CHK1(U, T, inc, MAX, false, 0);                                            \
                                                                               \
    /* ---- rem ---- */                                                        \
    WRAP2(U, T, rem, 7, 2, 1);                                                 \
    WRAP2(U, T, rem, 5, 7, 5);                                                 \
    WRAP2(U, T, rem, MAX, MAX, 0);                                             \
    WRAP2(U, T, rem, MAX, 2, 1);                                               \
                                                                               \
    OVF2(U, T, rem, 7, 2, 1, false);                                           \
    OVF2(U, T, rem, MAX, 1, 0, false);                                         \
                                                                               \
    CHK2(U, T, rem, 7, 3, true, 1);                                            \
    CHK2(U, T, rem, 0, 0, false, 0);                                           \
    CHK_DIV_ZERO_UNTOUCHED(U, T, rem, 7);                                      \
                                                                               \
    /* ---- neg ---- */                                                        \
    WRAP1(U, T, neg, 0, 0);                                                    \
    WRAP1(U, T, neg, 1, MAX);                                                  \
    WRAP1(U, T, neg, MAX, 1);                                                  \
    WRAP1(U, T, neg, HIGH, HIGH);                                              \
                                                                               \
    OVF1(U, T, neg, 0, 0, false);                                              \
    OVF1(U, T, neg, 1, MAX, true);                                             \
    OVF1(U, T, neg, MAX, 1, true);                                             \
                                                                               \
    CHK1(U, T, neg, 0, true, 0);                                               \
    CHK1(U, T, neg, 1, false, 0);                                              \
    CHK1(U, T, neg, MAX, false, 0);                                            \
                                                                               \
    /* ---- shl: amount is masked with BITS - 1 ---- */                        \
    WRAPS(U, T, shl, 1, 0, 1);                                                 \
    WRAPS(U, T, shl, 1, (BITS) - 1, HIGH);                                     \
    WRAPS(U, T, shl, 1, (BITS), 1);                                            \
    WRAPS(U, T, shl, 1, (BITS) + 1, 2);                                        \
    WRAPS(U, T, shl, MAX, 1, MAX - 1);                                         \
    WRAPS(U, T, shl, HIGH, 1, 0);                                              \
                                                                               \
    OVFS(U, T, shl, 1, (BITS) - 1, HIGH, false);                               \
    OVFS(U, T, shl, 1, (BITS), 1, true);                                       \
    OVFS(U, T, shl, 3, 2 * (BITS) + 1, 6, true);                               \
                                                                               \
    CHKS(U, T, shl, 1, (BITS) - 1, true, HIGH);                                \
    CHKS(U, T, shl, 1, (BITS), false, 0);                                      \
    CHKS(U, T, shl, 1, 0xFFFFFFFFu, false, 0);                                 \
                                                                               \
    /* ---- shr ---- */                                                        \
    WRAPS(U, T, shr, 2, 1, 1);                                                 \
    WRAPS(U, T, shr, MAX, (BITS) - 1, 1);                                      \
    WRAPS(U, T, shr, MAX, (BITS), MAX);                                        \
    WRAPS(U, T, shr, HIGH, (BITS) + 1, HIGH / 2);                              \
                                                                               \
    OVFS(U, T, shr, MAX, (BITS) - 1, 1, false);                                \
    OVFS(U, T, shr, MAX, (BITS), MAX, true);                                   \
                                                                               \
    CHKS(U, T, shr, MAX, (BITS) - 1, true, 1);                                 \
    CHKS(U, T, shr, MAX, (BITS), false, 0);                                    \
}

TEST_UNSIGNED(u8, 8, U8_MAX)
TEST_UNSIGNED(u16, 16, U16_MAX)
TEST_UNSIGNED(u32, 32, U32_MAX)
TEST_UNSIGNED(u64, 64, U64_MAX)

/* Tests for signed */

/*
 * HALF = 2^(BITS/2), QRT = 2^(BITS/2 - 1):
 *   QRT * HALF    ==  2^(BITS-1) == MAX + 1  -> overflows
 *   -QRT * HALF   == -2^(BITS-1) == MIN      -> does NOT overflow
 */
#define TEST_SIGNED(T, UT, BITS, MIN, MAX)                                     \
static void test_##T(void)                                                     \
{                                                                              \
    const T HALF = (T)((T)1 << ((BITS) / 2));                                  \
    const T QRT = (T)((T)1 << ((BITS) / 2 - 1));                               \
                                                                               \
    /* add */                                                                  \
    WRAP2(I, T, add, 1, 2, 3);                                                 \
    WRAP2(I, T, add, -1, 1, 0);                                                \
    WRAP2(I, T, add, MAX, 1, MIN);                                             \
    WRAP2(I, T, add, MIN, -1, MAX);                                            \
    WRAP2(I, T, add, MAX, MAX, -2);                                            \
    WRAP2(I, T, add, MIN, MIN, 0);                                             \
    WRAP2(I, T, add, MAX, MIN, -1);                                            \
                                                                               \
    OVF2(I, T, add, 1, 2, 3, false);                                           \
    OVF2(I, T, add, MAX, MIN, -1, false);                                      \
    OVF2(I, T, add, MAX - 1, 1, MAX, false);                                   \
    OVF2(I, T, add, MIN + 1, -1, MIN, false);                                  \
    OVF2(I, T, add, MAX, 1, MIN, true);                                        \
    OVF2(I, T, add, MIN, -1, MAX, true);                                       \
    OVF2(I, T, add, MAX, MAX, -2, true);                                       \
    OVF2(I, T, add, MIN, MIN, 0, true);                                        \
                                                                               \
    SAT2(I, T, add, 1, 2, 3);                                                  \
    SAT2(I, T, add, -5, 3, -2);                                                \
    SAT2(I, T, add, MAX, 1, MAX);                                              \
    SAT2(I, T, add, MAX, MAX, MAX);                                            \
    SAT2(I, T, add, MIN, -1, MIN);                                             \
    SAT2(I, T, add, MIN, MIN, MIN);                                            \
    SAT2(I, T, add, MAX, MIN, -1);                                             \
                                                                               \
    CHK2(I, T, add, -5, 3, true, -2);                                          \
    CHK2(I, T, add, MAX - 1, 1, true, MAX);                                    \
    CHK2(I, T, add, MAX, 1, false, 0);                                         \
    CHK2(I, T, add, MIN, -1, false, 0);                                        \
                                                                               \
    /* sub */                                                                  \
    WRAP2(I, T, sub, 3, 5, -2);                                                \
    WRAP2(I, T, sub, MIN, 1, MAX);                                             \
    WRAP2(I, T, sub, MAX, -1, MIN);                                            \
    WRAP2(I, T, sub, 0, MIN, MIN);                                             \
    WRAP2(I, T, sub, -1, MIN, MAX);                                            \
                                                                               \
    OVF2(I, T, sub, 3, 5, -2, false);                                          \
    OVF2(I, T, sub, -1, MIN, MAX, false);                                      \
    OVF2(I, T, sub, -1, MAX, MIN, false);                                      \
    OVF2(I, T, sub, MIN, 1, MAX, true);                                        \
    OVF2(I, T, sub, MAX, -1, MIN, true);                                       \
    OVF2(I, T, sub, 0, MIN, MIN, true);                                        \
    OVF2(I, T, sub, -2, MAX, MAX, true);                                       \
                                                                               \
    SAT2(I, T, sub, 3, 5, -2);                                                 \
    SAT2(I, T, sub, MIN, 1, MIN);                                              \
    SAT2(I, T, sub, MAX, -1, MAX);                                             \
    SAT2(I, T, sub, 0, MIN, MAX);                                              \
    SAT2(I, T, sub, -1, MIN, MAX);                                             \
    SAT2(I, T, sub, -2, MAX, MIN);                                             \
                                                                               \
    CHK2(I, T, sub, 3, 5, true, -2);                                           \
    CHK2(I, T, sub, -1, MIN, true, MAX);                                       \
    CHK2(I, T, sub, 0, MIN, false, 0);                                         \
    CHK2(I, T, sub, MIN, 1, false, 0);                                         \
                                                                               \
    /* mul */                                                                  \
    WRAP2(I, T, mul, -3, 4, -12);                                              \
    WRAP2(I, T, mul, -3, -4, 12);                                              \
    WRAP2(I, T, mul, MAX, 2, -2);                                              \
    WRAP2(I, T, mul, MIN, -1, MIN);                                            \
    WRAP2(I, T, mul, MIN, 2, 0);                                               \
    WRAP2(I, T, mul, MAX, MAX, 1);                                             \
    WRAP2(I, T, mul, MIN, MIN, 0);                                             \
                                                                               \
    OVF2(I, T, mul, -3, 4, -12, false);                                        \
    OVF2(I, T, mul, MAX, -1, -MAX, false);                                     \
    OVF2(I, T, mul, MIN, 1, MIN, false);                                       \
    OVF2(I, T, mul, -QRT, HALF, MIN, false);                                   \
    OVF2(I, T, mul, QRT, -HALF, MIN, false);                                   \
    OVF2(I, T, mul, QRT, HALF, MIN, true);                                     \
    OVF2(I, T, mul, -QRT, -HALF, MIN, true);                                   \
    OVF2(I, T, mul, MIN, -1, MIN, true);                                       \
    OVF2(I, T, mul, MAX, 2, -2, true);                                         \
    OVF2(I, T, mul, HALF, HALF, 0, true);                                      \
    /* exact boundaries for every sign combination (off-by-one killers) */     \
    OVF2(I, T, mul, 2, MAX / 2, MAX - 1, false);             /* + * + */       \
    OVF2(I, T, mul, 2, MAX / 2 + 1, MIN, true);                                \
    OVF2(I, T, mul, 2, MIN / 2, MIN, false);                 /* + * - */       \
    OVF2(I, T, mul, 2, MIN / 2 - 1, MAX - 1, true);                            \
    OVF2(I, T, mul, MIN / 2, 2, MIN, false);                 /* - * + */       \
    OVF2(I, T, mul, MIN / 2 - 1, 2, MAX - 1, true);                            \
    OVF2(I, T, mul, -1, -MAX, MAX, false);                   /* - * - */       \
    OVF2(I, T, mul, -2, -(MAX / 2), MAX - 1, false);                           \
    OVF2(I, T, mul, -2, -(MAX / 2) - 1, MIN, true);                            \
    OVF2(I, T, mul, -(MAX / 2) - 1, -2, MIN, true);                            \
    OVF2(I, T, mul, 0, MIN, 0, false);                                         \
    OVF2(I, T, mul, MIN, 0, 0, false);                                         \
                                                                               \
    SAT2(I, T, mul, -3, 4, -12);                                               \
    SAT2(I, T, mul, MAX, -1, -MAX);                                            \
    SAT2(I, T, mul, -QRT, HALF, MIN);                                          \
    SAT2(I, T, mul, QRT, HALF, MAX);                                           \
    SAT2(I, T, mul, MAX, 2, MAX);                                              \
    SAT2(I, T, mul, MIN, 2, MIN);                                              \
    SAT2(I, T, mul, MAX, -2, MIN);                                             \
    SAT2(I, T, mul, MIN, -1, MAX);                                             \
    SAT2(I, T, mul, MIN, MIN, MAX);                                            \
    SAT2(I, T, mul, MAX, MIN, MIN);                                            \
                                                                               \
    CHK2(I, T, mul, -3, 4, true, -12);                                         \
    CHK2(I, T, mul, -QRT, HALF, true, MIN);                                    \
    CHK2(I, T, mul, QRT, HALF, false, 0);                                      \
    CHK2(I, T, mul, MIN, -1, false, 0);                                        \
                                                                               \
    /* div (truncates toward zero) */                                          \
    WRAP2(I, T, div, 7, 2, 3);                                                 \
    WRAP2(I, T, div, -7, 2, -3);                                               \
    WRAP2(I, T, div, 7, -2, -3);                                               \
    WRAP2(I, T, div, MIN, 1, MIN);                                             \
    WRAP2(I, T, div, MIN, -1, MIN);                                            \
    WRAP2(I, T, div, MAX, -1, -MAX);                                           \
                                                                               \
    OVF2(I, T, div, -7, 2, -3, false);                                         \
    OVF2(I, T, div, MIN, 2, MIN / 2, false);                                   \
    OVF2(I, T, div, MIN, -2, -(MIN / 2), false);                               \
    OVF2(I, T, div, MIN, -1, MIN, true);                                       \
                                                                               \
    SAT2(I, T, div, -7, 2, -3);                                                \
    SAT2(I, T, div, MIN, 1, MIN);                                              \
    SAT2(I, T, div, MIN, -1, MAX);                                             \
    SAT2(I, T, div, MAX, -1, -MAX);                                            \
    SAT2(I, T, div, MIN + 1, -1, MAX);                                         \
                                                                               \
    CHK2(I, T, div, -7, 2, true, -3);                                          \
    CHK2(I, T, div, MIN, 1, true, MIN);                                        \
    CHK2(I, T, div, MIN, -2, true, -(MIN / 2));                                \
    CHK2(I, T, div, MIN, -1, false, 0);                                        \
    CHK2(I, T, div, 1, 0, false, 0);                                           \
    CHK_DIV_ZERO_UNTOUCHED(I, T, div, MIN);                                    \
                                                                               \
    /* ---- inc ---- */                                                        \
    WRAP1(I, T, inc, 1, 2);                                                    \
    WRAP1(I, T, inc, 0, 1);                                                    \
    WRAP1(I, T, inc, MAX, MIN);                                                \
                                                                               \
    OVF1(I, T, inc, 1, 2, false);                                              \
    OVF1(I, T, inc, MAX - 1, MAX, false);                                      \
    OVF1(I, T, inc, MAX, MIN, true);                                           \
                                                                               \
    SAT1(I, T, inc, 1, 2);                                                     \
    SAT1(I, T, inc, MAX - 1, MAX);                                             \
    SAT1(I, T, inc, MAX, MAX);                                                 \
    SAT1(I, T, inc, MAX, MAX);                                                 \
    SAT1(I, T, inc, 0, 1);                                                     \
                                                                               \
    CHK1(I, T, inc, 1, true, 2);                                               \
    CHK1(I, T, inc, MAX - 1, true, MAX);                                       \
    CHK1(I, T, inc, MAX, false, MIN);                                          \
                                                                               \
    /* rem (sign of dividend) */                                               \
    WRAP2(I, T, rem, 7, 2, 1);                                                 \
    WRAP2(I, T, rem, -7, 2, -1);                                               \
    WRAP2(I, T, rem, 7, -2, 1);                                                \
    WRAP2(I, T, rem, MIN, -1, 0);                                              \
    WRAP2(I, T, rem, MIN, MAX, -1);                                            \
                                                                               \
    OVF2(I, T, rem, -7, 2, -1, false);                                         \
    OVF2(I, T, rem, MIN, 1, 0, false);                                         \
    OVF2(I, T, rem, MIN, -1, 0, true);                                         \
                                                                               \
    CHK2(I, T, rem, -7, 2, true, -1);                                          \
    CHK2(I, T, rem, MIN, -1, false, 0);                                        \
    CHK2(I, T, rem, 1, 0, false, 0);                                           \
    CHK_DIV_ZERO_UNTOUCHED(I, T, rem, MIN);                                    \
                                                                               \
    /* neg */                                                                  \
    WRAP1(I, T, neg, 0, 0);                                                    \
    WRAP1(I, T, neg, -5, 5);                                                   \
    WRAP1(I, T, neg, MAX, MIN + 1);                                            \
    WRAP1(I, T, neg, MIN, MIN);                                                \
                                                                               \
    OVF1(I, T, neg, 5, -5, false);                                             \
    OVF1(I, T, neg, MIN + 1, MAX, false);                                      \
    OVF1(I, T, neg, MIN, MIN, true);                                           \
                                                                               \
    SAT1(I, T, neg, 5, -5);                                                    \
    SAT1(I, T, neg, MAX, -MAX);                                                \
    SAT1(I, T, neg, MIN, MAX);                                                 \
                                                                               \
    CHK1(I, T, neg, 0, true, 0);                                               \
    CHK1(I, T, neg, MAX, true, -MAX);                                          \
    CHK1(I, T, neg, MIN, false, 0);                                            \
                                                                               \
    /* abs */                                                                  \
    WRAP1(I, T, abs, 0, 0);                                                    \
    WRAP1(I, T, abs, 5, 5);                                                    \
    WRAP1(I, T, abs, -1, 1);                                                   \
    WRAP1(I, T, abs, -MAX, MAX);                                               \
    WRAP1(I, T, abs, MIN, MIN);                                                \
                                                                               \
    OVF1(I, T, abs, -5, 5, false);                                             \
    OVF1(I, T, abs, MIN + 1, MAX, false);                                      \
    OVF1(I, T, abs, MIN, MIN, true);                                           \
                                                                               \
    SAT1(I, T, abs, -5, 5);                                                    \
    SAT1(I, T, abs, MAX, MAX);                                                 \
    SAT1(I, T, abs, MIN, MAX);                                                 \
                                                                               \
    CHK1(I, T, abs, -5, true, 5);                                              \
    CHK1(I, T, abs, MIN + 1, true, MAX);                                       \
    CHK1(I, T, abs, MIN, false, 0);                                            \
                                                                               \
    /* unsigned_abs: total, MIN maps to MAX + 1 */                             \
    EXPECT_EQ_U(UT, T##_unsigned_abs((T)0), 0);                                \
    EXPECT_EQ_U(UT, T##_unsigned_abs((T)-1), 1);                               \
    EXPECT_EQ_U(UT, T##_unsigned_abs((T)(MAX)), (UT)(MAX));                    \
    EXPECT_EQ_U(UT, T##_unsigned_abs((T)(-MAX)), (UT)(MAX));                   \
    EXPECT_EQ_U(UT, T##_unsigned_abs((T)(MIN)), (UT)(MAX) + 1u);               \
                                                                               \
    /* shl (bits shifted out of the sign are not an overflow, as Rust) */      \
    WRAPS(I, T, shl, 1, 0, 1);                                                 \
    WRAPS(I, T, shl, 1, (BITS) - 1, MIN);                                      \
    WRAPS(I, T, shl, 1, (BITS), 1);                                            \
    WRAPS(I, T, shl, -1, 1, -2);                                               \
    WRAPS(I, T, shl, MAX, 1, -2);                                              \
    WRAPS(I, T, shl, MIN, 1, 0);                                               \
                                                                               \
    OVFS(I, T, shl, 1, (BITS) - 1, MIN, false);                                \
    OVFS(I, T, shl, 1, (BITS) + 2, 4, true);                                   \
                                                                               \
    CHKS(I, T, shl, -1, (BITS) - 1, true, MIN);                                \
    CHKS(I, T, shl, 1, (BITS), false, 0);                                      \
                                                                               \
    /* shr (arithmetic: sign is propagated) */                                 \
    WRAPS(I, T, shr, -8, 1, -4);                                               \
    WRAPS(I, T, shr, -1, 5, -1);                                               \
    WRAPS(I, T, shr, MIN, (BITS) - 1, -1);                                     \
    WRAPS(I, T, shr, MAX, (BITS) - 2, 1);                                      \
    WRAPS(I, T, shr, 4, (BITS) + 1, 2);                                        \
                                                                               \
    OVFS(I, T, shr, MIN, (BITS) - 1, -1, false);                               \
    OVFS(I, T, shr, MIN, (BITS), MIN, true);                                   \
                                                                               \
    CHKS(I, T, shr, MIN, (BITS) - 1, true, -1);                                \
    CHKS(I, T, shr, MIN, (BITS), false, 0);                                    \
}

TEST_SIGNED(i8, u8, 8, I8_MIN, I8_MAX)
TEST_SIGNED(i16, u16, 16, I16_MIN, I16_MAX)
TEST_SIGNED(i32, u32, 32, I32_MIN, I32_MAX)
TEST_SIGNED(i64, u64, 64, I64_MIN, I64_MAX)

/* Tests against wide (64-bit) arithmetic, types <= 32 bits */

static i64 clamp_i64(i64 v, i64 lo, i64 hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Checks wrapping / overflowing / checked (/ saturating) for one input pair. */
#define REF_BIN_NOSAT(S, T, op, a, b, wrapv, ovf)                      \
    do {                                                               \
        const bool ovf_ = (ovf);                                       \
        bool f_ = !ovf_;                                               \
        T o_ = (T)0;                                                   \
        EQ(S, T, T##_wrapping_##op(a, b), wrapv);                      \
        EQ(S, T, T##_overflowing_##op(a, b, &f_), wrapv);              \
        EXPECT(f_ == ovf_);                                            \
        EXPECT(T##_checked_##op(a, b, &o_) == !ovf_);                  \
        if(!ovf_)                                                      \
            EQ(S, T, o_, wrapv);                                       \
    } while(0)

#define REF_BIN(S, T, op, a, b, wrapv, ovf, satv)                      \
    do {                                                               \
        REF_BIN_NOSAT(S, T, op, a, b, wrapv, ovf);                     \
        EQ(S, T, T##_saturating_##op(a, b), satv);                     \
    } while(0)

#define REF_UNSIGNED(T, BITS, MAX)                                             \
static void ref_##T(T a, T b, u32 s)                                           \
{                                                                              \
    const u64 A = a, B = b;                                                    \
    const u32 sh = s & ((BITS) - 1);                                           \
    T o_;                                                                      \
                                                                               \
    { const u64 e = A + B; const bool o = e > (MAX);                           \
      REF_BIN(U, T, add, a, b, e, o, o ? (MAX) : e); }                         \
    { const bool o = a < b;                                                    \
      REF_BIN(U, T, sub, a, b, A - B, o, o ? 0 : A - B); }                     \
    { const u64 e = A * B; const bool o = e > (MAX);                           \
      REF_BIN(U, T, mul, a, b, e, o, o ? (MAX) : e); }                         \
                                                                               \
    if(b != 0)                                                                 \
    {                                                                          \
        REF_BIN(U, T, div, a, b, A / B, false, A / B);                         \
        REF_BIN_NOSAT(U, T, rem, a, b, A % B, false);                          \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        EXPECT(!T##_checked_div(a, b, &o_));                                   \
        EXPECT(!T##_checked_rem(a, b, &o_));                                   \
    }                                                                          \
                                                                               \
    { bool f; EQ(U, T, T##_wrapping_neg(a), 0u - A);                           \
      EQ(U, T, T##_overflowing_neg(a, &f), 0u - A); EXPECT(f == (a != 0));     \
      EXPECT(T##_checked_neg(a, &o_) == (a == 0)); }                           \
                                                                               \
    { bool f; const T e = (T)(A << sh);                                        \
      EQ(U, T, T##_wrapping_shl(a, s), e);                                     \
      EQ(U, T, T##_overflowing_shl(a, s, &f), e); EXPECT(f == (s >= (BITS)));  \
      EXPECT(T##_checked_shl(a, s, &o_) == (s < (BITS))); }                    \
    { bool f; const T e = (T)(A >> sh);                                        \
      EQ(U, T, T##_wrapping_shr(a, s), e);                                     \
      EQ(U, T, T##_overflowing_shr(a, s, &f), e); EXPECT(f == (s >= (BITS)));  \
      EXPECT(T##_checked_shr(a, s, &o_) == (s < (BITS))); }                    \
}

/* WRAP(e) reduces an exact i64 result modulo 2^BITS into T. */
#define REF_SIGNED(T, UT, BITS, MIN, MAX)                                      \
static void ref_##T(T a, T b, u32 s)                                           \
{                                                                              \
    const i64 A = a, B = b;                                                    \
    const u32 sh = s & ((BITS) - 1);                                           \
    T o_;                                                                      \
                                                                               \
    { const i64 e = A + B; const bool o = e < (MIN) || e > (MAX);              \
      REF_BIN(I, T, add, a, b, (T)(UT)(u64)e, o, clamp_i64(e, MIN, MAX)); }    \
    { const i64 e = A - B; const bool o = e < (MIN) || e > (MAX);              \
      REF_BIN(I, T, sub, a, b, (T)(UT)(u64)e, o, clamp_i64(e, MIN, MAX)); }    \
    { const i64 e = A * B; const bool o = e < (MIN) || e > (MAX);              \
      REF_BIN(I, T, mul, a, b, (T)(UT)(u64)e, o, clamp_i64(e, MIN, MAX)); }    \
                                                                               \
    if(b != 0)                                                                 \
    {                                                                          \
        const i64 q = A / B, r = A % B;                                        \
        const bool o = q > (MAX);                                              \
        REF_BIN(I, T, div, a, b, (T)(UT)(u64)q, o, clamp_i64(q, MIN, MAX));    \
        REF_BIN_NOSAT(I, T, rem, a, b, (T)r, o);                               \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        EXPECT(!T##_checked_div(a, b, &o_));                                   \
        EXPECT(!T##_checked_rem(a, b, &o_));                                   \
    }                                                                          \
                                                                               \
    { const i64 e = -A; const bool o = e > (MAX); bool f;                      \
      EQ(I, T, T##_wrapping_neg(a), (T)(UT)(u64)e);                            \
      EQ(I, T, T##_overflowing_neg(a, &f), (T)(UT)(u64)e); EXPECT(f == o);     \
      EQ(I, T, T##_saturating_neg(a), clamp_i64(e, MIN, MAX));                 \
      EXPECT(T##_checked_neg(a, &o_) == !o); }                                 \
                                                                               \
    { const i64 e = A < 0 ? -A : A; const bool o = e > (MAX); bool f;          \
      EQ(I, T, T##_wrapping_abs(a), (T)(UT)(u64)e);                            \
      EQ(I, T, T##_overflowing_abs(a, &f), (T)(UT)(u64)e); EXPECT(f == o);     \
      EQ(I, T, T##_saturating_abs(a), clamp_i64(e, MIN, MAX));                 \
      EXPECT(T##_checked_abs(a, &o_) == !o);                                   \
      EXPECT_EQ_U(UT, T##_unsigned_abs(a), (UT)e); }                           \
                                                                               \
    { bool f; const T e = (T)(UT)((u64)A << sh);                               \
      EQ(I, T, T##_wrapping_shl(a, s), e);                                     \
      EQ(I, T, T##_overflowing_shl(a, s, &f), e); EXPECT(f == (s >= (BITS)));  \
      EXPECT(T##_checked_shl(a, s, &o_) == (s < (BITS))); }                    \
    /* floor(A / 2^sh), written without right-shifting a negative value */     \
    { bool f; const T e = (T)(A >= 0 ? (A >> sh) : ~((~A) >> sh));             \
      EQ(I, T, T##_wrapping_shr(a, s), e);                                     \
      EQ(I, T, T##_overflowing_shr(a, s, &f), e); EXPECT(f == (s >= (BITS)));  \
      EXPECT(T##_checked_shr(a, s, &o_) == (s < (BITS))); }                    \
}

REF_UNSIGNED(u8, 8, U8_MAX)
REF_UNSIGNED(u16, 16, U16_MAX)
REF_UNSIGNED(u32, 32, U32_MAX)
REF_SIGNED(i8, u8, 8, I8_MIN, I8_MAX)
REF_SIGNED(i16, u16, 16, I16_MIN, I16_MAX)
REF_SIGNED(i32, u32, 32, I32_MIN, I32_MAX)

static void test_exhaustive_8bit(void)
{
    u32 a, b;

    for(a = 0; a < 256; a++)
    {
        for(b = 0; b < 256; b++)
        {
            ref_u8((u8)a, (u8)b, b);
            ref_i8((i8)(u8)a, (i8)(u8)b, b);
        }
    }
}

static bool property_16_32bit(FuzzSource* source, void* user_data)
{
    const u64 a = fuzz_u64_special(source);
    const u64 b = fuzz_u64_special(source);
    const u32 s = (u32)fuzz_range(source, 0, 79);

    ROMANO_UNUSED(user_data);

    ref_u16((u16)a, (u16)b, s);
    ref_i16((i16)(u16)a, (i16)(u16)b, s);
    ref_u32((u32)a, (u32)b, s);
    ref_i32((i32)(u32)a, (i32)(u32)b, s);

    return true;
}

static void test_fuzz_16_32bit(void)
{
    test_fuzz_property("numeric_16_32bit", 200000, property_16_32bit, NULL);
}

/* Misc: typedef widths and limits */

static void test_types(void)
{
    EXPECT(sizeof(u8) == 1 && sizeof(i8) == 1);
    EXPECT(sizeof(u16) == 2 && sizeof(i16) == 2);
    EXPECT(sizeof(u32) == 4 && sizeof(i32) == 4);
    EXPECT(sizeof(u64) == 8 && sizeof(i64) == 8);

    EXPECT((u8)-1 == U8_MAX && (u16)-1 == U16_MAX);
    EXPECT((u32)-1 == U32_MAX && (u64)-1 == U64_MAX);

    EXPECT(I8_MIN == -I8_MAX - 1 && I16_MIN == -I16_MAX - 1);
    EXPECT(I32_MIN == -I32_MAX - 1 && I64_MIN == -I64_MAX - 1);
}

TEST_MAIN(
    TEST(test_types),
    TEST(test_u8),
    TEST(test_u16),
    TEST(test_u32),
    TEST(test_u64),
    TEST(test_i8),
    TEST(test_i16),
    TEST(test_i32),
    TEST(test_i64),
    TEST(test_exhaustive_8bit),
    TEST(test_fuzz_16_32bit),
)
