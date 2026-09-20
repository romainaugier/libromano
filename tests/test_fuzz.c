/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include <float.h>

static void test_seed_determinism(void)
{
    FuzzSource a;
    FuzzSource b;
    FuzzSource c;
    size_t i;
    bool differs = false;

    fuzz_source_init_seed(&a, 42);
    fuzz_source_init_seed(&b, 42);
    fuzz_source_init_seed(&c, 43);

    for(i = 0; i < 64; i++)
    {
        uint64_t x = fuzz_u64(&a);
        TEST_CHECK_EQ_UINT(x, fuzz_u64(&b));
        differs |= x != fuzz_u64(&c);
    }

    TEST_CHECK(differs);
    TEST_CHECK(!fuzz_source_exhausted(&a));
}

static void test_data_source(void)
{
    const uint8_t data[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xAA };
    uint8_t bytes[4];
    FuzzSource source;

    fuzz_source_init_data(&source, data, sizeof(data));

    TEST_CHECK_EQ_UINT(fuzz_u32(&source), 0x04030201u);
    TEST_CHECK_EQ_UINT(fuzz_u8(&source), 0x05);
    TEST_CHECK(!fuzz_source_exhausted(&source));

    fuzz_bytes(&source, bytes, sizeof(bytes));
    TEST_CHECK_EQ_UINT(bytes[0], 0x06);
    TEST_CHECK_EQ_UINT(bytes[3], 0xAA);
    TEST_CHECK(!fuzz_source_exhausted(&source));

    TEST_CHECK_EQ_UINT(fuzz_u64(&source), 0);
    TEST_CHECK(fuzz_source_exhausted(&source));

    fuzz_bytes(&source, bytes, sizeof(bytes));
    TEST_CHECK_EQ_UINT(bytes[0], 0);

    fuzz_source_init_data(&source, NULL, 10);
    TEST_CHECK_EQ_UINT(fuzz_range(&source, 5, 10), 5);
    TEST_CHECK(fuzz_source_exhausted(&source));
}

static void test_data_source_ranges(void)
{
    const uint8_t data[] = { 0xFF, 0xFF, 0x07, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80 };
    FuzzSource source;

    fuzz_source_init_data(&source, data, sizeof(data));

    TEST_CHECK_EQ_UINT(fuzz_range(&source, 0, 9), 0xFF % 10);
    TEST_CHECK_EQ_UINT(fuzz_range(&source, 0, 1000), 0x07FF % 1001);
    TEST_CHECK_EQ_UINT(fuzz_range(&source, 0, UINT64_MAX), 0x8000000000000010ULL);
    TEST_CHECK_EQ_UINT(fuzz_range(&source, 7, 7), 7);
    TEST_CHECK_EQ_UINT(fuzz_range(&source, 9, 3), 9);
}

static bool property_ranges(FuzzSource* source, void* user_data)
{
    uint64_t low = fuzz_u64_special(source);
    uint64_t high = low + fuzz_range(source, 0, UINT64_MAX - low);
    int64_t ilow = fuzz_i64_special(source);
    int64_t ihigh = ilow + (int64_t)fuzz_range(source, 0, (uint64_t)(INT64_MAX - (ilow > 0 ? ilow : 0)));
    uint64_t x = fuzz_range(source, low, high);
    int64_t y = fuzz_range_i64(source, ilow, ihigh);
    size_t count = fuzz_size(source, 1000);
    size_t max = fuzz_size(source, 1 << 20);
    double unit = fuzz_f64_unit(source);

    ROMANO_UNUSED(user_data);

    TEST_FUZZ_CHECK(x >= low && x <= high);
    TEST_FUZZ_CHECK(y >= ilow && y <= ihigh);
    TEST_FUZZ_CHECK(count == 0 || fuzz_index(source, count) < count);
    TEST_FUZZ_CHECK(fuzz_size(source, max) <= max);
    TEST_FUZZ_CHECK(unit >= 0.0 && unit < 1.0);
    TEST_FUZZ_CHECK(isfinite(fuzz_f64_finite(source)));

    return true;
}

static void test_ranges(void)
{
    test_fuzz_property("ranges", 20000, property_ranges, NULL);
}

static void test_range_distribution(void)
{
    uint32_t buckets[10] = { 0 };
    FuzzSource source;
    size_t i;

    fuzz_source_init_seed(&source, 1);

    for(i = 0; i < 100000; i++)
        buckets[fuzz_range(&source, 0, 9)]++;

    for(i = 0; i < 10; i++)
        TEST_CHECK_MSG(buckets[i] > 9000 && buckets[i] < 11000, "bucket %zu: %u", i, buckets[i]);
}

static void test_special_values(void)
{
    FuzzSource source;
    bool seen_u64_zero = false, seen_u64_max = false;
    bool seen_i64_min = false, seen_i64_max = false;
    bool seen_i32_min = false, seen_u32_max = false;
    bool seen_nan = false, seen_inf = false, seen_neg_zero = false, seen_subnormal = false;
    bool seen_one_in = false, seen_bool = false;
    size_t i;

    fuzz_source_init_seed(&source, 7);

    for(i = 0; i < 20000; i++)
    {
        uint64_t u = fuzz_u64_special(&source);
        int64_t s = fuzz_i64_special(&source);
        double d = fuzz_f64_special(&source);

        seen_u64_zero |= u == 0;
        seen_u64_max |= u == UINT64_MAX;
        seen_i64_min |= s == INT64_MIN;
        seen_i64_max |= s == INT64_MAX;
        seen_i32_min |= fuzz_i32_special(&source) == INT32_MIN;
        seen_u32_max |= fuzz_u32_special(&source) == UINT32_MAX;
        seen_nan |= isnan(d);
        seen_inf |= isinf(d);
        seen_neg_zero |= d == 0.0 && signbit(d);
        seen_subnormal |= d != 0.0 && fabs(d) < DBL_MIN;
        seen_one_in |= fuzz_one_in(&source, 100);
        seen_bool |= fuzz_bool(&source);
    }

    TEST_CHECK(seen_u64_zero && seen_u64_max);
    TEST_CHECK(seen_i64_min && seen_i64_max);
    TEST_CHECK(seen_i32_min && seen_u32_max);
    TEST_CHECK(seen_nan && seen_inf && seen_neg_zero && seen_subnormal);
    TEST_CHECK(seen_one_in && seen_bool);
    TEST_CHECK(fuzz_one_in(&source, 1));
}

static void test_strings(void)
{
    FuzzSource source;
    char buffer[65];
    bool seen_empty = false;
    bool seen_full = false;
    size_t i;
    size_t j;

    fuzz_source_init_seed(&source, 3);

    for(i = 0; i < 2000; i++)
    {
        size_t size = fuzz_string(&source, buffer, 64, FUZZ_ALPHABET_HEX);

        TEST_ASSERT(size <= 64);
        TEST_CHECK_EQ_UINT(strlen(buffer), size);

        for(j = 0; j < size; j++)
            TEST_ASSERT(strchr(FUZZ_ALPHABET_HEX, buffer[j]) != NULL);

        seen_empty |= size == 0;
        seen_full |= size == 64;

        size = fuzz_string(&source, buffer, 64, NULL);
        TEST_CHECK_EQ_UINT(strlen(buffer), size);
    }

    TEST_CHECK(seen_empty && seen_full);
    TEST_CHECK_EQ_UINT(fuzz_string(&source, buffer, 0, NULL), 0);
    TEST_CHECK_EQ_UINT(fuzz_size(&source, 0), 0);
}

static bool contains(const uint8_t* data, size_t size, const char* token)
{
    size_t token_size = strlen(token);
    size_t i;

    for(i = 0; i + token_size <= size; i++)
        if(memcmp(data + i, token, token_size) == 0)
            return true;

    return false;
}

static void test_mutate(void)
{
    static const char* const tokens[] = { "TOKEN", "{}" };
    FuzzDictionary dictionary = { tokens, 2 };
    uint8_t buffer[128];
    FuzzSource source;
    bool seen_token = false;
    bool seen_growth = false;
    bool seen_shrink = false;
    size_t size = 0;
    size_t i;

    fuzz_source_init_seed(&source, 11);

    TEST_CHECK_EQ_UINT(fuzz_mutate(&source, buffer, 0, 0, NULL), 0);

    for(i = 0; i < 20000; i++)
    {
        size_t new_size = fuzz_mutate(&source, buffer, size, sizeof(buffer), i % 2 == 0 ? &dictionary : NULL);

        TEST_ASSERT(new_size <= sizeof(buffer));

        seen_growth |= new_size > size;
        seen_shrink |= new_size < size;
        seen_token |= contains(buffer, new_size, "TOKEN");

        size = new_size;

        if(i % 997 == 0)
            size = 0;
    }

    TEST_CHECK(seen_growth && seen_shrink && seen_token);
    TEST_CHECK_EQ_UINT(fuzz_mutate(&source, buffer, 200, sizeof(buffer), NULL) <= sizeof(buffer), 1);
}

/* These tests check exact iteration counts and seeds, the environment overrides must not apply */
static void clear_fuzz_environment(void)
{
    test_unsetenv("ROMANO_FUZZ_SEED");
    test_unsetenv("ROMANO_FUZZ_ITERATIONS");
    test_unsetenv("ROMANO_FUZZ_SCALE");
    test_unsetenv("ROMANO_FUZZ_SECONDS");
    test_unsetenv("ROMANO_FUZZ_REPLAY");
}

static uint64_t g_property_calls = 0;

static bool property_counting(FuzzSource* source, void* user_data)
{
    ROMANO_UNUSED(source);
    g_property_calls++;
    return *(uint64_t*)user_data == 0 || g_property_calls < *(uint64_t*)user_data;
}

static void test_run_property(void)
{
    FuzzOptions options;
    FuzzReport report;
    uint64_t fail_at = 0;

    clear_fuzz_environment();

    fuzz_options_init(&options, "counting");
    options.iterations = 50;

    g_property_calls = 0;
    TEST_CHECK(fuzz_run_property(&options, property_counting, &fail_at, &report));
    TEST_CHECK_EQ_UINT(report.iterations, 50);
    TEST_CHECK_EQ_UINT(g_property_calls, 50);
    TEST_CHECK(!report.failed);
    TEST_CHECK(fuzz_current_run() == NULL);
    fuzz_report_release(&report);

    fail_at = 10;
    g_property_calls = 0;
    logger_log_info("expecting a fuzz failure report below");
    TEST_CHECK(!fuzz_run_property(&options, property_counting, &fail_at, &report));
    TEST_CHECK(report.failed);
    TEST_CHECK_EQ_UINT(report.failing_iteration, 9);
    TEST_CHECK_EQ_UINT(report.seed, FUZZ_DEFAULT_SEED);
    fuzz_report_release(&report);

    g_property_calls = 0;
    fail_at = 0;
    TEST_CHECK(fuzz_run_property(NULL, property_counting, &fail_at, NULL));
    TEST_CHECK_EQ_UINT(g_property_calls, FUZZ_DEFAULT_ITERATIONS);
}

typedef struct ReplayState {
    uint64_t target_iteration;
    uint64_t observed;
    uint64_t calls;
} ReplayState;

static bool property_replay(FuzzSource* source, void* user_data)
{
    ReplayState* state = (ReplayState*)user_data;
    const FuzzRunInfo* run = fuzz_current_run();
    uint64_t value = fuzz_u64(source);

    state->calls++;

    if(run->iteration == state->target_iteration)
    {
        state->observed = value;
        return false;
    }

    return true;
}

static void test_replay(void)
{
    ReplayState state = { 17, 0, 0 };
    FuzzOptions options;
    FuzzReport report;
    FuzzSource source;
    char replay_seed[32];

    clear_fuzz_environment();

    fuzz_options_init(&options, "replay");
    options.iterations = 100;
    options.catch_crashes = false;

    logger_log_info("expecting fuzz failure reports below");
    TEST_ASSERT(!fuzz_run_property(&options, property_replay, &state, &report));

    fuzz_source_init_seed(&source, report.failing_seed);
    TEST_CHECK_EQ_UINT(fuzz_u64(&source), state.observed);

    snprintf(replay_seed, sizeof(replay_seed), "0x%llx", (unsigned long long)report.failing_seed);
    test_setenv("ROMANO_FUZZ_REPLAY", replay_seed);

    state.target_iteration = 0;
    state.calls = 0;
    state.observed = 0;
    TEST_CHECK(!fuzz_run_property(&options, property_replay, &state, NULL));
    TEST_CHECK_EQ_UINT(state.calls, 1);
    fuzz_source_init_seed(&source, report.failing_seed);
    TEST_CHECK_EQ_UINT(fuzz_u64(&source), state.observed);

    test_unsetenv("ROMANO_FUZZ_REPLAY");
    fuzz_report_release(&report);
}

static void test_env_overrides(void)
{
    FuzzOptions options;
    FuzzReport report;
    uint64_t never = 0;

    clear_fuzz_environment();

    fuzz_options_init(&options, "env");
    options.iterations = 10;

    test_setenv("ROMANO_FUZZ_SEED", "1234");
    test_setenv("ROMANO_FUZZ_ITERATIONS", "7");
    test_setenv("ROMANO_FUZZ_SCALE", "3");
    fuzz_run_property(&options, property_counting, &never, &report);
    TEST_CHECK_EQ_UINT(report.seed, 1234);
    TEST_CHECK_EQ_UINT(report.iterations, 21);

    test_setenv("ROMANO_FUZZ_SEED", "random");
    test_unsetenv("ROMANO_FUZZ_ITERATIONS");
    test_unsetenv("ROMANO_FUZZ_SCALE");
    fuzz_run_property(&options, property_counting, &never, &report);
    TEST_CHECK(report.seed != FUZZ_DEFAULT_SEED);
    TEST_CHECK_EQ_UINT(report.iterations, 10);

    test_unsetenv("ROMANO_FUZZ_SEED");
    test_setenv("ROMANO_FUZZ_SECONDS", "0.000001");
    options.iterations = UINT64_MAX;
    fuzz_run_property(&options, property_counting, &never, &report);
    TEST_CHECK(report.iterations < UINT64_MAX);
    test_unsetenv("ROMANO_FUZZ_SECONDS");

    fuzz_report_release(&report);
}

static bool target_rejects_marker(const uint8_t* data, size_t size, void* user_data)
{
    ROMANO_UNUSED(user_data);
    return memchr(data, 0xAB, size) == NULL || size < 3;
}

static bool target_accepts_all(const uint8_t* data, size_t size, void* user_data)
{
    size_t* max_size = (size_t*)user_data;

    ROMANO_UNUSED(data);

    if(size > *max_size)
        *max_size = size;

    return true;
}

static void test_run_input(void)
{
    static const char* const tokens[] = { "abc" };
    static const char corpus_data[] = "hello corpus";
    FuzzCorpusEntry corpus = { corpus_data, sizeof(corpus_data) - 1 };
    FuzzDictionary dictionary = { tokens, 1 };
    FuzzOptions options;
    FuzzReport report;
    size_t max_size = 0;

    clear_fuzz_environment();

    fuzz_options_init(&options, "accept");
    options.iterations = 500;
    options.max_input_size = 64;
    options.corpus = &corpus;
    options.corpus_count = 1;
    options.dictionary = &dictionary;

    TEST_CHECK(fuzz_run_input(&options, target_accepts_all, &max_size, &report));
    TEST_CHECK(max_size <= 64 && max_size > 0);
    fuzz_report_release(&report);

    fuzz_options_init(&options, "marker");
    options.iterations = 100000;
    options.max_input_size = 256;

    logger_log_info("expecting a fuzz failure report below");
    TEST_ASSERT(!fuzz_run_input(&options, target_rejects_marker, NULL, &report));
    TEST_CHECK(report.failing_input != NULL);
    TEST_CHECK_EQ_UINT(report.failing_input_size, 3);
    TEST_CHECK(!target_rejects_marker(report.failing_input, report.failing_input_size, NULL));
    fuzz_report_release(&report);
    TEST_CHECK(report.failing_input == NULL);

    options.minimize = false;
    options.max_input_size = 0;
    TEST_CHECK(!fuzz_run_input(&options, target_rejects_marker, NULL, NULL));
    fuzz_report_release(NULL);
}

static void test_log_input(void)
{
    uint8_t data[300];
    size_t i;

    for(i = 0; i < sizeof(data); i++)
        data[i] = (uint8_t)i;

    fuzz_log_input(data, 20, 64);
    fuzz_log_input(data, sizeof(data), 32);
    TEST_CHECK(true);
}

TEST_MAIN(
    TEST(test_seed_determinism),
    TEST(test_data_source),
    TEST(test_data_source_ranges),
    TEST(test_ranges),
    TEST(test_range_distribution),
    TEST(test_special_values),
    TEST(test_strings),
    TEST(test_mutate),
    TEST(test_run_property),
    TEST(test_replay),
    TEST(test_env_overrides),
    TEST(test_run_input),
    TEST(test_log_input),
)
