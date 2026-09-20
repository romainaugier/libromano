/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/cpu.h"

static void test_name(void)
{
    char name[ROMANO_CPU_NAME_SZ];

    memset(name, 0x7F, sizeof(name));
    cpu_get_name(name);

    TEST_CHECK(memchr(name, '\0', sizeof(name)) != NULL);
    logger_log_info("CPU name: %s", name);
}

static void test_frequency(void)
{
    const uint32_t frequency = cpu_get_frequency();
    const uint32_t current = cpu_get_current_frequency();

    logger_log_info("CPU frequency: %u MHz (current: %u MHz)", frequency, current);

    TEST_CHECK(frequency < 20000);
    TEST_CHECK(current < 20000);
}

static void test_rdtsc(void)
{
    const uint64_t start = cpu_rdtsc();
    volatile uint64_t sink = 0;
    uint64_t i;

    for(i = 0; i < 100000; i++)
        sink += i;

    TEST_CHECK(cpu_rdtsc() > start);
}

static void test_features(void)
{
    int feature;
    int count = 0;

    cpu_check();

    for(feature = 0; feature < CPUFeature_COUNT; feature++)
        count += cpu_has_feature((CPUFeature)feature);

#if defined(ROMANO_X86_64)
    TEST_CHECK(cpu_has_feature(CPUFeature_SSE2));
    TEST_CHECK(!cpu_has_feature(CPUFeature_AVX2) || cpu_has_feature(CPUFeature_AVX));
#endif /* defined(ROMANO_X86_64) */

    TEST_CHECK(!cpu_has_feature(CPUFeature_COUNT));
    TEST_CHECK(count > 0);

    cpu_print_features();
}

TEST_MAIN(
    TEST(test_name),
    TEST(test_frequency),
    TEST(test_rdtsc),
    TEST(test_features),
)
