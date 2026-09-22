/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/*
 * Tests of the internal sgemm driver and micro-kernels (compiled into this test, see
 * tests/CMakeLists.txt), with forced block sizes so that every block boundary is crossed on
 * small matrices.
 *
 * Inputs are multiples of 1/16 in [-4, 4]: every product and partial sum is exact in float for
 * the sizes used here, so the results must match the reference exactly, whatever the kernel,
 * the blocking or the summation order.
 */

#include "test.h"

#include "libromano/cpu.h"
#include "libromano/threadpool.h"
#include "libromano/memory.h"

#include <stdlib.h>

#include "math_linalg_internal.h"

#if defined(ROMANO_X86_64)

typedef struct KernelEntry {
    const SgemmKernel* kernel;
    bool available;
} KernelEntry;

static KernelEntry g_kernels[3];
static size_t g_num_kernels = 0;

static void init_kernels(void)
{
    g_num_kernels = 0;

    g_kernels[g_num_kernels].kernel = &linalg32_sgemm_kernel_sse;
    g_kernels[g_num_kernels++].available = cpu_has_feature(CPUFeature_SSE);

    g_kernels[g_num_kernels].kernel = &linalg32_sgemm_kernel_avx2;
    g_kernels[g_num_kernels++].available = cpu_has_feature(CPUFeature_AVX2) &&
                                           cpu_has_feature(CPUFeature_FMA3);

    g_kernels[g_num_kernels].kernel = &linalg32_sgemm_kernel_avx512;
    g_kernels[g_num_kernels++].available = cpu_has_feature(CPUFeature_AVX512F);
}

static uint64_t g_rng = 0x9E3779B97F4A7C15ull;

static uint32_t rng_next(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (uint32_t)(g_rng >> 16);
}

static size_t rng_range(size_t lo, size_t hi)
{
    return lo + (size_t)rng_next() % (hi - lo + 1);
}

static void fill(float* x, size_t n)
{
    size_t i;

    for(i = 0; i < n; i++)
        x[i] = (float)((int)(rng_next() % 129u) - 64) / 16.0f;
}

/* Column-major reference, exact for the inputs used here */
static void reference(const float* A, const float* B, float* C, size_t M, size_t K, size_t N)
{
    size_t i, j, k;

    for(j = 0; j < N; j++)
    {
        for(i = 0; i < M; i++)
        {
            double sum = 0.0;

            for(k = 0; k < K; k++)
                sum += (double)A[i + k * M] * (double)B[k + j * K];

            C[i + j * M] = (float)sum;
        }
    }
}

static void test_blocking(void)
{
    size_t k;

    init_kernels();

    for(k = 0; k < g_num_kernels; k++)
    {
        const SgemmKernel* kernel = g_kernels[k].kernel;
        SgemmBlocking blocking;

        linalg32_sgemm_compute_blocking(kernel, 0, &blocking);

        logger_log_info("%-13s %2zux%-2zu kc=%zu mc=%zu nc=%zu (A block %zu KiB, B block %zu KiB)",
                        kernel->name,
                        kernel->mr,
                        kernel->nr,
                        blocking.kc,
                        blocking.mc,
                        blocking.nc,
                        blocking.kc * blocking.mc * sizeof(float) / 1024,
                        blocking.kc * blocking.nc * sizeof(float) / 1024);

        TEST_CHECK(blocking.kc >= 64 && blocking.kc <= 1024 && blocking.kc % 8 == 0);
        TEST_CHECK(blocking.mc >= kernel->mr && blocking.mc % kernel->mr == 0);
        TEST_CHECK(blocking.nc >= kernel->nr && blocking.nc % kernel->nr == 0);
        TEST_CHECK(blocking.kc * blocking.nc * sizeof(float) <= (size_t)16 * 1024 * 1024);

        /* A shallow product (K < kc) gets bigger A/B blocks for the same cache footprint */
        {
            SgemmBlocking shallow;

            linalg32_sgemm_compute_blocking(kernel, 32, &shallow);

            TEST_CHECK(shallow.kc == blocking.kc);
            TEST_CHECK(shallow.mc >= blocking.mc && shallow.mc % kernel->mr == 0 && shallow.mc <= 8192);
            TEST_CHECK(shallow.nc >= blocking.nc && shallow.nc % kernel->nr == 0);
            TEST_CHECK(32 * shallow.nc * sizeof(float) <= (size_t)16 * 1024 * 1024);
        }

        /* The micro-panels fit the L1 unless kc hit its lower clamp */
        if(cpu_get_cache_size(CPUCacheLevel_L1) != 0 && blocking.kc > 64)
            TEST_CHECK(blocking.kc * (kernel->mr + kernel->nr) * sizeof(float) <=
                       cpu_get_cache_size(CPUCacheLevel_L1));
    }
}

/*
 * Every edge case of the micro-kernel itself: all mr x nr sub-tiles, overwrite and accumulate,
 * and nothing written outside of the tile (the masked stores must stay inside)
 */
static void test_micro_kernels(void)
{
    const size_t kcs[] = { 1, 2, 7, 33 };
    const float guard = 12345.0f;
    size_t k;

    init_kernels();

    for(k = 0; k < g_num_kernels; k++)
    {
        const SgemmKernel* kernel = g_kernels[k].kernel;
        const size_t MR = kernel->mr;
        const size_t NR = kernel->nr;
        const size_t ldc = MR + 5;
        size_t failures = 0;
        size_t c;

        if(!g_kernels[k].available)
        {
            logger_log_info("%s not supported by this cpu, skipped", kernel->name);
            continue;
        }

        for(c = 0; c < sizeof(kcs) / sizeof(kcs[0]); c++)
        {
            const size_t kc = kcs[c];
            float* Ap = (float*)mem_aligned_alloc(MR * kc * sizeof(float), 64);
            float* Bp = (float*)mem_aligned_alloc(NR * kc * sizeof(float), 64);
            float* C = (float*)malloc(ldc * (NR + 1) * sizeof(float));
            float* C0 = (float*)malloc(ldc * (NR + 1) * sizeof(float));
            size_t mr, nr, i, j, p;
            int accumulate;

            fill(Ap, MR * kc);
            fill(Bp, NR * kc);

            for(mr = 1; mr <= MR; mr++)
            {
                for(nr = 1; nr <= NR; nr++)
                {
                    for(accumulate = 0; accumulate < 2; accumulate++)
                    {
                        /* Padding of the packed panels is zero, like the driver does */
                        float* Apad = (float*)mem_aligned_alloc(MR * kc * sizeof(float), 64);
                        float* Bpad = (float*)mem_aligned_alloc(NR * kc * sizeof(float), 64);

                        for(p = 0; p < kc; p++)
                        {
                            for(i = 0; i < MR; i++)
                                Apad[p * MR + i] = i < mr ? Ap[p * MR + i] : 0.0f;

                            for(j = 0; j < NR; j++)
                                Bpad[p * NR + j] = j < nr ? Bp[p * NR + j] : 0.0f;
                        }

                        for(i = 0; i < ldc * (NR + 1); i++)
                            C[i] = C0[i] = guard;

                        for(j = 0; j < nr; j++)
                            for(i = 0; i < mr; i++)
                                C[j * ldc + i] = C0[j * ldc + i] = (float)((int)(i + 3 * j) % 7 - 3);

                        kernel->func(Apad, Bpad, C, mr, nr, kc, ldc, accumulate);

                        for(j = 0; j < NR + 1; j++)
                        {
                            for(i = 0; i < ldc; i++)
                            {
                                float expected = C0[j * ldc + i];

                                if(i < mr && j < nr)
                                {
                                    double sum = accumulate ? (double)C0[j * ldc + i] : 0.0;

                                    for(p = 0; p < kc; p++)
                                        sum += (double)Apad[p * MR + i] * (double)Bpad[p * NR + j];

                                    expected = (float)sum;
                                }

                                if(C[j * ldc + i] != expected)
                                {
                                    if(failures++ < 5)
                                        logger_log_error("%s kc=%zu mr=%zu nr=%zu acc=%d: C[%zu,%zu] = %f, expected %f%s",
                                                         kernel->name, kc, mr, nr, accumulate, i, j,
                                                         C[j * ldc + i], expected,
                                                         (i >= mr || j >= nr) ? " (outside the tile)" : "");
                                }
                            }
                        }

                        mem_aligned_free(Apad);
                        mem_aligned_free(Bpad);
                    }
                }
            }

            mem_aligned_free(Ap);
            mem_aligned_free(Bp);
            free(C);
            free(C0);
        }

        TEST_CHECK_MSG(failures == 0, "%s: %zu wrong values", kernel->name, failures);
    }
}

/* Whole driver with tiny block sizes: many K accumulation passes, M and N blocks, edge tiles */
static void test_forced_blocking(void)
{
    ThreadPool* pool = threadpool_init(3);
    LinAlgCtx ctx = linalg_ctx_new(pool);
    size_t k;
    int round;

    TEST_ASSERT(pool != NULL);

    init_kernels();

    for(k = 0; k < g_num_kernels; k++)
    {
        const SgemmKernel* kernel = g_kernels[k].kernel;
        size_t failures = 0;

        if(!g_kernels[k].available)
            continue;

        for(round = 0; round < 60; round++)
        {
            const size_t M = rng_range(1, 5 * kernel->mr + 3);
            const size_t K = rng_range(1, 150);
            const size_t N = rng_range(1, 7 * kernel->nr + 2);
            SgemmBlocking blocking;
            float* A = (float*)malloc(M * K * sizeof(float));
            float* B = (float*)malloc(K * N * sizeof(float));
            float* C = (float*)malloc((M * N + 16) * sizeof(float));
            float* R = (float*)malloc(M * N * sizeof(float));
            size_t i;

            blocking.kc = rng_range(1, 40);
            blocking.mc = kernel->mr * rng_range(1, 3);
            blocking.nc = kernel->nr * rng_range(1, 3);

            fill(A, M * K);
            fill(B, K * N);
            reference(A, B, R, M, K, N);

            for(i = 0; i < M * N + 16; i++)
                C[i] = -777.0f; /* garbage: the first K block must overwrite it */

            linalg32_sgemm_colmajor((round % 2) ? &ctx : NULL, kernel, &blocking, A, B, C, M, K, N);

            for(i = 0; i < M * N; i++)
                failures += C[i] != R[i];

            for(i = M * N; i < M * N + 16; i++)
                failures += C[i] != -777.0f;

            if(failures != 0)
            {
                logger_log_error("%s: %zux%zux%zu kc=%zu mc=%zu nc=%zu (%s) is wrong",
                                 kernel->name, M, K, N, blocking.kc, blocking.mc, blocking.nc,
                                 (round % 2) ? "threaded" : "single thread");
                free(A); free(B); free(C); free(R);
                break;
            }

            free(A);
            free(B);
            free(C);
            free(R);
        }

        TEST_CHECK_MSG(failures == 0, "%s with forced blocking", kernel->name);
    }

    threadpool_release(pool);
}

/* Default (cache derived) blocking, sizes crossing at least the kc block */
static void test_default_blocking(void)
{
    ThreadPool* pool = threadpool_init(2);
    LinAlgCtx ctx = linalg_ctx_new(pool);
    const size_t shapes[][3] = { { 301, 290, 97 }, { 33, 1100, 17 }, { 1, 300, 700 }, { 517, 5, 3 } };
    size_t k;
    size_t s;

    TEST_ASSERT(pool != NULL);

    init_kernels();

    for(k = 0; k < g_num_kernels; k++)
    {
        const SgemmKernel* kernel = g_kernels[k].kernel;

        if(!g_kernels[k].available)
            continue;

        for(s = 0; s < sizeof(shapes) / sizeof(shapes[0]); s++)
        {
            const size_t M = shapes[s][0], K = shapes[s][1], N = shapes[s][2];
            float* A = (float*)malloc(M * K * sizeof(float));
            float* B = (float*)malloc(K * N * sizeof(float));
            float* C = (float*)malloc(M * N * sizeof(float));
            float* R = (float*)malloc(M * N * sizeof(float));
            size_t failures = 0;
            size_t i;

            /* K up to 1100 is not exact anymore with random signs of the full range: use 0/1/2/3 */
            for(i = 0; i < M * K; i++) A[i] = (float)(rng_next() % 4u);
            for(i = 0; i < K * N; i++) B[i] = (float)(rng_next() % 4u) - 1.0f;

            reference(A, B, R, M, K, N);

            linalg32_sgemm_colmajor(s % 2 ? &ctx : NULL, kernel, NULL, A, B, C, M, K, N);

            for(i = 0; i < M * N; i++)
                failures += C[i] != R[i];

            TEST_CHECK_MSG(failures == 0, "%s: %zux%zux%zu, %zu wrong values", kernel->name, M, K, N, failures);

            free(A);
            free(B);
            free(C);
            free(R);
        }
    }

    threadpool_release(pool);
}

TEST_MAIN(
    TEST(test_blocking),
    TEST(test_micro_kernels),
    TEST(test_forced_blocking),
    TEST(test_default_blocking),
)

#else

static void test_skipped(void)
{
    logger_log_info("no sgemm kernels on this architecture");
}

TEST_MAIN(
    TEST(test_skipped),
)

#endif /* defined(ROMANO_X86_64) */
