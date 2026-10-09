/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/memory.h"

static void test_endianness(void)
{
    const uint32_t probe = 0x01020304;
    const Endianness expected = *(const uint8_t*)&probe == 0x04 ? Endianness_Little : Endianness_Big;

    TEST_CHECK_EQ_INT(mem_get_endianness(), expected);
}

static void test_byte_swap(void)
{
    TEST_CHECK_EQ_UINT(mem_bswapu16(0x1234), 0x3412);
    TEST_CHECK_EQ_UINT(mem_bswapu32(0x12345678u), 0x78563412u);
    TEST_CHECK_EQ_UINT(mem_bswapu64(0x0102030405060708ULL), 0x0807060504030201ULL);
}

static bool property_swap(FuzzSource* source, void* user_data)
{
    uint8_t a[300];
    uint8_t b[300];
    uint8_t a_copy[300];
    uint8_t b_copy[300];
    size_t size = fuzz_size(source, sizeof(a));

    ROMANO_UNUSED(user_data);

    fuzz_bytes(source, a, size);
    fuzz_bytes(source, b, size);
    memcpy(a_copy, a, size);
    memcpy(b_copy, b, size);

    mem_swap(a, b, size);

    TEST_FUZZ_CHECK(size == 0 || (memcmp(a, b_copy, size) == 0 && memcmp(b, a_copy, size) == 0));
    TEST_FUZZ_CHECK(mem_bswapu64(mem_bswapu64(a_copy[0])) == a_copy[0]);

    return true;
}

static void test_fuzz_swap(void)
{
    test_fuzz_property("mem_swap", 2000, property_swap, NULL);
}

static void test_aligned_alloc(void)
{
    size_t alignment;

    for(alignment = 16; alignment <= 4096; alignment *= 2)
    {
        void* ptr = mem_aligned_alloc(1000, alignment);

        TEST_ASSERT(ptr != NULL);
        TEST_CHECK_EQ_UINT((uintptr_t)ptr % alignment, 0);
        memset(ptr, 0xAB, 1000);
        mem_aligned_free(ptr);
    }
}

static void test_allocator(void)
{
    uint8_t* bytes = (uint8_t*)romano_malloc(100);
    uint64_t* zeros = (uint64_t*)romano_calloc(1000, sizeof(uint64_t));
    char* copy = romano_strndup("libromano", 6);
    size_t i;

    TEST_ASSERT(bytes != NULL && zeros != NULL && copy != NULL);

    for(i = 0; i < 100; i++)
        bytes[i] = (uint8_t)i;

    bytes = (uint8_t*)romano_realloc(bytes, 100000);
    TEST_ASSERT(bytes != NULL);

    for(i = 0; i < 100; i++)
        TEST_CHECK_EQ_UINT(bytes[i], i);

    for(i = 0; i < 1000; i++)
        TEST_CHECK_EQ_UINT(zeros[i], 0);

    TEST_CHECK_EQ_STR(copy, "librom");

    romano_free(bytes);
    romano_free(zeros);
    romano_free(copy);
    romano_free(NULL);

    TEST_CHECK(strcmp(romano_allocator_name(), "mimalloc") == 0 || strcmp(romano_allocator_name(), "crt") == 0);
}

static void test_alloca(void)
{
#if defined(ROMANO_X86_64)
    char* buffer = (char*)mem_alloca(64);

    TEST_ASSERT(buffer != NULL);
    memset(buffer, 'a', 64);
    TEST_CHECK_EQ_UINT(buffer[63], 'a');
#endif /* defined(ROMANO_X86_64) */
}

TEST_MAIN(
    TEST(test_endianness),
    TEST(test_byte_swap),
    TEST(test_fuzz_swap),
    TEST(test_aligned_alloc),
    TEST(test_allocator),
    TEST(test_alloca),
)
