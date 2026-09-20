/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/math/stats32.h"
#include "libromano/simd.h"

#define MAX_VALUES 4096

typedef struct Reference {
    double sum;
    double mean;
    double variance;
    double min;
    double max;
} Reference;

static void compute_reference(const float* values, size_t n, Reference* ref)
{
    size_t i;

    ref->sum = 0.0;
    ref->min = ref->max = values[0];

    for(i = 0; i < n; i++)
    {
        ref->sum += values[i];
        ref->min = values[i] < ref->min ? values[i] : ref->min;
        ref->max = values[i] > ref->max ? values[i] : ref->max;
    }

    ref->mean = ref->sum / (double)n;
    ref->variance = 0.0;

    for(i = 0; i < n; i++)
        ref->variance += (values[i] - ref->mean) * (values[i] - ref->mean);

    ref->variance /= (double)n;
}

static bool close_enough(double got, double expected, double scale)
{
    return fabs(got - expected) <= 1e-4 * (scale > 1.0 ? scale : 1.0);
}

static bool check_all_modes(const float* values, size_t n)
{
    const VectorizationMode initial = simd_get_vectorization_mode();
    Reference ref;
    int mode;
    bool ok = true;

    compute_reference(values, n, &ref);

    for(mode = 0; mode < VectorizationMode_COUNT && ok; mode++)
    {
        simd_force_vectorization_mode((VectorizationMode)mode);

        if((int)simd_get_vectorization_mode() != mode)
            continue;

        ok &= TEST_CHECK_MSG(close_enough(stats_sum(values, n), ref.sum, fabs(ref.sum) + n), "sum, mode %s, n %zu", simd_get_vectorization_mode_as_string((VectorizationMode)mode), n);
        ok &= TEST_CHECK_MSG(close_enough(stats_mean(values, n), ref.mean, fabs(ref.max) + fabs(ref.min)), "mean, mode %s, n %zu: %g vs %g",
                             simd_get_vectorization_mode_as_string((VectorizationMode)mode), n, stats_mean(values, n), ref.mean);
        ok &= TEST_CHECK_MSG(close_enough(stats_variance(values, n), ref.variance, ref.variance + 1.0), "variance, mode %s, n %zu: %g vs %g",
                             simd_get_vectorization_mode_as_string((VectorizationMode)mode), n, stats_variance(values, n), ref.variance);
        ok &= TEST_CHECK(close_enough(stats_std(values, n), sqrt(ref.variance), sqrt(ref.variance) + 1.0));
        ok &= TEST_CHECK_MSG(stats_min(values, n) == (float)ref.min, "min, mode %s, n %zu", simd_get_vectorization_mode_as_string((VectorizationMode)mode), n);
        ok &= TEST_CHECK_MSG(stats_max(values, n) == (float)ref.max, "max, mode %s, n %zu", simd_get_vectorization_mode_as_string((VectorizationMode)mode), n);
        ok &= TEST_CHECK(stats_range(values, n) == (float)(ref.max - ref.min));
    }

    simd_force_vectorization_mode(initial);

    return ok;
}

static void test_known_values(void)
{
    static const float values[] = { 2.0f, 4.0f, 4.0f, 4.0f, 5.0f, 5.0f, 7.0f, 9.0f };
    const size_t n = sizeof(values) / sizeof(values[0]);

    TEST_CHECK_NEAR(stats_sum(values, n), 40.0, 1e-6);
    TEST_CHECK_NEAR(stats_mean(values, n), 5.0, 1e-6);
    TEST_CHECK_NEAR(stats_variance(values, n), 4.0, 1e-5);
    TEST_CHECK_NEAR(stats_std(values, n), 2.0, 1e-5);
    TEST_CHECK_NEAR(stats_min(values, n), 2.0, 0.0);
    TEST_CHECK_NEAR(stats_max(values, n), 9.0, 0.0);
    TEST_CHECK_NEAR(stats_range(values, n), 7.0, 0.0);

    check_all_modes(values, n);
}

static void test_all_sizes(void)
{
    float values[40];
    size_t n;
    size_t i;

    for(n = 1; n <= 40; n++)
    {
        for(i = 0; i < n; i++)
            values[i] = (float)((i * 7919) % 97) - 48.0f;

        if(!check_all_modes(values, n))
            break;
    }
}

static bool property_stats(FuzzSource* source, void* user_data)
{
    static float values[MAX_VALUES];
    const size_t n = 1 + fuzz_size(source, MAX_VALUES - 1);
    const float offset = (float)fuzz_range_i64(source, -1000, 1000);
    size_t i;

    ROMANO_UNUSED(user_data);

    for(i = 0; i < n; i++)
        values[i] = offset + (float)fuzz_range_i64(source, -1000, 1000) / 8.0f;

    TEST_FUZZ_CHECK_MSG(check_all_modes(values, n), "stats mismatch for n = %zu", n);

    return true;
}

static void test_fuzz_stats(void)
{
    test_fuzz_property("stats32", 2000, property_stats, NULL);
}

TEST_MAIN(
    TEST(test_known_values),
    TEST(test_all_sizes),
    TEST(test_fuzz_stats),
)
