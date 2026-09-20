/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/stack.h"
#include "libromano/stack_no_alloc.h"

static void test_push_pop(void)
{
    Stack* stack = stack_init(4, sizeof(int));
    int value;
    int i;

    TEST_ASSERT(stack != NULL);
    TEST_CHECK_EQ_UINT(stack_size(stack), 0);
    TEST_CHECK(stack_top(stack) == NULL);

    for(i = 0; i < 1000; i++)
    {
        stack_push(stack, &i);
        TEST_ASSERT_EQ_INT(*(int*)stack_top(stack), i);
    }

    TEST_CHECK_EQ_UINT(stack_size(stack), 1000);

    for(i = 999; i >= 0; i--)
    {
        stack_pop(stack, &value);
        TEST_ASSERT_EQ_INT(value, i);
    }

    TEST_CHECK_EQ_UINT(stack_size(stack), 0);

    value = -1;
    stack_pop(stack, &value);
    TEST_CHECK_EQ_INT(value, -1);

    stack_push(stack, &value);
    stack_pop(stack, NULL);
    TEST_CHECK_EQ_UINT(stack_size(stack), 0);

    stack_free(stack);
}

static void test_small_capacities(void)
{
    size_t capacity;

    for(capacity = 0; capacity < 4; capacity++)
    {
        Stack* stack = stack_init(capacity, sizeof(uint64_t));
        uint64_t i;

        for(i = 0; i < 300; i++)
            stack_push(stack, &i);

        TEST_CHECK_EQ_UINT(*(uint64_t*)stack_top(stack), 299);
        stack_free(stack);
    }
}

static bool property_stack_model(FuzzSource* source, void* user_data)
{
    uint32_t reference[4096];
    size_t reference_size = 0;
    size_t operations = fuzz_range(source, 1, 4096);
    Stack* stack = stack_init(fuzz_size(source, 16), sizeof(uint32_t));
    bool ok = true;
    size_t i;

    ROMANO_UNUSED(user_data);

    for(i = 0; i < operations && ok; i++)
    {
        if(fuzz_range(source, 0, 2) != 0 && reference_size < 4096)
        {
            uint32_t value = fuzz_u32(source);
            stack_push(stack, &value);
            reference[reference_size++] = value;
        }
        else
        {
            uint32_t value = 0xFFFFFFFF;
            stack_pop(stack, &value);

            if(reference_size > 0)
                ok = value == reference[--reference_size];
            else
                ok = value == 0xFFFFFFFF;
        }

        ok &= stack_size(stack) == reference_size;
        ok &= reference_size == 0 ? stack_top(stack) == NULL : *(uint32_t*)stack_top(stack) == reference[reference_size - 1];
    }

    stack_free(stack);

    TEST_FUZZ_CHECK_MSG(ok, "stack diverged from the reference at operation %zu", i);

    return true;
}

static void test_fuzz_model(void)
{
    test_fuzz_property("stack_model", 1000, property_stack_model, NULL);
}

static void test_no_alloc(void)
{
    int i;

    stacknoa_init(int, stack, 5);

    TEST_CHECK(stacknoa_is_empty(stack));
    TEST_CHECK(stacknoa_top(stack) == NULL);

    for(i = 0; !stacknoa_is_full(stack); i++)
    {
        stacknoa_push(stack, i * 10);
        TEST_CHECK_EQ_INT(*stacknoa_top(stack), i * 10);
    }

    TEST_CHECK_EQ_INT(i, 5);

    while(!stacknoa_is_empty(stack))
        TEST_CHECK_EQ_INT(stacknoa_pop(stack), --i * 10);

    TEST_CHECK_EQ_INT(i, 0);
}

TEST_MAIN(
    TEST(test_push_pop),
    TEST(test_small_capacities),
    TEST(test_fuzz_model),
    TEST(test_no_alloc),
)
