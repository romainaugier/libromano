/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/* Private helpers shared by the linalg translation units, not installed */

#pragma once

#if !defined(__LIBROMANO_MATH_LINALG_INTERNAL)
#define __LIBROMANO_MATH_LINALG_INTERNAL

#include "libromano/math/linalg_ctx.h"

/*
 * Internal symbols are not exported from the shared library (on Windows only ROMANO_API symbols
 * are): they stay private to libromano, and the tests can compile the internal sources without
 * clashing with the library's copy
 */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
#define LINALG_INTERNAL __attribute__((visibility("hidden")))
#else
#define LINALG_INTERNAL
#endif /* (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32) */

#define LINALG_MIN(a, b) ((a) < (b) ? (a) : (b))
#define LINALG_MAX(a, b) ((a) > (b) ? (a) : (b))

/* Processes the items [begin, end) */
typedef void (*LinAlgRangeFunc)(void* data, size_t begin, size_t end);

/*
 * Splits [0, count) in chunks of at least grain items and runs func on them, in parallel when
 * the context has a pool. The calling thread runs the first chunk and helps with the others.
 * data is shared between all the chunks, func must only write to disjoint locations.
 */
LINALG_INTERNAL void linalg_parallel_for(LinAlgCtx* ctx,
                         size_t count,
                         size_t grain,
                         LinAlgRangeFunc func,
                         void* data);

/* SGEMM */

/*
 * Micro-kernel: C[0:mr, 0:nr] = (C +) Ap * Bp, over kc.
 * Ap: packed panel of MR rows (column after column, zero-padded rows), 64 bytes aligned.
 * Bp: packed panel of NR columns (row after row, zero-padded columns).
 * C: column-major with leading dimension ldc. mr <= MR and nr <= NR (edges of the matrix).
 * accumulate == 0 overwrites C (it does not need to be initialized).
 */
typedef void (*SgemmMicroKernel)(const float* ROMANO_RESTRICT Ap,
                                 const float* ROMANO_RESTRICT Bp,
                                 float* ROMANO_RESTRICT C,
                                 size_t mr,
                                 size_t nr,
                                 size_t kc,
                                 size_t ldc,
                                 int accumulate);

typedef struct SgemmKernel {
    const char* name;
    SgemmMicroKernel func;
    size_t mr; /* register tile rows */
    size_t nr; /* register tile columns */
} SgemmKernel;

/* Block sizes of the packed A (mc x kc) and B (kc x nc) blocks */
typedef struct SgemmBlocking {
    size_t kc;
    size_t mc;
    size_t nc;
} SgemmBlocking;

/*
 * Derives the block sizes of a kernel from the detected cache sizes (see cpu.h), for a product
 * of depth K (0 if unknown: sized for a full kc deep block)
 */
LINALG_INTERNAL void linalg32_sgemm_compute_blocking(const SgemmKernel* kernel,
                                                     size_t K,
                                                     SgemmBlocking* blocking);

/*
 * Column-major C = A * B with A: M x K (lda = M), B: K x N (ldb = K), C: M x N (ldc = M).
 * C does not need to be initialized. blocking can be NULL to derive it from the caches.
 * Goto/BLIS algorithm based on https://github.com/salykova/sgemm.c (ported from stdromano).
 */
LINALG_INTERNAL void linalg32_sgemm_colmajor(LinAlgCtx* ctx,
                             const SgemmKernel* kernel,
                             const SgemmBlocking* blocking,
                             const float* ROMANO_RESTRICT A,
                             const float* ROMANO_RESTRICT B,
                             float* ROMANO_RESTRICT C,
                             size_t M,
                             size_t K,
                             size_t N);

#if defined(ROMANO_X86_64)
LINALG_INTERNAL extern const SgemmKernel linalg32_sgemm_kernel_sse;    /* 8x6, SSE (no FMA) */
LINALG_INTERNAL extern const SgemmKernel linalg32_sgemm_kernel_avx2;   /* 16x6, AVX2 + FMA */
LINALG_INTERNAL extern const SgemmKernel linalg32_sgemm_kernel_avx512; /* 32x12, AVX-512F */
#endif /* defined(ROMANO_X86_64) */

#endif /* !defined(__LIBROMANO_MATH_LINALG_INTERNAL) */
