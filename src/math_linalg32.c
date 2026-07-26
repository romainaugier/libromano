/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/math/linalg32.h"
#include "libromano/memory.h"
#include "libromano/simd.h"
#include "libromano/logger.h"

#if defined(ROMANO_AARCH64)
#if defined(ROMANO_APPLE)
#include <Accelerate/Accelerate.h>
#endif /* defined(ROMANO_APPLE) */
#endif /* defined(ROMANO_AARCH64) */

#include <string.h>
#include <assert.h>
#include <stdio.h>

/* MATRIX */

#define SWAP_FLOAT(f1, f2) do { float tmp = f1; f1 = f2; f2 = tmp; } while (0)

#define ALIGNMENT 32

/*
    MatrixF stores data in a row major format

    M -> rows
    N -> columns
*/

MatrixF matrix_null(void)
{
    MatrixF A;
    A.data = NULL;
    A.M = 0;
    A.N = 0;

    return A;
}

MatrixF matrixf_create(const int M, const int N)
{
    MatrixF A;

    A.data = (float*)mem_aligned_alloc((M * N) * sizeof(float), ALIGNMENT);
    A.M = M;
    A.N = N;

    return A;
}

MatrixF matrixf_copy(MatrixF* A)
{
    MatrixF B;

    const size_t size = (A->M * A->N) * sizeof(float);

    B.data = (float*)mem_aligned_alloc(size, ALIGNMENT);
    B.M = A->M;
    B.N = A->N;

    memcpy(B.data, A->data, size);

    return B;
}

void matrixf_size(MatrixF* A, int* M, int* N)
{
    if(A->data != NULL)
    {
        *M = A->M;
        *N = A->N;
    }
}

void matrixf_resize(MatrixF* A, const int M, const int N)
{
    if(A->data != NULL)
        mem_aligned_free(A->data);

    A->data = (float*)mem_aligned_alloc((M * N) * sizeof(float), ALIGNMENT);
    A->M = M;
    A->N = N;
}

int matrixf_row_size(MatrixF* A)
{
    if(A->data != NULL)
        return A->M;

    return 0;
}

int matrixf_column_size(MatrixF* A)
{
    if(A->data != NULL)
        return A->N;

    return 0;
}

void matrixf_set_at(MatrixF* A, const float value, const int i, const int j)
{
    A->data[i * A->M + j] = value;
}

float matrixf_get_at(MatrixF* A, const int i, const int j)
{
    return A->data[i * A->M + j];
}

float matrixf_trace(MatrixF* A)
{
    size_t i;
    float t;

    if(A->M != A->N)
        return 0.0f;

    t = 0.0f;

    for(i = 0; i < A->M; i++)
        t += A->data[i * A->M + i];

    return t;
}

void matrixf_zero(MatrixF* A)
{
    memset(A->data, 0, A->M * A->N * sizeof(float));
}

void matrixf_transpose(MatrixF* A)
{
    uint32_t i;
    uint32_t j;

    float* new_data;

    const int M = A->M;
    const int N = A->N;

    if(M == N)
    {
        for(i = 0; i < M; i++)
            for(j = 0; j < N; j++)
                SWAP_FLOAT(A->data[i * M + j], A->data[j * M + i]);
    }
    else
    {
        new_data = (float*)mem_aligned_alloc((N * M) * sizeof(float), ALIGNMENT);

        if(new_data == NULL)
            return;

        for(i = 0; i < M; i++)
            for(j = 0; j < N; j++)
                new_data[j * N + i] = A->data[i * M + j];

        mem_aligned_free(A->data);

        A->data = new_data;
    }

    A->M = M;
    A->N = N;
}

MatrixF matrixf_transpose_from(MatrixF* A)
{
    uint32_t i;
    uint32_t j;

    const int M = A->M;
    const int N = A->N;

    MatrixF res = matrixf_create(N, M);

    for(i = 0; i < M; i++)
        for(j = 0; j < N; j++)
            res.data[j * N + i] = A->data[i * M + j];

    return res;
}

void _matrixf_mul_scalar(const float* ROMANO_RESTRICT A,
                         const float* ROMANO_RESTRICT B,
                         float* ROMANO_RESTRICT C,
                         const uint32_t M,
                         const uint32_t N,
                         const uint32_t P)
{
    float sum;

    uint32_t i;
    uint32_t j;
    uint32_t k;

    for(i = 0; i < M; i++)
    {
        for(j = 0; j < P; j++)
        {
            sum = 0.0f;

            for(k = 0; k < N; k++)
                sum += A[i * N + k] * B[k * P + j];

            C[i * P + j] = sum;
        }
    }
}

#if defined(ROMANO_X86_64)

#define NUM_MATRIXF_MUL_FUNCS 3

void _matrixf_mul_sse(const float* ROMANO_RESTRICT A,
                      const float* ROMANO_RESTRICT B,
                      float* ROMANO_RESTRICT C,
                      const uint32_t M,
                      const uint32_t N,
                      const uint32_t P)
{
    _matrixf_mul_scalar(A, B, C, M, N, P);
}

void _matrixf_mul_avx2(const float* ROMANO_RESTRICT A,
                       const float* ROMANO_RESTRICT B,
                       float* ROMANO_RESTRICT C,
                       const uint32_t M,
                       const uint32_t N,
                       const uint32_t P)
{
    _matrixf_mul_scalar(A, B, C, M, N, P);
}

#elif defined(ROMANO_AARCH64) || defined(ROMANO_APPLE)

#define NUM_MATRIXF_MUL_FUNCS 2

void _matrixf_mul_accelerate(const float* ROMANO_RESTRICT A,
                             const float* ROMANO_RESTRICT B,
                             float* ROMANO_RESTRICT C,
                             const uint32_t M,
                             const uint32_t N,
                             const uint32_t P)
{
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                M, P, N,          /* m, n, k */
                1.0f,
                A, N,             /* lda = cols of A */
                B, P,             /* ldb = cols of B */
                0.0f,
                C, P);            /* ldc = cols of C */
}

#else

#define NUM_MATRIXF_MUL_FUNCS 1

#endif /* defined(ROMANO_X86_64) */

typedef void (*matmul_func)(const float* ROMANO_RESTRICT,
                            const float* ROMANO_RESTRICT,
                            float* ROMANO_RESTRICT,
                            const uint32_t,
                            const uint32_t,
                            const uint32_t);

matmul_func __matmul_funcs[NUM_MATRIXF_MUL_FUNCS] = {
    _matrixf_mul_scalar,
#if defined(ROMANO_X86_64)
    _matrixf_mul_sse,
    _matrixf_mul_avx2,
#elif defined(ROMANO_AARCH64) || defined(ROMANO_APPLE)
    _matrixf_mul_accelerate,
#endif /* defined(ROMANO_X86_64) */
};

void matrixf_mul(MatrixF* A, MatrixF* B, MatrixF* C)
{
    uint32_t M;
    uint32_t N;
    uint32_t P;

    MatrixF B_t;

    float sum;

    ROMANO_ASSERT(A->N == B->M, "");

    M = A->M;
    N = A->N;
    P = B->N;

    matrixf_resize(C, M, P);
    matrixf_zero(C);

    if(M >= 8)
    {
#if defined(ROMANO_X86_64)
        B_t = matrixf_transpose_from(B);

        __matmul_funcs[simd_get_vectorization_mode()](A->data, B_t.data, C->data, M, N, P);

        matrixf_destroy(&B_t);
#else
        __matmul_funcs[simd_get_vectorization_mode()](A->data, B->data, C->data, M, N, P);
#endif /* defined(ROMANO_X86_64) */
    }
    else
    {
        _matrixf_mul_scalar(A->data, B->data, C->data, M, N, P);
    }
}

void _matrixf_add_f_scalar(MatrixF* A, const float f, const uint32_t M, const uint32_t N)
{
    uint32_t i;
    uint32_t j;

    for(i = 0; i < M; i++)
        for(j = 0; j < N; j++)
            A->data[i * M + j] += f;
}

void matrixf_add_f(MatrixF* A, float f)
{
    const uint32_t M = A->M;
    const uint32_t N = A->N;

    _matrixf_add_f_scalar(A, f, M, N);
}

void _matrixf_sub_f_scalar(MatrixF* A, const float f, const uint32_t M, const uint32_t N)
{
    uint32_t i;
    uint32_t j;

    for(i = 0; i < M; i++)
        for(j = 0; j < N; j++)
            A->data[i * M + j] -= f;
}

void matrixf_sub_f(MatrixF* A, float f)
{
    const uint32_t M = A->M;
    const uint32_t N = A->N;

    _matrixf_sub_f_scalar(A, f, M, N);
}

void _matrixf_mul_by_f_scalar(MatrixF* A, const float f, const uint32_t M, const uint32_t N)
{
    uint32_t i;
    uint32_t j;

    for(i = 0; i < M; i++)
        for(j = 0; j < N; j++)
            A->data[i * M + j] *= f;
}

void matrixf_mul_by_f(MatrixF* A, float f)
{
    const uint32_t M = A->M;
    const uint32_t N = A->N;

    _matrixf_mul_by_f_scalar(A, f, M, N);
}

void _matrixf_div_by_f_scalar(MatrixF* A, const float f, const uint32_t M, const uint32_t N)
{
    uint32_t i;
    uint32_t j;

    for(i = 0; i < M; i++)
        for(j = 0; j < N; j++)
            A->data[i * M + j] /= f;
}

void matrixf_div_by_f(MatrixF* A, float f)
{
    const uint32_t M = A->M;
    const uint32_t N = A->N;

    _matrixf_div_by_f_scalar(A, f, M, N);
}

void _matrixf_debug_full(MatrixF* A, const uint32_t M, const uint32_t N)
{
    uint32_t i;
    uint32_t j;

    for(i = 0; i < M; i++)
    {
        for(j = 0; j < N; j++)
            printf(j == (N - 1) ? "%.3f" : "%.3f ", A->data[i * M + j]);

        printf("\n");
    }
}

void _matrixf_debug_limited(MatrixF* A, const uint32_t M, const uint32_t N, const uint32_t max_M, const uint32_t max_N)
{
    uint32_t i;
    uint32_t j;

    const uint32_t max_rows_to_print = (max_M - (max_M % 2)) / 2;
    const uint32_t max_columns_to_print = (max_M - (max_M % 2)) / 2;

    for(i = 0; i < M && i < max_rows_to_print; i++)
    {
        for(j = 0; j < N && j < max_columns_to_print; j++)
        {
            const float v = matrixf_get_at(A, i, j);
            printf("%.3f ", v);
        }

        if(j < (N / 2))
            printf("... ");

        for(j = (N - max_columns_to_print); j < N; j++)
        {
            const float v = matrixf_get_at(A, i, j);
            printf("%.3f ", v);
        }

        printf("\n");
    }

    if(i < (M / 2))
        printf("...\n");

    for(i = (M - max_rows_to_print); i < M; i++)
    {
        for(j = 0; j < N && j < max_columns_to_print; j++)
        {
            const float v = matrixf_get_at(A, i, j);
            printf("%.3f ", v);
        }

        if(j < (N / 2))
            printf("... ");

        for(j = (N - max_columns_to_print); j < N; j++)
        {
            const float v = matrixf_get_at(A, i, j);
            printf("%.3f ", v);
        }

        printf("\n");
    }
}

void matrixf_debug(MatrixF* A, uint32_t max_rows, uint32_t max_columns)
{
    const uint32_t M = A->M;
    const uint32_t N = A->N;

    printf("Matrix f32: %u x %u\n", M, N);

    if(max_rows == 0 || max_columns == 0)
        _matrixf_debug_full(A, M, N);
    else
        _matrixf_debug_limited(A, M, N, max_rows, max_columns);
}

void matrixf_destroy(MatrixF* A)
{
    if(A->data != NULL)
    {
        mem_aligned_free(A->data);
        A->data = NULL;
    }
}

bool _matrixf_cholesky_decomposition_scalar(MatrixF* A, MatrixF* L)
{
    uint32_t i;
    uint32_t j;
    uint32_t k;

    float sum;
    float value;
    float tmp;

    const uint32_t N = A->N;

    if(A->M != A->N)
    {
        logger_log(LogLevel_Error, "Cholesky Decomposition failed: non-square matrix");
        return false;
    }

    matrixf_resize(L, N, N);
    matrixf_zero(L);

    for(i = 0; i < N; i++)
    {
        for(j = 0; j <= i; j++)
        {
            sum = 0.0f;

            for(k = 0; k < j; k++)
                sum += L->data[i * N + k] * L->data[j * N + k];

            if(i == j)
            {
                tmp = A->data[i * N + i] - sum;

                if(tmp <= 0.0f)
                {
                    logger_log(LogLevel_Error, "Cholesky Decomposition failed: non-positive definite matrix");
                    logger_log(LogLevel_Error, "Problem at diag %u: %f", i, tmp);
                    return false;
                }

                value = mathf_sqrt(tmp);

                L->data[i * N + j] = value;
            }
            else
            {
                tmp = A->data[j * N + j];

                // if(mathf_float_eq(tmp, 0.0f))
                // {
                //     return false;
                // }

                value = (A->data[i * N + j] - sum) / L->data[j * N + j];

                L->data[i * N + j] = value;
            }
        }
    }

    return true;
}

bool _matrixf_cholesky_solve_scalar(MatrixF* A, MatrixF* b, MatrixF* x)
{
    uint32_t i;
    uint32_t j;
    uint32_t k;

    int32_t i2;
    int32_t j2;

    uint32_t b_n;
    uint32_t b_m;

    float sum;
    float value;
    float tmp;

    MatrixF L = matrix_null();
    MatrixF y = matrix_null();

    const uint32_t N = A->N;

    if(A->M != A->N)
    {
        logger_log(LogLevel_Error, "Cholesky Solve failed: non-square matrix");
        return false;
    }

    if(!_matrixf_cholesky_decomposition_scalar(A, &L))
    {
        logger_log(LogLevel_Error, "Cholesky Solve failed: cannot decompose matrix");
        matrixf_destroy(&L);
        return false;
    }

    /* Ly = b */

    b_m = b->M;
    b_n = b->N;

    /* Forward substitution: L y = b  (L is lower triangular) */
    y = matrixf_create(b_m, b_n);

    for (i = 0; i < N; i++)            /* row of the system */
    {
        for (j = 0; j < b_n; j++)      /* column of RHS */
        {
            sum = 0.0f;

            for (k = 0; k < i; k++)
                sum += L.data[i * N + k] * y.data[k * b_n + j];

            y.data[i * b_n + j] = (b->data[i * b_n + j] - sum) / L.data[i * N + i];
        }
    }

    /* Back substitution: L^T x = y (L^T is upper triangular) */
    matrixf_resize(x, b_m, b_n);

    for(i2 = (int32_t)N - 1; i2 >= 0; i2--)
    {
        for (j = 0; j < b_n; j++)
        {
            sum = 0.0f;

            for (k = i2 + 1; k < N; k++)
                sum += L.data[k * N + i2] * x->data[k * b_n + j];   /* L^T[i][k] = L[k][i] */

            x->data[i2 * b_n + j] = (y.data[i2 * b_n + j] - sum) / L.data[i2 * N + i2];
        }
    }

    matrixf_destroy(&y);
    matrixf_destroy(&L);

    return true;
}

#if defined(ROMANO_X86_64)

#define NUM_CHOL_SOLVE_FUNCS 3

bool _matrixf_cholesky_solve_sse(MatrixF* A, MatrixF* b, MatrixF* x)
{
    return _matrixf_cholesky_solve_scalar(A, b, x);
}

bool _matrixf_cholesky_solve_avx2(MatrixF* A, MatrixF* b, MatrixF* x)
{
    return _matrixf_cholesky_solve_scalar(A, b, x);
}
#elif defined(ROMANO_AARCH64) || defined(ROMANO_APPLE)

#define NUM_CHOL_SOLVE_FUNCS 2

bool _matrixf_cholesky_solve_accelerate(MatrixF* A, MatrixF* b, MatrixF* x)
{
    __LAPACK_int n = (__LAPACK_int)A->N;
    __LAPACK_int nrhs = (__LAPACK_int)b->N;
    __LAPACK_int info = 0;

    /* LAPACK is column-major; a symmetric matrix is its own transpose,
       so row-major A == column-major A^T == column-major A. Same for
       the RHS as long as we treat 'uplo' consistently. */

    matrixf_resize(x, b->M, b->N);
    memcpy(x->data, b->data, b->M * b->N * sizeof(float));

    /* Copy A because sposv destroys it (overwrites with L) */
    MatrixF Ac = matrixf_copy(A);

    sposv_("L",          /* lower triangle; symmetric so row/col-major doesn't matter */
           &n,
           &nrhs,
           Ac.data,
           &n,
           x->data,
           &n,  /* b overwritten with the solution */
           &info);

    matrixf_destroy(&Ac);

    if(info != 0)
    {
        logger_log(LogLevel_Error, "Cholesky solve failed: info=%d (not positive definite)", (int)info);
        return false;
    }

    return true;
}
#else
#define NUM_CHOL_SOLVE_FUNCS 1
#endif /* defined(ROMANO_X86_64) */

typedef bool (*cholesky_solve_func)(MatrixF*,MatrixF*,MatrixF*);

cholesky_solve_func __cholesky_solver_funcs[NUM_CHOL_SOLVE_FUNCS] = {
    _matrixf_cholesky_solve_scalar,
#if defined(ROMANO_X86_64)
    _matrixf_cholesky_solve_sse,
    _matrixf_cholesky_solve_avx2,
#elif defined(ROMANO_AARCH64) || defined(ROMANO_APPLE)
    _matrixf_cholesky_solve_accelerate,
#endif /* defined(ROMANO_X86_64) */
};

bool matrixf_cholesky_solve(MatrixF* A, MatrixF* b, MatrixF* x)
{
    return __cholesky_solver_funcs[simd_get_vectorization_mode()](A, b, x);
}
