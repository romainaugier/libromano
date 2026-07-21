/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_SIMD)
#define __LIBROMANO_SIMD

#include "libromano/common.h"

#include <stdlib.h>

#if defined(ROMANO_X86_64)
#include <immintrin.h>
#elif defined(ROMANO_AARCH64)
#include <arm_neon.h>
#endif /* defined(ROMANO_X86_64) */

ROMANO_CPP_ENTER

#if defined(ROMANO_X86_64)

typedef enum
{
    VectorizationMode_Scalar = 0,
    VectorizationMode_SSE = 1,
    VectorizationMode_AVX = 2,
    VectorizationMode_AVX2 = 3,
    VectorizationMode_AVX512 = 4,
    VectorizationMode_COUNT = 5,
} VectorizationMode;

#define VECTORIZATION_MODE_STR(mode) mode == 2 ? "AVX" : mode == 1 ? "SSE" : "Scalar (None)"

void simd_check_vectorization(void);

ROMANO_API int simd_has_sse(void);

ROMANO_API int simd_has_avx(void);

ROMANO_API VectorizationMode simd_get_vectorization_mode(void);

ROMANO_API void simd_force_vectorization_mode(const VectorizationMode mode);

ROMANO_API const char* simd_get_vectorization_mode_as_string(VectorizationMode mode);

/* SIMD helper functions */

/* https://stackoverflow.com/questions/6996764/fastest-way-to-do-horizontal-sse-vector-sum-or-other-reduction */

/*
 * Returns the horizontal sum of the packed 4 floats
 */
ROMANO_FORCE_INLINE float _mm_hsum_ps(__m128 x)
{
    __m128 shuf = _mm_shuffle_ps(x, x, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(x, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
}

/*
 * Returns the horizontal sum of the packed 8 floats
 */
ROMANO_FORCE_INLINE float _mm256_hsum_ps(__m256 x)
{
    __m128 hi = _mm256_extractf128_ps(x, 1);
    __m128 lo = _mm256_castps256_ps128(x);
    __m128 sum = _mm_add_ps(hi, lo);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
}

/*
 * Returns the linear interpolation of a and b depending on parameter t
 */
ROMANO_FORCE_INLINE __m128 _mm_lerp_ps(__m128 a, __m128 b, __m128 t)
{
    __m128 one_minus_t = _mm_sub_ps(_mm_set1_ps(1.0f), t);
    a = _mm_mul_ps(one_minus_t, a);
    b = _mm_mul_ps(b, t);
    return _mm_add_ps(a, b);
}

/*
 * Returns the linear interpolation of a and b depending on parameter t
 */
ROMANO_FORCE_INLINE __m256 _mm256_lerp_ps(__m256 a, __m256 b, __m256 t)
{
    __m256 one_minus_t = _mm256_sub_ps(_mm256_set1_ps(1.0f), t);
    a = _mm256_mul_ps(one_minus_t, a);
    b = _mm256_mul_ps(b, t);
    return _mm256_add_ps(a, b);
}

/*
 * Returns the horizontal mean of the 4 packed floats
 */
ROMANO_FORCE_INLINE float _mm_hmean_ps(__m128 x)
{
    return _mm_hsum_ps(x) / 4.0;
}

/*
 * Returns the horizontal mean of the 8 packed floats
 */
ROMANO_FORCE_INLINE float _mm256_hmean_ps(__m256 x)
{
    return _mm256_hsum_ps(x) / 8.0;
}

/*
 * Returns the minimum value of the 4 packed floats
 */
ROMANO_FORCE_INLINE float _mm_hmin_ps(__m128 x)
{
    x = _mm_min_ps(x, _mm_shuffle_ps(x, x, _MM_SHUFFLE(2, 3, 0, 0)));
    x = _mm_min_ps(x, _mm_shuffle_ps(x, x, _MM_SHUFFLE(1, 1, 0, 0)));
    return _mm_cvtss_f32(x);
}

/*
 * Returns the minimum value of the 8 packed floats
 */
ROMANO_FORCE_INLINE float _mm256_hmin_ps(__m256 v)
{
    __m256 perm_halves = _mm256_permute2f128_ps(v, v, 1);
    __m256 m0 = _mm256_min_ps(perm_halves, v);

    __m256 perm0 = _mm256_permute_ps(m0, 0x4E);
    __m256 m1 = _mm256_min_ps(m0, perm0);

    __m256 perm1 = _mm256_permute_ps(m1, 0xB1);
    __m256 m2 = _mm256_min_ps(perm1, m1);

    return _mm256_cvtss_f32(m2);
}

/*
 * Returns the maximum value of the 4 packed floats
 */
ROMANO_FORCE_INLINE float _mm_hmax_ps(__m128 v)
{
    v = _mm_max_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 0)));
    v = _mm_max_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 0, 0)));
    return _mm_cvtss_f32(v);
}

/*
 * Returns the maximum value of the 8 packed floats
 */
ROMANO_FORCE_INLINE float _mm256_hmax_ps(__m256 v)
{
    __m256 perm_halves = _mm256_permute2f128_ps(v, v, 1);
    __m256 m0 = _mm256_max_ps(perm_halves, v);

    __m256 perm0 = _mm256_permute_ps(m0, 0x4E);
    __m256 m1 = _mm256_max_ps(m0, perm0);

    __m256 perm1 = _mm256_permute_ps(m1, 0xB1);
    __m256 m2 = _mm256_max_ps(perm1, m1);

    return _mm256_cvtss_f32(m2);
}

#elif defined(ROMANO_AARCH64)

/* https://arm-software.github.io/acle/neon_intrinsics/advsimd.html */

typedef enum
{
    VectorizationMode_Scalar = 0,
    VectorizationMode_NEON = 1,
    VectorizationMode_COUNT = 2,
} VectorizationMode;

#define VECTORIZATION_MODE_STR(mode) mode == 1 ? "NEON" : "Scalar (None)"

void simd_check_vectorization(void);

ROMANO_API int simd_has_neon(void);

ROMANO_API VectorizationMode simd_get_vectorization_mode(void);

ROMANO_API void simd_force_vectorization_mode(const VectorizationMode mode);

ROMANO_API const char* simd_get_vectorization_mode_as_string(VectorizationMode mode);

/* Helpers */

ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE float32x4_t vzeroq_f32(void) { return vdupq_n_f32(0.0f); }

ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE float32x4_t voneq_f32(void) { return vdupq_n_f32(1.0f); }

ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE float32x4_t vlerpq_f32(float32x4_t a, float32x4_t b, float32x4_t t)
{
    float32x4_t one_minus_t = vsubq_f32(voneq_f32(), t);
    a = vmulq_f32(one_minus_t, a);
    b = vmulq_f32(t, b);
    return vaddq_f32(a, b);
}

ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE float vhsumq_f32(float32x4_t x)
{
    return vaddvq_f32(x);
}

ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE float vhmean_f32(float32x4_t x)
{
    return vaddvq_f32(x) * 0.25f;
}

#endif /* defined(ROMANO_X86_64) */

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_SIMD) */
