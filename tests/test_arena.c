/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/arena.h"

static void test_push(void)
{
    Arena arena;
    size_t i;

    TEST_ASSERT(arena_init(&arena, ARENA_BLOCK_SIZE));
    TEST_CHECK_EQ_UINT(arena.capacity, ARENA_BLOCK_SIZE);

    for(i = 0; i < 100000; i++)
    {
        float f = (float)i;
        float* pushed = (float*)arena_push(&arena, &f, sizeof(float));

        TEST_ASSERT(pushed != NULL);
        TEST_ASSERT(*pushed == f);
    }

    TEST_CHECK(arena.capacity >= 100000 * sizeof(float));

    arena_release(&arena);
    TEST_CHECK_EQ_UINT(arena.capacity, 0);
    TEST_CHECK(arena.current_block == NULL);
}

static void test_emplace(void)
{
    Arena* arena = arena_new(64);

    TEST_ASSERT(arena != NULL);

    arena_emplace(arena, sizeof(uint64_t), uint64_t) = 42;
    TEST_CHECK_EQ_UINT(*(uint64_t*)arena->current_block->address, 42);

    arena_free(arena);
}

static void test_clear_and_reuse(void)
{
    Arena arena;
    uint64_t* first;
    size_t round;
    size_t i;

    TEST_ASSERT(arena_init(&arena, 256));

    for(round = 0; round < 4; round++)
    {
        first = NULL;

        for(i = 0; i < 1000; i++)
        {
            uint64_t value = round * 1000 + i;
            uint64_t* pushed = (uint64_t*)arena_push(&arena, &value, sizeof(value));

            TEST_ASSERT(pushed != NULL);

            if(first == NULL)
                first = pushed;
        }

        TEST_CHECK_EQ_UINT(*first, round * 1000);

        arena_clear(&arena);
        TEST_CHECK_EQ_UINT(arena.current_block->offset, 0);
        TEST_CHECK(arena.current_block->previous == NULL);
    }

    arena_release(&arena);
}

typedef struct Allocation {
    uint8_t* address;
    size_t size;
    uint8_t pattern;
} Allocation;

static bool property_allocations_do_not_overlap(FuzzSource* source, void* user_data)
{
    Allocation allocations[256];
    size_t block_size = fuzz_range(source, 16, 4096);
    size_t count = fuzz_range(source, 1, 256);
    size_t live = 0;
    Arena arena;
    bool ok = true;
    size_t i;
    size_t j;

    ROMANO_UNUSED(user_data);

    TEST_FUZZ_CHECK(arena_init(&arena, block_size));

    for(i = 0; i < count && ok; i++)
    {
        Allocation* allocation = &allocations[live];

        if(fuzz_one_in(source, 64))
        {
            arena_clear(&arena);
            live = 0;
            continue;
        }

        allocation->size = fuzz_range(source, 1, fuzz_one_in(source, 8) ? block_size * 3 : block_size);
        allocation->pattern = fuzz_u8(source);
        allocation->address = (uint8_t*)arena_push(&arena, NULL, allocation->size);

        ok = allocation->address != NULL;

        if(ok)
        {
            memset(allocation->address, allocation->pattern, allocation->size);
            live++;
        }
    }

    for(i = 0; i < live && ok; i++)
        for(j = 0; j < allocations[i].size && ok; j++)
            ok = allocations[i].address[j] == allocations[i].pattern;

    arena_release(&arena);

    TEST_FUZZ_CHECK_MSG(ok, "allocation %zu was overwritten or failed", i);

    return true;
}

static void test_fuzz_allocations(void)
{
    test_fuzz_property("arena_allocations", 2000, property_allocations_do_not_overlap, NULL);
}

TEST_MAIN(
    TEST(test_push),
    TEST(test_emplace),
    TEST(test_clear_and_reuse),
    TEST(test_fuzz_allocations),
)
