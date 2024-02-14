/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#if !defined(__LIBROMANO_SIMD)
#define __LIBROMANO_SIMD

#include "libromano/libromano.h"
#include <stdlib.h>

ROMANO_CPP_ENTER

#if defined(ROMANO_MSVC)
#include <intrin.h>
#define cpuid(regs, mode) __cpuid(regs, mode)
#else
#define cpuid(regs, mode) asm volatile ("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3]) : "a" (mode), "c" (0))
#endif /* defined(ROMANO_MSVC) */

typedef enum 
{
    VectorizationMode_Scalar = 0,
    VectorizationMode_SSE = 1,
    VectorizationMode_AVX = 2
} vectorization_mode;


ROMANO_API int simd_has_sse(void);

ROMANO_API int simd_has_avx(void);

ROMANO_API vectorization_mode simd_get_vectorization_mode(void);


ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_SIMD) */
