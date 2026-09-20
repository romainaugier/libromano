/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/buffer.h"

static void test_basic(void)
{
    Buffer buffer;
    uint32_t value = 0xDEADBEEF;

    TEST_ASSERT(buffer_init(&buffer, 16));
    TEST_CHECK(buffer_is_empty(&buffer));
    TEST_CHECK_EQ_UINT(buffer_size(&buffer), 0);
    TEST_CHECK(buffer_front(&buffer) == buffer_back(&buffer));

    TEST_ASSERT(buffer_append(&buffer, "hello", 5));
    TEST_ASSERT(buffer_append(&buffer, &value, sizeof(value)));
    TEST_CHECK_EQ_UINT(buffer_size(&buffer), 9);
    TEST_CHECK(!buffer_is_empty(&buffer));
    TEST_CHECK_EQ_MEM(buffer_front(&buffer), "hello", 5);
    TEST_CHECK((char*)buffer_back(&buffer) - (char*)buffer_front(&buffer) == 9);

    TEST_ASSERT(buffer_prepare_emplace(&buffer, 100));
    TEST_CHECK(buffer.capacity >= 109);
    memset(buffer_back(&buffer), 'x', 100);
    buffer_emplace_size(&buffer, 100);
    TEST_CHECK_EQ_UINT(buffer_size(&buffer), 109);
    TEST_CHECK_EQ_UINT(((char*)buffer_front(&buffer))[108], 'x');

    buffer_reset(&buffer);
    TEST_CHECK(buffer_is_empty(&buffer));
    TEST_CHECK(buffer.capacity >= 109);

    buffer_release(&buffer);
    TEST_CHECK(buffer.data == NULL);
    TEST_CHECK_EQ_UINT(buffer.capacity, 0);
}

static void test_zero_capacity(void)
{
    Buffer buffer;

    TEST_ASSERT(buffer_init(&buffer, 0));
    TEST_ASSERT(buffer_append(&buffer, "abc", 3));
    TEST_CHECK_EQ_UINT(buffer_size(&buffer), 3);
    TEST_CHECK_EQ_MEM(buffer_front(&buffer), "abc", 3);
    buffer_release(&buffer);
}

static bool property_append(FuzzSource* source, void* user_data)
{
    uint8_t reference[8192];
    uint8_t chunk[512];
    size_t reference_size = 0;
    size_t operations = fuzz_range(source, 1, 64);
    Buffer buffer;
    size_t i;
    bool ok = true;

    ROMANO_UNUSED(user_data);

    TEST_FUZZ_CHECK(buffer_init(&buffer, fuzz_size(source, 64)));

    for(i = 0; i < operations && ok; i++)
    {
        size_t size = fuzz_size(source, sizeof(chunk));

        if(reference_size + size > sizeof(reference))
            break;

        fuzz_bytes(source, chunk, size);

        if(fuzz_one_in(source, 4))
        {
            ok &= buffer_prepare_emplace(&buffer, size);

            if(ok)
            {
                memcpy(buffer_back(&buffer), chunk, size);
                buffer_emplace_size(&buffer, size);
            }
        }
        else if(fuzz_one_in(source, 32))
        {
            buffer_reset(&buffer);
            reference_size = 0;
            continue;
        }
        else
        {
            ok &= buffer_append(&buffer, chunk, size);
        }

        memcpy(reference + reference_size, chunk, size);
        reference_size += size;

        ok &= buffer_size(&buffer) == reference_size && buffer.capacity >= reference_size;
    }

    ok &= buffer_size(&buffer) == reference_size;
    ok &= reference_size == 0 || memcmp(buffer_front(&buffer), reference, reference_size) == 0;

    buffer_release(&buffer);

    TEST_FUZZ_CHECK_MSG(ok, "buffer content diverged from the reference");

    return true;
}

static void test_fuzz_append(void)
{
    test_fuzz_property("buffer_append", 5000, property_append, NULL);
}

TEST_MAIN(
    TEST(test_basic),
    TEST(test_zero_capacity),
    TEST(test_fuzz_append),
)
