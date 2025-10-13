/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/logger.h"
#include "libromano/math/stats32.h"
#include "libromano/simd.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#if ROMANO_DEBUG
#define VALUES_SIZE 10000
#else
#define VALUES_SIZE 1000000
#endif /* ROMANO_DEBUG*/

void log_stats(float* stats, const char* name)
{
    logger_log(LogLevel_Info,
               "Stats: %s : %.03f, %.03f, %.03f, %.03f, %.03f",
               name,
               stats[0],
               stats[1],
               stats[2],
               stats[3],
               stats[4]);
}

int main(void)
{
    size_t i;
    float sum[5];
    float mean[5];
    float variance[5];
    float std[5];
    float min[5];
    float max[5];
    float range[5];

    logger_init();

    float* values = malloc(VALUES_SIZE * sizeof(float));

    for(i = 0; i < VALUES_SIZE; i++)
    {
        values[i] = (float)i + 1.0f;
    }

    for(i = 0; i < VectorizationMode_COUNT; i++)
    {
        simd_force_vectorization_mode((VectorizationMode)i);

        logger_log(LogLevel_Info, "Vectorization mode: %s", simd_get_vectorization_mode_as_string(i));

        SCOPED_PROFILE_MS_START(profiling);

        sum[i] = stats_sum(values, VALUES_SIZE);
        mean[i] = stats_mean(values, VALUES_SIZE);
        variance[i] = stats_variance(values, VALUES_SIZE);
        std[i] = stats_std(values, VALUES_SIZE);
        min[i] = stats_min(values, VALUES_SIZE);
        max[i] = stats_max(values, VALUES_SIZE);
        range[i] = stats_range(values, VALUES_SIZE);

        SCOPED_PROFILE_MS_END(profiling);
    }

    simd_force_vectorization_mode(VectorizationMode_Scalar);

    log_stats(sum, "sum");
    log_stats(mean, "mean");
    log_stats(variance, "variance");
    log_stats(std, "std");
    log_stats(min, "min");
    log_stats(max, "max");
    log_stats(range, "range");

    // ROMANO_ASSERT(stats_variance(sum, 5) < 1000.0f, "sum variance is too high");
    // ROMANO_ASSERT(stats_variance(mean, 5) < 0.001f, "mean variance is too high");
    // ROMANO_ASSERT(stats_variance(variance, 5) < 0.001f, "variance variance is too high");
    // ROMANO_ASSERT(stats_variance(std, 5) < 0.001f, "std variance is too high");
    // ROMANO_ASSERT(stats_variance(min, 5) < 0.001f, "min variance is too high");
    // ROMANO_ASSERT(stats_variance(max, 5) < 0.001f, "max variance is too high");
    // ROMANO_ASSERT(stats_variance(range, 5) < 0.001f, "range variance is too high");

    free(values);

    logger_release();

    return 0;
}