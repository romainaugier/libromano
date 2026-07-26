/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/math/linalg32.h"
#include "libromano/random.h"
#include "libromano/logger.h"
#include "libromano/simd.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#if ROMANO_DEBUG
#define MATMUL_SIZE_M 350
#define MATMUL_SIZE_N 213
#define M_CHOL 16
#else
#define MATMUL_SIZE_M 1024
#define MATMUL_SIZE_N 1024
#define M_CHOL 256
#endif /* ROMANO_DEBUG */


#define DEBUG_SIZE 4

#define EPSILON 0.01f

int main(void)
{
    size_t i;
    size_t j;
    size_t k;

    logger_init();

    logger_log(LogLevel_Info, "Starting math_linalg32 test");

    MatrixF A = matrixf_create(MATMUL_SIZE_M, MATMUL_SIZE_N);
    MatrixF B = matrixf_create(MATMUL_SIZE_N, MATMUL_SIZE_M);

    for(i = 0; i < MATMUL_SIZE_M; i++)
    {
        for(j = 0; j < MATMUL_SIZE_N; j++)
        {
            float r1 = random_float_01((i + 1) * (j + 1) * 4738);
            float r2 = random_float_01((i + 1) * (j + 1) * 2341);

            matrixf_set_at(&A, (float)r1 + 1.0f, i, j);
            matrixf_set_at(&B, (float)r2 + 1.0f, j, i);
        }
    }

    logger_log(LogLevel_Info, "A");
    matrixf_debug(&A, DEBUG_SIZE, DEBUG_SIZE);

    logger_log(LogLevel_Info, "B");
    matrixf_debug(&B, DEBUG_SIZE, DEBUG_SIZE);

    logger_log(LogLevel_Info, "Matrix transposition");

    MatrixF B_t = matrixf_transpose_from(&B);

    matrixf_debug(&B_t, DEBUG_SIZE, DEBUG_SIZE);

    matrixf_destroy(&B_t);

    logger_log(LogLevel_Info, "Matrix Multiplication");

    MatrixF C = matrix_null();

    for(i = 0; i < VectorizationMode_COUNT; i++)
    {
        simd_force_vectorization_mode((VectorizationMode)i);

        logger_log(LogLevel_Info,
                   "Vectorization mode: %s",
                   simd_get_vectorization_mode_as_string((VectorizationMode)i));

        SCOPED_PROFILE_MS_START(matrixf_mul);
        matrixf_mul(&A, &B, &C);
        SCOPED_PROFILE_MS_END(matrixf_mul);

        matrixf_debug(&C, DEBUG_SIZE, DEBUG_SIZE);
    }

    matrixf_destroy(&A);
    matrixf_destroy(&B);
    matrixf_destroy(&C);

    logger_log(LogLevel_Info, "Cholesky Solving");

    for(k = 0; k < VectorizationMode_COUNT; k++)
    {
        MatrixF _a = matrixf_create(M_CHOL, M_CHOL);

        for(i = 0; i < M_CHOL; i++)
        {
            for(j = 0; j < M_CHOL; j++)
            {
                float r1 = random_float_01((i + 1) * (j + 1) * 8439);
                matrixf_set_at(&_a, (float)r1 + 1.0f, i, j);
            }
        }

        MatrixF b = matrixf_create(M_CHOL, 1);

        for(i = 0; i < M_CHOL; i++)
            matrixf_set_at(&b, (float)i + 1.0f, i, 0);

        MatrixF _at = matrixf_copy(&_a);
        matrixf_transpose(&_at);

        MatrixF a = matrix_null();
        matrixf_mul(&_a, &_at, &a);

        matrixf_destroy(&_a);
        matrixf_destroy(&_at);

        matrixf_mul_by_f(&a, 0.01f);

        logger_log(LogLevel_Info, "A");
        matrixf_debug(&a, 4, 4);

        MatrixF x = matrix_null();

        SCOPED_PROFILE_MS_START(matrixf_cholesky_solve);
        bool res = matrixf_cholesky_solve(&a, &b, &x);
        SCOPED_PROFILE_MS_END(matrixf_cholesky_solve);

        if(!res)
        {
            logger_log(LogLevel_Error, "Cannot solve linear system with Cholesky Decomposition: %u", res);

            matrixf_destroy(&a);
            matrixf_destroy(&b);

            continue;
        }

        logger_log(LogLevel_Info, "Cholesky solve successful");

        logger_log(LogLevel_Info, "x");
        matrixf_debug(&x, 4, 4);

        logger_log(LogLevel_Info, "B");
        matrixf_debug(&b, 4, 4);

        matrixf_destroy(&a);
        matrixf_destroy(&b);
        matrixf_destroy(&x);
    }

    logger_log(LogLevel_Info, "Finished math_linalg32 test");

    logger_release();

    return 0;
}
