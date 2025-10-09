/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/math/linalg32.h"
#include "libromano/memory.h"
#include "libromano/simd.h"
#include "libromano/logger.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>

/* MATRIX */

#define GET_AT(A, i, j) (A.data[i * A.N + j])
#define SET_AT(A, value, i, j) (A.data[i * A.N + j] = value)

#define GET_AT_WITH_N(A, N, i, j) (A.data[i * N + j])
#define SET_AT_WITH_N(A, N, value, i, j) (A.data[i * N + j] = value)

#define SWAP_FLOAT(f1, f2) do { float tmp = f1; f1 = f2; f2 = tmp; } while (0)

#define ALIGNMENT 32

/* 
    M -> rows
    N -> columns
*/

MatrixF matrix_null()
{
    MatrixF A;
    A.data = NULL;
    A.N = 0;
    A.M = 0;

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
    {
        mem_aligned_free(A->data);
    }

    A->data = (float*)mem_aligned_alloc((M * N) * sizeof(float), ALIGNMENT);
    A->M = M;
    A->N = N;
}

int matrixf_row_size(MatrixF* A)
{
    if(A->data)
    {
        return A->M;
    }

    return 0;
}

int matrixf_column_size(MatrixF* A)
{
    if(A->data)
    {
        return A->N;
    }

    return 0;
}

void matrixf_set_at(MatrixF* A, const float value, const int i, const int j)
{
    A->data[i * A->N + j] = value;
}

float matrixf_get_at(MatrixF* A, const int i, const int j)
{
    return A->data[i * A->N + j];
}

float matrixf_trace(MatrixF* A)
{
    return 0.0f;
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
        {
            for(j = 0; j < N; j++) 
            {
                SWAP_FLOAT(A->data[i * M + j], A->data[j * M + i]);
            }
        }
    }
    else 
    {
        new_data = (float*)mem_aligned_alloc((N * M) * sizeof(float), ALIGNMENT);

        for(i = 0; i < M; i++)
        {
            for(j = 0; j < N; j++)
            {
                new_data[j * M + i] = A->data[i * N + j];
            }
        }

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
    {
        for(j = 0; j < N; j++)
        {
            res.data[i * N + j] = A->data[j * M + i];
        }
    }

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
            {
                sum += A[i * N + k] * B[j * N + k];
            }

            C[i * P + j] = sum;
        }
    }
}

#define M_BLOCK_SIZE 4
#define SSE_N_BLOCK_SIZE 4
#define AVX_N_BLOCK_SIZE 8

void _matrixf_mul_sse(const float* ROMANO_RESTRICT A, 
                      const float* ROMANO_RESTRICT B,
                      float* ROMANO_RESTRICT C,
                      const uint32_t M,
                      const uint32_t N,
                      const uint32_t P)
{
    float sum1, sum2, sum3, sum4;

    __m128 a1_sse, a2_sse, a3_sse, a4_sse;
    __m128 b_sse;
    __m128 sse_sum1, sse_sum2, sse_sum3, sse_sum4;

    uint32_t i;
    uint32_t j;
    uint32_t k;

    const uint32_t m_blocks = M - (M % M_BLOCK_SIZE);
    const uint32_t n_blocks = N - (N % SSE_N_BLOCK_SIZE);

    for(i = 0; i < m_blocks; i += M_BLOCK_SIZE)
    {
        for(j = 0; j < P; j++)
        {
            sse_sum1 = _mm_setzero_ps();
            sse_sum2 = _mm_setzero_ps();
            sse_sum3 = _mm_setzero_ps();
            sse_sum4 = _mm_setzero_ps();

            for(k = 0; k < n_blocks; k += SSE_N_BLOCK_SIZE)
            {
                a1_sse = _mm_load_ps(&A[i * N + k]);
                a2_sse = _mm_load_ps(&A[(i + 1) * N + k]);
                a3_sse = _mm_load_ps(&A[(i + 2) * N + k]);
                a4_sse = _mm_load_ps(&A[(i + 3) * N + k]);

                b_sse = _mm_loadu_ps(&B[j * N + k]);

                sse_sum1 = _mm_fmadd_ps(a1_sse, b_sse, sse_sum1);
                sse_sum2 = _mm_fmadd_ps(a2_sse, b_sse, sse_sum2);
                sse_sum3 = _mm_fmadd_ps(a3_sse, b_sse, sse_sum3);
                sse_sum4 = _mm_fmadd_ps(a4_sse, b_sse, sse_sum4);
            }

            sum1 = _mm_hsum_ps(sse_sum1);
            sum2 = _mm_hsum_ps(sse_sum2);
            sum3 = _mm_hsum_ps(sse_sum3);
            sum4 = _mm_hsum_ps(sse_sum4);

            for(k = n_blocks; k < N; k++)
            {
                sum1 += A[i * N + k] * B[j * N + k];
                sum2 += A[(i + 1) * N + k] * B[j * N + k];
                sum3 += A[(i + 2) * N + k] * B[j * N + k];
                sum4 += A[(i + 3) * N + k] * B[j * N + k];
            }

            C[i * P + j] = sum1;
            C[(i + 1) * P + j] = sum2;
            C[(i + 2) * P + j] = sum3;
            C[(i + 3) * P + j] = sum4;
        }
    }

    for(i = m_blocks; i < M; i++)
    {
        for(j = 0; j < P; j++)
        {
            sse_sum1 = _mm_setzero_ps();

            for(k = 0; k < n_blocks; k += SSE_N_BLOCK_SIZE)
            {
                a1_sse = _mm_load_ps(&A[i * N + k]);
                b_sse = _mm_load_ps(&B[j * N + k]);

                sse_sum1 = _mm_fmadd_ps(a1_sse, b_sse, sse_sum1);
            }

            sum1 = _mm_hsum_ps(sse_sum1);

            for(k = n_blocks; k < N; k++)
            {
                sum1 += A[i * N + k] * B[j * N + k];
            }

            C[i * P + j] = sum1;
        }
    }
}

void _matrixf_mul_avx2(const float* ROMANO_RESTRICT A, 
                       const float* ROMANO_RESTRICT B,
                       float* ROMANO_RESTRICT C,
                       const uint32_t M,
                       const uint32_t N,
                       const uint32_t P)
{
    float sum1, sum2, sum3, sum4;

    __m256 a1_avx, a2_avx, a3_avx, a4_avx;
    __m256 b_avx;
    __m256 avx_sum1, avx_sum2, avx_sum3, avx_sum4;

    uint32_t i;
    uint32_t j;
    uint32_t k;

    const uint32_t m_blocks = M - (M % M_BLOCK_SIZE);
    const uint32_t n_blocks = N - (N % AVX_N_BLOCK_SIZE);

    for(i = 0; i < m_blocks; i += M_BLOCK_SIZE)
    {
        for(j = 0; j < P; j++)
        {
            avx_sum1 = _mm256_setzero_ps();
            avx_sum2 = _mm256_setzero_ps();
            avx_sum3 = _mm256_setzero_ps();
            avx_sum4 = _mm256_setzero_ps();

            for(k = 0; k < n_blocks; k += AVX_N_BLOCK_SIZE)
            {
                a1_avx = _mm256_load_ps(&A[i * N + k]);
                a2_avx = _mm256_load_ps(&A[(i + 1) * N + k]);
                a3_avx = _mm256_load_ps(&A[(i + 2) * N + k]);
                a4_avx = _mm256_load_ps(&A[(i + 3) * N + k]);

                b_avx = _mm256_loadu_ps(&B[j * N + k]);

                avx_sum1 = _mm256_fmadd_ps(a1_avx, b_avx, avx_sum1);
                avx_sum2 = _mm256_fmadd_ps(a2_avx, b_avx, avx_sum2);
                avx_sum3 = _mm256_fmadd_ps(a3_avx, b_avx, avx_sum3);
                avx_sum4 = _mm256_fmadd_ps(a4_avx, b_avx, avx_sum4);
            }

            sum1 = _mm256_hsum_ps(avx_sum1);
            sum2 = _mm256_hsum_ps(avx_sum2);
            sum3 = _mm256_hsum_ps(avx_sum3);
            sum4 = _mm256_hsum_ps(avx_sum4);

            for(k = n_blocks; k < N; k++)
            {
                sum1 += A[i * N + k] * B[j * N + k];
                sum2 += A[(i + 1) * N + k] * B[j * N + k];
                sum3 += A[(i + 2) * N + k] * B[j * N + k];
                sum4 += A[(i + 3) * N + k] * B[j * N + k];
            }

            C[i * P + j] = sum1;
            C[(i + 1) * P + j] = sum2;
            C[(i + 2) * P + j] = sum3;
            C[(i + 3) * P + j] = sum4;
        }
    }

    for(i = m_blocks; i < M; i++)
    {
        for(j = 0; j < P; j++)
        {
            avx_sum1 = _mm256_setzero_ps();

            for(k = 0; k < n_blocks; k += AVX_N_BLOCK_SIZE)
            {
                a1_avx = _mm256_load_ps(&A[i * N + k]);
                b_avx = _mm256_load_ps(&B[j * N + k]);

                avx_sum1 = _mm256_fmadd_ps(a1_avx, b_avx, avx_sum1);
            }

            sum1 = _mm256_hsum_ps(avx_sum1);

            for(k = n_blocks; k < N; k++)
            {
                sum1 += A[i * N + k] * B[j * N + k];
            }

            C[i * P + j] = sum1;
        }
    }
}

typedef void (*matmul_func)(const float* ROMANO_RESTRICT, 
                            const float* ROMANO_RESTRICT,
                            float* ROMANO_RESTRICT,
                            const uint32_t,
                            const uint32_t,
                            const uint32_t);

matmul_func __matmul_funcs[3] = {
    _matrixf_mul_scalar,
    _matrixf_mul_sse,
    _matrixf_mul_avx2,
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
        B_t = matrixf_transpose_from(B);

        __matmul_funcs[simd_get_vectorization_mode()](A->data, B_t.data, C->data, M, N, P);

        matrixf_destroy(&B_t);
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
    {
        for(j = 0; j < N; j++)
        {
            GET_AT_WITH_N((*A), N, i, j) += f;
        }
    }
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
    {
        for(j = 0; j < N; j++)
        {
            GET_AT_WITH_N((*A), N, i, j) -= f;
        }
    }
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
    {
        for(j = 0; j < N; j++)
        {
            GET_AT_WITH_N((*A), N, i, j) *= f;
        }
    }
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
    {
        for(j = 0; j < N; j++)
        {
            GET_AT_WITH_N((*A), N, i, j) /= f;
        }
    }
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
        {
            printf(j == (N - 1) ? "%.3f" : "%.3f ", GET_AT_WITH_N((*A), N, i, j));             
        }

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
        {
            printf("... ");
        }

        for(j = (N - max_columns_to_print); j < N; j++)
        {
            const float v = matrixf_get_at(A, i, j);
            printf("%.3f ", v);
        }

        printf("\n");
    }

    if(i < (M / 2))
    {
        printf("...\n");
    }

    for(i = (M - max_rows_to_print); i < M; i++)
    {
        for(j = 0; j < N && j < max_columns_to_print; j++)
        {
            const float v = matrixf_get_at(A, i, j);
            printf("%.3f ", v);
        }

        if(j < (N / 2))
        {
            printf("... ");
        }

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
    {
        _matrixf_debug_full(A, M, N);
    }
    else
    {
        _matrixf_debug_limited(A, M, N, max_rows, max_columns);
    }
}

void matrixf_destroy(MatrixF* A)
{
    if(A->data != NULL)
    {
        mem_aligned_free(A->data);
        A->data = NULL;
    }
}

bool matrixf_cholesky_decomposition(MatrixF* A, MatrixF* L)
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
            {
                sum += GET_AT_WITH_N((*L), N, i, k) * GET_AT_WITH_N((*L), N, j, k);
            }

            if(i == j)
            {
                tmp = GET_AT_WITH_N((*A), N, i, i) - sum;

                if(tmp <= 0.0f)
                {
                    logger_log(LogLevel_Error, "Cholesky Decomposition failed: non-positive definite matrix");
                    logger_log(LogLevel_Error, "Problem at diag %u: %f", i, tmp);
                    return false;
                }

                value = mathf_sqrt(tmp);

                SET_AT_WITH_N((*L), N, value, i, j);
            }
            else
            {
                tmp = GET_AT_WITH_N((*A), N, j, j);

                // if(mathf_float_eq(tmp, 0.0f))
                // {
                //     return false;
                // }

                value = (1.0f / GET_AT_WITH_N((*L), N, j, j) * (GET_AT_WITH_N((*A), N, i, j) - sum));

                SET_AT_WITH_N((*L), N, value, i, j);
            }
        }
    }

    return true;
}

bool matrixf_cholesky_solve(MatrixF* A, MatrixF* b, MatrixF* x)
{
    uint32_t i;
    uint32_t j;
    uint32_t k;

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

    if(!matrixf_cholesky_decomposition(A, &L))
    {
        logger_log(LogLevel_Error, "Cholesky Solve failed: cannot decompose matrix");
        matrixf_destroy(&L);
        return false;
    }

    /* Ly = b */

    b_m = b->M;
    b_n = b->N;

    y = matrixf_create(b_m, b_n);

    for(i = 0; i < b_n; i++)
    {
        for(j = 0; j < N; j++)
        {
            tmp = GET_AT_WITH_N((*b), b_n, j, i);
            SET_AT_WITH_N(y, b_n, tmp, j, i);

            for(k = 0; k < j; k++)
            {
                value = GET_AT_WITH_N(y, b_n, j, i) - GET_AT_WITH_N(L, N, j, k) * GET_AT_WITH_N(y, b_n, k, i);
                SET_AT_WITH_N(y, b_n, value, j, i);
            }

            value = GET_AT_WITH_N(y, b_n, j, i) / GET_AT_WITH_N(L, N, j, j);
            SET_AT_WITH_N(y, b_n, value, j, i);
        }
    }

    /* L*x = y */

    matrixf_resize(x, b_m, b_n);

    matrixf_transpose(&L);

    for(i = 0; i < b_n; i++)
    {
        for(j2 = (N - 1); j2 >= 0; j2--)
        {
            sum = 0.0f;

            for(k = j2 + 1; k < N; k++)
            {
                sum += GET_AT_WITH_N(L, N, j2, k) * GET_AT_WITH_N((*x), b_n, k, i);
            }

            value = (GET_AT_WITH_N(y, b_n, j2, i) - sum) / GET_AT_WITH_N(L, N, j2, j2);

            SET_AT_WITH_N((*x), b_n, value, j2, i);
        }
    }

    matrixf_destroy(&y);
    matrixf_destroy(&L);

    return true;
}