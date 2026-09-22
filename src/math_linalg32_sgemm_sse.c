/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/*
 * SSE 8x6 sgemm micro-kernel (SSE1 only: no FMA, mul + add)
 *
 * Same register budget as the AVX2 kernel at half the width: 12 xmm accumulators + 2 for A +
 * 1 broadcast of B = 15 of the 16 xmm registers of x86_64.
 * SSE has no masked loads/stores: partial tiles (matrix edges) go through a small buffer.
 */

#include "math_linalg_internal.h"

#if defined(ROMANO_X86_64)

#include <xmmintrin.h>

#define MR 8
#define NR 6

#define SGEMM_SSE_COLUMN(j)                                     \
    b = _mm_load1_ps(Bp + (j));                                 \
    c##j##0 = _mm_add_ps(c##j##0, _mm_mul_ps(a0, b));           \
    c##j##1 = _mm_add_ps(c##j##1, _mm_mul_ps(a1, b))

static void sgemm_kernel_sse_8x6(const float* ROMANO_RESTRICT Ap,
                                 const float* ROMANO_RESTRICT Bp,
                                 float* ROMANO_RESTRICT C,
                                 const size_t mr,
                                 const size_t nr,
                                 const size_t kc,
                                 const size_t ldc,
                                 const int accumulate)
{
    __m128 c00 = _mm_setzero_ps(), c01 = _mm_setzero_ps();
    __m128 c10 = _mm_setzero_ps(), c11 = _mm_setzero_ps();
    __m128 c20 = _mm_setzero_ps(), c21 = _mm_setzero_ps();
    __m128 c30 = _mm_setzero_ps(), c31 = _mm_setzero_ps();
    __m128 c40 = _mm_setzero_ps(), c41 = _mm_setzero_ps();
    __m128 c50 = _mm_setzero_ps(), c51 = _mm_setzero_ps();
    __m128 a0;
    __m128 a1;
    __m128 b;
    size_t p;
    size_t i;
    size_t j;

    for(p = 0; p < kc; p++)
    {
        a0 = _mm_load_ps(Ap);
        a1 = _mm_load_ps(Ap + 4);

        SGEMM_SSE_COLUMN(0);
        SGEMM_SSE_COLUMN(1);
        SGEMM_SSE_COLUMN(2);
        SGEMM_SSE_COLUMN(3);
        SGEMM_SSE_COLUMN(4);
        SGEMM_SSE_COLUMN(5);

        Ap += MR;
        Bp += NR;
    }

    {
        const __m128 acc[2 * NR] = { c00, c01, c10, c11, c20, c21, c30, c31, c40, c41, c50, c51 };

        if(mr == MR)
        {
            for(j = 0; j < nr; j++)
            {
                float* c = C + j * ldc;
                __m128 lo = acc[2 * j];
                __m128 hi = acc[2 * j + 1];

                if(accumulate)
                {
                    lo = _mm_add_ps(lo, _mm_loadu_ps(c));
                    hi = _mm_add_ps(hi, _mm_loadu_ps(c + 4));
                }

                _mm_storeu_ps(c, lo);
                _mm_storeu_ps(c + 4, hi);
            }
        }
        else
        {
            float tile[MR * NR];

            for(j = 0; j < nr; j++)
            {
                _mm_storeu_ps(tile + j * MR, acc[2 * j]);
                _mm_storeu_ps(tile + j * MR + 4, acc[2 * j + 1]);
            }

            for(j = 0; j < nr; j++)
            {
                float* c = C + j * ldc;

                for(i = 0; i < mr; i++)
                    c[i] = accumulate ? c[i] + tile[j * MR + i] : tile[j * MR + i];
            }
        }
    }
}

const SgemmKernel linalg32_sgemm_kernel_sse = { "sse_8x6", sgemm_kernel_sse_8x6, MR, NR };

#endif /* defined(ROMANO_X86_64) */
