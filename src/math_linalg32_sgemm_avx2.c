/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/*
 * AVX2 + FMA 16x6 sgemm micro-kernel, ported from stdromano (salykova/sgemm.c)
 *
 * 12 ymm accumulators + 2 for A + 1 broadcast of B = 15 of the 16 ymm registers.
 * The hot loop always computes the full 16x6 tile (packed panels are zero-padded), only the
 * loads/stores of C are masked on the edges.
 */

#include "math_linalg_internal.h"

#if defined(ROMANO_X86_64)

#include <immintrin.h>

#define MR 16
#define NR 6

static void sgemm_kernel_avx2_16x6(const float* ROMANO_RESTRICT Ap,
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

        Ap += MR;
        Bp += NR;
    }

    {
        /* Spilled once per tile, keeps the hot loop above in registers */
        const __m256 acc[2 * NR] = { c00, c01, c10, c11, c20, c21, c30, c31, c40, c41, c50, c51 };

        if(mr == MR)
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

const SgemmKernel linalg32_sgemm_kernel_avx2 = { "avx2_16x6", sgemm_kernel_avx2_16x6, MR, NR };

#endif /* defined(ROMANO_X86_64) */
