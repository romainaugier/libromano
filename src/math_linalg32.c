/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/math/linalg32.h"

/* MATRIX */

#define MATRIX_SIZE_N(A) ((int)A.data[0])
#define MATRIX_SIZE_M(A) ((int)A.data[1])
#define MATRIX_SET_SIZE_N(A, N) A.data[0] = (float)N
#define MATRIX_SET_SIZE_M(A, M) A.data[0] = (float)M

struct matrix {
    float* data;
};

matrix_t matrix_create(const int N, const int M)
{
    matrix_t A;
    A.data = (float*)calloc(N * M + 2, sizeof(float));
    MATRIX_SET_SIZE_N(A, N);
    MATRIX_SET_SIZE_M(A, M);

    return A;
}

void matrix_destroy(matrix_t* A)
{

}