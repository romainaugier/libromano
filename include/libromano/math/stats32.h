/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_MATH_STATS32)
#define __LIBROMANO_MATH_STATS32

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

ROMANO_API float stats_sum(const float* array, const size_t n);

ROMANO_API float stats_mean(const float* array, const size_t n);

ROMANO_API float stats_std(const float* array, const size_t n);

ROMANO_API float stats_variance(const float* array, const size_t n);

ROMANO_API float stats_min(const float* array, const size_t n);

ROMANO_API float stats_max(const float* array, const size_t n);

ROMANO_API float stats_range(const float* array, const size_t n);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_MATH_STATS32) */