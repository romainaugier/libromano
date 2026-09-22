/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/*
 * AVX-512F 32x12 sgemm micro-kernel
 *
 * 24 zmm accumulators + 2 for A + 1 broadcast of B = 27 of the 32 zmm registers. Per k step:
 * 2 loads of A, 12 broadcasts of B (folded into the FMAs as {1to16} memory operands) and
 * 24 FMAs, so the loop is bound by the FMA ports, not by the loads.
 * The edges use the AVX-512 mask registers for the loads/stores of C.
 *
 * Only this function is compiled for AVX-512 (target attribute), the rest of the library keeps
 * the global flags and the kernel is only reached when the cpu reports AVX512F.
 */

#include "math_linalg_internal.h"

#if defined(ROMANO_X86_64)

#include <immintrin.h>

#if defined(__GNUC__) || defined(__clang__)
#define SGEMM_TARGET_AVX512 __attribute__((target("avx512f")))
#else
#define SGEMM_TARGET_AVX512 /* MSVC exposes the AVX-512 intrinsics without any flag */
#endif /* defined(__GNUC__) || defined(__clang__) */

#define MR 32
#define NR 12

#define SGEMM_AVX512_COLUMN(j)                                  \
    b = _mm512_set1_ps(Bp[j]);                                  \
    c##j##_0 = _mm512_fmadd_ps(a0, b, c##j##_0);                \
    c##j##_1 = _mm512_fmadd_ps(a1, b, c##j##_1)

#define SGEMM_AVX512_ZERO(j) __m512 c##j##_0 = _mm512_setzero_ps(), c##j##_1 = _mm512_setzero_ps()

SGEMM_TARGET_AVX512
static void sgemm_kernel_avx512_32x12(const float* ROMANO_RESTRICT Ap,
                                      const float* ROMANO_RESTRICT Bp,
                                      float* ROMANO_RESTRICT C,
                                      const size_t mr,
                                      const size_t nr,
                                      const size_t kc,
                                      const size_t ldc,
                                      const int accumulate)
{
    SGEMM_AVX512_ZERO(0);
    SGEMM_AVX512_ZERO(1);
    SGEMM_AVX512_ZERO(2);
    SGEMM_AVX512_ZERO(3);
    SGEMM_AVX512_ZERO(4);
    SGEMM_AVX512_ZERO(5);
    SGEMM_AVX512_ZERO(6);
    SGEMM_AVX512_ZERO(7);
    SGEMM_AVX512_ZERO(8);
    SGEMM_AVX512_ZERO(9);
    SGEMM_AVX512_ZERO(10);
    SGEMM_AVX512_ZERO(11);
    __m512 a0;
    __m512 a1;
    __m512 b;
    __mmask16 mask_lo;
    __mmask16 mask_hi;
    size_t p;
    size_t j;

    for(p = 0; p < kc; p++)
    {
        a0 = _mm512_load_ps(Ap);
        a1 = _mm512_load_ps(Ap + 16);

        SGEMM_AVX512_COLUMN(0);
        SGEMM_AVX512_COLUMN(1);
        SGEMM_AVX512_COLUMN(2);
        SGEMM_AVX512_COLUMN(3);
        SGEMM_AVX512_COLUMN(4);
        SGEMM_AVX512_COLUMN(5);
        SGEMM_AVX512_COLUMN(6);
        SGEMM_AVX512_COLUMN(7);
        SGEMM_AVX512_COLUMN(8);
        SGEMM_AVX512_COLUMN(9);
        SGEMM_AVX512_COLUMN(10);
        SGEMM_AVX512_COLUMN(11);

        Ap += MR;
        Bp += NR;
    }

    /* Rows [0, 16) in the low half, [16, 32) in the high half */
    mask_lo = (__mmask16)(mr >= 16 ? 0xFFFFu : (1u << mr) - 1u);
    mask_hi = (__mmask16)(mr <= 16 ? 0u : (mr >= 32 ? 0xFFFFu : (1u << (mr - 16)) - 1u));

    {
        /* Spilled once per tile, keeps the hot loop above in registers */
        const __m512 acc[2 * NR] = {
            c0_0, c0_1, c1_0, c1_1, c2_0, c2_1, c3_0, c3_1, c4_0, c4_1, c5_0, c5_1,
            c6_0, c6_1, c7_0, c7_1, c8_0, c8_1, c9_0, c9_1, c10_0, c10_1, c11_0, c11_1,
        };

        if(mr == MR)
        {
            for(j = 0; j < nr; j++)
            {
                float* c = C + j * ldc;
                __m512 lo = acc[2 * j];
                __m512 hi = acc[2 * j + 1];

                if(accumulate)
                {
                    lo = _mm512_add_ps(lo, _mm512_loadu_ps(c));
                    hi = _mm512_add_ps(hi, _mm512_loadu_ps(c + 16));
                }

                _mm512_storeu_ps(c, lo);
                _mm512_storeu_ps(c + 16, hi);
            }
        }
        else
        {
            for(j = 0; j < nr; j++)
            {
                float* c = C + j * ldc;
                __m512 lo = acc[2 * j];
                __m512 hi = acc[2 * j + 1];

                if(accumulate)
                {
                    lo = _mm512_add_ps(lo, _mm512_maskz_loadu_ps(mask_lo, c));
                    hi = _mm512_add_ps(hi, _mm512_maskz_loadu_ps(mask_hi, c + 16));
                }

                _mm512_mask_storeu_ps(c, mask_lo, lo);
                _mm512_mask_storeu_ps(c + 16, mask_hi, hi);
            }
        }
    }
}

const SgemmKernel linalg32_sgemm_kernel_avx512 = { "avx512_32x12", sgemm_kernel_avx512_32x12, MR, NR };

#endif /* defined(ROMANO_X86_64) */
