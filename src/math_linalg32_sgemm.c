/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/*
 * Single precision gemm driver (Goto / BLIS algorithm), independent of the instruction set:
 * the micro-kernels (math_linalg32_sgemm_<isa>.c) only compute one MR x NR register tile.
 *
 * Everything is column-major: A is M x K (lda = M), B is K x N (ldb = K), C is M x N (ldc = M).
 * The row-major MatrixF API maps onto it with C^T = B^T * A^T, see math_linalg32.c.
 *
 *   for j in N by nc                  B block (kc x nc) packed, lives in the L3, shared by all threads
 *     for p in K by kc
 *       pack B[p:p+kc, j:j+nc]
 *       for i in M by mc              A block (mc x kc) packed, lives in the L2
 *         pack A[i:i+mc, p:p+kc]
 *         for each MR x NR tile       B micro-panel (kc x NR) stays in the L1 while the
 *           micro-kernel              A micro-panels (MR x kc) stream from the L2
 *
 * The tiles of an (mc x nc) block are split between the threads of the context.
 */

#include "math_linalg_internal.h"

#include "libromano/cpu.h"
#include "libromano/memory.h"

#include <string.h>

#define SGEMM_MIN(a, b) ((a) < (b) ? (a) : (b))
#define SGEMM_MAX(a, b) ((a) > (b) ? (a) : (b))
#define SGEMM_CLAMP(x, lo, hi) SGEMM_MIN(SGEMM_MAX((x), (lo)), (hi))
#define SGEMM_ROUND_UP(x, m) ((((x) + (m) - 1) / (m)) * (m))
#define SGEMM_ROUND_DOWN(x, m) (((x) / (m)) * (m))

/* Blocking */

/* Used when a cache size could not be detected */
#define SGEMM_DEFAULT_L1_SIZE ((size_t)32 * 1024)
#define SGEMM_DEFAULT_L2_SIZE ((size_t)512 * 1024)
#define SGEMM_DEFAULT_L3_SIZE ((size_t)8 * 1024 * 1024)

/* Upper bound of the packed B buffer, big L3 (servers, VMs) do not need a bigger block */
#define SGEMM_MAX_B_BLOCK_SIZE ((size_t)16 * 1024 * 1024)

#define SGEMM_KC_MIN 64
#define SGEMM_KC_MAX 1024

/* Bounds the A block when K is tiny (the packed A block would otherwise span a huge mc) */
#define SGEMM_MC_MAX 8192

void linalg32_sgemm_compute_blocking(const SgemmKernel* kernel, size_t K, SgemmBlocking* blocking)
{
    size_t l1 = cpu_get_cache_size(CPUCacheLevel_L1);
    size_t l2 = cpu_get_cache_size(CPUCacheLevel_L2);
    size_t l3 = cpu_get_cache_size(CPUCacheLevel_L3);
    size_t kc;
    size_t depth;
    size_t mc;
    size_t nc;

    l1 = l1 != 0 ? l1 : SGEMM_DEFAULT_L1_SIZE;
    l2 = l2 != 0 ? l2 : SGEMM_DEFAULT_L2_SIZE;

    /* No L3 (Apple Silicon, many ARM cores): the L2 is the last level, shared by a cluster */
    l3 = l3 != 0 ? l3 : (cpu_get_cache_size(CPUCacheLevel_L2) != 0 ? l2 : SGEMM_DEFAULT_L3_SIZE);

    /* kc: one A micro-panel (MR x kc) and one B micro-panel (kc x NR) fill the L1 */
    kc = l1 / ((kernel->mr + kernel->nr) * sizeof(float));
    kc = SGEMM_CLAMP(SGEMM_ROUND_DOWN(kc, 8), (size_t)SGEMM_KC_MIN, (size_t)SGEMM_KC_MAX);

    /*
     * The A and B blocks are sized on the depth they really have: with K < kc a block of the
     * same size in bytes holds more rows / columns, which means fewer passes over the other one
     * (with K = 64, sizing on kc would split M in ~9 blocks and stream the packed B 9 times)
     */
    depth = (K != 0 && K < kc) ? K : kc;

    /*
     * mc: the packed A block (mc x kc) takes half of the L2, the other half is left to the B
     * micro-panel and the C tiles going through it. Every thread reads the whole A block, so
     * it is sized on the L2 of one core whatever the number of threads.
     */
    mc = SGEMM_MIN((l2 / 2) / (depth * sizeof(float)), (size_t)SGEMM_MC_MAX);
    mc = SGEMM_MAX(SGEMM_ROUND_DOWN(mc, kernel->mr), kernel->mr);

    /* nc: the packed B block (kc x nc) takes half of the L3, it is also read by all the threads */
    nc = SGEMM_MIN(l3 / 2, SGEMM_MAX_B_BLOCK_SIZE) / (depth * sizeof(float));
    nc = SGEMM_MAX(SGEMM_ROUND_DOWN(nc, kernel->nr), kernel->nr);

    blocking->kc = kc;
    blocking->mc = mc;
    blocking->nc = nc;
}

/* Packing */

/*
 * The packing loops are instantiated for the tile sizes of the kernels (MR/NR known at compile
 * time): the copies get unrolled/vectorized instead of looping on a runtime width. On shapes
 * where each packed element is reused little (small M or N), packing is a large part of the time.
 */

ROMANO_FORCE_INLINE void sgemm_pack_panel_A_impl(const float* ROMANO_RESTRICT A,
                                                 float* ROMANO_RESTRICT Ap,
                                                 const size_t mr,
                                                 const size_t MR,
                                                 const size_t kc,
                                                 const size_t lda)
{
    size_t p;
    size_t i;

    if(mr == MR)
    {
        for(p = 0; p < kc; p++)
        {
            for(i = 0; i < MR; i++)
                Ap[i] = A[p * lda + i];

            Ap += MR;
        }

        return;
    }

    for(p = 0; p < kc; p++)
    {
        for(i = 0; i < mr; i++)
            Ap[i] = A[p * lda + i];

        for(i = mr; i < MR; i++)
            Ap[i] = 0.0f;

        Ap += MR;
    }
}

ROMANO_FORCE_INLINE void sgemm_pack_panel_B_impl(const float* ROMANO_RESTRICT B,
                                                 float* ROMANO_RESTRICT Bp,
                                                 const size_t nr,
                                                 const size_t NR,
                                                 const size_t kc,
                                                 const size_t ldb)
{
    size_t p;
    size_t j;

    if(nr == NR)
    {
        for(p = 0; p < kc; p++)
        {
            for(j = 0; j < NR; j++)
                Bp[j] = B[j * ldb + p];

            Bp += NR;
        }

        return;
    }

    for(p = 0; p < kc; p++)
    {
        for(j = 0; j < nr; j++)
            Bp[j] = B[j * ldb + p];

        for(j = nr; j < NR; j++)
            Bp[j] = 0.0f;

        Bp += NR;
    }
}

static void sgemm_pack_panel_A(const float* ROMANO_RESTRICT A,
                               float* ROMANO_RESTRICT Ap,
                               const size_t mr,
                               const size_t MR,
                               const size_t kc,
                               const size_t lda)
{
    switch(MR)
    {
        case 8: sgemm_pack_panel_A_impl(A, Ap, mr, 8, kc, lda); break;
        case 16: sgemm_pack_panel_A_impl(A, Ap, mr, 16, kc, lda); break;
        case 32: sgemm_pack_panel_A_impl(A, Ap, mr, 32, kc, lda); break;
        default: sgemm_pack_panel_A_impl(A, Ap, mr, MR, kc, lda); break;
    }
}

static void sgemm_pack_panel_B(const float* ROMANO_RESTRICT B,
                               float* ROMANO_RESTRICT Bp,
                               const size_t nr,
                               const size_t NR,
                               const size_t kc,
                               const size_t ldb)
{
    switch(NR)
    {
        case 6: sgemm_pack_panel_B_impl(B, Bp, nr, 6, kc, ldb); break;
        case 12: sgemm_pack_panel_B_impl(B, Bp, nr, 12, kc, ldb); break;
        default: sgemm_pack_panel_B_impl(B, Bp, nr, NR, kc, ldb); break;
    }
}

typedef struct SgemmPack {
    const float* src; /* top-left of the block */
    float* dst;
    size_t extent;    /* rows (A) or columns (B) of the block */
    size_t kc;
    size_t ld;
    size_t R;         /* MR (A) or NR (B) */
} SgemmPack;

static void sgemm_pack_A_range(void* data, size_t begin, size_t end)
{
    const SgemmPack* job = (const SgemmPack*)data;
    size_t panel;

    for(panel = begin; panel < end; panel++)
    {
        const size_t i = panel * job->R;

        sgemm_pack_panel_A(job->src + i,
                           job->dst + i * job->kc,
                           SGEMM_MIN(job->R, job->extent - i),
                           job->R,
                           job->kc,
                           job->ld);
    }
}

static void sgemm_pack_B_range(void* data, size_t begin, size_t end)
{
    const SgemmPack* job = (const SgemmPack*)data;
    size_t panel;

    for(panel = begin; panel < end; panel++)
    {
        const size_t j = panel * job->R;

        sgemm_pack_panel_B(job->src + j * job->ld,
                           job->dst + j * job->kc,
                           SGEMM_MIN(job->R, job->extent - j),
                           job->R,
                           job->kc,
                           job->ld);
    }
}

/* Macro-kernel: all the MR x NR tiles of an mc x nc block of C */

typedef struct SgemmCompute {
    const SgemmKernel* kernel;
    const float* Ap;
    const float* Bp;
    float* C;         /* top-left of the mc x nc block */
    size_t mc;
    size_t nc;
    size_t kc;
    size_t ldc;
    size_t panels_m;  /* number of MR-row panels in mc */
    int accumulate;
} SgemmCompute;

/*
 * Tiles are numbered with the row panel varying fastest, so a chunk of consecutive tiles keeps
 * reusing the same packed B micro-panel (L1) while streaming the packed A block (L2)
 */
static void sgemm_compute_range(void* data, size_t begin, size_t end)
{
    const SgemmCompute* job = (const SgemmCompute*)data;
    const SgemmMicroKernel func = job->kernel->func;
    const size_t MR = job->kernel->mr;
    const size_t NR = job->kernel->nr;
    size_t tile;

    for(tile = begin; tile < end; tile++)
    {
        const size_t ir = (tile % job->panels_m) * MR;
        const size_t jr = (tile / job->panels_m) * NR;

        func(job->Ap + ir * job->kc,
             job->Bp + jr * job->kc,
             job->C + jr * job->ldc + ir,
             SGEMM_MIN(MR, job->mc - ir),
             SGEMM_MIN(NR, job->nc - jr),
             job->kc,
             job->ldc,
             job->accumulate);
    }
}

/* Minimum amount of items per job, below that the job overhead dominates */
#define SGEMM_PACK_GRAIN 8
#define SGEMM_TILE_GRAIN 8

void linalg32_sgemm_colmajor(LinAlgCtx* ctx,
                             const SgemmKernel* kernel,
                             const SgemmBlocking* blocking,
                             const float* ROMANO_RESTRICT A,
                             const float* ROMANO_RESTRICT B,
                             float* ROMANO_RESTRICT C,
                             size_t M,
                             size_t K,
                             size_t N)
{
    SgemmBlocking computed;
    size_t MC;
    size_t NC;
    size_t KC;
    size_t mc_max;
    size_t nc_max;
    size_t kc_max;
    float* Ap;
    float* Bp;
    size_t i;
    size_t j;
    size_t p;

    if(M == 0 || N == 0 || K == 0)
        return;

    if(blocking == NULL)
    {
        linalg32_sgemm_compute_blocking(kernel, K, &computed);
        blocking = &computed;
    }

    MC = blocking->mc;
    NC = blocking->nc;
    KC = blocking->kc;

    /* Do not allocate more than the problem needs */
    mc_max = SGEMM_MIN(MC, SGEMM_ROUND_UP(M, kernel->mr));
    nc_max = SGEMM_MIN(NC, SGEMM_ROUND_UP(N, kernel->nr));
    kc_max = SGEMM_MIN(KC, K);

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
        const size_t panels_n = (nc + kernel->nr - 1) / kernel->nr;

        for(p = 0; p < K; p += KC)
        {
            const size_t kc = SGEMM_MIN(KC, K - p);
            SgemmPack pack_B;

            pack_B.src = B + j * K + p;
            pack_B.dst = Bp;
            pack_B.extent = nc;
            pack_B.kc = kc;
            pack_B.ld = K;
            pack_B.R = kernel->nr;

            linalg_parallel_for(ctx, panels_n, SGEMM_PACK_GRAIN, sgemm_pack_B_range, &pack_B);

            for(i = 0; i < M; i += MC)
            {
                const size_t mc = SGEMM_MIN(MC, M - i);
                const size_t panels_m = (mc + kernel->mr - 1) / kernel->mr;
                SgemmPack pack_A;
                SgemmCompute compute;

                pack_A.src = A + p * M + i;
                pack_A.dst = Ap;
                pack_A.extent = mc;
                pack_A.kc = kc;
                pack_A.ld = M;
                pack_A.R = kernel->mr;

                linalg_parallel_for(ctx, panels_m, SGEMM_PACK_GRAIN, sgemm_pack_A_range, &pack_A);

                compute.kernel = kernel;
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
