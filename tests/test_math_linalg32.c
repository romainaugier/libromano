/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/math/linalg32.h"
#include "libromano/simd.h"
#include "libromano/threadpool.h"

#include <math.h>

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

    matrixf_add_f(NULL, &B, 1.0f);
    TEST_CHECK(matrixf_get_at(&B, 2, 2) == 9.0f);
    matrixf_mul_by_f(NULL, &B, 2.0f);
    TEST_CHECK(matrixf_get_at(&B, 2, 2) == 18.0f);
    matrixf_sub_f(NULL, &B, 2.0f);
    TEST_CHECK(matrixf_get_at(&B, 2, 2) == 16.0f);
    matrixf_div_by_f(NULL, &B, 4.0f);
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

    T = matrixf_transpose_from(NULL, &A);
    in_place = matrixf_copy(&A);
    matrixf_transpose(NULL, &in_place);

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

        matrixf_mul(NULL, &A, &B, &C);

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

    Rt = matrixf_transpose_from(NULL, &R);
    matrixf_mul(NULL, &R, &Rt, &A);

    for(i = 0; i < n; i++)
        matrixf_set_at(&A, matrixf_get_at(&A, i, i) + (float)n, i, i);

    for(mode = 0; mode < VectorizationMode_COUNT && ok; mode++)
    {
        uint32_t k;

        simd_force_vectorization_mode((VectorizationMode)mode);

        if((int)simd_get_vectorization_mode() != mode)
            continue;

        ok &= matrixf_cholesky_solve(NULL, &A, &b, &x);

        if(ok)
        {
            ok &= x.M == b.M && x.N == b.N;
            matrixf_mul(NULL, &A, &x, &Ax);
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

/* Reference in double precision, row-major */
static bool check_mul(MatrixF* A, MatrixF* B, MatrixF* C, double tolerance)
{
    uint32_t i, j, k;

    if(C->M != A->M || C->N != B->N)
        return false;

    for(i = 0; i < A->M; i++)
    {
        for(j = 0; j < B->N; j++)
        {
            double expected = 0.0;
            double scale = 0.0;

            for(k = 0; k < A->N; k++)
            {
                const double t = (double)A->data[i * A->N + k] * (double)B->data[k * B->N + j];
                expected += t;
                scale += fabs(t);
            }

            if(fabs((double)C->data[i * C->N + j] - expected) > tolerance * (1.0 + scale))
                return false;
        }
    }

    return true;
}

typedef struct MulFuzzData {
    LinAlgCtx* ctx;
    int max_size;
} MulFuzzData;

/* Sizes crossing the 16x6 tile edges and the KC = 256 block, in every vectorization mode */
static bool property_mul_large(FuzzSource* source, void* user_data)
{
    const MulFuzzData* data = (const MulFuzzData*)user_data;
    const VectorizationMode initial = simd_get_vectorization_mode();
    const int M = (int)fuzz_range(source, 1, data->max_size);
    const int N = (int)fuzz_range(source, 1, data->max_size);
    const int P = (int)fuzz_range(source, 1, data->max_size);
    MatrixF A = matrixf_create(M, N);
    MatrixF B = matrixf_create(N, P);
    MatrixF C = matrix_null();
    bool ok = true;
    int failed_mode = -1;
    int mode;

    fill_matrix(source, &A);
    fill_matrix(source, &B);

    for(mode = 0; mode < VectorizationMode_COUNT && ok; mode++)
    {
        simd_force_vectorization_mode((VectorizationMode)mode);

        if((int)simd_get_vectorization_mode() != mode)
            continue;

        matrixf_mul(data->ctx, &A, &B, &C);

        ok = check_mul(&A, &B, &C, 1e-5);

        if(!ok)
            failed_mode = mode;
    }

    simd_force_vectorization_mode(initial);

    matrixf_destroy(&A);
    matrixf_destroy(&B);
    matrixf_destroy(&C);

    TEST_FUZZ_CHECK_MSG(ok, "(%dx%d) * (%dx%d) is wrong in mode %s (%s)", M, N, N, P,
                        simd_get_vectorization_mode_as_string((VectorizationMode)failed_mode),
                        data->ctx != NULL ? "threaded" : "single thread");

    return true;
}

static void test_mul_large(void)
{
    ThreadPool* pool = threadpool_init(3);
    LinAlgCtx ctx = linalg_ctx_new(pool);
    MulFuzzData data;

    TEST_ASSERT(pool != NULL);

    data.ctx = NULL;
    data.max_size = 300;
    test_fuzz_property("matrix_mul_large", 40, property_mul_large, &data);

    data.ctx = &ctx;
    test_fuzz_property("matrix_mul_large_threaded", 40, property_mul_large, &data);

    threadpool_release(pool);
}

/* K > KC (several accumulation passes) and N > 6 * 800 / threads (several NC blocks) */
static void test_mul_blocking_edges(void)
{
    ThreadPool* pool = threadpool_init(2);
    LinAlgCtx ctx = linalg_ctx_new(pool);
    const int sizes[][3] = { { 17, 600, 7 }, { 33, 5, 2500 }, { 1, 1000, 1 }, { 257, 3, 1 } };
    size_t s;
    int t;

    TEST_ASSERT(pool != NULL);

    for(s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
    {
        MatrixF A = matrixf_create(sizes[s][0], sizes[s][1]);
        MatrixF B = matrixf_create(sizes[s][1], sizes[s][2]);
        MatrixF C = matrix_null();
        uint32_t i;

        for(i = 0; i < A.M * A.N; i++)
            A.data[i] = (float)((int)(i * 7u % 13u) - 6) / 8.0f;

        for(i = 0; i < B.M * B.N; i++)
            B.data[i] = (float)((int)(i * 5u % 11u) - 5) / 8.0f;

        for(t = 0; t < 2; t++)
        {
            matrixf_mul(t == 0 ? NULL : &ctx, &A, &B, &C);
            TEST_CHECK_MSG(check_mul(&A, &B, &C, 1e-5), "(%dx%d) * (%dx%d), %s",
                           sizes[s][0], sizes[s][1], sizes[s][1], sizes[s][2],
                           t == 0 ? "single thread" : "threaded");
        }

        matrixf_destroy(&A);
        matrixf_destroy(&B);
        matrixf_destroy(&C);
    }

    threadpool_release(pool);
}

/* Element-wise ops and transposes must give the same result with and without a pool */
static void test_ctx_consistency(void)
{
    ThreadPool* pool = threadpool_init(3);
    LinAlgCtx ctx = linalg_ctx_new(pool);
    LinAlgCtx no_pool = linalg_ctx_new(NULL);
    MatrixF A = matrixf_create(300, 500);
    MatrixF B;
    MatrixF T1;
    MatrixF T2;
    uint32_t i;

    TEST_ASSERT(pool != NULL);

    for(i = 0; i < A.M * A.N; i++)
        A.data[i] = (float)(i % 97) - 48.0f;

    B = matrixf_copy(&A);

    matrixf_add_f(&ctx, &A, 3.0f);
    matrixf_mul_by_f(&ctx, &A, 2.0f);
    matrixf_sub_f(&ctx, &A, 1.0f);
    matrixf_div_by_f(&ctx, &A, 4.0f);

    matrixf_add_f(&no_pool, &B, 3.0f);
    matrixf_mul_by_f(&no_pool, &B, 2.0f);
    matrixf_sub_f(&no_pool, &B, 1.0f);
    matrixf_div_by_f(&no_pool, &B, 4.0f);

    TEST_CHECK_EQ_MEM(A.data, B.data, A.M * A.N * sizeof(float));
    TEST_CHECK(matrixf_get_at(&A, 1, 2) == ((((float)((500 + 2) % 97) - 48.0f) + 3.0f) * 2.0f - 1.0f) / 4.0f);

    T1 = matrixf_transpose_from(&ctx, &A);
    T2 = matrixf_copy(&A);
    matrixf_transpose(&ctx, &T2);

    TEST_CHECK(T1.M == 500 && T1.N == 300 && T2.M == 500 && T2.N == 300);
    TEST_CHECK_EQ_MEM(T1.data, T2.data, T1.M * T1.N * sizeof(float));
    TEST_CHECK(matrixf_get_at(&T1, 7, 11) == matrixf_get_at(&A, 11, 7));

    matrixf_destroy(&A);
    matrixf_destroy(&B);
    matrixf_destroy(&T1);
    matrixf_destroy(&T2);

    threadpool_release(pool);
}

typedef struct NestedMulData {
    LinAlgCtx* ctx;
    MatrixF* A;
    MatrixF* B;
    MatrixF C;
} NestedMulData;

static void* nested_mul_job(void* arg)
{
    NestedMulData* data = (NestedMulData*)arg;

    matrixf_mul(data->ctx, data->A, data->B, &data->C);

    return NULL;
}

/* A linalg call from inside a job of the same pool must not deadlock */
static void test_nested_ctx(void)
{
    ThreadPool* pool = threadpool_init(2);
    LinAlgCtx ctx = linalg_ctx_new(pool);
    ThreadPoolWaiter waiter = threadpool_waiter_new();
    MatrixF A = matrixf_create(96, 128);
    MatrixF B = matrixf_create(128, 80);
    NestedMulData jobs[8];
    uint32_t i;

    TEST_ASSERT(pool != NULL);

    for(i = 0; i < A.M * A.N; i++)
        A.data[i] = (float)(i % 17) / 16.0f;

    for(i = 0; i < B.M * B.N; i++)
        B.data[i] = (float)(i % 13) / 16.0f;

    for(i = 0; i < 8; i++)
    {
        jobs[i].ctx = &ctx;
        jobs[i].A = &A;
        jobs[i].B = &B;
        jobs[i].C = matrix_null();

        TEST_ASSERT(threadpool_work_add(pool, nested_mul_job, &jobs[i], &waiter));
    }

    threadpool_waiter_wait_help(pool, &waiter);

    for(i = 0; i < 8; i++)
    {
        TEST_CHECK(check_mul(&A, &B, &jobs[i].C, 1e-5));
        matrixf_destroy(&jobs[i].C);
    }

    matrixf_destroy(&A);
    matrixf_destroy(&B);

    threadpool_release(pool);
}

static void test_cholesky_threaded(void)
{
    ThreadPool* pool = threadpool_init(3);
    LinAlgCtx ctx = linalg_ctx_new(pool);
    const int n = 64;
    const int rhs = 40;
    MatrixF R = matrixf_create(n, n);
    MatrixF Rt;
    MatrixF A = matrix_null();
    MatrixF b = matrixf_create(n, rhs);
    MatrixF x = matrix_null();
    MatrixF Ax = matrix_null();
    uint32_t k;
    int i;

    TEST_ASSERT(pool != NULL);

    for(k = 0; k < R.M * R.N; k++)
        R.data[k] = (float)((int)(k * 7u % 19u) - 9) / 16.0f;

    for(k = 0; k < b.M * b.N; k++)
        b.data[k] = (float)((int)(k * 3u % 23u) - 11) / 8.0f;

    Rt = matrixf_transpose_from(&ctx, &R);
    matrixf_mul(&ctx, &R, &Rt, &A);

    for(i = 0; i < n; i++)
        matrixf_set_at(&A, matrixf_get_at(&A, i, i) + (float)n, i, i);

    TEST_ASSERT(matrixf_cholesky_solve(&ctx, &A, &b, &x));
    matrixf_mul(&ctx, &A, &x, &Ax);

    for(k = 0; k < b.M * b.N; k++)
        TEST_CHECK(fabsf(Ax.data[k] - b.data[k]) <= 1e-3f * (1.0f + fabsf(b.data[k])));

    matrixf_destroy(&R);
    matrixf_destroy(&Rt);
    matrixf_destroy(&A);
    matrixf_destroy(&b);
    matrixf_destroy(&x);
    matrixf_destroy(&Ax);

    threadpool_release(pool);
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
    TEST_CHECK(!matrixf_cholesky_solve(NULL, &A, &b, &x));
    TEST_CHECK(!matrixf_cholesky_solve(NULL, &rectangular, &b, &x));

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
    TEST(test_mul_large),
    TEST(test_mul_blocking_edges),
    TEST(test_ctx_consistency),
    TEST(test_nested_ctx),
    TEST(test_cholesky_threaded),
)