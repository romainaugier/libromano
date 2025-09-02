/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashmap.h"
#include "libromano/vector.h"
#include "libromano/random.h"
#include "libromano/logger.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#define TEST_NAME(test_name, name) (strcmp(test_name, name) == 0)

void get_range_uint64_t_vector(Vector* vector, size_t n)
{
    size_t i;

    for(i = 0; i < n; i++)
    {
        vector_push_back(vector, (void*)&i);
    }
}

void get_random_uint64_t_vector(Vector* vector, size_t n)
{
    size_t i;
    uint64_t k;
    uint64_t seed;

    seed = random_next_uint64();

    for(i = 0; i < n; i++)
    {
        k = murmur_64(seed + i);

        vector_push_back(vector, (void*)&k);
    }
}

uint32_t hash_identity(const void* key, const size_t key_len, const uint32_t hashkey)
{
    switch(key_len)
    {
        case 4:
            return *(uint32_t*)key;
        case 8:
            return *(uint64_t*)key;
        
        default:
            return hash_murmur3(key, key_len, hashkey);
    }
}

int main(int argc, char** argv)
{
    uint64_t num_keys;
    uint64_t i;
    uint64_t k;
    uint64_t one;

    char* test_name;

    HashMap* hashmap = NULL;
    Vector* keys_insert = NULL;
    Vector* keys_read = NULL;

    logger_init();

    if(argc != 3)
    {
        logger_log(LogLevel_Error, "Usage: %s num_keys test_type", argv[0]);

        return 0;
    }

    one = 1;

    num_keys = (uint64_t)atol(argv[1]);
    test_name = argv[2];

    logger_log(LogLevel_Info, "Starting test: %s (num_keys: %zu)", test_name, num_keys);

    if(TEST_NAME(test_name, "insert_random_shuffle_range"))
    {
        keys_insert = vector_new(num_keys, sizeof(uint64_t));

        get_range_uint64_t_vector(keys_insert, num_keys);

        vector_shuffle(keys_insert, random_next_uint64());

        hashmap = hashmap_new(0);
        hashmap_set_hash_func(hashmap, hash_identity);

        SCOPED_PROFILE_MS_START(insert_random_shuffle_range);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_insert(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t), &one, sizeof(uint64_t));
        }

        SCOPED_PROFILE_MS_END(insert_random_shuffle_range);
    }
    else if(TEST_NAME(test_name, "read_random_shuffle_range"))
    {
        keys_insert = vector_new(num_keys, sizeof(uint64_t));

        get_range_uint64_t_vector(keys_insert, num_keys);

        vector_shuffle(keys_insert, random_next_uint64());

        hashmap = hashmap_new(0);
        hashmap_set_hash_func(hashmap, hash_identity);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_insert(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t), &one, sizeof(uint64_t));
        }

        SCOPED_PROFILE_MS_START(read_random_shuffle_range);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_get(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t), NULL);
        }

        SCOPED_PROFILE_MS_END(read_random_shuffle_range);
    }
    else if(TEST_NAME(test_name, "insert_random_full"))
    {
        keys_insert = vector_new(num_keys, sizeof(uint64_t));

        get_random_uint64_t_vector(keys_insert, num_keys);

        hashmap = hashmap_new(0);
        hashmap_set_hash_func(hashmap, hash_identity);

        SCOPED_PROFILE_MS_START(insert_random_full);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_insert(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t), &one, sizeof(uint64_t));
        }

        SCOPED_PROFILE_MS_END(insert_random_full);
    }
    else if(TEST_NAME(test_name, "insert_random_full_reserve"))
    {
        keys_insert = vector_new(num_keys, sizeof(uint64_t));

        get_random_uint64_t_vector(keys_insert, num_keys);

        hashmap = hashmap_new(num_keys);
        hashmap_set_hash_func(hashmap, hash_identity);

        SCOPED_PROFILE_MS_START(insert_random_full_reserve);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_insert(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t), &one, sizeof(uint64_t));
        }

        SCOPED_PROFILE_MS_END(insert_random_full_reserve);
    }
    else if(TEST_NAME(test_name, "read_random_full"))
    {
        keys_insert = vector_new(num_keys, sizeof(uint64_t));

        get_random_uint64_t_vector(keys_insert, num_keys);

        vector_shuffle(keys_insert, random_next_uint64());

        hashmap = hashmap_new(0);
        hashmap_set_hash_func(hashmap, hash_identity);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_insert(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t), &one, sizeof(uint64_t));
        }

        SCOPED_PROFILE_MS_START(read_random_shuffle_range);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_get(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t), NULL);
        }

        SCOPED_PROFILE_MS_END(read_random_shuffle_range);
    }
    else if(TEST_NAME(test_name, "read_miss_random_full"))
    {
        keys_insert = vector_new(num_keys, sizeof(uint64_t));
        keys_read = vector_new(num_keys, sizeof(uint64_t));

        get_random_uint64_t_vector(keys_insert, num_keys);
        get_random_uint64_t_vector(keys_read, num_keys);

        vector_shuffle(keys_insert, random_next_uint64());
        vector_shuffle(keys_read, random_next_uint64());

        hashmap = hashmap_new(0);
        hashmap_set_hash_func(hashmap, hash_identity);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_insert(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t), &one, sizeof(uint64_t));
        }

        SCOPED_PROFILE_MS_START(read_miss_random_full);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_get(hashmap, (const void*)vector_at(keys_read, i), sizeof(uint64_t), NULL);
        }

        SCOPED_PROFILE_MS_END(read_miss_random_full);
    }
    else if(TEST_NAME(test_name, "read_random_full_after_delete"))
    {
        keys_insert = vector_new(num_keys, sizeof(uint64_t));

        get_random_uint64_t_vector(keys_insert, num_keys);

        hashmap = hashmap_new(0);
        hashmap_set_hash_func(hashmap, hash_identity);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_insert(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t), &one, sizeof(uint64_t));
        }

        vector_shuffle(keys_insert, random_next_uint64());

        for(i = 0; i < num_keys; i++)
        {
            hashmap_remove(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t));
        }

        vector_shuffle(keys_insert, random_next_uint64());

        SCOPED_PROFILE_MS_START(read_random_full_after_delete);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_get(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t), NULL);
        }

        SCOPED_PROFILE_MS_END(read_random_full_after_delete);
    }
    else if(TEST_NAME(test_name, "delete_random_full"))
    {
        keys_insert = vector_new(num_keys, sizeof(uint64_t));

        get_random_uint64_t_vector(keys_insert, num_keys);

        hashmap = hashmap_new(0);
        hashmap_set_hash_func(hashmap, hash_identity);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_insert(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t), &one, sizeof(uint64_t));
        }

        vector_shuffle(keys_insert, random_next_uint64());

        SCOPED_PROFILE_MS_START(read_random_full_after_delete);

        for(i = 0; i < num_keys; i++)
        {
            hashmap_remove(hashmap, (const void*)vector_at(keys_insert, i), sizeof(uint64_t));
        }

        SCOPED_PROFILE_MS_END(read_random_full_after_delete);
    }

    hashmap_free(hashmap);
    vector_destroy(keys_insert);
    vector_destroy(keys_read);

    logger_release();

    return 0;
}
