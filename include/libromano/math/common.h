/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#if !defined(__LIBROMANO_MATH_ALG)
#define __LIBROMANO_MATH_ALG

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


#if defined(ROMANO_WIN)
ROMANO_FORCE_INLINE bool math_isinf(float x) { return _finitef(x) == 0; }
ROMANO_FORCE_INLINE bool math_isnan(float x) { return _isnanf(x) != 0; }
ROMANO_FORCE_INLINE bool math_isfinite(float x) { return _finitef(x) != 0; }
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
ROMANO_FORCE_INLINE bool math_isinf(float x) { return __builtin_isinf(x) == 0; }
ROMANO_FORCE_INLINE bool math_isnan(float x) { return __builtin_isnan(x) != 0; }
ROMANO_FORCE_INLINE bool math_isfinite(float x) { return __builtin_isinf(x) != 0; }
#endif /* defined(ROMANO_WIN) */

ROMANO_FORCE_INLINE int math_to_int(float a) { return (int)a; }
ROMANO_FORCE_INLINE float math_to_float(int a) { return (float)a; }
ROMANO_FORCE_INLINE float math_sqr(float x) { return x * x; }

ROMANO_FORCE_INLINE float math_rcp(float x)
{ 
    __m128 a = _mm_set_ss(x); 
    __m128 r = _mm_rcp_ss(a); 

#if defined(__AVX2__)
    return _mm_cvtss_f32(_mm_mul_ss(r,_mm_fnmadd_ss(r, a, _mm_set_ss(2.0f))));
#else
    return _mm_cvtss_f32(_mm_mul_ss(r,_mm_sub_ss(_mm_set_ss(2.0f), _mm_mul_ss(r, a))));
#endif
}

ROMANO_FORCE_INLINE float math_rcp_safe(float a) { return 1.0f / a; }
ROMANO_FORCE_INLINE float math_min(float a, float b) { return a < b ? a : b; }
ROMANO_FORCE_INLINE float math_max(float a, float b) { return a > b ? a : b; }
ROMANO_FORCE_INLINE float math_fit(float s, float a1, float a2, float b1, float b2) { return b1 + ((s - a1) * (b2 - b1)) / (a2 - a1); }
ROMANO_FORCE_INLINE float math_fit01(float x, float a, float b) { return x * (b - a) + a; }
ROMANO_FORCE_INLINE float math_lerp(float a, float b, float t) { return (1.0f - t) * a + t * b; }
ROMANO_FORCE_INLINE float math_clamp(float n, float lower, float upper) { return math_max(lower, math_min(n, upper)); }
ROMANO_FORCE_INLINE float math_clampz(float n, float upper) { return math_max(ZERO, math_min(n, upper)); }
ROMANO_FORCE_INLINE float math_deg2rad(float deg) { return deg * PI / 180.0f; }
ROMANO_FORCE_INLINE float math_rad2deg(float rad) { return rad * 180.0f / PI; }
ROMANO_FORCE_INLINE float math_abs(float x) { return fabsf(x); }
ROMANO_FORCE_INLINE float math_exp(float x) { return expf(x); }
ROMANO_FORCE_INLINE float math_sqrt(float x) { return sqrtf(x); }

ROMANO_FORCE_INLINE float math_rsqrt(float x) 
{
    __m128 a = _mm_set_ss(x);
    __m128 r = _mm_rsqrt_ss(a);
    r = _mm_add_ss(_mm_mul_ss(_mm_set_ss(1.5f), r), _mm_mul_ss(_mm_mul_ss(_mm_mul_ss(a, _mm_set_ss(-0.5f)), r), _mm_mul_ss(r, r)));
    return _mm_cvtss_f32(r);
}

ROMANO_FORCE_INLINE float math_fmod(float x, float y) { return fmodf(x, y); }
ROMANO_FORCE_INLINE float math_log(float x) { return logf(x); }
ROMANO_FORCE_INLINE float math_log10(float x) { return log10f(x); }
ROMANO_FORCE_INLINE float math_pow(float x, float y) { return powf(x, y); }
ROMANO_FORCE_INLINE float math_floor(float x) { return floorf(x); }
ROMANO_FORCE_INLINE float math_ceil(float x) { return ceilf(x); }
ROMANO_FORCE_INLINE float math_frac(float x) { return x - floorf(x); }
ROMANO_FORCE_INLINE float math_acos(float x) { return acosf(x); }
ROMANO_FORCE_INLINE float math_asin(float x) { return asinf(x); }
ROMANO_FORCE_INLINE float math_atan(float x) { return atanf(x); }
ROMANO_FORCE_INLINE float math_atan2(float y, float x) { return atan2f(y, x); }
ROMANO_FORCE_INLINE float math_cos(float x) { return cosf(x); }
ROMANO_FORCE_INLINE float math_sin(float x) { return sinf(x); }
ROMANO_FORCE_INLINE float math_tan(float x) { return tanf(x); }
ROMANO_FORCE_INLINE float math_cosh(float x) { return coshf(x); }
ROMANO_FORCE_INLINE float math_sinh(float x) { return sinhf(x); }
ROMANO_FORCE_INLINE float math_tanh(float x) { return tanhf(x); }

#if defined(__AVX2__)
ROMANO_FORCE_INLINE float math_madd(float a, float b, float c) { return _mm_cvtss_f32(_mm_fmadd_ss(_mm_set_ss(a),_mm_set_ss(b),_mm_set_ss(c))); }
ROMANO_FORCE_INLINE float math_msub(float a, float b, float c) { return _mm_cvtss_f32(_mm_fmsub_ss(_mm_set_ss(a),_mm_set_ss(b),_mm_set_ss(c))); }
ROMANO_FORCE_INLINE float math_nmadd(float a, float b, float c) { return _mm_cvtss_f32(_mm_fnmadd_ss(_mm_set_ss(a),_mm_set_ss(b),_mm_set_ss(c))); }
ROMANO_FORCE_INLINE float math_nmsub(float a, float b, float c) { return _mm_cvtss_f32(_mm_fnmsub_ss(_mm_set_ss(a),_mm_set_ss(b),_mm_set_ss(c))); }
#else
ROMANO_FORCE_INLINE float math_madd(float a, float b, float c) { return fmaf(a, b, c); }
ROMANO_FORCE_INLINE float math_msub(float a, float b, float c) { return a * b - c; } 
ROMANO_FORCE_INLINE float math_nmadd( float a, float b, float c) { return -a * b + c;}
ROMANO_FORCE_INLINE float math_nmsub( float a, float b, float c) { return -a * b - c; }
#endif


ROMANO_CPP_END

#endif /* #define __LIBROMANO_MATH_ALG */