/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/logger.h"
#include "libromano/math/stats32.h"

#define VALUES_SIZE 16384

int main(void)
{
    size_t i;
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
        values[i] = (float)i;
    }

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

    free(values);

    logger_release();

    return 0;
}