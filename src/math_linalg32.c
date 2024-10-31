/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/math/linalg32.h"
#include "libromano/memory.h"

#include <string.h>

/* MATRIX */

#define MATRIXF_SIZE_N(A) ((int)A.data[0])
#define MATRIXF_SIZE_M(A) ((int)A.data[1])
#define MATRIXF_SET_SIZE_N(A, N) A.data[0] = (float)N
#define MATRIXF_SET_SIZE_M(A, M) A.data[1] = (float)M

#define SWAP_FLOAT(f1, f2) do { float tmp = f1; f1 = f2; f2 = tmp; } while (0)

/* 
    M -> rows
    N -> columns
*/

matrixf_t matrixf_create(const int M, const int N)
{
    matrixf_t A;

    A.data = (float*)mem_aligned_alloc((M * N + 2) * sizeof(float), sizeof(float));
    MATRIXF_SET_SIZE_M(A, M);
    MATRIXF_SET_SIZE_N(A, N);

    return A;
}

void matrixf_size(matrixf_t* A, int* M, int* N)
{
    if(A->data != NULL)
    {
        *M = MATRIXF_SIZE_M((*A));
        *N = MATRIXF_SIZE_N((*A));
    }
}

void matrixf_resize(matrixf_t* A, const int M, const int N)
{
    if(A->data != NULL)
    {
        mem_aligned_free(A->data);
    }

    A->data = (float*)mem_aligned_alloc((M * N + 2) * sizeof(float), sizeof(float));
    MATRIXF_SET_SIZE_M((*A), M);
    MATRIXF_SET_SIZE_N((*A), N);
}

int matrixf_row_size(matrixf_t* A)
{
    if(A->data)
    {
        return MATRIXF_SIZE_M((*A));
    }

    return 0;
}

int matrixf_column_size(matrixf_t* A)
{
    if(A->data)
    {
        return MATRIXF_SIZE_N((*A));
    }

    return 0;
}

void matrixf_set_at(matrixf_t* A, const float value, const int i, const int j)
{
    A->data[i * MATRIXF_SIZE_N((*A)) + j + 2] = value;
}

float matrixf_get_at(matrixf_t* A, const int i, const int j)
{
    return A->data[i * MATRIXF_SIZE_N((*A)) + j + 2];
}

float matrixf_trace(matrixf_t* A)
{
    return 0.0f;
}

void matrixf_zero(matrixf_t* A)
{
    memset(A->data + 2, 0, MATRIXF_SIZE_M((*A)) * MATRIXF_SIZE_N((*A)) * sizeof(float));
}

void matrixf_transpose(matrixf_t* A)
{
    const int M = MATRIXF_SIZE_M((*A));
    const int N = MATRIXF_SIZE_N((*A));

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

    MATRIXF_SET_SIZE_N((*A), N);
    MATRIXF_SET_SIZE_M((*A), M);
}

void matrixf_destroy(matrixf_t* A)
{
    if(A->data != NULL)
    {
        free(A->data);
        A->data == NULL;
    }
}