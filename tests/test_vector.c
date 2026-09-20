/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/vector.h"

static int compare_u32(const void* a, const void* b)
{
    const uint32_t x = *(const uint32_t*)a;
    const uint32_t y = *(const uint32_t*)b;
    return (x > y) - (x < y);
}

static void test_basic(void)
{
    Vector* vector = vector_new(0, sizeof(uint32_t));
    uint32_t i;

    TEST_ASSERT(vector != NULL);
    TEST_CHECK_EQ_UINT(vector_size(vector), 0);
    TEST_CHECK_EQ_UINT(vector_capacity(vector), 128);
    TEST_CHECK_EQ_UINT(vector_element_size(vector), sizeof(uint32_t));

    for(i = 0; i < 1000; i++)
        vector_push_back(vector, &i);

    vector_emplace_back(vector, uint32_t) = 1000;

    TEST_CHECK_EQ_UINT(vector_size(vector), 1001);
    TEST_CHECK_EQ_UINT(*(uint32_t*)vector_at(vector, 500), 500);
    TEST_CHECK_EQ_UINT(*(uint32_t*)vector_back(vector), 1000);

    i = 777;
    TEST_CHECK_EQ_UINT(vector_find(vector, &i), 777);
    i = 5000;
    TEST_CHECK_EQ_UINT(vector_find(vector, &i), VECTOR_NOT_FOUND);

    vector_shuffle(vector, 1234);
    vector_sort(vector, compare_u32);

    for(i = 0; i < 1001; i++)
        TEST_ASSERT_EQ_UINT(*(uint32_t*)vector_at(vector, i), i);

    vector_free(vector);
}

static void test_shrink_and_regrow(void)
{
    Vector vector;
    uint64_t i;

    vector_init(&vector, 64, sizeof(uint64_t));

    for(i = 0; i < 10; i++)
        vector_push_back(&vector, &i);

    vector_shrink_to_fit(&vector);
    TEST_CHECK_EQ_UINT(vector_capacity(&vector), 10);
    TEST_CHECK_EQ_UINT(*(uint64_t*)vector_at(&vector, 9), 9);

    for(i = 10; i < 20; i++)
        vector_push_back(&vector, &i);

    TEST_CHECK_EQ_UINT(*(uint64_t*)vector_back(&vector), 19);

    while(vector_size(&vector) > 0)
        vector_pop(&vector);

    vector_shrink_to_fit(&vector);
    TEST_CHECK_EQ_UINT(vector_capacity(&vector), 0);

    vector_shuffle(&vector, 1);

    i = 42;
    vector_push_back(&vector, &i);
    TEST_CHECK_EQ_UINT(*(uint64_t*)vector_at(&vector, 0), 42);

    vector_resize(&vector, 100);
    TEST_CHECK_EQ_UINT(vector_capacity(&vector), 100);
    vector_resize(&vector, 10);
    TEST_CHECK_EQ_UINT(vector_capacity(&vector), 100);

    vector_release(&vector);
}

static int g_dtor_calls = 0;

static void count_dtor(void* element)
{
    ROMANO_UNUSED(element);
    g_dtor_calls++;
}

static void test_dtor(void)
{
    Vector vector;
    Vector* heap_vector = vector_new(2, sizeof(int));
    int i;

    vector_init(&vector, 2, sizeof(int));

    for(i = 0; i < 5; i++)
    {
        vector_push_back(&vector, &i);
        vector_push_back(heap_vector, &i);
    }

    g_dtor_calls = 0;
    vector_release_with_dtor(&vector, count_dtor);
    TEST_CHECK_EQ_INT(g_dtor_calls, 5);

    vector_free_with_dtor(heap_vector, count_dtor);
    TEST_CHECK_EQ_INT(g_dtor_calls, 10);

    vector_free(NULL);
}

static bool property_vector_model(FuzzSource* source, void* user_data)
{
    uint32_t reference[2048];
    size_t reference_size = 0;
    size_t operations = fuzz_range(source, 1, 512);
    Vector vector;
    bool ok = true;
    size_t i;

    ROMANO_UNUSED(user_data);

    vector_init(&vector, fuzz_size(source, 8), sizeof(uint32_t));

    for(i = 0; i < operations && ok; i++)
    {
        uint32_t value = fuzz_u32(source);

        switch(fuzz_range(source, 0, 9))
        {
            case 0:
            case 1:
                if(reference_size < 2048)
                {
                    vector_push_back(&vector, &value);
                    reference[reference_size++] = value;
                }
                break;
            case 2:
                if(reference_size < 2048)
                {
                    vector_emplace_back(&vector, uint32_t) = value;
                    reference[reference_size++] = value;
                }
                break;
            case 3:
                if(reference_size < 2048)
                {
                    size_t position = fuzz_index(source, reference_size + 1);
                    vector_insert(&vector, &value, position);
                    memmove(reference + position + 1, reference + position, (reference_size - position) * sizeof(uint32_t));
                    reference[position] = value;
                    reference_size++;
                }
                break;
            case 4:
                if(reference_size > 0)
                {
                    size_t position = fuzz_index(source, reference_size);
                    vector_remove(&vector, position);
                    memmove(reference + position, reference + position + 1, (reference_size - position - 1) * sizeof(uint32_t));
                    reference_size--;
                }
                break;
            case 5:
                if(reference_size > 0)
                {
                    vector_pop(&vector);
                    reference_size--;
                }
                break;
            case 6:
                if(reference_size > 0)
                {
                    vector_pop_front(&vector);
                    memmove(reference, reference + 1, (reference_size - 1) * sizeof(uint32_t));
                    reference_size--;
                }
                break;
            case 7:
                vector_shrink_to_fit(&vector);
                ok &= vector_capacity(&vector) == reference_size;
                break;
            case 8:
                if(reference_size > 0)
                {
                    size_t index = fuzz_index(source, reference_size);
                    ok &= vector_find(&vector, &reference[index]) <= index;
                }
                break;
            default:
                vector_resize(&vector, reference_size + fuzz_size(source, 64));
                break;
        }

        ok &= vector_size(&vector) == reference_size && vector_capacity(&vector) >= reference_size;
    }

    ok &= reference_size == 0 || memcmp(vector_at(&vector, 0), reference, reference_size * sizeof(uint32_t)) == 0;

    vector_release(&vector);

    TEST_FUZZ_CHECK_MSG(ok, "vector diverged from the reference at operation %zu", i);

    return true;
}

static void test_fuzz_model(void)
{
    test_fuzz_property("vector_model", 2000, property_vector_model, NULL);
}

static bool property_shuffle_is_permutation(FuzzSource* source, void* user_data)
{
    size_t size = fuzz_size(source, 512);
    Vector* vector = vector_new(size, sizeof(uint32_t));
    uint32_t i;
    bool ok = true;

    ROMANO_UNUSED(user_data);

    for(i = 0; i < size; i++)
        vector_push_back(vector, &i);

    vector_shuffle(vector, fuzz_u64(source));
    vector_sort(vector, compare_u32);

    for(i = 0; i < size && ok; i++)
        ok = *(uint32_t*)vector_at(vector, i) == i;

    vector_free(vector);

    TEST_FUZZ_CHECK(ok);

    return true;
}

static void test_fuzz_shuffle(void)
{
    test_fuzz_property("vector_shuffle", 500, property_shuffle_is_permutation, NULL);
}

TEST_MAIN(
    TEST(test_basic),
    TEST(test_shrink_and_regrow),
    TEST(test_dtor),
    TEST(test_fuzz_model),
    TEST(test_fuzz_shuffle),
)
