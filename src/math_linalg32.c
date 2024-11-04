/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/math/linalg32.h"
#include "libromano/memory.h"
#include "libromano/cpu.h"
#include "libromano/simd.h"
#include "libromano/logger.h"

#include <string.h>
#include <immintrin.h>
#include <assert.h>

/* MATRIX */

#define SIZE_N(A) ((int)A.data[0])
#define SIZE_M(A) ((int)A.data[1])
#define SET_SIZE_N(A, N) A.data[0] = (float)N
#define SET_SIZE_M(A, M) A.data[1] = (float)M

#define GET_AT(A, i, j) (A.data[i * SIZE_N(A) + j + 2])
#define SET_AT(A, value, i, j) (A.data[i * SIZE_N(A) + j + 2] = value)

#define GET_PTR(A) ((float*)&(A.data[2]))

#define GET_AT_WITH_N(A, N, i, j) (A.data[i * N + j + 2])
#define SET_AT_WITH_N(A, N, value, i, j) (A.data[i * N + j + 2] = value)

#define SWAP_FLOAT(f1, f2) do { float tmp = f1; f1 = f2; f2 = tmp; } while (0)

#define ALIGNMENT simd_has_avx() ? 32 : 16

/* 
    M -> rows
    N -> columns
*/

matrixf_t matrix_null()
{
    matrixf_t A;
    A.data = NULL;

    return A;
}

matrixf_t matrixf_create(const int M, const int N)
{
    matrixf_t A;

    A.data = (float*)mem_aligned_alloc((M * N + 2) * sizeof(float), ALIGNMENT);
    SET_SIZE_M(A, M);
    SET_SIZE_N(A, N);

    return A;
}

matrixf_t matrixf_copy(matrixf_t* A)
{
    matrixf_t B;

    const uint64_t size = (SIZE_M((*A)) * SIZE_N((*A)) + 2) * sizeof(float);

    B.data = (float*)mem_aligned_alloc(size, ALIGNMENT);
    memcpy(B.data, A->data, size);

    return B;
}

void matrixf_size(matrixf_t* A, int* M, int* N)
{
    if(A->data != NULL)
    {
        *M = SIZE_M((*A));
        *N = SIZE_N((*A));
    }
}

void matrixf_resize(matrixf_t* A, const int M, const int N)
{
    if(A->data != NULL)
    {
        mem_aligned_free(A->data);
    }

    A->data = (float*)mem_aligned_alloc((M * N + 2) * sizeof(float), ALIGNMENT);
    SET_SIZE_M((*A), M);
    SET_SIZE_N((*A), N);
}

int matrixf_row_size(matrixf_t* A)
{
    if(A->data)
    {
        return SIZE_M((*A));
    }

    return 0;
}

int matrixf_column_size(matrixf_t* A)
{
    if(A->data)
    {
        return SIZE_N((*A));
    }

    return 0;
}

void matrixf_set_at(matrixf_t* A, const float value, const int i, const int j)
{
    A->data[i * SIZE_N((*A)) + j + 2] = value;
}

float matrixf_get_at(matrixf_t* A, const int i, const int j)
{
    return A->data[i * SIZE_N((*A)) + j + 2];
}

float matrixf_trace(matrixf_t* A)
{
    return 0.0f;
}

void matrixf_zero(matrixf_t* A)
{
    memset(A->data + 2, 0, SIZE_M((*A)) * SIZE_N((*A)) * sizeof(float));
}

void matrixf_transpose(matrixf_t* A)
{
    const int M = SIZE_M((*A));
    const int N = SIZE_N((*A));

    if(M == N)
    {
        for(int i = 0; i < M; i++)
        {
            for(int j = (i + 1); j < M; j++)
            {
                SWAP_FLOAT(A->data[i * M + j + 2], A->data[j * M + i + 2]);
            }
        }
    }
    else
    {
        float* new_data = (float*)mem_aligned_alloc((N * M + 2) * sizeof(float), sizeof(float));

        for(int i = 0; i < M; i++)
        {
            for(int j = 0; j < N; j++)
            {
                new_data[j * N + i + 2] = A->data[i * N + j + 2];
            }
        }

        mem_aligned_free(A->data);

        A->data = new_data;
    }

    SET_SIZE_N((*A), N);
    SET_SIZE_M((*A), M);
}

void _matrixf_mul_scalar(const float* restrict A, 
                         const float* restrict B,
                         float* restrict C,
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
                sum += A[i * N + k] * B[k * P + j];
            }

            C[i * P + j] = sum;
        }
    }
}

#define AVX2_BLOCK_SIZE 8

void _matrixf_mul_avx2(const float* restrict A, 
                       const float* restrict B,
                       float* restrict C,
                       const uint32_t M,
                       const uint32_t N,
                       const uint32_t P)
{
    uint32_t i;
    uint32_t j;
    uint32_t k;

    float value;

    __m256 a_vec;
    __m256 b_vec;
    __m256 sum_vec;

    uint32_t block_end = P / (AVX2_BLOCK_SIZE * AVX2_BLOCK_SIZE);

    for(i = 0; i < M; i++) 
    {
        for(j = 0; j < N; j++) 
        {
            sum_vec = _mm256_setzero_ps();

            for (k = 0; k < block_end; k += AVX2_BLOCK_SIZE) 
            {
                __m256 a_vec = _mm256_load_ps(&A[i * P + k]);
                __m256 b_vec = _mm256_load_ps(&B[k * N + j]);

                sum_vec = _mm256_fmadd_ps(a_vec, b_vec, sum_vec);
            }

            C[i * N + j] = _mm256_hsum_ps(sum_vec);

            for(k = block_end; k < P; k++) 
            {
                C[i * N + j] += A[i * P + k] * B[k * N + j];
            }
        }
    }
}

typedef void (*matmul_func)(const float* restrict, 
                            const float* restrict,
                            float* restrict,
                            const uint32_t,
                            const uint32_t,
                            const uint32_t);

matmul_func __matmul_funcs[3] = {
    _matrixf_mul_scalar,
    _matrixf_mul_scalar,
    _matrixf_mul_avx2,
};

void matrixf_mul(matrixf_t* A, matrixf_t* B, matrixf_t* C)
{
    uint32_t M;
    uint32_t N;
    uint32_t P;

    float sum;

    assert(SIZE_N((*A)) == SIZE_M((*B)));    

    M = SIZE_M((*A));
    P = SIZE_N((*A));
    N = SIZE_N((*B));

    matrixf_resize(C, M, P);
    matrixf_zero(C);

    if(M >= 8)
    {
        __matmul_funcs[simd_get_vectorization_mode()](GET_PTR((*A)), GET_PTR((*B)), GET_PTR((*C)), M, N, P);
    }
    else
    {
        _matrixf_mul_scalar(GET_PTR((*A)), GET_PTR((*B)), GET_PTR((*C)), M, N, P);
    }
}

void _matrixf_add_f_scalar(matrixf_t* A, const float f, const uint32_t M, const uint32_t N)
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

void matrixf_add_f(matrixf_t* A, float f)
{
    const uint32_t M = SIZE_M((*A));
    const uint32_t N = SIZE_N((*A));

    _matrixf_add_f_scalar(A, f, M, N);
}

void _matrixf_sub_f_scalar(matrixf_t* A, const float f, const uint32_t M, const uint32_t N)
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

void matrixf_sub_f(matrixf_t* A, float f)
{
    const uint32_t M = SIZE_M((*A));
    const uint32_t N = SIZE_N((*A));

    _matrixf_sub_f_scalar(A, f, M, N);
}

void _matrixf_mul_by_f_scalar(matrixf_t* A, const float f, const uint32_t M, const uint32_t N)
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

void matrixf_mul_by_f(matrixf_t* A, float f)
{
    const uint32_t M = SIZE_M((*A));
    const uint32_t N = SIZE_N((*A));

    _matrixf_mul_by_f_scalar(A, f, M, N);
}

void _matrixf_div_by_f_scalar(matrixf_t* A, const float f, const uint32_t M, const uint32_t N)
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

void matrixf_div_by_f(matrixf_t* A, float f)
{
    const uint32_t M = SIZE_M((*A));
    const uint32_t N = SIZE_N((*A));

    _matrixf_div_by_f_scalar(A, f, M, N);
}

void _matrixf_debug_full(matrixf_t* A, const uint32_t M, const uint32_t N)
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

void _matrixf_debug_limited(matrixf_t* A, const uint32_t M, const uint32_t N, const uint32_t max_M, const uint32_t max_N)
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

void matrixf_debug(matrixf_t* A, uint32_t max_rows, uint32_t max_columns)
{
    const uint32_t M = SIZE_M((*A));
    const uint32_t N = SIZE_N((*A));
    
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

void matrixf_destroy(matrixf_t* A)
{
    if(A->data != NULL)
    {
        mem_aligned_free(A->data);
        A->data = NULL;
    }
}

bool matrixf_cholesky_decomposition(matrixf_t* A, matrixf_t* L)
{
    uint32_t i;
    uint32_t j;
    uint32_t k;

    float sum;
    float value;
    float tmp;

    const uint32_t N = SIZE_N((*A));

    if(SIZE_M((*A)) != SIZE_N((*A)))
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

bool matrixf_cholesky_solve(matrixf_t* A, matrixf_t* b, matrixf_t* x)
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

    matrixf_t L = matrix_null();
    matrixf_t y = matrix_null();

    const uint32_t N = SIZE_N((*A));

    if(SIZE_M((*A)) != SIZE_N((*A)))
    {
        logger_log(LogLevel_Error, "Cholesky Solve failed: non-square matrix");
        return false;
    }

    if(!matrixf_cholesky_decomposition(A, &L))
    {
        logger_log(LogLevel_Error, "Cholesky Solve failed: cannot decompose matrix");
        return false;
    }

    /* Ly = b */

    b_m = SIZE_M((*b));
    b_n = SIZE_N((*b));

    y = matrixf_create(b_m, b_n);

    for(i = 0; i < b_n; i++)
    {
        for(j = 0; j < N; j++)
        {
            tmp = GET_AT_WITH_N((*b), b_n, i, j);
            SET_AT_WITH_N(y, b_n, tmp, j, i);

            for(k = 0; k < j; k++)
            {
                value = GET_AT_WITH_N(y, b_n, j, i) - GET_AT_WITH_N(L, N, i, j) * GET_AT_WITH_N(y, b_n, k, i);
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

    return true;
}