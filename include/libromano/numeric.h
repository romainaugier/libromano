/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/*
 * Types: u8 u16 u32 u64 / i8 i16 i32 i64
 * Limits: U8_MAX .. U64_MAX, I8_MIN .. I64_MIN, I8_MAX .. I64_MAX
 *
 * For every type T (u8 .. i64) the following are provided:
 *
 *   T T_wrapping_OP(T a, T b) modular (two's complement) result
 *   T T_saturating_OP(T a, T b) clamped to [T_MIN, T_MAX]
 *   bool T_checked_OP(T a, T b, T* out) true on success, result in *out
 *   T T_overflowing_OP(T a, T b, bool* o) wrapped result, *o = overflowed
 *
 *   OP = add, sub, mul, div, rem, inc (saturating: add, sub, mul, div, inc)
 *   Unary: neg (all), abs (signed only), plus T_unsigned_abs for signed types
 *   Shifts: shl, shr with a u32 amount (wrapping masks the amount like Rust,
 *           checked/overflowing report amount >= bit width)
 *
 * Contracts (same as Rust, minus the panic):
 *   - div/rem with b == 0 is undefined behaviour except for checked_div /
 *     checked_rem, which return false.
 *   - checked_* functions may write *out even on failure (it then holds the
 *     wrapped value, or is left untouched for div/rem). Only trust *out when
 *     true is returned. This keeps them branch-free.
 *   - Signed results are produced by converting from the unsigned type, which
 *     assumes two's complement (true for every compiler libromano targets and
 *     mandated by C23). No signed overflow UB is ever triggered.
 *
 * Configuration defs:
 *   ROMANO_NUMERIC_NO_TYPEDEFS define if u8..i64 are already typedef'd
 *   ROMANO_NUMERIC_NO_BUILTINS force the portable fallback implementation
 */

#if !defined(__LIBROMANO_NUMERIC)
#define __LIBROMANO_NUMERIC

#include "libromano/common.h"

#include <stdint.h>
#include <stdbool.h>

/* Types and limits */

#if !defined(ROMANO_NUMERIC_NO_TYPEDEFS)
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
#endif /* !defined(ROMANO_NUMERIC_NO_TYPEDEFS) */

#define U8_MAX UINT8_MAX
#define U16_MAX UINT16_MAX
#define U32_MAX UINT32_MAX
#define U64_MAX UINT64_MAX
#define I8_MIN INT8_MIN
#define I16_MIN INT16_MIN
#define I32_MIN INT32_MIN
#define I64_MIN INT64_MIN
#define I8_MAX INT8_MAX
#define I16_MAX INT16_MAX
#define I32_MAX INT32_MAX
#define I64_MAX INT64_MAX

/* Backend selection */

#if !defined(ROMANO_NUMERIC_NO_BUILTINS)
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 5)
#define ROMANO__NUM_BUILTINS 1
#elif defined(__has_builtin)
#if __has_builtin(__builtin_add_overflow) && \
    __has_builtin(__builtin_sub_overflow) && \
    __has_builtin(__builtin_mul_overflow)
#define ROMANO__NUM_BUILTINS 1
#endif
#endif
#endif /* !defined(ROMANO_NUMERIC_NO_BUILTINS) */

#if !defined(ROMANO__NUM_BUILTINS) && defined(_MSC_VER) && \
    (defined(_M_X64) || defined(_M_ARM64))
#include <intrin.h>
#endif

/*
 * Branch-free select on an unsigned type: yields x if o is true, y otherwise.
 * y ^ ((y ^ x) & -o): GCC/Clang cannot emit cmovo from C, and a ternary on
 * the overflow flag usually becomes a jo branch, so blend with a mask.
 */
#define ROMANO__NUM_SEL(UT, o, x, y)                                    \
    ((UT)((UT)(y) ^ (UT)(((UT)(y) ^ (UT)(x)) & (UT)(0u - (UT)(o)))))

/*
 * Signed saturation value selected by the sign of x (lockless trick):
 * x >= 0 -> MAX, x < 0 -> MAX + 1 == MIN (as a bit pattern).
 */
#define ROMANO__NUM_SSAT(UT, BITS, MAXV, x) \
    ((UT)(((UT)(x) >> ((BITS) - 1)) + (UT)(MAXV)))

/* Overflowing primitives: add, sub, mul */

#if defined(ROMANO__NUM_BUILTINS)

/* The compiler emits add/sub/mul/inc + jo/jc/seto/setc/cmov directly. */
#define ROMANO__NUM_OVF(T)                                          \
    ROMANO_FORCE_INLINE T T##_overflowing_add(T a, T b, bool* o)    \
    {                                                               \
        T r;                                                        \
        *o = __builtin_add_overflow(a, b, &r);                      \
        return r;                                                   \
    }                                                               \
    ROMANO_FORCE_INLINE T T##_overflowing_sub(T a, T b, bool* o)    \
    {                                                               \
        T r;                                                        \
        *o = __builtin_sub_overflow(a, b, &r);                      \
        return r;                                                   \
    }                                                               \
    ROMANO_FORCE_INLINE T T##_overflowing_mul(T a, T b, bool* o)    \
    {                                                               \
        T r;                                                        \
        *o = __builtin_mul_overflow(a, b, &r);                      \
        return r;                                                   \
    }                                                               \
    ROMANO_FORCE_INLINE T T##_overflowing_inc(T a, bool* o)         \
    {                                                               \
        T r;                                                        \
        *o = __builtin_add_overflow(a, (T)1, &r);                   \
        return r;                                                   \
    }

ROMANO__NUM_OVF(u8)
ROMANO__NUM_OVF(u16)
ROMANO__NUM_OVF(u32)
ROMANO__NUM_OVF(u64)
ROMANO__NUM_OVF(i8)
ROMANO__NUM_OVF(i16)
ROMANO__NUM_OVF(i32)
ROMANO__NUM_OVF(i64)

#else /* Portable fallback */

#define ROMANO__NUM_OVF_ADDSUB_U(T, W)                              \
    ROMANO_FORCE_INLINE T T##_overflowing_add(T a, T b, bool* o)    \
    {                                                               \
        const T r = (T)((W)a + (W)b);                               \
        *o = r < a;                                                 \
        return r;                                                   \
    }                                                               \
    ROMANO_FORCE_INLINE T T##_overflowing_sub(T a, T b, bool* o)    \
    {                                                               \
        *o = a < b;                                                 \
        return (T)((W)a - (W)b);                                    \
    }

/* Overflow iff operands share a sign and the result's sign differs. */
#define ROMANO__NUM_OVF_ADDSUB_I(T, UT, W, BITS)                    \
    ROMANO_FORCE_INLINE T T##_overflowing_add(T a, T b, bool* o)    \
    {                                                               \
        const W ua = (UT)a, ub = (UT)b, ur = (UT)(ua + ub);         \
        *o = (bool)((((ua ^ ur) & (ub ^ ur)) >> ((BITS) - 1)) & 1u);\
        return (T)(UT)ur;                                           \
    }                                                               \
    ROMANO_FORCE_INLINE T T##_overflowing_sub(T a, T b, bool* o)    \
    {                                                               \
        const W ua = (UT)a, ub = (UT)b, ur = (UT)(ua - ub);         \
        *o = (bool)((((ua ^ ub) & (ua ^ ur)) >> ((BITS) - 1)) & 1u);\
        return (T)(UT)ur;                                           \
    }

/* 8/16/32-bit multiplies: widen to 64 bits, cannot overflow there. */
#define ROMANO__NUM_OVF_MUL_U(T, MAXV)                              \
    ROMANO_FORCE_INLINE T T##_overflowing_mul(T a, T b, bool* o)    \
    {                                                               \
        const uint64_t p = (uint64_t)a * (uint64_t)b;               \
        *o = p > (MAXV);                                            \
        return (T)p;                                                \
    }

#define ROMANO__NUM_OVF_MUL_I(T, UT, MINV, MAXV)                    \
    ROMANO_FORCE_INLINE T T##_overflowing_mul(T a, T b, bool* o)    \
    {                                                               \
        const int64_t p = (int64_t)a * (int64_t)b;                  \
        *o = (p < (MINV)) | (p > (MAXV));                           \
        return (T)(UT)(uint64_t)p;                                  \
    }

#define ROMANO__NUM_OVF_INC_U(T, W)                                 \
    ROMANO_FORCE_INLINE T T##_overflowing_inc(T a, bool* o)         \
    {                                                               \
        const T r = (T)((W)a + (W)1);                               \
        *o = r < a;                                                 \
        return r;                                                   \
    }                                                               

#define ROMANO__NUM_OVF_INC_I(T, W)                                 \
    ROMANO_FORCE_INLINE T T##_overflowing_inc(T a, bool* o)         \
    {                                                               \
        const T r = (T)((W)a + (W)1);                               \
        *o = r < a;                                                 \
        return r;                                                   \
    }                                                               

#define ROMANO__NUM_OVF_INC_I(T, UT, W, BITS)                       \
    ROMANO_FORCE_INLINE T T##_overflowing_inc(T a, bool* o)         \
    {                                                               \
        const W ua = (UT)a, ub = (UT)1, ur = (UT)(ua + ub);         \
        *o = (bool)((((ua ^ ur) & (ub ^ ur)) >> ((BITS) - 1)) & 1u);\
        return (T)(UT)ur;                                           \
    }                                                               \

ROMANO__NUM_OVF_ADDSUB_U(u8, u32)
ROMANO__NUM_OVF_ADDSUB_U(u16, u32)
ROMANO__NUM_OVF_ADDSUB_U(u32, u32)
ROMANO__NUM_OVF_ADDSUB_U(u64, u64)
ROMANO__NUM_OVF_ADDSUB_I(i8, u8, u32, 8)
ROMANO__NUM_OVF_ADDSUB_I(i16, u16, u32, 16)
ROMANO__NUM_OVF_ADDSUB_I(i32, u32, u32, 32)
ROMANO__NUM_OVF_ADDSUB_I(i64, u64, u64, 64)

ROMANO__NUM_OVF_MUL_U(u8, U8_MAX)
ROMANO__NUM_OVF_MUL_U(u16, U16_MAX)
ROMANO__NUM_OVF_MUL_U(u32, U32_MAX)
ROMANO__NUM_OVF_MUL_I(i8, u8, I8_MIN, I8_MAX)
ROMANO__NUM_OVF_MUL_I(i16, u16, I16_MIN, I16_MAX)
ROMANO__NUM_OVF_MUL_I(i32, u32, I32_MIN, I32_MAX)

ROMANO__NUM_OVF_INC_U(u8, u32)
ROMANO__NUM_OVF_INC_U(u16, u32)
ROMANO__NUM_OVF_INC_U(u32, u32)
ROMANO__NUM_OVF_INC_U(u64, u64)
ROMANO__NUM_OVF_INC_I(i8, u8, u32, 8)
ROMANO__NUM_OVF_INC_I(i16, u16, u32, 16)
ROMANO__NUM_OVF_INC_I(i32, u32, u32, 32)
ROMANO__NUM_OVF_INC_I(i64, u64, u64, 64)

ROMANO_FORCE_INLINE u64 u64_overflowing_mul(u64 a, u64 b, bool* o)
{
#if defined(_MSC_VER) && defined(_M_X64)
    u64 hi;
    const u64 lo = _umul128(a, b, &hi);
    *o = hi != 0;
    return lo;
#elif defined(_MSC_VER) && defined(_M_ARM64)
    *o = __umulh(a, b) != 0;
    return a * b;
#else
    *o = (a != 0) && (b > U64_MAX / a);
    return a * b;
#endif
}

ROMANO_FORCE_INLINE i64 i64_overflowing_mul(i64 a, i64 b, bool* o)
{
#if defined(_MSC_VER) && defined(_M_X64)
    i64 hi;
    const i64 lo = _mul128(a, b, &hi);
    *o = hi != (lo >> 63);
    return lo;
#elif defined(_MSC_VER) && defined(_M_ARM64)
    const i64 lo = (i64)((u64)a * (u64)b);
    *o = __mulh(a, b) != (lo >> 63);
    return lo;
#else
    if(a > 0)
        *o = (b > 0) ? (a > I64_MAX / b) : (b < I64_MIN / a);
    else
        *o = (b > 0) ? (a < I64_MIN / b) : ((a != 0) && (b < I64_MAX / a));

    return (i64)((u64)a * (u64)b);
#endif
}

#endif /* defined(ROMANO__NUM_BUILTINS) */

/* Unsigned API */
/* W: computation type (u32 for <= 32 bits, avoids int promotion UB) */

#define ROMANO__NUM_DEF_UNSIGNED(T, W, BITS, MAXV)                           \
    /* wrapping */                                                           \
    ROMANO_FORCE_INLINE T T##_wrapping_add(T a, T b) { return (T)((W)a + (W)b); } \
    ROMANO_FORCE_INLINE T T##_wrapping_sub(T a, T b) { return (T)((W)a - (W)b); } \
    ROMANO_FORCE_INLINE T T##_wrapping_mul(T a, T b) { return (T)((W)a * (W)b); } \
    ROMANO_FORCE_INLINE T T##_wrapping_div(T a, T b) { return (T)(a / b); }  \
    ROMANO_FORCE_INLINE T T##_wrapping_inc(T a) { return (T)((W)a + (W)1); } \
    ROMANO_FORCE_INLINE T T##_wrapping_rem(T a, T b) { return (T)(a % b); }  \
    ROMANO_FORCE_INLINE T T##_wrapping_neg(T a) { return (T)((W)0 - (W)a); } \
    ROMANO_FORCE_INLINE T T##_wrapping_shl(T a, u32 s)                       \
    {                                                                        \
        return (T)((W)a << (s & ((BITS) - 1)));                              \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_wrapping_shr(T a, u32 s)                       \
    {                                                                        \
        return (T)(a >> (s & ((BITS) - 1)));                                 \
    }                                                                        \
                                                                             \
    /* overflowing (add/sub/mul are the primitives above) */                 \
    ROMANO_FORCE_INLINE T T##_overflowing_div(T a, T b, bool* o)             \
    {                                                                        \
        *o = false;                                                          \
        return (T)(a / b);                                                   \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_overflowing_rem(T a, T b, bool* o)             \
    {                                                                        \
        *o = false;                                                          \
        return (T)(a % b);                                                   \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_overflowing_neg(T a, bool* o)                  \
    {                                                                        \
        *o = a != 0;                                                         \
        return T##_wrapping_neg(a);                                          \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_overflowing_shl(T a, u32 s, bool* o)           \
    {                                                                        \
        *o = s >= (BITS);                                                    \
        return T##_wrapping_shl(a, s);                                       \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_overflowing_shr(T a, u32 s, bool* o)           \
    {                                                                        \
        *o = s >= (BITS);                                                    \
        return T##_wrapping_shr(a, s);                                       \
    }                                                                        \
                                                                             \
    /* saturating: add -> sbb/or, sub -> cmov/and, mul -> or with mask */    \
    ROMANO_FORCE_INLINE T T##_saturating_add(T a, T b)                       \
    {                                                                        \
        bool o;                                                              \
        const T r = T##_overflowing_add(a, b, &o);                           \
        return (T)(r | (T)(0u - (T)o)); /* add; sbb; or */                   \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_saturating_sub(T a, T b)                       \
    {                                                                        \
        bool o;                                                              \
        const T r = T##_overflowing_sub(a, b, &o);                           \
        return o ? (T)0 : r; /* sub; cmovnb */                               \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_saturating_mul(T a, T b)                       \
    {                                                                        \
        bool o;                                                              \
        const T r = T##_overflowing_mul(a, b, &o);                           \
        return (T)(r | (T)(0u - (T)o)); /* mul; sbb; or */                   \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_saturating_div(T a, T b) { return (T)(a / b); } \
                                                                             \
    ROMANO_FORCE_INLINE T T##_saturating_inc(T a)                            \
    {                                                                        \
        return T##_saturating_add(a, (T)1);                                  \
    }                                                                        \
                                                                             \
    /* checked */                                                            \
    ROMANO_FORCE_INLINE bool T##_checked_add(T a, T b, T* out)               \
    {                                                                        \
        bool o;                                                              \
        *out = T##_overflowing_add(a, b, &o);                                \
        return !o;                                                           \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_sub(T a, T b, T* out)               \
    {                                                                        \
        bool o;                                                              \
        *out = T##_overflowing_sub(a, b, &o);                                \
        return !o;                                                           \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_mul(T a, T b, T* out)               \
    {                                                                        \
        bool o;                                                              \
        *out = T##_overflowing_mul(a, b, &o);                                \
        return !o;                                                           \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_div(T a, T b, T* out)               \
    {                                                                        \
        if(b == 0)                                                           \
            return false;                                                    \
        *out = (T)(a / b);                                                   \
        return true;                                                         \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_inc(T a, T* out)                    \
    {                                                                        \
        return T##_checked_add(a, 1, out);                                   \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_rem(T a, T b, T* out)               \
    {                                                                        \
        if(b == 0)                                                           \
            return false;                                                    \
        *out = (T)(a % b);                                                   \
        return true;                                                         \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_neg(T a, T* out)                    \
    {                                                                        \
        *out = T##_wrapping_neg(a);                                          \
        return a == 0;                                                       \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_shl(T a, u32 s, T* out)             \
    {                                                                        \
        *out = T##_wrapping_shl(a, s);                                       \
        return s < (BITS);                                                   \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_shr(T a, u32 s, T* out)             \
    {                                                                        \
        *out = T##_wrapping_shr(a, s);                                       \
        return s < (BITS);                                                   \
    }

/* Signed API */
/* All arithmetic is done on the unsigned counterpart UT (no signed UB) */

#define ROMANO__NUM_DEF_SIGNED(T, UT, W, BITS, MINV, MAXV)                   \
    /* wrapping */                                                           \
    ROMANO_FORCE_INLINE T T##_wrapping_add(T a, T b)                         \
    {                                                                        \
        return (T)(UT)((W)(UT)a + (W)(UT)b);                                 \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_wrapping_sub(T a, T b)                         \
    {                                                                        \
        return (T)(UT)((W)(UT)a - (W)(UT)b);                                 \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_wrapping_mul(T a, T b)                         \
    {                                                                        \
        return (T)(UT)((W)(UT)a * (W)(UT)b);                                 \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_wrapping_inc(T a)                              \
    {                                                                        \
        return (T)(UT)((W)(UT)a + (W)(UT)1);                                 \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_wrapping_neg(T a)                              \
    {                                                                        \
        return (T)(UT)((W)0 - (W)(UT)a);                                     \
    }                                                                        \
    /* abs via sign mask: (a ^ m) - m, m = a >> (BITS - 1) */                \
    ROMANO_FORCE_INLINE UT T##_unsigned_abs(T a)                             \
    {                                                                        \
        const W m = (UT)(a >> ((BITS) - 1));                                 \
        return (UT)(((W)(UT)a ^ m) - m);                                     \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_wrapping_abs(T a)                              \
    {                                                                        \
        return (T)T##_unsigned_abs(a);                                       \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_wrapping_shl(T a, u32 s)                       \
    {                                                                        \
        return (T)(UT)((W)(UT)a << (s & ((BITS) - 1)));                      \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_wrapping_shr(T a, u32 s)                       \
    {                                                                        \
        return (T)(a >> (s & ((BITS) - 1)));                                 \
    }                                                                        \
                                                                             \
    /* overflowing. div/rem: MIN / -1 is the only overflow (and traps on  */ \
    /* x86). Turn the divisor -1 into 1: MIN / 1 == MIN, MIN % 1 == 0.    */ \
    ROMANO_FORCE_INLINE T T##_overflowing_div(T a, T b, bool* o)             \
    {                                                                        \
        const bool ov = (a == (MINV)) & (b == -1);                           \
        *o = ov;                                                             \
        return (T)(a / (T)(b + 2 * (int)ov));                                \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_overflowing_rem(T a, T b, bool* o)             \
    {                                                                        \
        const bool ov = (a == (MINV)) & (b == -1);                           \
        *o = ov;                                                             \
        return (T)(a % (T)(b + 2 * (int)ov));                                \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_wrapping_div(T a, T b)                         \
    {                                                                        \
        bool o;                                                              \
        return T##_overflowing_div(a, b, &o);                                \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_wrapping_rem(T a, T b)                         \
    {                                                                        \
        bool o;                                                              \
        return T##_overflowing_rem(a, b, &o);                                \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_overflowing_neg(T a, bool* o)                  \
    {                                                                        \
        *o = a == (MINV);                                                    \
        return T##_wrapping_neg(a);                                          \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_overflowing_abs(T a, bool* o)                  \
    {                                                                        \
        *o = a == (MINV);                                                    \
        return T##_wrapping_abs(a);                                          \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_overflowing_shl(T a, u32 s, bool* o)           \
    {                                                                        \
        *o = s >= (BITS);                                                    \
        return T##_wrapping_shl(a, s);                                       \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_overflowing_shr(T a, u32 s, bool* o)           \
    {                                                                        \
        *o = s >= (BITS);                                                    \
        return T##_wrapping_shr(a, s);                                       \
    }                                                                        \
                                                                             \
    /* saturating: result or MIN/MAX picked by sign, via cmov/mask */       \
    ROMANO_FORCE_INLINE T T##_saturating_add(T a, T b)                       \
    {                                                                        \
        bool o;                                                              \
        const T r = T##_overflowing_add(a, b, &o);                           \
        return (T)ROMANO__NUM_SEL(UT, o, ROMANO__NUM_SSAT(UT, BITS, MAXV, a), (UT)r); \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_saturating_sub(T a, T b)                       \
    {                                                                        \
        bool o;                                                              \
        const T r = T##_overflowing_sub(a, b, &o);                           \
        return (T)ROMANO__NUM_SEL(UT, o, ROMANO__NUM_SSAT(UT, BITS, MAXV, a), (UT)r); \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_saturating_mul(T a, T b)                       \
    {                                                                        \
        bool o;                                                              \
        const T r = T##_overflowing_mul(a, b, &o);                           \
        return (T)ROMANO__NUM_SEL(UT, o, ROMANO__NUM_SSAT(UT, BITS, MAXV, a ^ b), (UT)r); \
    }                                                                        \
    /* lockless: (MIN + 1) / -1 == MAX, no select needed */                 \
    ROMANO_FORCE_INLINE T T##_saturating_div(T a, T b)                       \
    {                                                                        \
        const bool ov = (a == (MINV)) & (b == -1);                           \
        return (T)((T)(UT)((UT)a + (UT)ov) / b);                             \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_saturating_inc(T a)                            \
    {                                                                        \
        return T##_saturating_add(a, (T)1);                                  \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_saturating_neg(T a)                            \
    {                                                                        \
        bool o;                                                              \
        const T r = T##_overflowing_neg(a, &o);                              \
        return (T)ROMANO__NUM_SEL(UT, o, MAXV, (UT)r);                       \
    }                                                                        \
    ROMANO_FORCE_INLINE T T##_saturating_abs(T a)                            \
    {                                                                        \
        bool o;                                                              \
        const T r = T##_overflowing_abs(a, &o);                              \
        return (T)ROMANO__NUM_SEL(UT, o, MAXV, (UT)r);                       \
    }                                                                        \
                                                                             \
    /* checked */                                                            \
    ROMANO_FORCE_INLINE bool T##_checked_add(T a, T b, T* out)               \
    {                                                                        \
        bool o;                                                              \
        *out = T##_overflowing_add(a, b, &o);                                \
        return !o;                                                           \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_sub(T a, T b, T* out)               \
    {                                                                        \
        bool o;                                                              \
        *out = T##_overflowing_sub(a, b, &o);                                \
        return !o;                                                           \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_mul(T a, T b, T* out)               \
    {                                                                        \
        bool o;                                                              \
        *out = T##_overflowing_mul(a, b, &o);                                \
        return !o;                                                           \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_div(T a, T b, T* out)               \
    {                                                                        \
        if((b == 0) | ((a == (MINV)) & (b == -1)))                           \
            return false;                                                    \
        *out = (T)(a / b);                                                   \
        return true;                                                         \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_inc(T a, T* out)                    \
    {                                                                        \
        return T##_checked_add(a, (T)1, out);                                \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_rem(T a, T b, T* out)               \
    {                                                                        \
        if((b == 0) | ((a == (MINV)) & (b == -1)))                           \
            return false;                                                    \
        *out = (T)(a % b);                                                   \
        return true;                                                         \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_neg(T a, T* out)                    \
    {                                                                        \
        *out = T##_wrapping_neg(a);                                          \
        return a != (MINV);                                                  \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_abs(T a, T* out)                    \
    {                                                                        \
        *out = T##_wrapping_abs(a);                                          \
        return a != (MINV);                                                  \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_shl(T a, u32 s, T* out)             \
    {                                                                        \
        *out = T##_wrapping_shl(a, s);                                       \
        return s < (BITS);                                                   \
    }                                                                        \
    ROMANO_FORCE_INLINE bool T##_checked_shr(T a, u32 s, T* out)             \
    {                                                                        \
        *out = T##_wrapping_shr(a, s);                                       \
        return s < (BITS);                                                   \
    }

ROMANO__NUM_DEF_UNSIGNED(u8, u32, 8, U8_MAX)
ROMANO__NUM_DEF_UNSIGNED(u16, u32, 16, U16_MAX)
ROMANO__NUM_DEF_UNSIGNED(u32, u32, 32, U32_MAX)
ROMANO__NUM_DEF_UNSIGNED(u64, u64, 64, U64_MAX)

ROMANO__NUM_DEF_SIGNED(i8, u8, u32, 8, I8_MIN, I8_MAX)
ROMANO__NUM_DEF_SIGNED(i16, u16, u32, 16, I16_MIN, I16_MAX)
ROMANO__NUM_DEF_SIGNED(i32, u32, u32, 32, I32_MIN, I32_MAX)
ROMANO__NUM_DEF_SIGNED(i64, u64, u64, 64, I64_MIN, I64_MAX)

#endif /* !defined(__LIBROMANO_NUMERIC) */