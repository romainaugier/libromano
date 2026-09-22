/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/math/linalg32.h"
#include "libromano/memory.h"
#include "libromano/simd.h"
#include "libromano/logger.h"

#include "math_linalg_internal.h"

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

/* Elements per job for the memory bound element-wise functions */
#define ELEMENTWISE_GRAIN (1 << 15)

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

    A.data = (float*)mem_aligned_alloc(((size_t)M * (size_t)N) * sizeof(float), ALIGNMENT);
    A.M = M;
    A.N = N;

    return A;
}

MatrixF matrixf_copy(MatrixF* A)
{
    MatrixF B;

    const size_t size = ((size_t)A->M * (size_t)A->N) * sizeof(float);

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
        /* Same number of elements, the buffer can be reused as is */
        if((size_t)A->M * (size_t)A->N == (size_t)M * (size_t)N)
        {
            A->M = M;
            A->N = N;
            return;
        }

        mem_aligned_free(A->data);
    }

    A->data = (float*)mem_aligned_alloc(((size_t)M * (size_t)N) * sizeof(float), ALIGNMENT);
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
    A->data[(size_t)i * A->N + j] = value;
}

float matrixf_get_at(MatrixF* A, const int i, const int j)
{
    return A->data[(size_t)i * A->N + j];
}

float matrixf_trace(MatrixF* A)
{
    size_t i;
    float t;

    if(A->M != A->N)
        return 0.0f;

    t = 0.0f;

    for(i = 0; i < A->M; i++)
        t += A->data[i * A->N + i];

    return t;
}

void matrixf_zero(MatrixF* A)
{
    memset(A->data, 0, (size_t)A->M * (size_t)A->N * sizeof(float));
}

/* Transpose */

typedef struct TransposeJob {
    const float* src;
    float* dst;
    size_t M; /* rows of src */
    size_t N; /* columns of src */
} TransposeJob;

/* dst (N x M) = src^T (M x N), rows of src in [begin, end) */
static void transpose_out_of_place_range(void* data, size_t begin, size_t end)
{
    const TransposeJob* job = (const TransposeJob*)data;
    size_t i;
    size_t j;

    for(i = begin; i < end; i++)
        for(j = 0; j < job->N; j++)
            job->dst[j * job->M + i] = job->src[i * job->N + j];
}

/* Square in place: row i swaps its upper part with column i, rows touch disjoint pairs */
static void transpose_in_place_range(void* data, size_t begin, size_t end)
{
    const TransposeJob* job = (const TransposeJob*)data;
    float* A = job->dst;
    const size_t N = job->N;
    size_t i;
    size_t j;

    for(i = begin; i < end; i++)
        for(j = i + 1; j < N; j++)
            SWAP_FLOAT(A[i * N + j], A[j * N + i]);
}

static size_t transpose_grain(size_t row_size)
{
    return LINALG_MAX((size_t)1, (size_t)ELEMENTWISE_GRAIN / LINALG_MAX((size_t)1, row_size));
}

void matrixf_transpose(LinAlgCtx* ctx, MatrixF* A)
{
    TransposeJob job;
    float* new_data;

    const size_t M = A->M;
    const size_t N = A->N;

    job.M = M;
    job.N = N;

    if(M == N)
    {
        job.src = A->data;
        job.dst = A->data;

        linalg_parallel_for(ctx, M, transpose_grain(N), transpose_in_place_range, &job);
    }
    else
    {
        new_data = (float*)mem_aligned_alloc((N * M) * sizeof(float), ALIGNMENT);

        if(new_data == NULL)
            return;

        job.src = A->data;
        job.dst = new_data;

        linalg_parallel_for(ctx, M, transpose_grain(N), transpose_out_of_place_range, &job);

        mem_aligned_free(A->data);

        A->data = new_data;
    }

    A->M = (uint32_t)N;
    A->N = (uint32_t)M;
}

MatrixF matrixf_transpose_from(LinAlgCtx* ctx, MatrixF* A)
{
    TransposeJob job;
    MatrixF res = matrixf_create(A->N, A->M);

    job.src = A->data;
    job.dst = res.data;
    job.M = A->M;
    job.N = A->N;

    linalg_parallel_for(ctx, job.M, transpose_grain(job.N), transpose_out_of_place_range, &job);

    return res;
}

/* Matrix multiplication */

/*
 * All the kernels compute the row-major C (M x P) = A (M x N) * B (N x P)
 * C is sized but not initialized
 */
typedef void (*matmul_func)(LinAlgCtx* ctx,
                            const float* ROMANO_RESTRICT A,
                            const float* ROMANO_RESTRICT B,
                            float* ROMANO_RESTRICT C,
                            const uint32_t M,
                            const uint32_t N,
                            const uint32_t P);

typedef struct MatmulScalarJob {
    const float* A;
    const float* B;
    float* C;
    size_t N;
    size_t P;
} MatmulScalarJob;

/*
 * i-k-j order: the inner loop streams contiguous rows of B and C, which the compiler
 * vectorizes (the i-j-k order walks B column-wise with a stride of P)
 */
static void matmul_scalar_rows(void* data, size_t begin, size_t end)
{
    const MatmulScalarJob* job = (const MatmulScalarJob*)data;
    const size_t N = job->N;
    const size_t P = job->P;
    size_t i;
    size_t j;
    size_t k;

    for(i = begin; i < end; i++)
    {
        float* ROMANO_RESTRICT c = job->C + i * P;
        const float* ROMANO_RESTRICT a = job->A + i * N;

        memset(c, 0, P * sizeof(float));

        for(k = 0; k < N; k++)
        {
            const float a_ik = a[k];
            const float* ROMANO_RESTRICT b = job->B + k * P;

            for(j = 0; j < P; j++)
                c[j] += a_ik * b[j];
        }
    }
}

static void _matrixf_mul_scalar(LinAlgCtx* ctx,
                                const float* ROMANO_RESTRICT A,
                                const float* ROMANO_RESTRICT B,
                                float* ROMANO_RESTRICT C,
                                const uint32_t M,
                                const uint32_t N,
                                const uint32_t P)
{
    MatmulScalarJob job;
    const size_t row_flops = LINALG_MAX((size_t)1, (size_t)N * (size_t)P);

    job.A = A;
    job.B = B;
    job.C = C;
    job.N = N;
    job.P = P;

    linalg_parallel_for(ctx, M, LINALG_MAX((size_t)1, (size_t)(1 << 16) / row_flops), matmul_scalar_rows, &job);
}

#if defined(ROMANO_X86_64)

#define NUM_MATRIXF_MUL_FUNCS 5

/*
 * Row-major C = A * B is column-major C^T = B^T * A^T, and a row-major matrix is its own
 * transpose in column-major: the column-major gemm runs on the same buffers with A and B
 * swapped, no copy needed
 */
#define DEFINE_MATRIXF_MUL_SGEMM(name, kernel)                                  \
    static void name(LinAlgCtx* ctx,                                            \
                     const float* ROMANO_RESTRICT A,                            \
                     const float* ROMANO_RESTRICT B,                            \
                     float* ROMANO_RESTRICT C,                                  \
                     const uint32_t M,                                          \
                     const uint32_t N,                                          \
                     const uint32_t P)                                          \
    {                                                                           \
        linalg32_sgemm_colmajor(ctx, &(kernel), NULL, B, A, C, P, N, M);        \
    }

DEFINE_MATRIXF_MUL_SGEMM(_matrixf_mul_sse, linalg32_sgemm_kernel_sse)
DEFINE_MATRIXF_MUL_SGEMM(_matrixf_mul_avx256, linalg32_sgemm_kernel_avx2)
DEFINE_MATRIXF_MUL_SGEMM(_matrixf_mul_avx512, linalg32_sgemm_kernel_avx512)

/* AVX without AVX2/FMA: the SSE kernel (a 256 bits mul + add kernel is a possible addition) */
#define _matrixf_mul_avx _matrixf_mul_sse

#elif defined(ROMANO_AARCH64) && defined(ROMANO_APPLE)

#define NUM_MATRIXF_MUL_FUNCS 2

/* Accelerate manages its own threads (AMX), the context is not used */
static void _matrixf_mul_accelerate(LinAlgCtx* ctx,
                                    const float* ROMANO_RESTRICT A,
                                    const float* ROMANO_RESTRICT B,
                                    float* ROMANO_RESTRICT C,
                                    const uint32_t M,
                                    const uint32_t N,
                                    const uint32_t P)
{
    ROMANO_UNUSED(ctx);

    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                M, P, N,          /* m, n, k */
                1.0f,
                A, N,             /* lda = cols of A */
                B, P,             /* ldb = cols of B */
                0.0f,
                C, P);            /* ldc = cols of C */
}

#elif defined(ROMANO_AARCH64)

#define NUM_MATRIXF_MUL_FUNCS 2
#define _matrixf_mul_accelerate _matrixf_mul_scalar

#else

#define NUM_MATRIXF_MUL_FUNCS 1

#endif /* defined(ROMANO_X86_64) */

static const matmul_func __matmul_funcs[NUM_MATRIXF_MUL_FUNCS] = {
    _matrixf_mul_scalar,
#if defined(ROMANO_X86_64)
    _matrixf_mul_sse,
    _matrixf_mul_avx,
    _matrixf_mul_avx256,
    _matrixf_mul_avx512,
#elif defined(ROMANO_AARCH64)
    _matrixf_mul_accelerate,
#endif /* defined(ROMANO_X86_64) */
};

/* Below this many multiply-adds, packing costs more than it saves */
#define MATMUL_SMALL_THRESHOLD 4096

void matrixf_mul(LinAlgCtx* ctx, MatrixF* A, MatrixF* B, MatrixF* C)
{
    uint32_t M;
    uint32_t N;
    uint32_t P;

    ROMANO_ASSERT(A->N == B->M, "matrixf_mul: A columns must match B rows");
    ROMANO_ASSERT(C != A && C != B, "matrixf_mul: C cannot alias A or B");

    M = A->M;
    N = A->N;
    P = B->N;

    matrixf_resize(C, M, P);

    if(N == 0)
    {
        matrixf_zero(C);
        return;
    }

    if((uint64_t)M * N * P < MATMUL_SMALL_THRESHOLD)
        _matrixf_mul_scalar(NULL, A->data, B->data, C->data, M, N, P);
    else
        __matmul_funcs[simd_get_vectorization_mode()](ctx, A->data, B->data, C->data, M, N, P);
}

/* Element-wise operations with a scalar */

typedef enum {
    ElementwiseOp_Add,
    ElementwiseOp_Sub,
    ElementwiseOp_Mul,
    ElementwiseOp_Div,
} ElementwiseOp;

typedef struct ElementwiseJob {
    float* data;
    float f;
    ElementwiseOp op;
} ElementwiseJob;

/* The data is contiguous: one flat loop per op, vectorized by the compiler */
static void elementwise_range(void* data, size_t begin, size_t end)
{
    const ElementwiseJob* job = (const ElementwiseJob*)data;
    float* ROMANO_RESTRICT x = job->data;
    const float f = job->f;
    size_t i;

    switch(job->op)
    {
        case ElementwiseOp_Add:
            for(i = begin; i < end; i++) x[i] += f;
            break;
        case ElementwiseOp_Sub:
            for(i = begin; i < end; i++) x[i] -= f;
            break;
        case ElementwiseOp_Mul:
            for(i = begin; i < end; i++) x[i] *= f;
            break;
        case ElementwiseOp_Div:
            for(i = begin; i < end; i++) x[i] /= f;
            break;
    }
}

static void matrixf_elementwise(LinAlgCtx* ctx, MatrixF* A, const float f, const ElementwiseOp op)
{
    ElementwiseJob job;

    job.data = A->data;
    job.f = f;
    job.op = op;

    linalg_parallel_for(ctx, (size_t)A->M * (size_t)A->N, ELEMENTWISE_GRAIN, elementwise_range, &job);
}

void matrixf_add_f(LinAlgCtx* ctx, MatrixF* A, const float f)
{
    matrixf_elementwise(ctx, A, f, ElementwiseOp_Add);
}

void matrixf_sub_f(LinAlgCtx* ctx, MatrixF* A, const float f)
{
    matrixf_elementwise(ctx, A, f, ElementwiseOp_Sub);
}

void matrixf_mul_by_f(LinAlgCtx* ctx, MatrixF* A, const float f)
{
    matrixf_elementwise(ctx, A, f, ElementwiseOp_Mul);
}

void matrixf_div_by_f(LinAlgCtx* ctx, MatrixF* A, const float f)
{
    matrixf_elementwise(ctx, A, f, ElementwiseOp_Div);
}

/* Debug */

static void _matrixf_debug_row(MatrixF* A, const uint32_t i, const uint32_t half_columns)
{
    const uint32_t N = A->N;
    uint32_t j;

    if(half_columns == 0 || N <= 2 * half_columns)
    {
        for(j = 0; j < N; j++)
            printf(j == (N - 1) ? "%.3f" : "%.3f ", matrixf_get_at(A, i, j));
    }
    else
    {
        for(j = 0; j < half_columns; j++)
            printf("%.3f ", matrixf_get_at(A, i, j));

        printf("...");

        for(j = N - half_columns; j < N; j++)
            printf(" %.3f", matrixf_get_at(A, i, j));
    }

    printf("\n");
}

void matrixf_debug(MatrixF* A, uint32_t max_rows, uint32_t max_columns)
{
    const uint32_t M = A->M;
    const uint32_t N = A->N;
    const uint32_t half_rows = max_rows / 2;
    const uint32_t half_columns = (max_rows == 0 || max_columns == 0) ? 0 : max_columns / 2;
    uint32_t i;

    printf("Matrix f32: %u x %u\n", M, N);

    if(max_rows == 0 || max_columns == 0 || M <= 2 * half_rows)
    {
        for(i = 0; i < M; i++)
            _matrixf_debug_row(A, i, half_columns);

        return;
    }

    for(i = 0; i < half_rows; i++)
        _matrixf_debug_row(A, i, half_columns);

    printf("...\n");

    for(i = M - half_rows; i < M; i++)
        _matrixf_debug_row(A, i, half_columns);
}

void matrixf_destroy(MatrixF* A)
{
    if(A->data != NULL)
    {
        mem_aligned_free(A->data);
        A->data = NULL;
    }
}

/* Cholesky */

bool _matrixf_cholesky_decomposition_scalar(MatrixF* A, MatrixF* L)
{
    uint32_t i;
    uint32_t j;
    uint32_t k;

    float sum;
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

                L->data[i * N + j] = mathf_sqrt(tmp);
            }
            else
            {
                L->data[i * N + j] = (A->data[i * N + j] - sum) / L->data[j * N + j];
            }
        }
    }

    return true;
}

typedef struct CholeskySolveJob {
    const float* L;
    const float* b;
    float* y;
    float* x;
    size_t N;
    size_t nrhs;
} CholeskySolveJob;

/* Forward then back substitution for the right-hand sides [begin, end), columns are independent */
static void cholesky_solve_columns(void* data, size_t begin, size_t end)
{
    const CholeskySolveJob* job = (const CholeskySolveJob*)data;
    const float* L = job->L;
    const size_t N = job->N;
    const size_t nrhs = job->nrhs;
    size_t j;
    size_t i;
    size_t k;
    float sum;

    for(j = begin; j < end; j++)
    {
        /* L y = b (L lower triangular) */
        for(i = 0; i < N; i++)
        {
            sum = 0.0f;

            for(k = 0; k < i; k++)
                sum += L[i * N + k] * job->y[k * nrhs + j];

            job->y[i * nrhs + j] = (job->b[i * nrhs + j] - sum) / L[i * N + i];
        }

        /* L^T x = y (L^T upper triangular, L^T[i][k] = L[k][i]) */
        for(i = N; i-- > 0;)
        {
            sum = 0.0f;

            for(k = i + 1; k < N; k++)
                sum += L[k * N + i] * job->x[k * nrhs + j];

            job->x[i * nrhs + j] = (job->y[i * nrhs + j] - sum) / L[i * N + i];
        }
    }
}

bool _matrixf_cholesky_solve_scalar(LinAlgCtx* ctx, MatrixF* A, MatrixF* b, MatrixF* x)
{
    CholeskySolveJob job;
    MatrixF L = matrix_null();
    MatrixF y = matrix_null();

    const size_t N = A->N;

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

    y = matrixf_create(b->M, b->N);
    matrixf_resize(x, b->M, b->N);

    job.L = L.data;
    job.b = b->data;
    job.y = y.data;
    job.x = x->data;
    job.N = N;
    job.nrhs = b->N;

    /* Each right-hand side costs ~2 N^2 flops */
    linalg_parallel_for(ctx,
                        b->N,
                        LINALG_MAX((size_t)1, (size_t)(1 << 16) / LINALG_MAX((size_t)1, 2 * N * N)),
                        cholesky_solve_columns,
                        &job);

    matrixf_destroy(&y);
    matrixf_destroy(&L);

    return true;
}

typedef bool (*cholesky_solve_func)(LinAlgCtx*, MatrixF*, MatrixF*, MatrixF*);

#if defined(ROMANO_X86_64)

#define NUM_CHOL_SOLVE_FUNCS 5

/* TODO: vectorized kernels */
#define _matrixf_cholesky_solve_sse _matrixf_cholesky_solve_scalar
#define _matrixf_cholesky_solve_avx _matrixf_cholesky_solve_scalar
#define _matrixf_cholesky_solve_avx256 _matrixf_cholesky_solve_scalar
#define _matrixf_cholesky_solve_avx512 _matrixf_cholesky_solve_scalar

#elif defined(ROMANO_AARCH64) && defined(ROMANO_APPLE)

#define NUM_CHOL_SOLVE_FUNCS 2

/* Accelerate manages its own threads, the context is not used */
bool _matrixf_cholesky_solve_accelerate(LinAlgCtx* ctx, MatrixF* A, MatrixF* b, MatrixF* x)
{
    __LAPACK_int n = (__LAPACK_int)A->N;
    __LAPACK_int nrhs = (__LAPACK_int)b->N;
    __LAPACK_int info = 0;
    MatrixF Ac;
    MatrixF b_col_major;

    ROMANO_UNUSED(ctx);

    /*
     * LAPACK is column-major. A is symmetric so its layout does not matter, but b (n x nrhs,
     * row-major) has to be transposed, the row-major transpose being the column-major b
     */
    b_col_major = matrixf_transpose_from(NULL, b);

    /* sposv overwrites A with its factorization */
    Ac = matrixf_copy(A);

    sposv_("L", &n, &nrhs, Ac.data, &n, b_col_major.data, &n, &info);

    matrixf_destroy(&Ac);

    if(info != 0)
    {
        matrixf_destroy(&b_col_major);
        logger_log(LogLevel_Error, "Cholesky solve failed: info=%d (not positive definite)", (int)info);
        return false;
    }

    matrixf_destroy(x);
    *x = matrixf_transpose_from(NULL, &b_col_major);
    matrixf_destroy(&b_col_major);

    return true;
}

#elif defined(ROMANO_AARCH64)

#define NUM_CHOL_SOLVE_FUNCS 2
#define _matrixf_cholesky_solve_accelerate _matrixf_cholesky_solve_scalar

#else

#define NUM_CHOL_SOLVE_FUNCS 1

#endif /* defined(ROMANO_X86_64) */

static const cholesky_solve_func __cholesky_solver_funcs[NUM_CHOL_SOLVE_FUNCS] = {
    _matrixf_cholesky_solve_scalar,
#if defined(ROMANO_X86_64)
    _matrixf_cholesky_solve_sse,
    _matrixf_cholesky_solve_avx,
    _matrixf_cholesky_solve_avx256,
    _matrixf_cholesky_solve_avx512,
#elif defined(ROMANO_AARCH64)
    _matrixf_cholesky_solve_accelerate,
#endif /* defined(ROMANO_X86_64) */
};

bool matrixf_cholesky_solve(LinAlgCtx* ctx, MatrixF* A, MatrixF* b, MatrixF* x)
{
    return __cholesky_solver_funcs[simd_get_vectorization_mode()](ctx, A, b, x);
}
