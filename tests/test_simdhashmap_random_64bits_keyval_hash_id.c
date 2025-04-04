/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashmap.h"
#include "libromano/vector.h"
#include "libromano/random.h"
#include "libromano/logger.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#if ROMANO_DEBUG
#define SIMDHASHMAP_LOOP_COUNT 0xFFFF
#else
#define SIMDHASHMAP_LOOP_COUNT 0xFFFFF
#endif /* ROMANO_DEBUG */

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

int main(void)
{
    logger_init();

    uint64_t i;
    SimdHashMap* hashmap = simdhashmap_new(0);
    Vector* keys = vector_new(SIMDHASHMAP_LOOP_COUNT, sizeof(uint64_t));

    simdhashmap_set_hash_func(hashmap, hash_identity);

    /* Insertion */

    SCOPED_PROFILE_MS_START(_simdhashmap_set);

    MEAN_PROFILE_NS_INIT(_simdhashmap_set);

    for(i = 0; i < SIMDHASHMAP_LOOP_COUNT; i++)
    {
        uint64_t key = random_next_uint64();

        vector_push_back(keys, (void*)&key);

        MEAN_PROFILE_NS_START(_simdhashmap_set);

        simdhashmap_set(hashmap, (const void*)&key, sizeof(uint64_t), &i, sizeof(uint64_t));

        MEAN_PROFILE_NS_STOP(_simdhashmap_set);
    }

    logger_log(LogLevel_Info, "Hashmap size : %zu", simdhashmap_size(hashmap));

    MEAN_PROFILE_NS_RELEASE(_simdhashmap_set);

    SCOPED_PROFILE_MS_END(_simdhashmap_set);

    /* Get */

    SCOPED_PROFILE_MS_START(_simdhashmap_get);

    MEAN_PROFILE_NS_INIT(_simdhashmap_get);

    for(i = 0; i < SIMDHASHMAP_LOOP_COUNT; i++)
    {
        const uint64_t* key = vector_at(keys, (size_t)i);

        uint64_t size;

        MEAN_PROFILE_NS_START(_simdhashmap_get);

        uint64_t* num_ptr = (uint64_t*)simdhashmap_get(hashmap, (const void*)key, sizeof(uint64_t), &size);

        MEAN_PROFILE_NS_STOP(_simdhashmap_get);

        if(num_ptr == NULL)
        {
            logger_log(LogLevel_Error, "Cannot find value for key \"%zu\"", i);
            return 1;
        }
        else if(*num_ptr != (uint64_t)i)
        {
            logger_log(LogLevel_Error, "Num_ptr does not correspond to num: %zu != %zu", i, *num_ptr);
            return 1;
        }
    }

    MEAN_PROFILE_NS_RELEASE(_simdhashmap_get);

    SCOPED_PROFILE_MS_END(_simdhashmap_get);

    /* Delete */

    SCOPED_PROFILE_MS_START(_simdhashmap_delete);

    MEAN_PROFILE_NS_INIT(_simdhashmap_delete);

    for(i = 0; i < SIMDHASHMAP_LOOP_COUNT; i++)
    {
        const uint64_t* key = vector_at(keys, (size_t)i);

        MEAN_PROFILE_NS_START(_simdhashmap_delete);

        simdhashmap_remove(hashmap, (const void*)key, sizeof(size_t));

        MEAN_PROFILE_NS_STOP(_simdhashmap_delete);
    }

    MEAN_PROFILE_NS_RELEASE(_simdhashmap_delete);

    logger_log(LogLevel_Info, "Hashmap size : %zu", simdhashmap_size(hashmap));

    if(simdhashmap_size(hashmap) != 0)
    {
        logger_log(LogLevel_Error, "Hashmap is not completely empty");
        return 1;
    }

    SCOPED_PROFILE_MS_END(_simdhashmap_delete);

    simdhashmap_free(hashmap);

    vector_free(keys);

    logger_release();

    return 0;
}
