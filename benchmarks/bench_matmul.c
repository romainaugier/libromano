/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/*
 * Matrix multiplication benchmark (matrixf_mul), over a set of shapes, every vectorization mode
 * supported by the cpu and several thread counts.
 *
 *   bench_matmul                              default suite, all modes, 1 thread and all threads
 *   bench_matmul --suite quick                a few shapes, fast sanity run
 *   bench_matmul --suite full                 adds big squares (4096) and more shapes
 *   bench_matmul --case square                only the cases whose name contains "square"
 *   bench_matmul --modes avx2,avx512          only these kernels
 *   bench_matmul --threads 1,2,4,8,max        thread scaling
 *   bench_matmul --check                      also verify the results against a reference
 *   bench_matmul --csv results.csv            also write every measurement as CSV
 *   bench_matmul --list                       list the cases of the suite and exit
 *
 * Each measurement is repeated until --min-time seconds are spent (after one warmup run), and
 * the best and median times are kept. GFLOP/s = 2 * M * N * P / time.
 */

#include "libromano/math/linalg32.h"
#include "libromano/cpu.h"
#include "libromano/simd.h"
#include "libromano/thread.h"
#include "libromano/threadpool.h"
#include "libromano/cli.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Cases */

typedef enum {
    Suite_Quick = 0,
    Suite_Default = 1,
    Suite_Full = 2,
} Suite;

typedef struct BenchCase {
    const char* name;
    uint32_t M; /* rows of A and C */
    uint32_t N; /* columns of A, rows of B (the reduction depth) */
    uint32_t P; /* columns of B and C */
    Suite suite; /* smallest suite containing the case */
} BenchCase;

static const BenchCase g_cases[] = {
    /* Squares */
    { "square_64",          64,    64,    64,   Suite_Default },
    { "square_128",         128,   128,   128,  Suite_Default },
    { "square_256",         256,   256,   256,  Suite_Quick },
    { "square_384",         384,   384,   384,  Suite_Default },
    { "square_512",         512,   512,   512,  Suite_Quick },
    { "square_768",         768,   768,   768,  Suite_Default },
    { "square_1024",        1024,  1024,  1024, Suite_Quick },
    { "square_1536",        1536,  1536,  1536, Suite_Default },
    { "square_2048",        2048,  2048,  2048, Suite_Default },
    { "square_3072",        3072,  3072,  3072, Suite_Full },
    { "square_4096",        4096,  4096,  4096, Suite_Full },

    /* Sizes that are not multiples of the register tiles (edge tiles everywhere) */
    { "odd_253",            257,   255,   253,  Suite_Default },
    { "odd_1001",           1001,  999,   1003, Suite_Quick },
    { "odd_2047",           2047,  2049,  2045, Suite_Full },

    /* Shallow products: small depth, packing and C traffic dominate */
    { "small_k_16",         2048,  16,    2048, Suite_Default },
    { "small_k_64",         4000,  64,    4000, Suite_Quick },
    { "small_k_256",        3000,  256,   3000, Suite_Default },

    /* Deep products: small output, long reduction */
    { "large_k_64",         64,    4000,  64,   Suite_Quick },
    { "large_k_256",        256,   8192,  256,  Suite_Default },
    { "large_k_512",        512,   16384, 512,  Suite_Full },

    /* Rectangular */
    { "tall_skinny",        8192,  512,   64,   Suite_Default },
    { "tall_wide_k",        8192,  64,    512,  Suite_Default },
    { "short_wide",         64,    512,   8192, Suite_Default },
    { "rect_1500x300",      1500,  1500,  300,  Suite_Default },
    { "rect_300x1500",      300,   1500,  1500, Suite_Full },

    /* Matrix-vector and vector-matrix (degenerate gemm, memory bound) */
    { "matvec_4096",        4096,  4096,  1,    Suite_Default },
    { "vecmat_4096",        1,     4096,  4096, Suite_Default },

    /* Many small products are dominated by the per-call overhead */
    { "tiny_16",            16,    16,    16,   Suite_Default },
    { "tiny_32",            32,    32,    32,   Suite_Default },
};

#define NUM_CASES (sizeof(g_cases) / sizeof(g_cases[0]))

/* Timing */

static double bench_now(void)
{
#if defined(ROMANO_WIN)
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);

    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif /* defined(ROMANO_WIN) */
}

static int compare_doubles(const void* a, const void* b)
{
    const double x = *(const double*)a;
    const double y = *(const double*)b;

    return (x > y) - (x < y);
}

/* Data */

static uint64_t g_rng = 0x2545F4914F6CDD1Dull;

static uint32_t rng_next(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (uint32_t)(g_rng >> 32);
}

/* Uniform in [-1, 1] */
static void fill_matrix(MatrixF* A)
{
    size_t i;
    const size_t n = (size_t)A->M * A->N;

    for(i = 0; i < n; i++)
        A->data[i] = (float)rng_next() / 2147483648.0f - 1.0f;
}

/*
 * Checks a sample of entries of C against a double precision reference, with a tolerance
 * relative to the magnitude of the terms (float accumulation over N terms)
 */
static bool check_result(MatrixF* A, MatrixF* B, MatrixF* C)
{
    const uint32_t samples = 256;
    uint32_t s;

    if(C->M != A->M || C->N != B->N)
        return false;

    for(s = 0; s < samples; s++)
    {
        const uint32_t i = rng_next() % A->M;
        const uint32_t j = rng_next() % B->N;
        double expected = 0.0;
        double magnitude = 0.0;
        uint32_t k;

        for(k = 0; k < A->N; k++)
        {
            const double t = (double)A->data[(size_t)i * A->N + k] * (double)B->data[(size_t)k * B->N + j];
            expected += t;
            magnitude += fabs(t);
        }

        /*
         * Rounding errors of a float sum of N random-sign terms grow like sqrt(N) * eps (random
         * walk), a 16x margin on that still catches a single missing or doubled term
         */
        if(fabs((double)C->data[(size_t)i * C->N + j] - expected) >
           16.0 * sqrt((double)A->N) * 1.2e-7 * magnitude + 1e-6)
            return false;
    }

    return true;
}

/* Options */

typedef struct Options {
    Suite suite;
    const char* case_filter;
    char case_filter_buffer[128];
    char csv_path_buffer[1024];
    bool modes[VectorizationMode_COUNT];
    uint32_t threads[16];
    uint32_t num_threads;
    double min_time;
    double scalar_max_gflop;
    bool check;
    bool list;
    const char* csv_path;
} Options;

static void lowercase(char* dst, const char* src, size_t size)
{
    size_t i;

    for(i = 0; i + 1 < size && src[i] != '\0'; i++)
        dst[i] = (src[i] >= 'A' && src[i] <= 'Z') ? (char)(src[i] - 'A' + 'a') : src[i];

    dst[i] = '\0';
}

/* Returns the vectorization mode named by token ("sse", "avx2", "avx512", "neon"...), -1 if none */
static int parse_mode(const char* token)
{
    char name[32];
    int mode;

    if(strcmp(token, "avx2") == 0)
        token = "avx256";

    for(mode = 0; mode < (int)VectorizationMode_COUNT; mode++)
    {
        lowercase(name, simd_get_vectorization_mode_as_string((VectorizationMode)mode), sizeof(name));

        if(strcmp(name, token) == 0)
            return mode;
    }

    return -1;
}

static bool mode_is_supported(int mode)
{
    const VectorizationMode initial = simd_get_vectorization_mode();
    bool supported;

    simd_force_vectorization_mode((VectorizationMode)mode);
    supported = (int)simd_get_vectorization_mode() == mode;
    simd_force_vectorization_mode(initial);

    return supported;
}

static bool parse_modes(Options* options, const char* str)
{
    char buffer[256];
    char* token;
    int mode;

    memset(options->modes, 0, sizeof(options->modes));

    if(str == NULL || strcmp(str, "all") == 0)
    {
        for(mode = 0; mode < (int)VectorizationMode_COUNT; mode++)
            options->modes[mode] = true;

        return true;
    }

    lowercase(buffer, str, sizeof(buffer));

    for(token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ","))
    {
        mode = parse_mode(token);

        if(mode < 0)
        {
            fprintf(stderr, "Unknown mode \"%s\" (expected scalar, sse, avx, avx2, avx512, neon or all)\n", token);
            return false;
        }

        options->modes[mode] = true;
    }

    return true;
}

static bool parse_threads(Options* options, const char* str)
{
    char buffer[256];
    char* token;
    const uint32_t max_threads = (uint32_t)get_num_procs();

    options->num_threads = 0;

    snprintf(buffer, sizeof(buffer), "%s", str != NULL ? str : "1,max");

    for(token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ","))
    {
        uint32_t threads;
        uint32_t i;
        bool duplicate = false;

        if(strcmp(token, "max") == 0)
        {
            threads = max_threads;
        }
        else
        {
            char* end = NULL;
            const long value = strtol(token, &end, 10);

            if(end == token || *end != '\0' || value < 1)
            {
                fprintf(stderr, "Invalid thread count \"%s\" (expected a number >= 1 or max)\n", token);
                return false;
            }

            threads = (uint32_t)value;
        }

        for(i = 0; i < options->num_threads; i++)
            duplicate |= options->threads[i] == threads;

        if(!duplicate && options->num_threads < sizeof(options->threads) / sizeof(options->threads[0]))
            options->threads[options->num_threads++] = threads;
    }

    return options->num_threads > 0;
}

static bool parse_options(Options* options, int argc, char** argv)
{
    CLIParser parser;
    char* str;
    double* f64;
    bool* flag;
    bool ok = true;

    memset(options, 0, sizeof(Options));
    options->suite = Suite_Default;
    options->min_time = 0.3;
    options->scalar_max_gflop = 2.0;

    cli_parser_init(&parser);
    cli_parser_set_program_info(&parser,
                                "bench_matmul",
                                "Benchmark of matrixf_mul over several shapes, kernels and thread counts");

    cli_parser_add_arg(&parser, "suite", 0, 's', CLIArgMode_Optional, CLIArgType_Str, CLIArgAction_Store,
                       "Set of cases: quick, default or full (default: default)");
    cli_parser_add_arg(&parser, "case", 0, 'c', CLIArgMode_Optional, CLIArgType_Str, CLIArgAction_Store,
                       "Only run the cases whose name contains this string");
    cli_parser_add_arg(&parser, "modes", 0, 'm', CLIArgMode_Optional, CLIArgType_Str, CLIArgAction_Store,
                       "Comma separated kernels: scalar, sse, avx, avx2, avx512, neon, all (default: all supported)");
    cli_parser_add_arg(&parser, "threads", 0, 't', CLIArgMode_Optional, CLIArgType_Str, CLIArgAction_Store,
                       "Comma separated total thread counts, max = all logical cpus (default: 1,max)");
    cli_parser_add_arg(&parser, "min-time", 0, CLI_NO_SHORT_NAME, CLIArgMode_Optional, CLIArgType_Float, CLIArgAction_Store,
                       "Seconds spent per measurement (default: 0.3)");
    cli_parser_add_arg(&parser, "scalar-max-gflop", 0, CLI_NO_SHORT_NAME, CLIArgMode_Optional, CLIArgType_Float, CLIArgAction_Store,
                       "Skip the scalar kernel on cases above this many GFLOP, it is very slow (default: 2)");
    cli_parser_add_arg(&parser, "check", 0, CLI_NO_SHORT_NAME, CLIArgMode_Optional, CLIArgType_Bool, CLIArgAction_StoreTrue,
                       "Verify every result against a double precision reference (sampled)");
    cli_parser_add_arg(&parser, "csv", 0, CLI_NO_SHORT_NAME, CLIArgMode_Optional, CLIArgType_Str, CLIArgAction_Store,
                       "Also write every measurement to this CSV file");
    cli_parser_add_arg(&parser, "list", 0, 'l', CLIArgMode_Optional, CLIArgType_Bool, CLIArgAction_StoreTrue,
                       "List the cases of the suite and exit");

    if(!cli_parser_parse(&parser, argc, argv))
    {
        cli_parser_release(&parser);
        return false;
    }

    /* The getters return a zeroed value for options that were not passed: check presence first */
#define HAS_ARG(name) cli_parser_has_arg(&parser, name, 0)

    if(HAS_ARG("suite") && (str = cli_parser_arg_get_str(&parser, "suite", 0, NULL)) != NULL)
    {
        if(strcmp(str, "quick") == 0)
            options->suite = Suite_Quick;
        else if(strcmp(str, "default") == 0)
            options->suite = Suite_Default;
        else if(strcmp(str, "full") == 0)
            options->suite = Suite_Full;
        else
        {
            fprintf(stderr, "Unknown suite \"%s\" (expected quick, default or full)\n", str);
            ok = false;
        }
    }

    if(HAS_ARG("case") && (str = cli_parser_arg_get_str(&parser, "case", 0, NULL)) != NULL)
    {
        snprintf(options->case_filter_buffer, sizeof(options->case_filter_buffer), "%s", str);
        options->case_filter = options->case_filter_buffer;
    }

    ok = ok && parse_modes(options, HAS_ARG("modes") ? cli_parser_arg_get_str(&parser, "modes", 0, NULL) : NULL);
    ok = ok && parse_threads(options, HAS_ARG("threads") ? cli_parser_arg_get_str(&parser, "threads", 0, NULL) : NULL);

    if(HAS_ARG("min-time") && (f64 = cli_parser_arg_get_f64(&parser, "min-time", 0)) != NULL)
        options->min_time = *f64 > 0.0 ? *f64 : 0.0;

    if(HAS_ARG("scalar-max-gflop") && (f64 = cli_parser_arg_get_f64(&parser, "scalar-max-gflop", 0)) != NULL)
        options->scalar_max_gflop = *f64;

    if(HAS_ARG("check") && (flag = cli_parser_arg_get_bool(&parser, "check", 0)) != NULL)
        options->check = *flag;

    if(HAS_ARG("list") && (flag = cli_parser_arg_get_bool(&parser, "list", 0)) != NULL)
        options->list = *flag;

    if(HAS_ARG("csv") && (str = cli_parser_arg_get_str(&parser, "csv", 0, NULL)) != NULL)
    {
        snprintf(options->csv_path_buffer, sizeof(options->csv_path_buffer), "%s", str);
        options->csv_path = options->csv_path_buffer;
    }

#undef HAS_ARG

    cli_parser_release(&parser);

    return ok;
}

static bool case_selected(const Options* options, const BenchCase* bench_case)
{
    if(bench_case->suite > options->suite)
        return false;

    return options->case_filter == NULL || strstr(bench_case->name, options->case_filter) != NULL;
}

/* Measurements */

typedef struct Result {
    double best_gflops;
    double median_gflops;
    double best_ms;
    double median_ms;
    uint32_t reps;
    bool ran;
    bool checked;
    bool check_ok;
} Result;

#define MAX_REPS 1000

static void run_case(const Options* options,
                     LinAlgCtx* ctx,
                     MatrixF* A,
                     MatrixF* B,
                     MatrixF* C,
                     Result* result)
{
    static double times[MAX_REPS];
    const double flop = 2.0 * (double)A->M * (double)A->N * (double)B->N;
    const double start = bench_now();
    uint32_t reps = 0;

    /* Warmup: allocates C, faults the pages in, wakes the pool up */
    matrixf_mul(ctx, A, B, C);

    do
    {
        const double t0 = bench_now();
        matrixf_mul(ctx, A, B, C);
        times[reps++] = bench_now() - t0;
    }
    while(reps < MAX_REPS && (reps < 3 || bench_now() - start < options->min_time) &&
          !(reps >= 1 && bench_now() - start > 10.0 * options->min_time + 1.0));

    qsort(times, reps, sizeof(double), compare_doubles);

    result->ran = true;
    result->reps = reps;
    result->best_ms = times[0] * 1e3;
    result->median_ms = times[reps / 2] * 1e3;
    result->best_gflops = flop / times[0] * 1e-9;
    result->median_gflops = flop / times[reps / 2] * 1e-9;

    if(options->check)
    {
        result->checked = true;
        result->check_ok = check_result(A, B, C);
    }
}

/* Output */

static void print_header(void)
{
    const char* names[CPUCacheLevel_COUNT] = { "L1d", "L2", "L3" };
    int level;

    printf("==============================================================================\n");
    printf(" libromano matrixf_mul benchmark\n");
    printf("==============================================================================\n");

    cpu_print_features();

    printf("\nLogical cpus : %u\n", (uint32_t)get_num_procs());
    printf("Max SIMD     : %s\n", simd_get_vectorization_mode_as_string(simd_get_vectorization_mode()));

    for(level = 0; level < CPUCacheLevel_COUNT; level++)
    {
        const size_t size = cpu_get_cache_size((CPUCacheLevel)level);

        if(size == 0)
            printf("%-12s : unknown (the sgemm blocking uses a default)\n", names[level]);
    }

    printf("\n");
}

int main(int argc, char** argv)
{
    static Options options;
    static Result results[NUM_CASES][16][VectorizationMode_COUNT];
    FILE* csv = NULL;
    const VectorizationMode initial_mode = simd_get_vectorization_mode();
    size_t c;
    uint32_t t;
    int mode;
    bool any_check_failed = false;

    if(!parse_options(&options, argc, argv))
        return 1;

    if(options.list)
    {
        for(c = 0; c < NUM_CASES; c++)
        {
            const BenchCase* bench_case = &g_cases[c];

            if(case_selected(&options, bench_case))
                printf("%-16s %6u x %6u x %6u  %9.3f GFLOP\n",
                       bench_case->name, bench_case->M, bench_case->N, bench_case->P,
                       2.0 * bench_case->M * bench_case->N * bench_case->P * 1e-9);
        }

        return 0;
    }

    print_header();

    for(mode = 0; mode < (int)VectorizationMode_COUNT; mode++)
    {
        if(options.modes[mode] && !mode_is_supported(mode))
        {
            printf("Kernel %s is not supported by this cpu, skipped\n",
                   simd_get_vectorization_mode_as_string((VectorizationMode)mode));
            options.modes[mode] = false;
        }
    }

    if(options.csv_path != NULL)
    {
        csv = fopen(options.csv_path, "w");

        if(csv == NULL)
        {
            fprintf(stderr, "Cannot open %s for writing\n", options.csv_path);
            return 1;
        }

        fprintf(csv, "case,M,N,P,gflop,threads,mode,best_gflops,median_gflops,best_ms,median_ms,reps,check\n");
    }

    memset(results, 0, sizeof(results));

    for(t = 0; t < options.num_threads; t++)
    {
        const uint32_t threads = options.threads[t];
        ThreadPool* pool = NULL;
        LinAlgCtx ctx = linalg_ctx_new(NULL);

        /* The calling thread takes part in the work: T threads = pool of T - 1 workers */
        if(threads > 1)
        {
            pool = threadpool_init(threads - 1);

            if(pool == NULL)
            {
                fprintf(stderr, "Cannot create a pool of %u workers\n", threads - 1);
                return 1;
            }

            ctx = linalg_ctx_new(pool);
        }

        printf("Threads: %u   (GFLOP/s, best of the runs; '-' = skipped)\n\n", threads);
        printf("%-16s %20s %9s", "case", "M x N x P", "GFLOP");

        for(mode = 0; mode < (int)VectorizationMode_COUNT; mode++)
            if(options.modes[mode])
                printf(" %9s", simd_get_vectorization_mode_as_string((VectorizationMode)mode));

        printf("\n");

        for(c = 0; c < NUM_CASES; c++)
        {
            const BenchCase* bench_case = &g_cases[c];
            const double gflop = 2.0 * bench_case->M * bench_case->N * bench_case->P * 1e-9;
            MatrixF A;
            MatrixF B;
            MatrixF C = matrix_null();
            char shape[32];

            if(!case_selected(&options, bench_case))
                continue;

            A = matrixf_create((int)bench_case->M, (int)bench_case->N);
            B = matrixf_create((int)bench_case->N, (int)bench_case->P);

            if(A.data == NULL || B.data == NULL)
            {
                printf("%-16s cannot allocate the matrices, skipped\n", bench_case->name);
                matrixf_destroy(&A);
                matrixf_destroy(&B);
                continue;
            }

            fill_matrix(&A);
            fill_matrix(&B);

            snprintf(shape, sizeof(shape), "%ux%ux%u", bench_case->M, bench_case->N, bench_case->P);
            printf("%-16s %20s %9.3f", bench_case->name, shape, gflop);
            fflush(stdout);

            for(mode = 0; mode < (int)VectorizationMode_COUNT; mode++)
            {
                Result* result = &results[c][t][mode];

                if(!options.modes[mode])
                    continue;

                if(mode == VectorizationMode_Scalar && gflop > options.scalar_max_gflop)
                {
                    printf(" %9s", "-");
                    fflush(stdout);
                    continue;
                }

                simd_force_vectorization_mode((VectorizationMode)mode);

                run_case(&options, pool != NULL ? &ctx : NULL, &A, &B, &C, result);

                if(result->checked && !result->check_ok)
                {
                    printf(" %9s", "FAIL");
                    any_check_failed = true;
                }
                else
                {
                    printf(" %9.1f", result->best_gflops);
                }

                fflush(stdout);

                if(csv != NULL)
                    fprintf(csv, "%s,%u,%u,%u,%.6f,%u,%s,%.3f,%.3f,%.4f,%.4f,%u,%s\n",
                            bench_case->name, bench_case->M, bench_case->N, bench_case->P, gflop,
                            threads, simd_get_vectorization_mode_as_string((VectorizationMode)mode),
                            result->best_gflops, result->median_gflops,
                            result->best_ms, result->median_ms, result->reps,
                            result->checked ? (result->check_ok ? "ok" : "fail") : "");
            }

            simd_force_vectorization_mode(initial_mode);

            printf("\n");

            matrixf_destroy(&A);
            matrixf_destroy(&B);
            matrixf_destroy(&C);
        }

        printf("\n");

        if(pool != NULL)
            threadpool_release(pool);
    }

    /* Thread scaling of the fastest kernel, relative to the smallest thread count */
    if(options.num_threads > 1)
    {
        printf("Scaling of the fastest kernel, relative to %u thread%s\n\n",
               options.threads[0], options.threads[0] > 1 ? "s" : "");
        printf("%-16s", "case");

        for(t = 1; t < options.num_threads; t++)
            printf("   %3u thr (eff.)", options.threads[t]);

        printf("\n");

        for(c = 0; c < NUM_CASES; c++)
        {
            double base = 0.0;
            int best_mode = -1;

            if(!case_selected(&options, &g_cases[c]))
                continue;

            for(mode = 0; mode < (int)VectorizationMode_COUNT; mode++)
            {
                if(results[c][0][mode].ran && results[c][0][mode].best_gflops > base)
                {
                    base = results[c][0][mode].best_gflops;
                    best_mode = mode;
                }
            }

            if(best_mode < 0)
                continue;

            printf("%-16s", g_cases[c].name);

            for(t = 1; t < options.num_threads; t++)
            {
                const Result* result = &results[c][t][best_mode];

                if(result->ran)
                {
                    const double speedup = result->best_gflops / base;
                    const double ideal = (double)options.threads[t] / (double)options.threads[0];

                    printf("   %5.2fx (%3.0f%%)", speedup, 100.0 * speedup / ideal);
                }
                else
                {
                    printf("   %15s", "-");
                }
            }

            printf("   [%s]\n", simd_get_vectorization_mode_as_string((VectorizationMode)best_mode));
        }

        printf("\n");
    }

    if(csv != NULL)
    {
        fclose(csv);
        printf("Results written to %s\n", options.csv_path);
    }

    if(options.check)
        printf("Check: %s\n", any_check_failed ? "FAILED (see FAIL entries)" : "all results correct");

    return any_check_failed ? 1 : 0;
}
