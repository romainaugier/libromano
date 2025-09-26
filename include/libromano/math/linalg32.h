/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_MATH_LINALG32)
#define __LIBROMANO_MATH_LINALG32

#include "libromano/common.h"
#include "libromano/math/common32.h"

#include <stdlib.h>

ROMANO_CPP_ENTER

/* vec3f */

typedef struct Vec3F {
    float x, y, z;
} Vec3F;

static ROMANO_FORCE_INLINE Vec3F vec3f_add(const Vec3F a, const Vec3F b)
{
    Vec3F res;

    res.x = a.x + b.x;
    res.y = a.y + b.y;
    res.z = a.z + b.z;

    return res;
}

static ROMANO_FORCE_INLINE Vec3F vec3f_sub(const Vec3F a, const Vec3F b)
{
    Vec3F res;

    res.x = a.x - b.x;
    res.y = a.y - b.y;
    res.z = a.z - b.z;

    return res;
}

static ROMANO_FORCE_INLINE Vec3F vec3f_mul(const Vec3F a, const Vec3F b)
{
    Vec3F res;

    res.x = a.x * b.x;
    res.y = a.y * b.y;
    res.z = a.z * b.z;

    return res;
}

static ROMANO_FORCE_INLINE Vec3F vec3f_div(const Vec3F a, const Vec3F b)
{
    Vec3F res;

    res.x = a.x / b.x;
    res.y = a.y / b.y;
    res.z = a.z / b.z;

    return res;
}

static ROMANO_FORCE_INLINE float vec3f_dot(const Vec3F a, const Vec3F b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static ROMANO_FORCE_INLINE Vec3F vec3f_cross(const Vec3F a, const Vec3F b)
{
    Vec3F res;

    res.x = a.y * b.z - a.z * b.y; 
    res.y = a.z * b.x - a.x * b.z; 
    res.z = a.x * b.y - a.y * b.x;

    return res;
}

static ROMANO_FORCE_INLINE Vec3F vec3f_norm(const Vec3F v)
{
    float t;
    Vec3F res;

    t = mathf_rsqrt(vec3f_dot(v, v));

    res.x = v.x * t;
    res.y = v.y * t;
    res.z = v.z * t;

    return res;
}

static ROMANO_FORCE_INLINE float vec3f_length(const Vec3F v)
{
    return mathf_sqrt(vec3f_dot(v, v));
}

static ROMANO_FORCE_INLINE float vec3f_length2(const Vec3F v)
{
    return vec3f_dot(v, v);
}

static ROMANO_FORCE_INLINE float vec3f_dist(const Vec3F a, const Vec3F b)
{
    return mathf_sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

/* MATRIX */

typedef struct matrix44f {
    float data[16];
} matrix44f_t;

struct MatrixF {
    float* data;
    uint32_t N;
    uint32_t M;
};

typedef struct MatrixF MatrixF;

ROMANO_API MatrixF matrix_null();

ROMANO_API MatrixF matrixf_create(const int M, const int N);

ROMANO_API MatrixF matrixf_copy(MatrixF* A);

ROMANO_API void matrixf_size(MatrixF* A, int* M, int* N);

ROMANO_API void matrixf_resize(MatrixF* A, const int M, const int N);

ROMANO_API int matrixf_row_size(MatrixF* A);

ROMANO_API int matrixf_column_size(MatrixF* A);

ROMANO_API void matrixf_set_at(MatrixF* A, const float value, const int i, const int j);

ROMANO_API float matrixf_get_at(MatrixF* A, const int i, const int j);

ROMANO_API float matrixf_trace(MatrixF* A);

ROMANO_API void matrixf_zero(MatrixF* A);

ROMANO_API void matrixf_transpose(MatrixF* A);

ROMANO_API MatrixF matrixf_transpose_from(MatrixF* A);

ROMANO_API void matrixf_mul(MatrixF* A, MatrixF* B, MatrixF* C);

ROMANO_API void matrixf_add_f(MatrixF* A, const float f);

ROMANO_API void matrixf_sub_f(MatrixF* A, const float f);

ROMANO_API void matrixf_mul_by_f(MatrixF* A, const float f);

ROMANO_API void matrixf_div_by_f(MatrixF* A, const float f);

ROMANO_API void matrixf_debug(MatrixF* A, uint32_t max_rows, uint32_t max_columns);

ROMANO_API void matrixf_destroy(MatrixF* A);

ROMANO_API bool matrixf_cholesky_decomposition(MatrixF* A, MatrixF* L);

ROMANO_API bool matrixf_cholesky_solve(MatrixF* A, MatrixF* b, MatrixF* x);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_MATH_LINALG32) */
