/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/*
 * AVX2 + FMA single precision gemm, ported from stdromano (linalg_dense_matrix_kernels.cpp),
 * itself based on https://github.com/salykova/sgemm.c
 *
 * Everything here is column-major: A is M x K (lda = M), B is K x N (ldb = K), C is M x N
 * (ldc = M). The row-major MatrixF API maps onto it with C^T = B^T * A^T, see math_linalg32.c.
 *
 * Differences with the stdromano version:
 *  - one generic 16x6 micro-kernel instead of the per-nr unrolled variants: the hot loop always
 *    computes the full 16x6 tile (the packed B panel is zero-padded, so this costs nothing
 *    compared to the specialized loops), only the loads/stores are masked
 *  - edge masks are built with a compare instead of a lookup table
 *  - the zero-init / load-accum kernels are merged (accumulate flag)
 *  - work is split in 2D tiles (16 rows x 6 columns) and chunked through linalg_parallel_for,
 *    instead of one job per 6-column panel, so small N still gets parallelism and big N does
 *    not flood the pool with tiny jobs
 */

#include "math_linalg_internal.h"

#include "libromano/memory.h"

#if defined(ROMANO_X86_64)

#include <immintrin.h>
#include <string.h>

#define SGEMM_MR 16
#define SGEMM_NR 6
#define SGEMM_KC 256

#define SGEMM_MIN(a, b) ((a) < (b) ? (a) : (b))
#define SGEMM_MAX(a, b) ((a) > (b) ? (a) : (b))
#define SGEMM_ROUND_UP(x, m) ((((x) + (m) - 1) / (m)) * (m))

/* Micro-kernel */

/*
 * C[0:mr, 0:nr] (+)= Ap * Bp over kc
 * Ap: packed 16 x kc panel (column by column, zero-padded rows)
 * Bp: packed kc x 6 panel (row by row, zero-padded columns)
 */
static void sgemm_kernel_16x6(const float* ROMANO_RESTRICT Ap,
                              const float* ROMANO_RESTRICT Bp,
                              float* ROMANO_RESTRICT C,
                              const size_t mr,
                              const size_t nr,
                              const size_t kc,
                              const size_t ldc,
                              const int accumulate)
{
    __m256 c00 = _mm256_setzero_ps(), c01 = _mm256_setzero_ps();
    __m256 c10 = _mm256_setzero_ps(), c11 = _mm256_setzero_ps();
    __m256 c20 = _mm256_setzero_ps(), c21 = _mm256_setzero_ps();
    __m256 c30 = _mm256_setzero_ps(), c31 = _mm256_setzero_ps();
    __m256 c40 = _mm256_setzero_ps(), c41 = _mm256_setzero_ps();
    __m256 c50 = _mm256_setzero_ps(), c51 = _mm256_setzero_ps();
    __m256 a0;
    __m256 a1;
    __m256 b;
    size_t p;
    size_t j;

    /* 12 accumulators + 2 A registers + 1 broadcast = 15 of the 16 ymm registers */
    for(p = 0; p < kc; p++)
    {
        a0 = _mm256_load_ps(Ap);
        a1 = _mm256_load_ps(Ap + 8);

        b = _mm256_broadcast_ss(Bp + 0);
        c00 = _mm256_fmadd_ps(a0, b, c00);
        c01 = _mm256_fmadd_ps(a1, b, c01);

        b = _mm256_broadcast_ss(Bp + 1);
        c10 = _mm256_fmadd_ps(a0, b, c10);
        c11 = _mm256_fmadd_ps(a1, b, c11);

        b = _mm256_broadcast_ss(Bp + 2);
        c20 = _mm256_fmadd_ps(a0, b, c20);
        c21 = _mm256_fmadd_ps(a1, b, c21);

        b = _mm256_broadcast_ss(Bp + 3);
        c30 = _mm256_fmadd_ps(a0, b, c30);
        c31 = _mm256_fmadd_ps(a1, b, c31);

        b = _mm256_broadcast_ss(Bp + 4);
        c40 = _mm256_fmadd_ps(a0, b, c40);
        c41 = _mm256_fmadd_ps(a1, b, c41);

        b = _mm256_broadcast_ss(Bp + 5);
        c50 = _mm256_fmadd_ps(a0, b, c50);
        c51 = _mm256_fmadd_ps(a1, b, c51);

        Ap += SGEMM_MR;
        Bp += SGEMM_NR;
    }

    {
        /* Spilled once per tile, keeps the hot loop above in registers */
        const __m256 acc[12] = { c00, c01, c10, c11, c20, c21, c30, c31, c40, c41, c50, c51 };

        if(mr == SGEMM_MR)
        {
            for(j = 0; j < nr; j++)
            {
                float* c = C + j * ldc;
                __m256 lo = acc[2 * j];
                __m256 hi = acc[2 * j + 1];

                if(accumulate)
                {
                    lo = _mm256_add_ps(lo, _mm256_loadu_ps(c));
                    hi = _mm256_add_ps(hi, _mm256_loadu_ps(c + 8));
                }

                _mm256_storeu_ps(c, lo);
                _mm256_storeu_ps(c + 8, hi);
            }
        }
        else
        {
            /* Lane l is active when l < mr (low half) or l + 8 < mr (high half) */
            const __m256i iota = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
            const __m256i mask_lo = _mm256_cmpgt_epi32(_mm256_set1_epi32((int)mr), iota);
            const __m256i mask_hi = _mm256_cmpgt_epi32(_mm256_set1_epi32((int)mr - 8), iota);

            for(j = 0; j < nr; j++)
            {
                float* c = C + j * ldc;
                __m256 lo = acc[2 * j];
                __m256 hi = acc[2 * j + 1];

                if(accumulate)
                {
                    lo = _mm256_add_ps(lo, _mm256_maskload_ps(c, mask_lo));
                    hi = _mm256_add_ps(hi, _mm256_maskload_ps(c + 8, mask_hi));
                }

                _mm256_maskstore_ps(c, mask_lo, lo);
                _mm256_maskstore_ps(c + 8, mask_hi, hi);
            }
        }
    }
}

/* Packing */

static void sgemm_pack_panel_A(const float* ROMANO_RESTRICT A,
                               float* ROMANO_RESTRICT Ap,
                               const size_t mr,
                               const size_t kc,
                               const size_t lda)
{
    size_t p;
    size_t i;

    if(mr == SGEMM_MR)
    {
        for(p = 0; p < kc; p++)
        {
            memcpy(Ap, A + p * lda, SGEMM_MR * sizeof(float));
            Ap += SGEMM_MR;
        }

        return;
    }

    for(p = 0; p < kc; p++)
    {
        for(i = 0; i < mr; i++)
            Ap[i] = A[p * lda + i];

        for(i = mr; i < SGEMM_MR; i++)
            Ap[i] = 0.0f;

        Ap += SGEMM_MR;
    }
}

static void sgemm_pack_panel_B(const float* ROMANO_RESTRICT B,
                               float* ROMANO_RESTRICT Bp,
                               const size_t nr,
                               const size_t kc,
                               const size_t ldb)
{
    size_t p;
    size_t j;

    for(p = 0; p < kc; p++)
    {
        for(j = 0; j < nr; j++)
            Bp[j] = B[j * ldb + p];

        for(j = nr; j < SGEMM_NR; j++)
            Bp[j] = 0.0f;

        Bp += SGEMM_NR;
    }
}

typedef struct SgemmPackA {
    const float* A;   /* top-left of the mc x kc block */
    float* Ap;
    size_t mc;
    size_t kc;
    size_t lda;
} SgemmPackA;

static void sgemm_pack_A_range(void* data, size_t begin, size_t end)
{
    const SgemmPackA* job = (const SgemmPackA*)data;
    size_t panel;

    for(panel = begin; panel < end; panel++)
    {
        const size_t i = panel * SGEMM_MR;
        const size_t mr = SGEMM_MIN(SGEMM_MR, job->mc - i);

        sgemm_pack_panel_A(job->A + i, job->Ap + i * job->kc, mr, job->kc, job->lda);
    }
}

typedef struct SgemmPackB {
    const float* B;   /* top-left of the kc x nc block */
    float* Bp;
    size_t nc;
    size_t kc;
    size_t ldb;
} SgemmPackB;

static void sgemm_pack_B_range(void* data, size_t begin, size_t end)
{
    const SgemmPackB* job = (const SgemmPackB*)data;
    size_t panel;

    for(panel = begin; panel < end; panel++)
    {
        const size_t j = panel * SGEMM_NR;
        const size_t nr = SGEMM_MIN(SGEMM_NR, job->nc - j);

        sgemm_pack_panel_B(job->B + j * job->ldb, job->Bp + j * job->kc, nr, job->kc, job->ldb);
    }
}

/* Macro-kernel: all the 16x6 tiles of an mc x nc block of C */

typedef struct SgemmCompute {
    const float* Ap;
    const float* Bp;
    float* C;         /* top-left of the mc x nc block */
    size_t mc;
    size_t nc;
    size_t kc;
    size_t ldc;
    size_t panels_m;  /* number of 16-row panels in mc */
    int accumulate;
} SgemmCompute;

/*
 * Tiles are numbered with the row panel varying fastest, so a chunk of consecutive tiles keeps
 * reusing the same packed B panel (L1) while streaming the packed A block (L2)
 */
static void sgemm_compute_range(void* data, size_t begin, size_t end)
{
    const SgemmCompute* job = (const SgemmCompute*)data;
    size_t tile;

    for(tile = begin; tile < end; tile++)
    {
        const size_t ir = (tile % job->panels_m) * SGEMM_MR;
        const size_t jr = (tile / job->panels_m) * SGEMM_NR;
        const size_t mr = SGEMM_MIN(SGEMM_MR, job->mc - ir);
        const size_t nr = SGEMM_MIN(SGEMM_NR, job->nc - jr);

        sgemm_kernel_16x6(job->Ap + ir * job->kc,
                          job->Bp + jr * job->kc,
                          job->C + jr * job->ldc + ir,
                          mr,
                          nr,
                          job->kc,
                          job->ldc,
                          job->accumulate);
    }
}

/* Minimum amount of items per job, below that the job overhead dominates */
#define SGEMM_PACK_GRAIN 8
#define SGEMM_TILE_GRAIN 8

void linalg32_sgemm_colmajor_avx2(LinAlgCtx* ctx,
                                  const float* ROMANO_RESTRICT A,
                                  const float* ROMANO_RESTRICT B,
                                  float* ROMANO_RESTRICT C,
                                  size_t M,
                                  size_t K,
                                  size_t N)
{
    /* Same blocking as stdromano (salykova), sized from the number of threads */
    const size_t nthreads = SGEMM_MIN((size_t)16, (size_t)linalg_ctx_get_threads_count(ctx));
    const size_t MC = SGEMM_MR * SGEMM_MAX((size_t)1, 42 / nthreads) * nthreads;
    const size_t NC = SGEMM_NR * (800 / nthreads) * nthreads;
    const size_t KC = SGEMM_KC;

    /* Do not allocate more than the problem needs */
    const size_t mc_max = SGEMM_MIN(MC, SGEMM_ROUND_UP(M, SGEMM_MR));
    const size_t nc_max = SGEMM_MIN(NC, SGEMM_ROUND_UP(N, SGEMM_NR));
    const size_t kc_max = SGEMM_MIN(KC, K);

    float* Ap;
    float* Bp;
    size_t i;
    size_t j;
    size_t p;

    if(M == 0 || N == 0 || K == 0)
        return;

    Ap = (float*)mem_aligned_alloc(mc_max * kc_max * sizeof(float), 64);
    Bp = (float*)mem_aligned_alloc(nc_max * kc_max * sizeof(float), 64);

    if(Ap == NULL || Bp == NULL)
    {
        if(Ap != NULL) mem_aligned_free(Ap);
        if(Bp != NULL) mem_aligned_free(Bp);
        return;
    }

    for(j = 0; j < N; j += NC)
    {
        const size_t nc = SGEMM_MIN(NC, N - j);

        for(p = 0; p < K; p += KC)
        {
            const size_t kc = SGEMM_MIN(KC, K - p);
            SgemmPackB pack_B;

            pack_B.B = B + j * K + p;
            pack_B.Bp = Bp;
            pack_B.nc = nc;
            pack_B.kc = kc;
            pack_B.ldb = K;

            linalg_parallel_for(ctx,
                                (nc + SGEMM_NR - 1) / SGEMM_NR,
                                SGEMM_PACK_GRAIN,
                                sgemm_pack_B_range,
                                &pack_B);

            for(i = 0; i < M; i += MC)
            {
                const size_t mc = SGEMM_MIN(MC, M - i);
                const size_t panels_m = (mc + SGEMM_MR - 1) / SGEMM_MR;
                const size_t panels_n = (nc + SGEMM_NR - 1) / SGEMM_NR;
                SgemmPackA pack_A;
                SgemmCompute compute;

                pack_A.A = A + p * M + i;
                pack_A.Ap = Ap;
                pack_A.mc = mc;
                pack_A.kc = kc;
                pack_A.lda = M;

                linalg_parallel_for(ctx, panels_m, SGEMM_PACK_GRAIN, sgemm_pack_A_range, &pack_A);

                compute.Ap = Ap;
                compute.Bp = Bp;
                compute.C = C + j * M + i;
                compute.mc = mc;
                compute.nc = nc;
                compute.kc = kc;
                compute.ldc = M;
                compute.panels_m = panels_m;
                compute.accumulate = p > 0; /* the first K block overwrites C */

                linalg_parallel_for(ctx,
                                    panels_m * panels_n,
                                    SGEMM_TILE_GRAIN,
                                    sgemm_compute_range,
                                    &compute);
            }
        }
    }

    mem_aligned_free(Ap);
    mem_aligned_free(Bp);
}

#endif /* defined(ROMANO_X86_64) */
