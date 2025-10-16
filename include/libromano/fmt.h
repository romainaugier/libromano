/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_FMT)
#define __LIBROMANO_FMT

#include "libromano/common.h"

/*
 * Returns the size needed to format i64
 */
ROMANO_API int fmt_size_i64(int64_t i64);

/*
 * buffer should be at least 20 bytes, and there will be no null terminator added
 */
ROMANO_API int fmt_i64(char* buffer, int64_t i64);

/*
 * Returns the size needed to format u64
 */
ROMANO_API int fmt_size_u64(uint64_t u64);

/*
 * buffer should be at least 20 bytes, and there will be no null terminator added
 */
ROMANO_API int fmt_u64(char* buffer, uint64_t u64);

/*
 * Returns the size needed to format f64
 */
ROMANO_API int fmt_size_f64(double f64, int precision);

/*
 * buffer should be at least 312 bytes, and there will be no null terminator added
 */
ROMANO_API int fmt_f64(char* buffer, double f64, int precision);

#endif /* !defined(__LIBROMANO_FMT) */