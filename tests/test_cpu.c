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

static bool is_power_of_two(uint64_t x)
{
    return x != 0 && (x & (x - 1)) == 0;
}

static void test_caches(void)
{
    const uint32_t line_size = cpu_get_cache_line_size();
    CPUCacheInfo previous;
    CPUCacheInfo info;
    int level;
    int found = 0;

    TEST_CHECK_MSG(is_power_of_two(line_size) && line_size >= 16 && line_size <= 1024,
                   "cache line size %u", line_size);

    memset(&previous, 0, sizeof(previous));

    for(level = 0; level < CPUCacheLevel_COUNT; level++)
    {
        const bool present = cpu_get_cache_info((CPUCacheLevel)level, &info);

        TEST_CHECK(cpu_get_cache_size((CPUCacheLevel)level) == info.size);

        if(!present)
        {
            TEST_CHECK(info.size == 0 && info.line_size == 0 && info.shared_by == 0);
            continue;
        }

        found++;

        logger_log_info("L%d%s: %llu KiB, %u B lines, shared by %u",
                        level + 1,
                        level == 0 ? "d" : "",
                        (unsigned long long)(info.size / 1024),
                        info.line_size,
                        info.shared_by);

        /* Plausible sizes: 1 KiB .. 4 GiB, whole KiB */
        TEST_CHECK(info.size >= 1024 && info.size % 1024 == 0);
        TEST_CHECK((uint64_t)info.size <= ((uint64_t)4 << 30));
        TEST_CHECK(is_power_of_two(info.line_size) && info.line_size >= 16 && info.line_size <= 1024);

        /* Outer levels are at least as big and at least as shared as the inner ones */
        if(previous.size != 0)
        {
            TEST_CHECK(info.size >= previous.size);
            TEST_CHECK(info.shared_by == 0 || previous.shared_by == 0 || info.shared_by >= previous.shared_by);
        }

        previous = info;
    }

    if(found > 0)
        TEST_CHECK(cpu_get_cache_info(CPUCacheLevel_L1, &info) && info.line_size == line_size);

#if defined(ROMANO_X86_64) || defined(ROMANO_APPLE) || defined(ROMANO_WIN)
    /* Always available there (CPUID, sysctl, GetLogicalProcessorInformation) */
    TEST_CHECK(found >= 2);
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_APPLE) || defined(ROMANO_WIN) */

    TEST_CHECK(!cpu_get_cache_info(CPUCacheLevel_COUNT, &info) && info.size == 0);
    TEST_CHECK(cpu_get_cache_size(CPUCacheLevel_COUNT) == 0);
}

TEST_MAIN(
    TEST(test_name),
    TEST(test_frequency),
    TEST(test_rdtsc),
    TEST(test_features),
    TEST(test_caches),
)
