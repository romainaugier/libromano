/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/math/linalg32.h"
#include "libromano/random.h"
#include "libromano/logger.h"
#include "libromano/simd.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#define MATMUL_SIZE_M 1578
#define MATMUL_SIZE_N 1578

#define M_CHOL 4

#define DEBUG_SIZE 4

#define EPSILON 0.01f

int main(void)
{
    size_t i;
    size_t j;

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

    simd_force_vectorization_mode(VectorizationMode_Scalar);
    SCOPED_PROFILE_START_SECONDS(matrixf_scalar_mul);

    MatrixF C_scalar = matrix_null();
    matrixf_mul(&A, &B, &C_scalar);

    SCOPED_PROFILE_END_SECONDS(matrixf_scalar_mul);

    matrixf_debug(&C_scalar, DEBUG_SIZE, DEBUG_SIZE);

    simd_force_vectorization_mode(VectorizationMode_SSE);
    SCOPED_PROFILE_START_SECONDS(matrixf_sse_mul);

    MatrixF C_sse = matrix_null();
    matrixf_mul(&A, &B, &C_sse);

    SCOPED_PROFILE_END_SECONDS(matrixf_sse_mul);

    simd_force_vectorization_mode(VectorizationMode_AVX);
    SCOPED_PROFILE_START_SECONDS(matrixf_avx_mul);

    MatrixF C_avx = matrix_null();
    matrixf_mul(&A, &B, &C_avx);

    SCOPED_PROFILE_END_SECONDS(matrixf_avx_mul);

    float sse_err = 0.0f;
    float avx_err = 0.0f;   
    uint32_t count = 1;

    for(i = 0; i < MATMUL_SIZE_M; i++)
    {
        for(j = 0; j < MATMUL_SIZE_M; j++)
        {
            const float scalar = matrixf_get_at(&C_scalar, i, j);
            const float sse = matrixf_get_at(&C_sse, i, j);
            const float avx = matrixf_get_at(&C_avx, i, j);

            sse_err = mathf_lerp(sse_err, mathf_abs(scalar - sse), 1.0f / (float)count);
            avx_err = mathf_lerp(avx_err, mathf_abs(scalar - avx), 1.0f / (float)count);
            count++;
        }
    }

    logger_log(LogLevel_Info, "Average sse_err: %f", sse_err);
    logger_log(LogLevel_Info, "Average avx_err: %f", avx_err);

    if(sse_err > EPSILON)
    {
        logger_log(LogLevel_Error, "Average err is too high between sse and scalar matmul");
        return 1;
    }

    if(avx_err > EPSILON)
    {
        logger_log(LogLevel_Error, "Average err is too high between avx and scalar matmul");
        return 1;
    }

    matrixf_destroy(&A);
    matrixf_destroy(&B);
    matrixf_destroy(&C_scalar);
    matrixf_destroy(&C_sse);
    matrixf_destroy(&C_avx);

    logger_log(LogLevel_Info, "Cholesky Solving");

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
    {
        matrixf_set_at(&b, (float)i + 1.0f, i, 0);
    }

    MatrixF _at = matrixf_copy(&_a);
    matrixf_transpose(&_at);

    MatrixF a = matrix_null();
    matrixf_mul(&_a, &_at, &a);

    matrixf_destroy(&_a);
    matrixf_destroy(&_at);

    logger_log(LogLevel_Info, "A");
    matrixf_debug(&a, 0, 0);

    MatrixF x = matrix_null();

    bool res = matrixf_cholesky_solve(&a, &b, &x);

    if(!res)
    {
        logger_log(LogLevel_Error, "Cannot solve linear system with Cholesky Decomposition: %u", res);

        matrixf_destroy(&a);
        matrixf_destroy(&b);

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