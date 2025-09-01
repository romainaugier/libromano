/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_SIMD)
#define __LIBROMANO_SIMD

#include "libromano/common.h"

#include <stdlib.h>
#include <immintrin.h>

ROMANO_CPP_ENTER

typedef enum
{
    VectorizationMode_Scalar = 0,
    VectorizationMode_SSE = 1,
    VectorizationMode_AVX = 2
} VectorizationMode;

#define VECTORIZATION_MODE_STR(mode) mode == 2 ? "AVX" : mode == 1 ? "SSE" : "Scalar (None)" 

void simd_check_vectorization(void);

ROMANO_API int simd_has_sse(void);

ROMANO_API int simd_has_avx(void);

ROMANO_API VectorizationMode simd_get_vectorization_mode(void);

ROMANO_API void simd_force_vectorization_mode(const VectorizationMode mode);

/* SIMD helper functions */

/* Horizontal sums (sum the entire vector to a single element) */
/* https://stackoverflow.com/questions/6996764/fastest-way-to-do-horizontal-sse-vector-sum-or-other-reduction */

static ROMANO_FORCE_INLINE float _mm_hsum_ps(__m128 x)
{
    __m128 shuf = _mm_shuffle_ps(x, x, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(x, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
}

static ROMANO_FORCE_INLINE float _mm256_hsum_ps(__m256 x)
{
    const __m128 hi = _mm256_extractf128_ps(x, 1);
    const __m128 lo = _mm256_castps256_ps128(x);
    __m128 sum = _mm_add_ps(hi, lo);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
}

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_SIMD) */
