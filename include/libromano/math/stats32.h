/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_MATH_STATS32)
#define __LIBROMANO_MATH_STATS32

#include "libromano/common.h"

ROMANO_CPP_ENTER

/*
 * Returns the sum of the given data (Uses Kahan algorithm for stability)
 */
ROMANO_API float stats_sum(const float* ROMANO_RESTRICT array, size_t n);

/*
 * Returns the mean of the given data
 */
ROMANO_API float stats_mean(const float* ROMANO_RESTRICT array, size_t n);

/*
 * Returns the standard-deviation of the given data
 */
ROMANO_API float stats_std(const float* ROMANO_RESTRICT array, size_t n);

/*
 * Returns the variance of the given data (Uses Youngs and Cramer algorithm for stability)
 */
ROMANO_API float stats_variance(const float* ROMANO_RESTRICT array, size_t n);

/*
 * Returns the minimum value of the given data
 */
ROMANO_API float stats_min(const float* ROMANO_RESTRICT array, size_t n);

/*
 * Returns the maximum value of the given data
 */
ROMANO_API float stats_max(const float* ROMANO_RESTRICT array, size_t n);

/*
 * Returns the range value of the given data abs(max - min)
 */
ROMANO_API float stats_range(const float* ROMANO_RESTRICT array, size_t n);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_MATH_STATS32) */