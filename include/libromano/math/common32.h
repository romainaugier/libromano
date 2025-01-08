/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_MATH_COMMON)
#define __LIBROMANO_MATH_COMMON

#include "libromano/libromano.h"
#include "libromano/bool.h"

#include <immintrin.h>

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
#endif /* defined(ROMANO_GCC) || defined(ROMANO_CLANG) */

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
static ROMANO_FORCE_INLINE bool mathf_float_eq(const float a, const float b) { return _fdpcomp(a, b) == _FP_EQ; }
static ROMANO_FORCE_INLINE bool mathf_float_gt(const float a, const float b) { return _fdpcomp(a, b) == _FP_GT; }
static ROMANO_FORCE_INLINE bool mathf_float_lt(const float a, const float b) { return _fdpcomp(a, b) == _FP_LT; }
#elif defined(ROMANO_GCC)
static ROMANO_FORCE_INLINE bool mathf_float_eq(const float a, const float b) { return a == b; }
static ROMANO_FORCE_INLINE bool mathf_float_gt(const float a, const float b) { return __builtin_isgreater(a, b); }
static ROMANO_FORCE_INLINE bool mathf_float_lt(const float a, const float b) { return __builtin_isless(a, b); }
#else
static ROMANO_FORCE_INLINE bool mathf_float_eq(const float a, const float b) { return a == b; }
static ROMANO_FORCE_INLINE bool mathf_float_gt(const float a, const float b) { return a > b; }
static ROMANO_FORCE_INLINE bool mathf_float_lt(const float a, const float b) { return a < b; }
#endif /* defined(ROMANO_MSVC) */


#if defined(ROMANO_MSVC)
static ROMANO_FORCE_INLINE bool mathf_isinf(const float x) { return _finitef(x) == 0; }
static ROMANO_FORCE_INLINE bool mathf_isnan(const float x) { return _isnanf(x) != 0; }
static ROMANO_FORCE_INLINE bool mathf_isfinite(const float x) { return _finitef(x) != 0; }
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
static ROMANO_FORCE_INLINE bool mathf_isinf(const float x) { return __builtin_isinf(x) == 0; }
static ROMANO_FORCE_INLINE bool mathf_isnan(const float x) { return __builtin_isnan(x) != 0; }
static ROMANO_FORCE_INLINE bool mathf_isfinite(const float x) { return __builtin_isinf(x) != 0; }
#endif /* defined(ROMANO_MSVC) */

static ROMANO_FORCE_INLINE int mathf_to_int(const float a) { return (int)a; }
static ROMANO_FORCE_INLINE float mathf_to_float(const int a) { return (float)a; }
static ROMANO_FORCE_INLINE float mathf_sqr(const float x) { return x * x; }

static ROMANO_FORCE_INLINE float mathf_rcp(const float x)
{ 
    __m128 a = _mm_set_ss(x); 
    __m128 r = _mm_rcp_ss(a); 

#if defined(__AVX2__)
    return _mm_cvtss_f32(_mm_mul_ss(r,_mm_fnmadd_ss(r, a, _mm_set_ss(2.0f))));
#else
    return _mm_cvtss_f32(_mm_mul_ss(r,_mm_sub_ss(_mm_set_ss(2.0f), _mm_mul_ss(r, a))));
#endif
}

static ROMANO_FORCE_INLINE float mathf_rcp_safe(const float a) { return 1.0f / a; }

#if defined(ROMANO_MSVC)
static ROMANO_FORCE_INLINE float mathf_min(const float a, const float b) { return fminf(a, b); }
static ROMANO_FORCE_INLINE float mathf_max(const float a, const float b) { return fmaxf(a, b); }
#elif defined(ROMANO_GCC)
static ROMANO_FORCE_INLINE float mathf_min(const float a, const float b) { return __builtin_fminf(a, b); }
static ROMANO_FORCE_INLINE float mathf_max(const float a, const float b) { return __builtin_fmaxf(a, b); }
#endif /* defined(ROMANO_MSVC) */
static ROMANO_FORCE_INLINE float mathf_fit(const float s, const float a1, const float a2, const float b1, const float b2) { return b1 + ((s - a1) * (b2 - b1)) / (a2 - a1); }
static ROMANO_FORCE_INLINE float mathf_fit01(const float x, const float a, const float b) { return x * (b - a) + a; }
static ROMANO_FORCE_INLINE float mathf_lerp(const float a, const float b, const float t) { return (1.0f - t) * a + t * b; }
static ROMANO_FORCE_INLINE float mathf_clamp(const float n, const float lower, const float upper) { return mathf_max(lower, mathf_min(n, upper)); }
static ROMANO_FORCE_INLINE float mathf_clampz(const float n, const float upper) { return mathf_max(ZERO, mathf_min(n, upper)); }
static ROMANO_FORCE_INLINE float mathf_deg2rad(const float deg) { return deg * PI / 180.0f; }
static ROMANO_FORCE_INLINE float mathf_rad2deg(const float rad) { return rad * 180.0f / PI; }
#if defined(ROMANO_MSVC)
static ROMANO_FORCE_INLINE float mathf_abs(const float x) { return fabsf(x); }
static ROMANO_FORCE_INLINE float mathf_exp(const float x) { return expf(x); }
static ROMANO_FORCE_INLINE float mathf_sqrt(const float x) { return sqrtf(x); }
#elif defined(ROMANO_GCC)
static ROMANO_FORCE_INLINE float mathf_abs(const float x) { return __builtin_fabsf(x); }
static ROMANO_FORCE_INLINE float mathf_exp(const float x) { return __builtin_expf(x); }
static ROMANO_FORCE_INLINE float mathf_sqrt(const float x) { return __builtin_sqrtf(x); }
#endif /* defined(ROMANO_MSVC) */

static ROMANO_FORCE_INLINE float mathf_rsqrt(const float x) 
{
    __m128 a = _mm_set_ss(x);
    __m128 r = _mm_rsqrt_ss(a);
    r = _mm_add_ss(_mm_mul_ss(_mm_set_ss(1.5f), r), _mm_mul_ss(_mm_mul_ss(_mm_mul_ss(a, _mm_set_ss(-0.5f)), r), _mm_mul_ss(r, r)));
    return _mm_cvtss_f32(r);
}

#if defined(ROMANO_MSVC)
static ROMANO_FORCE_INLINE float mathf_fmod(const float x, const float y) { return fmodf(x, y); }
static ROMANO_FORCE_INLINE float mathf_log(const float x) { return logf(x); }
static ROMANO_FORCE_INLINE float mathf_log2(const float x) { return log2f(x); }
static ROMANO_FORCE_INLINE float mathf_log10(const float x) { return log10f(x); }
static ROMANO_FORCE_INLINE float mathf_logN(const float x, const float base) { return log2f(x) / log2f(base); }
static ROMANO_FORCE_INLINE float mathf_pow(const float x, const float y) { return powf(x, y); }
static ROMANO_FORCE_INLINE float mathf_floor(const float x) { return floorf(x); }
static ROMANO_FORCE_INLINE float mathf_ceil(const float x) { return ceilf(x); }
static ROMANO_FORCE_INLINE float mathf_frac(const float x) { return x - floorf(x); }
static ROMANO_FORCE_INLINE float mathf_acos(const float x) { return acosf(x); }
static ROMANO_FORCE_INLINE float mathf_asin(const float x) { return asinf(x); }
static ROMANO_FORCE_INLINE float mathf_atan(const float x) { return atanf(x); }
static ROMANO_FORCE_INLINE float mathf_atan2(const float y, float x) { return atan2f(y, x); }
static ROMANO_FORCE_INLINE float mathf_cos(const float x) { return cosf(x); }
static ROMANO_FORCE_INLINE float mathf_sin(const float x) { return sinf(x); }
static ROMANO_FORCE_INLINE float mathf_tan(const float x) { return tanf(x); }
static ROMANO_FORCE_INLINE float mathf_cosh(const float x) { return coshf(x); }
static ROMANO_FORCE_INLINE float mathf_sinh(const float x) { return sinhf(x); }
static ROMANO_FORCE_INLINE float mathf_tanh(const float x) { return tanhf(x); }
#elif defined(ROMANO_GCC)
static ROMANO_FORCE_INLINE float mathf_fmod(const float x, const float y) { return __builtin_fmodf(x, y); }
static ROMANO_FORCE_INLINE float mathf_log(const float x) { return __builtin_logf(x); }
static ROMANO_FORCE_INLINE float mathf_log2(const float x) { return __builtin_log2f(x); }
static ROMANO_FORCE_INLINE float mathf_log10(const float x) { return __builtin_log10f(x); }
static ROMANO_FORCE_INLINE float mathf_logN(const float x, const float base) { return __builtin_log2f(x) / __builtin_log2f(base); }
static ROMANO_FORCE_INLINE float mathf_pow(const float x, const float y) { return __builtin_powf(x, y); }
static ROMANO_FORCE_INLINE float mathf_floor(const float x) { return __builtin_floorf(x); }
static ROMANO_FORCE_INLINE float mathf_ceil(const float x) { return __builtin_ceilf(x); }
static ROMANO_FORCE_INLINE float mathf_frac(const float x) { return x - __builtin_floorf(x); }
static ROMANO_FORCE_INLINE float mathf_acos(const float x) { return __builtin_acosf(x); }
static ROMANO_FORCE_INLINE float mathf_asin(const float x) { return __builtin_asinf(x); }
static ROMANO_FORCE_INLINE float mathf_atan(const float x) { return __builtin_atanf(x); }
static ROMANO_FORCE_INLINE float mathf_atan2(const float y, const float x) { return __builtin_atan2f(y, x); }
static ROMANO_FORCE_INLINE float mathf_cos(const float x) { return __builtin_cosf(x); }
static ROMANO_FORCE_INLINE float mathf_sin(const float x) { return __builtin_sinf(x); }
static ROMANO_FORCE_INLINE float mathf_tan(const float x) { return __builtin_tanf(x); }
static ROMANO_FORCE_INLINE float mathf_cosh(const float x) { return __builtin_coshf(x); }
static ROMANO_FORCE_INLINE float mathf_sinh(const float x) { return __builtin_sinhf(x); }
static ROMANO_FORCE_INLINE float mathf_tanh(const float x) { return __builtin_tanhf(x); }
#endif /* defined(ROMANO_MSVC) */

#if defined(__AVX2__)
static ROMANO_FORCE_INLINE float mathf_madd(const float a, const float b, const float c) { return _mm_cvtss_f32(_mm_fmadd_ss(_mm_set_ss(a),_mm_set_ss(b),_mm_set_ss(c))); }
static ROMANO_FORCE_INLINE float mathf_msub(const float a, const float b, const float c) { return _mm_cvtss_f32(_mm_fmsub_ss(_mm_set_ss(a),_mm_set_ss(b),_mm_set_ss(c))); }
static ROMANO_FORCE_INLINE float mathf_nmadd(const float a, const float b, const float c) { return _mm_cvtss_f32(_mm_fnmadd_ss(_mm_set_ss(a),_mm_set_ss(b),_mm_set_ss(c))); }
static ROMANO_FORCE_INLINE float mathf_nmsub(const float a, const float b, const float c) { return _mm_cvtss_f32(_mm_fnmsub_ss(_mm_set_ss(a),_mm_set_ss(b),_mm_set_ss(c))); }
#else
#if defined(ROMANO_MSVC)
static ROMANO_FORCE_INLINE float mathf_madd(const float a, const float b, const float c) { return fmaf(a, b, c); }
#else
static ROMANO_FORCE_INLINE float mathf_madd(const float a, const float b, const float c) { return __builtin_fmaf(a, b, c); }
#endif /* defined(ROMANO_MSVC) */
static ROMANO_FORCE_INLINE float mathf_msub(const float a, const float b, const float c) { return a * b - c; } 
static ROMANO_FORCE_INLINE float mathf_nmadd(const float a, const float b, const float c) { return -a * b + c;}
static ROMANO_FORCE_INLINE float mathf_nmsub(const float a, const float b, const float c) { return -a * b - c; }
#endif

ROMANO_CPP_END

#endif /* #define __LIBROMANO_MATH_COMMON */