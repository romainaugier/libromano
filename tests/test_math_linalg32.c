/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/math/linalg32.h"
#include "libromano/simd.h"

static void fill_matrix(FuzzSource* source, MatrixF* A)
{
    uint32_t i;

    for(i = 0; i < A->M * A->N; i++)
        A->data[i] = (float)fuzz_range_i64(source, -64, 64) / 16.0f;
}

static void test_vec3(void)
{
    const Vec3F a = { 1.0f, 2.0f, 3.0f };
    const Vec3F b = { 4.0f, -5.0f, 6.0f };
    Vec3F r;

    r = vec3f_add(a, b);
    TEST_CHECK(r.x == 5.0f && r.y == -3.0f && r.z == 9.0f);
    r = vec3f_sub(a, b);
    TEST_CHECK(r.x == -3.0f && r.y == 7.0f && r.z == -3.0f);
    r = vec3f_mul(a, b);
    TEST_CHECK(r.x == 4.0f && r.y == -10.0f && r.z == 18.0f);
    r = vec3f_div(b, a);
    TEST_CHECK(r.x == 4.0f && r.y == -2.5f && r.z == 2.0f);
    TEST_CHECK(vec3f_dot(a, b) == 12.0f);

    r = vec3f_cross(a, b);
    TEST_CHECK(r.x == 27.0f && r.y == 6.0f && r.z == -13.0f);
    TEST_CHECK(vec3f_dot(r, a) == 0.0f && vec3f_dot(r, b) == 0.0f);

    TEST_CHECK(vec3f_length2(a) == 14.0f);
    TEST_CHECK_NEAR(vec3f_length(a), sqrt(14.0), 1e-6);
    TEST_CHECK_NEAR(vec3f_length(vec3f_norm(b)), 1.0, 1e-5);
    TEST_CHECK_NEAR(vec3f_dist(a, b), sqrt(9.0 + 49.0 + 9.0), 1e-5);
}

static void test_basic_operations(void)
{
    MatrixF A = matrix_null();
    MatrixF B;
    int M = -1;
    int N = -1;
    int i;
    int j;

    TEST_CHECK(A.data == NULL);
    TEST_CHECK_EQ_INT(matrixf_row_size(&A), 0);
    TEST_CHECK_EQ_INT(matrixf_column_size(&A), 0);
    matrixf_size(&A, &M, &N);
    TEST_CHECK(M == -1 && N == -1);

    matrixf_resize(&A, 3, 3);
    TEST_ASSERT(A.data != NULL);

    for(i = 0; i < 3; i++)
        for(j = 0; j < 3; j++)
            matrixf_set_at(&A, (float)(i * 3 + j), i, j);

    matrixf_size(&A, &M, &N);
    TEST_CHECK(M == 3 && N == 3);
    TEST_CHECK_EQ_INT(matrixf_row_size(&A), 3);
    TEST_CHECK_EQ_INT(matrixf_column_size(&A), 3);
    TEST_CHECK(matrixf_get_at(&A, 1, 2) == 5.0f);
    TEST_CHECK(matrixf_trace(&A) == 0.0f + 4.0f + 8.0f);

    B = matrixf_copy(&A);
    TEST_CHECK(B.data != A.data);
    TEST_CHECK_EQ_MEM(B.data, A.data, 9 * sizeof(float));

    matrixf_add_f(&B, 1.0f);
    TEST_CHECK(matrixf_get_at(&B, 2, 2) == 9.0f);
    matrixf_mul_by_f(&B, 2.0f);
    TEST_CHECK(matrixf_get_at(&B, 2, 2) == 18.0f);
    matrixf_sub_f(&B, 2.0f);
    TEST_CHECK(matrixf_get_at(&B, 2, 2) == 16.0f);
    matrixf_div_by_f(&B, 4.0f);
    TEST_CHECK(matrixf_get_at(&B, 2, 2) == 4.0f);
    TEST_CHECK(matrixf_get_at(&B, 0, 0) == 0.0f);

    matrixf_zero(&B);
    TEST_CHECK(matrixf_trace(&B) == 0.0f && matrixf_get_at(&B, 1, 2) == 0.0f);

    matrixf_destroy(&B);
    TEST_CHECK(B.data == NULL);
    matrixf_destroy(&B);

    matrixf_resize(&A, 2, 3);
    TEST_CHECK(matrixf_trace(&A) == 0.0f);

    matrixf_debug(&A, 8, 8);
    matrixf_destroy(&A);

    A = matrixf_create(20, 20);
    matrixf_zero(&A);
    matrixf_debug(&A, 4, 4);
    matrixf_destroy(&A);
}

static bool property_transpose(FuzzSource* source, void* user_data)
{
    const int M = (int)fuzz_range(source, 1, 12);
    const int N = fuzz_bool(source) ? M : (int)fuzz_range(source, 1, 12);
    MatrixF A = matrixf_create(M, N);
    MatrixF T;
    MatrixF in_place;
    bool ok = true;
    int i;
    int j;

    ROMANO_UNUSED(user_data);

    fill_matrix(source, &A);

    T = matrixf_transpose_from(&A);
    in_place = matrixf_copy(&A);
    matrixf_transpose(&in_place);

    ok &= T.M == (uint32_t)N && T.N == (uint32_t)M;
    ok &= in_place.M == (uint32_t)N && in_place.N == (uint32_t)M;

    for(i = 0; i < M && ok; i++)
        for(j = 0; j < N && ok; j++)
            ok = matrixf_get_at(&T, j, i) == matrixf_get_at(&A, i, j) &&
                 matrixf_get_at(&in_place, j, i) == matrixf_get_at(&A, i, j);

    matrixf_destroy(&A);
    matrixf_destroy(&T);
    matrixf_destroy(&in_place);

    TEST_FUZZ_CHECK_MSG(ok, "transpose of a %dx%d matrix", M, N);

    return true;
}

static bool property_mul(FuzzSource* source, void* user_data)
{
    const VectorizationMode initial = simd_get_vectorization_mode();
    const int M = (int)fuzz_range(source, 1, 20);
    const int N = (int)fuzz_range(source, 1, 20);
    const int P = (int)fuzz_range(source, 1, 20);
    MatrixF A = matrixf_create(M, N);
    MatrixF B = matrixf_create(N, P);
    MatrixF C = matrix_null();
    bool ok = true;
    int failed_mode = -1;
    int mode;
    int i, j, k;

    ROMANO_UNUSED(user_data);

    fill_matrix(source, &A);
    fill_matrix(source, &B);

    for(mode = 0; mode < VectorizationMode_COUNT && ok; mode++)
    {
        simd_force_vectorization_mode((VectorizationMode)mode);

        if((int)simd_get_vectorization_mode() != mode)
            continue;

        matrixf_mul(&A, &B, &C);

        ok &= C.M == (uint32_t)M && C.N == (uint32_t)P;

        for(i = 0; i < M && ok; i++)
        {
            for(j = 0; j < P && ok; j++)
            {
                double expected = 0.0;

                for(k = 0; k < N; k++)
                    expected += (double)matrixf_get_at(&A, i, k) * matrixf_get_at(&B, k, j);

                ok = fabs(matrixf_get_at(&C, i, j) - expected) <= 1e-3;
            }
        }

        if(!ok)
            failed_mode = mode;
    }

    simd_force_vectorization_mode(initial);

    matrixf_destroy(&A);
    matrixf_destroy(&B);
    matrixf_destroy(&C);

    TEST_FUZZ_CHECK_MSG(ok, "(%dx%d) * (%dx%d) is wrong in mode %s", M, N, N, P,
                        simd_get_vectorization_mode_as_string((VectorizationMode)failed_mode));

    return true;
}

static bool property_cholesky(FuzzSource* source, void* user_data)
{
    const VectorizationMode initial = simd_get_vectorization_mode();
    const int n = (int)fuzz_range(source, 1, 16);
    const int rhs = (int)fuzz_range(source, 1, 3);
    MatrixF R = matrixf_create(n, n);
    MatrixF A = matrix_null();
    MatrixF Rt;
    MatrixF b = matrixf_create(n, rhs);
    MatrixF x = matrix_null();
    MatrixF Ax = matrix_null();
    bool ok = true;
    int failed_mode = -1;
    int mode;
    int i;

    ROMANO_UNUSED(user_data);

    fill_matrix(source, &R);
    fill_matrix(source, &b);

    Rt = matrixf_transpose_from(&R);
    matrixf_mul(&R, &Rt, &A);

    for(i = 0; i < n; i++)
        matrixf_set_at(&A, matrixf_get_at(&A, i, i) + (float)n, i, i);

    for(mode = 0; mode < VectorizationMode_COUNT && ok; mode++)
    {
        uint32_t k;

        simd_force_vectorization_mode((VectorizationMode)mode);

        if((int)simd_get_vectorization_mode() != mode)
            continue;

        ok &= matrixf_cholesky_solve(&A, &b, &x);

        if(ok)
        {
            ok &= x.M == b.M && x.N == b.N;
            matrixf_mul(&A, &x, &Ax);
        }

        for(k = 0; k < b.M * b.N && ok; k++)
            ok = fabsf(Ax.data[k] - b.data[k]) <= 1e-3f * (1.0f + fabsf(b.data[k]));

        if(!ok)
            failed_mode = mode;
    }

    simd_force_vectorization_mode(initial);

    matrixf_destroy(&R);
    matrixf_destroy(&Rt);
    matrixf_destroy(&A);
    matrixf_destroy(&b);
    matrixf_destroy(&x);
    matrixf_destroy(&Ax);

    TEST_FUZZ_CHECK_MSG(ok, "cholesky solve of a %dx%d system with %d right-hand sides failed in mode %s", n, n, rhs,
                        simd_get_vectorization_mode_as_string((VectorizationMode)failed_mode));

    return true;
}

static void test_fuzz(void)
{
    test_fuzz_property("matrix_transpose", 2000, property_transpose, NULL);
    test_fuzz_property("matrix_mul", 1000, property_mul, NULL);
    test_fuzz_property("cholesky_solve", 1000, property_cholesky, NULL);
}

static void test_cholesky_failures(void)
{
    MatrixF A = matrixf_create(2, 2);
    MatrixF rectangular = matrixf_create(2, 3);
    MatrixF b = matrixf_create(2, 1);
    MatrixF x = matrix_null();

    matrixf_set_at(&A, 1.0f, 0, 0);
    matrixf_set_at(&A, 2.0f, 0, 1);
    matrixf_set_at(&A, 2.0f, 1, 0);
    matrixf_set_at(&A, 1.0f, 1, 1);
    matrixf_zero(&b);
    matrixf_zero(&rectangular);

    logger_log_info("expecting cholesky errors below");
    TEST_CHECK(!matrixf_cholesky_solve(&A, &b, &x));
    TEST_CHECK(!matrixf_cholesky_solve(&rectangular, &b, &x));

    matrixf_destroy(&A);
    matrixf_destroy(&rectangular);
    matrixf_destroy(&b);
    matrixf_destroy(&x);
}

TEST_MAIN(
    TEST(test_vec3),
    TEST(test_basic_operations),
    TEST(test_fuzz),
    TEST(test_cholesky_failures),
)