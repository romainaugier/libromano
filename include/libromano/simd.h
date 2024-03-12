/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_SIMD)
#define __LIBROMANO_SIMD

#include "libromano/libromano.h"

#include <stdlib.h>

ROMANO_CPP_ENTER

typedef enum
{
    VectorizationMode_Scalar = 0,
    VectorizationMode_SSE = 1,
    VectorizationMode_AVX = 2
} VectorizationMode;

#define VECTORIZATION_MODE_STR(mode) mode == 2 ? "AVX" : mode == 1 ? "SSE" : "Scalar (None)" 

void simd_check_vectorization(void);

ROMANO_API int simd_has_sse(void);

ROMANO_API int simd_has_avx(void);

ROMANO_API VectorizationMode simd_get_vectorization_mode(void);


ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_SIMD) */
