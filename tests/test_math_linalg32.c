/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/math/linalg32.h"
#include "libromano/random.h"
#include "libromano/logger.h"
#include "libromano/simd.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#if defined(ROMANO_DEBUG)
#define MATMUL_SIZE_M 500
#define MATMUL_SIZE_N 500
#else
#define MATMUL_SIZE_M 1000
#define MATMUL_SIZE_N 1000
#endif /* defined(ROMANO_DEBUG) */

#define M_CHOL 4

int main(void)
{
    logger_init();

    logger_log(LogLevel_Info, "Starting math_linalg32 test");

    matrixf_t A = matrixf_create(MATMUL_SIZE_M, MATMUL_SIZE_N);
    matrixf_t B = matrixf_create(MATMUL_SIZE_N, MATMUL_SIZE_M);

    for(int i = 0; i < MATMUL_SIZE_M; i++)
    {
        for(int j = 0; j < MATMUL_SIZE_N; j++)
        {
            float r1 = random_float_01((i + 1) * (j + 1) * 4738);
            float r2 = random_float_01((i + 1) * (j + 1) * 8439);

            matrixf_set_at(&A, (float)r1 + 1.0f, i, j);
            matrixf_set_at(&B, (float)r2 + 1.0f, j, i);
        }
    }

    logger_log(LogLevel_Info, "A");
    matrixf_debug(&A, 4, 4);

    logger_log(LogLevel_Info, "B");
    matrixf_debug(&B, 4, 4);

    logger_log(LogLevel_Info, "Matrix Multiplication");

    simd_force_vectorization_mode(VectorizationMode_Scalar);
    SCOPED_PROFILE_START_SECONDS(matrixf_scalar_mul);

    matrixf_t C_scalar = matrix_null();
    matrixf_mul(&A, &B, &C_scalar);

    SCOPED_PROFILE_END_SECONDS(matrixf_scalar_mul);

    simd_force_vectorization_mode(VectorizationMode_SSE);
    SCOPED_PROFILE_START_SECONDS(matrixf_sse_mul);

    matrixf_t C_sse = matrix_null();
    matrixf_mul(&A, &B, &C_sse);

    SCOPED_PROFILE_END_SECONDS(matrixf_sse_mul);

    simd_force_vectorization_mode(VectorizationMode_AVX);
    SCOPED_PROFILE_START_SECONDS(matrixf_avx_mul);

    matrixf_t C_avx = matrix_null();
    matrixf_mul(&A, &B, &C_avx);

    SCOPED_PROFILE_END_SECONDS(matrixf_avx_mul);

    matrixf_destroy(&A);
    matrixf_destroy(&B);
    matrixf_destroy(&C_scalar);
    matrixf_destroy(&C_sse);
    matrixf_destroy(&C_avx);

    logger_log(LogLevel_Info, "Cholesky Solving");

    matrixf_t _a = matrixf_create(M_CHOL, M_CHOL);

    for(int i = 0; i < M_CHOL; i++)
    {
        for(int j = 0; j < M_CHOL; j++)
        {
            float r1 = random_float_01((i + 1) * (j + 1) * 8439);
            matrixf_set_at(&_a, (float)r1 + 1.0f, i, j);
        }
    }

    matrixf_t b = matrixf_create(M_CHOL, 1);

    for(int i = 0; i < M_CHOL; i++)
    {
        matrixf_set_at(&b, (float)i + 1.0f, i, 0);
    }

    matrixf_t _at = matrixf_copy(&_a);
    matrixf_transpose(&_at);

    matrixf_t a = matrix_null();
    matrixf_mul(&_a, &_at, &a);

    matrixf_destroy(&_a);
    matrixf_destroy(&_at);

    logger_log(LogLevel_Info, "A");
    matrixf_debug(&a, 0, 0);

    matrixf_t x = matrix_null();

    bool res = matrixf_cholesky_solve(&a, &b, &x);

    if(!res)
    {
        logger_log(LogLevel_Error, "Cannot solve linear system with Cholesky Decomposition: %u", res);
        return 1;
    }

    logger_log(LogLevel_Info, "Cholesky solve successful");

    logger_log(LogLevel_Info, "x");
    matrixf_debug(&x, 0, 0);

    logger_log(LogLevel_Info, "B");
    matrixf_debug(&b, 0, 0);

    matrixf_destroy(&a);
    matrixf_destroy(&b);
    matrixf_destroy(&x);

    logger_log(LogLevel_Info, "Finished math_linalg32 test");

    logger_release();

    return 0;
}