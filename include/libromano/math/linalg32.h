/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#if !defined(__LIBROMANO_MATH_LINALG32)
#define __LIBROMANO_MATH_LINALG32

#include "libromano/libromano.h"
#include <stdlib.h>

ROMANO_CPP_ENTER

/* VEC3 */

struct vec3;
typedef struct vec3 vec3_t;

ROMANO_API vec3_t vec3_add(const vec3_t a, const vec3_t b);

ROMANO_API vec3_t vec3_sub(const vec3_t a, const vec3_t b);

ROMANO_API vec3_t vec3_mul(const vec3_t a, const vec3_t b);

ROMANO_API vec3_t vec3_div(const vec3_t a, const vec3_t b);

ROMANO_API float vec3_dot(const vec3_t a, const vec3_t b);

/* MATRIX */

struct matrix;
typedef struct matrix matrix_t;

ROMANO_API matrix_t matrix_create(const int N, const int M);

ROMANO_API void matrix_size(matrix_t* A, int* N, int* M);

ROMANO_API int matrix_row_size(matrix_t* A);

ROMANO_API int matrix_column_size(matrix_t* A);

ROMANO_API float matrix_trace(matrix_t* A);

ROMANO_API void matrix_destroy(matrix_t* A);



ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_MATH_LINALG32) */
