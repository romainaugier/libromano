/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/* Private helpers shared by the linalg translation units, not installed */

#pragma once

#if !defined(__LIBROMANO_MATH_LINALG_INTERNAL)
#define __LIBROMANO_MATH_LINALG_INTERNAL

#include "libromano/math/linalg_ctx.h"

#define LINALG_MIN(a, b) ((a) < (b) ? (a) : (b))
#define LINALG_MAX(a, b) ((a) > (b) ? (a) : (b))

/* Processes the items [begin, end) */
typedef void (*LinAlgRangeFunc)(void* data, size_t begin, size_t end);

/*
 * Splits [0, count) in chunks of at least grain items and runs func on them, in parallel when
 * the context has a pool. The calling thread runs the first chunk and helps with the others.
 * data is shared between all the chunks, func must only write to disjoint locations.
 */
void linalg_parallel_for(LinAlgCtx* ctx,
                         size_t count,
                         size_t grain,
                         LinAlgRangeFunc func,
                         void* data);

#if defined(ROMANO_X86_64)
/*
 * Column-major C = A * B with A: M x K (lda = M), B: K x N (ldb = K), C: M x N (ldc = M).
 * C does not need to be initialized. Requires AVX2 + FMA.
 * AVX2 kernel ported from stdromano, itself based on https://github.com/salykova/sgemm.c
 */
void linalg32_sgemm_colmajor_avx2(LinAlgCtx* ctx,
                                  const float* ROMANO_RESTRICT A,
                                  const float* ROMANO_RESTRICT B,
                                  float* ROMANO_RESTRICT C,
                                  size_t M,
                                  size_t K,
                                  size_t N);
#endif /* defined(ROMANO_X86_64) */

#endif /* !defined(__LIBROMANO_MATH_LINALG_INTERNAL) */
