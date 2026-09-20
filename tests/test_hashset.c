/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/hashset.h"

#define KEY_PREFIX "long_key"

static uint32_t make_key(char* out, size_t i)
{
    return (uint32_t)snprintf(out, 32, KEY_PREFIX "%zu", i);
}

static void test_add_contains_iterate_remove(void)
{
    const size_t count = test_scaled(0xFFFFF);
    Hashset* hashset = hashset_new(0);
    HashsetIterator it = 0;
    bool* seen;
    void* key = NULL;
    uint32_t key_size = 0;
    size_t iterated = 0;
    char buffer[32];
    size_t i;

    TEST_ASSERT(hashset != NULL);

    for(i = 0; i < count; i++)
    {
        const uint32_t size = make_key(buffer, i);
        TEST_ASSERT(hashset_add(hashset, buffer, size));
    }

    TEST_CHECK_EQ_UINT(hashset_size(hashset), count);
    TEST_CHECK(hashset_capacity(hashset) >= count);

    for(i = 0; i < count; i += 8)
    {
        const uint32_t size = make_key(buffer, i);
        TEST_ASSERT(!hashset_add(hashset, buffer, size));
    }

    TEST_CHECK_EQ_UINT(hashset_size(hashset), count);

    for(i = 0; i < count; i++)
    {
        const uint32_t size = make_key(buffer, i);
        TEST_ASSERT(hashset_contains(hashset, buffer, size));
    }

    TEST_CHECK(!hashset_contains(hashset, buffer, make_key(buffer, count + 1)));

    seen = (bool*)calloc(count, sizeof(bool));
    TEST_ASSERT(seen != NULL);

    while(hashset_iterate(hashset, &it, &key, &key_size))
    {
        char number[32] = { 0 };
        long index;

        iterated++;

        if(!TEST_CHECK(key_size > strlen(KEY_PREFIX) && key_size < 32 && memcmp(key, KEY_PREFIX, strlen(KEY_PREFIX)) == 0))
            break;

        memcpy(number, (const char*)key + strlen(KEY_PREFIX), key_size - strlen(KEY_PREFIX));
        index = strtol(number, NULL, 10);

        if(!TEST_CHECK(index >= 0 && (size_t)index < count && !seen[index]))
            break;

        seen[index] = true;
    }

    TEST_CHECK_EQ_UINT(iterated, count);
    TEST_CHECK(!hashset_iterate(hashset, &it, &key, &key_size));
    free(seen);

    for(i = 0; i < count; i++)
    {
        const uint32_t size = make_key(buffer, i);
        hashset_remove(hashset, buffer, size);
    }

    TEST_CHECK_EQ_UINT(hashset_size(hashset), 0);
    hashset_remove(hashset, buffer, make_key(buffer, 0));
    TEST_CHECK_EQ_UINT(hashset_size(hashset), 0);

    hashset_free(hashset);
}

static uint32_t hash_constant(const void* key, const size_t size, const uint32_t seed)
{
    ROMANO_UNUSED(key);
    ROMANO_UNUSED(size);
    ROMANO_UNUSED(seed);
    return 0x5BD1E995u;
}

static uint32_t hash_zero(const void* key, const size_t size, const uint32_t seed)
{
    ROMANO_UNUSED(key);
    ROMANO_UNUSED(size);
    ROMANO_UNUSED(seed);
    return 0;
}

static uint32_t hash_all_ones(const void* key, const size_t size, const uint32_t seed)
{
    ROMANO_UNUSED(key);
    ROMANO_UNUSED(size);
    ROMANO_UNUSED(seed);
    return 0xFFFFFFFFu;
}

static uint32_t hash_3bits(const void* key, const size_t size, const uint32_t seed)
{
    const uint8_t* bytes = (const uint8_t*)key;
    uint32_t h = 2166136261u ^ seed;
    size_t i;

    for(i = 0; i < size; i++)
        h = (h ^ bytes[i]) * 16777619u;

    return h & 7u;
}

static const hashset_hash_func g_hash_funcs[] = { NULL, hash_constant, hash_zero, hash_all_ones, hash_3bits };

#define NUM_HASH_FUNCS (sizeof(g_hash_funcs) / sizeof(g_hash_funcs[0]))
#define MODEL_KEYS 256

/* Keys are ids in [0, MODEL_KEYS) encoded with a variable length and embedded zeros */
static uint32_t model_key(uint32_t id, uint8_t* out)
{
    const uint32_t size = 1 + (id * 7) % 40;
    uint32_t i;

    memset(out, 0, size);

    for(i = 0; i < 4 && i < size; i++)
        out[i] = (uint8_t)(id >> (8 * i));

    out[size - 1] ^= (uint8_t)size;

    return size;
}

static bool property_model(FuzzSource* source, void* user_data)
{
    const hashset_hash_func func = g_hash_funcs[fuzz_index(source, NUM_HASH_FUNCS)];
    const size_t num_keys = func == NULL ? MODEL_KEYS : 64;
    const size_t operations = fuzz_range(source, 1, 2000);
    Hashset* hashset = hashset_new(fuzz_size(source, 100));
    bool present[MODEL_KEYS] = { false };
    size_t count = 0;
    uint8_t key[64];
    bool ok = true;
    size_t i;

    ROMANO_UNUSED(user_data);

    TEST_FUZZ_CHECK(hashset != NULL);

    if(func != NULL)
        hashset_set_hash_func(hashset, func);

    for(i = 0; i < operations && ok; i++)
    {
        const uint32_t id = (uint32_t)fuzz_index(source, num_keys);
        const uint32_t key_size = model_key(id, key);

        switch(fuzz_range(source, 0, 3))
        {
            case 0:
            case 1:
                ok &= hashset_add(hashset, key, key_size) == !present[id];
                count += !present[id];
                present[id] = true;
                break;
            case 2:
                hashset_remove(hashset, key, key_size);
                count -= present[id];
                present[id] = false;
                break;
            default:
                ok &= hashset_contains(hashset, key, key_size) == present[id];
                break;
        }

        ok &= hashset_size(hashset) == count;

        if(!ok)
            logger_log_error("operation %zu on key id %u: size %zu, model %zu", i, id, hashset_size(hashset), count);
    }

    for(i = 0; i < num_keys && ok; i++)
    {
        const uint32_t key_size = model_key((uint32_t)i, key);
        ok = hashset_contains(hashset, key, key_size) == present[i];

        if(!ok)
            logger_log_error("final check: key id %zu (size %u) present %d", i, key_size, present[i]);
    }

    if(ok)
    {
        HashsetIterator it = 0;
        void* iterated_key;
        uint32_t iterated_size;
        size_t iterated = 0;

        while(hashset_iterate(hashset, &it, &iterated_key, &iterated_size))
            iterated++;

        ok = iterated == count;
    }

    hashset_free(hashset);

    TEST_FUZZ_CHECK_MSG(ok, "hashset diverged from the model");

    return true;
}

static void test_fuzz_model(void)
{
    test_fuzz_property("hashset_model", 2000, property_model, NULL);
}

TEST_MAIN(
    TEST(test_add_contains_iterate_remove),
    TEST(test_fuzz_model),
)
