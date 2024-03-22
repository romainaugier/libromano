/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_MATH_STATS32)
#define __LIBROMANO_MATH_STATS32

#include "libromano/libromano.h"
#include "libromano/math/common32.h"
#include "libromano/simd.h"

/*
    Simple statistics library using float32 
    We use dynamic vectorization dispatching where possible to accelerate calculations
*/

/* Sum functions */
static float __stats_sum_avx2(const float* array, const size_t n)
{
    size_t i;
    float sum;
    __m256 sums;

    sums = _mm256_setzero_ps();

    for(i = 0; i < (n - 8); i += 8)
    {
        sums = _mm256_add_ps(sums, _mm256_loadu_ps(&array[i]));
    }

    sum = _mm256_hsum_ps(sums);

    for(; i < n; i++)
    {
        sum += array[i];
    }

    return sum;
}

static float __stats_sum_sse(const float* array, const size_t n)
{
    size_t i;
    float sum;
    __m128 sums;

    sums = _mm_setzero_ps();

    for(i = 0; i < (n - 4); i += 4)
    {
        sums = _mm_add_ps(sums, _mm_loadu_ps(&array[i]));
    }

    sum = _mm_hsum_ps(sums);

    for(; i < n; i++)
    {
        sum += array[i];
    }

    return sum;
}

static float __stats_sum_scalar(const float* array, const size_t n)
{
    size_t i;
    float sum;

    sum = 0.0f;

    for(i = 0; i < n; i++)
    {
        sum += array[i];
    }

    return sum;
}

typedef float (*sum_func)(const float*, const size_t);

sum_func __stats_sum_funcs[3] = {
    __stats_sum_scalar,
    __stats_sum_sse,
    __stats_sum_avx2
};

static ROMANO_FORCE_INLINE float stats_sum(const float* array, const size_t n)
{
    return __stats_sum_funcs[simd_get_vectorization_mode()](array, n);
}

/* Mean functions */
static float __stats_mean_avx2(const float* array, const size_t n)
{
    size_t i;
    float mean;
    __m256 means;

    for(i = 0; i < (n - 8); i += 8)
    {

    }



    for(; i < n; i++)
    {
        mean = math_lerp(mean, array[i], 1.0f / (float)(i + 1));
    }

    return mean;
}

static ROMANO_FORCE_INLINE float stats_mean(const float* array, const size_t n)
{
    size_t i;
    float mean = 0.0f;

    for(i = 0; i < n; i++)
    {
        mean = math_lerp(mean, array[i], 1.0f / (float)(i + 1));
    }

    return mean;
}

static ROMANO_FORCE_INLINE float stats_std(const float* array, const size_t n)
{
    size_t i;
    float mean;
    float variance;

    mean = stats_mean(array, n);

    variance = 0.0f;

    for(i = 0; i < n; i++)
    {
        variance = math_lerp(variance, math_sqr((array[i] - mean)), 1.0f / (float)(i + 1));
    }

    return math_sqrt(variance);
}

static ROMANO_FORCE_INLINE float stats_variance(const float* array, const size_t n)
{
    size_t i;
    float mean;
    float variance;

    mean = stats_mean(array, n);

    variance = 0.0f;

    for(i = 0; i < n; i++)
    {
        variance = math_lerp(variance, math_sqr((array[i] - mean)), 1.0f / (float)(i + 1));
    }

    return variance;
}

static ROMANO_FORCE_INLINE float stats_min(const float* array, const size_t n)
{
    size_t i;
    float min;

    min = array[0];

    for(i = 1; i < n; i++)
    {
        min = math_min(min, array[i]);
    }

    return min;
}

static ROMANO_FORCE_INLINE float stats_max(const float* array, const size_t n)
{
    size_t i;
    float max;

    max = array[0];

    for(i = 1; i < n; i++)
    {
        max = math_max(max, array[i]);
    }

    return max;
}

static ROMANO_FORCE_INLINE float stats_range(const float* array, const size_t n)
{
    size_t i;
    float min;
    float max;

    min = array[0];
    max = array[1];

    for(i = 1; i < n; i++)
    {
        min = math_min(min, array[i]); 
        max = math_max(max, array[i]); 
    }

    return max - min;
}

#endif /* !defined(__LIBROMANO_MATH_STATS32) */