/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_MATH_LINALG32)
#define __LIBROMANO_MATH_LINALG32

#include "libromano/libromano.h"
#include "libromano/math/common.h"

#include <stdlib.h>

ROMANO_CPP_ENTER

/* VEC3 */

typedef struct vec3 {
    float x, y, z;
} vec3_t;

ROMANO_FORCE_INLINE vec3_t vec3_add(const vec3_t a, const vec3_t b)
{
    vec3_t res;

    res.x = a.x + b.x;
    res.y = a.y + b.y;
    res.z = a.z + b.z;

    return res;
}

ROMANO_FORCE_INLINE vec3_t vec3_sub(const vec3_t a, const vec3_t b)
{
    vec3_t res;

    res.x = a.x - b.x;
    res.y = a.y - b.y;
    res.z = a.z - b.z;

    return res;
}

ROMANO_FORCE_INLINE vec3_t vec3_mul(const vec3_t a, const vec3_t b)
{
    vec3_t res;

    res.x = a.x * b.x;
    res.y = a.y * b.y;
    res.z = a.z * b.z;

    return res;
}

ROMANO_FORCE_INLINE vec3_t vec3_div(const vec3_t a, const vec3_t b)
{
    vec3_t res;

    res.x = a.x / b.x;
    res.y = a.y / b.y;
    res.z = a.z / b.z;

    return res;
}

ROMANO_FORCE_INLINE float vec3_dot(const vec3_t a, const vec3_t b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

ROMANO_FORCE_INLINE vec3_t vec3_cross(const vec3_t a, const vec3_t b)
{
    vec3_t res;

    res.x = a.y * b.z - a.z * b.y; 
    res.y = a.z * b.x - a.x * b.z; 
    res.z = a.x * b.y - a.y * b.x;

    return res;
}

ROMANO_FORCE_INLINE vec3_t vec3_norm(const vec3_t v)
{
    float t;
    vec3_t res;

    t = math_rsqrt(vec3_dot(v, v));

    res.x = v.x * t;
    res.y = v.y * t;
    res.z = v.z * t;

    return res;
}

ROMANO_FORCE_INLINE float vec3_length(const vec3_t v)
{
    return math_sqrt(vec3_dot(v, v));
}

ROMANO_FORCE_INLINE float vec3_length2(const vec3_t v)
{
    return vec3_dot(v, v);
}

ROMANO_FORCE_INLINE float vec3_dist(const vec3_t a, const vec3_t b)
{
    return math_sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

/* MATRIX */

typedef struct matrix44 {
    float data[16];
} matrix44_t;

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
