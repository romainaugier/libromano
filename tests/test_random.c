/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/random.h"

#define NUM_SAMPLES 100000

static void test_float_01(void)
{
    double sum = 0.0;
    size_t i;

    for(i = 0; i < NUM_SAMPLES; i++)
    {
        const float f = random_next_float_01();

        TEST_ASSERT(f >= 0.0f && f <= 1.0f);
        sum += f;

        TEST_ASSERT(random_float_01((uint32_t)i) >= 0.0f);
    }

    TEST_CHECK_NEAR(sum / NUM_SAMPLES, 0.5, 0.01);
}

static void test_ranges(void)
{
    uint32_t histogram[10] = { 0 };
    size_t i;

    for(i = 0; i < NUM_SAMPLES; i++)
    {
        const uint32_t x = random_next_uint32_range(10, 20);

        TEST_ASSERT(x >= 10 && x <= 20);

        if(x < 20)
            histogram[x - 10]++;

        TEST_ASSERT(random_uint32_range((uint32_t)i, 5, 6) >= 5);
    }

    for(i = 0; i < 10; i++)
        TEST_CHECK_MSG(histogram[i] > NUM_SAMPLES / 20, "bucket %zu: %u", i, histogram[i]);
}

static void test_generators_vary(void)
{
    uint32_t bits_seen_32 = 0;
    uint64_t bits_seen_64 = 0;
    uint64_t bits_seen_wy = 0;
    uint64_t bits_seen_lehmer = 0;
    uint64_t previous = random_next_uint64();
    size_t repeats = 0;
    size_t i;

    for(i = 0; i < 1000; i++)
    {
        const uint64_t next = random_next_uint64();

        repeats += next == previous;
        previous = next;

        bits_seen_32 |= random_next_uint32();
        bits_seen_64 |= next;
#if !defined(ROMANO_MSVC)
        bits_seen_wy |= random_wyhash_64();
        bits_seen_lehmer |= random_lehmer_64();
#else
        bits_seen_wy = bits_seen_lehmer = UINT64_MAX;
#endif /* !defined(ROMANO_MSVC) */
    }

    TEST_CHECK_EQ_UINT(repeats, 0);
    TEST_CHECK_EQ_UINT(bits_seen_32, UINT32_MAX);
    TEST_CHECK_EQ_UINT(bits_seen_64, UINT64_MAX);
    TEST_CHECK_EQ_UINT(bits_seen_wy, UINT64_MAX);
    TEST_CHECK_EQ_UINT(bits_seen_lehmer, UINT64_MAX);
}

static void test_hash_functions(void)
{
    TEST_CHECK(murmur_64(1) != murmur_64(2));
    TEST_CHECK_EQ_UINT(murmur_64(0), 0);
    TEST_CHECK(wang_hash(0) != wang_hash(1));
    TEST_CHECK(xorshift32(1) != 1);
    TEST_CHECK_EQ_UINT(xorshift32(0), 0);
}

TEST_MAIN(
    TEST(test_float_01),
    TEST(test_ranges),
    TEST(test_generators_vary),
    TEST(test_hash_functions),
)
