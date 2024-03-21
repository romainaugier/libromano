/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_MATH_STATS32)
#define __LIBROMANO_MATH_STATS32

#include "libromano/libromano.h"
#include "libromano/math/common32.h"

static ROMANO_FORCE_INLINE float stats_mean(const float* array, const size_t n)
{
    size_t i;
    float sum = 0.0f;

    for(i = 0; i < n; i++)
    {
        sum = math_lerp(sum, array[i], 1.0f / (float)(i + 1));
    }

    return sum;
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