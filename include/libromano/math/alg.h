/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#if !defined(__LIBROMANO_MATH_ALG)
#define __LIBROMANO_MATH_ALG

#include "libromano/libromano.h"
#include "libromano/bool.h"

#if defined(ROMANO_MSVC)
#include <corecrt_math.h>
#endif /* defined(ROMANO_MSVC) */

ROMANO_CPP_ENTER

/* 32 bits float constants */

#define PI 3.14159265358979323846f
#define PI_OVER_TWO 1.57079632679489661923f
#define PI_OVER_FOUR 0.785398163397448309616f
#define ONE_OVER_PI 0.318309886183790671538f
#define TWO_OVER_PI 0.636619772367581343076f

#if defined(ROMANO_GCC) || defined(ROMANO_CLANG)
#define INF __builtin_huge_valf()
#define NEGINF -__builtin_huge_valf()
#elif defined(ROMANO_MSVC)
#define INF FP_INFINITE
#define NEGINF FP_INFINITE
#endif /* defined(ROMANO_GCC) */

#define SQRT2 1.41421356237309504880f
#define ONE_OVER_SQRT2 0.707106781186547524401f

#define E 2.71828182845904523536f

#define LOG2E 1.44269504088896340736f
#define LOG10E 0.434294481903251827651f
#define LN2 0.693147180559945309417f
#define LN10 2.30258509299404568402f

#define ZERO 0.0f
#define ONE 1.0f
#define MIN_FLOAT 1.175494351e-38f
#define MAX_FLOAT 3.402823466e+38f

#if defined(ROMANO_MSVC)
ROMANO_FORCE_INLINE bool float_eq(float a, float b) { return _fdpcomp(a, b) == _FP_EQ; }
ROMANO_FORCE_INLINE bool float_gt(float a, float b) { return _fdpcomp(a, b) == _FP_GT; }
ROMANO_FORCE_INLINE bool float_lt(float a, float b) { return _fdpcomp(a, b) == _FP_LT; }
#elif defined(ROMANO_GCC)
ROMANO_FORCE_INLINE bool float_eq(float a, float b) { return a == b; }
ROMANO_FORCE_INLINE bool float_gt(float a, float b) { return __builtin_isgreater(a, b); }
ROMANO_FORCE_INLINE bool float_lt(float a, float b) { return __builtin_isless(a, b); }
#else
ROMANO_FORCE_INLINE bool float_eq(float a, float b) { return a == b; }
ROMANO_FORCE_INLINE bool float_gt(float a, float b) { return a > b; }
ROMANO_FORCE_INLINE bool float_lt(float a, float b) { return a < b; }
#endif /* defined(ROMANO_MSVC) */


ROMANO_CPP_END

#endif /* #define __LIBROMANO_MATH_ALG */