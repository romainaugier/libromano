/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/logger.h"
#include "libromano/math/stats32.h"
#include "libromano/simd.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#define VALUES_SIZE 10000

int main(void)
{
    size_t i;
    float __sum;
    float mean;
    float variance;
    float std;
    float min;
    float max;
    float range;

    logger_init();

    float* values = malloc(VALUES_SIZE * sizeof(float));

    for(i = 0; i < VALUES_SIZE; i++)
    {
        values[i] = (float)i + 1.0f;
    }

    __sum = stats_sum(values, VALUES_SIZE);
    logger_log(LogLevel_Info, "The sum is: %f", __sum);

    mean = stats_mean(values, VALUES_SIZE);
    logger_log(LogLevel_Info, "The mean is: %f", mean);

    variance = stats_variance(values, VALUES_SIZE);
    logger_log(LogLevel_Info, "The variance is: %f", variance);

    std = stats_std(values, VALUES_SIZE);
    logger_log(LogLevel_Info, "The std is: %f", std);

    min = stats_min(values, VALUES_SIZE);
    logger_log(LogLevel_Info, "The min is: %f", min);

    max = stats_max(values, VALUES_SIZE);
    logger_log(LogLevel_Info, "The max is: %f", max);

    range = stats_range(values, VALUES_SIZE);
    logger_log(LogLevel_Info, "The range is: %f", range);

    logger_log(LogLevel_Info, "Starting performance profiling");

    /* SUM */

    simd_force_vectorization_mode(VectorizationMode_Scalar);
    SCOPED_PROFILE_START(scalar_sum_profiling);

    __sum = stats_sum(values, VALUES_SIZE);

    SCOPED_PROFILE_END(scalar_sum_profiling);

    logger_log(LogLevel_Info, "Scalar sum: %f", __sum);

    simd_force_vectorization_mode(VectorizationMode_SSE);
    SCOPED_PROFILE_START(sse_sum_profiling);

    __sum = stats_sum(values, VALUES_SIZE);

    SCOPED_PROFILE_END(sse_sum_profiling);

    logger_log(LogLevel_Info, "SSE sum: %f", __sum);

    simd_force_vectorization_mode(VectorizationMode_AVX);
    SCOPED_PROFILE_START(avx2_sum_profiling);

    __sum = stats_sum(values, VALUES_SIZE);

    SCOPED_PROFILE_END(avx2_sum_profiling);

    logger_log(LogLevel_Info, "AVX2 sum: %f", __sum);

    free(values);

    logger_release();

    return 0;
}