/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashmap.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#include "libromano/logger.h"
#include "libromano/string.h"

#if ROMANO_DEBUG
#define SIMDHASHMAP_LOOP_COUNT 0xFFFF
#else
#define SIMDHASHMAP_LOOP_COUNT 0xFFFFF
#endif /* ROMANO_DEBUG */
#define KEY_NAME "long_key"


int main(void)
{
    logger_init();

    size_t i;
    SimdHashMap* hashmap = simdhashmap_new(SIMDHASHMAP_LOOP_COUNT);

    /* Insertion */

    SCOPED_PROFILE_MS_START(_simdhashmap_set);

    MEAN_PROFILE_NS_INIT(_simdhashmap_set);

    for(i = 0; i < SIMDHASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;
        String key = string_newf(KEY_NAME"%zu", i);

        MEAN_PROFILE_NS_START(_simdhashmap_set);

        simdhashmap_set(hashmap, (const void*)key, string_length(key), &num, sizeof(int));

        MEAN_PROFILE_NS_STOP(_simdhashmap_set);

        string_free(key);
    }

    logger_log(LogLevel_Info, "Hashmap size : %zu", simdhashmap_size(hashmap));

    MEAN_PROFILE_NS_RELEASE(_simdhashmap_set);

    SCOPED_PROFILE_MS_END(_simdhashmap_set);

    /* Get */

    SCOPED_PROFILE_MS_START(_simdhashmap_get);

    MEAN_PROFILE_NS_INIT(_simdhashmap_get);

    for(i = 0; i < SIMDHASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;
        String key = string_newf(KEY_NAME"%zu", i);

        uint64_t size;

        MEAN_PROFILE_NS_START(_simdhashmap_get);

        int* num_ptr = (int*)simdhashmap_get(hashmap, (const void*)key, string_length(key), &size);

        MEAN_PROFILE_NS_STOP(_simdhashmap_get);

        if(num_ptr == NULL)
        {
            logger_log(LogLevel_Error, "Cannot find value for key \"%s\"", key);
            return 1;
        }
        else if(*num_ptr != num)
        {
            logger_log(LogLevel_Error, "Num_ptr does not correspond to num: %d != %d", num, *num_ptr);
            return 1;
        }

        string_free(key);
    }

    MEAN_PROFILE_NS_RELEASE(_simdhashmap_get);

    SCOPED_PROFILE_MS_END(_simdhashmap_get);

    /* Iterate */
    uint64_t noop_res;

    SimdHashMapIterator it = 0;
    void* key = NULL;
    uint64_t key_size = 0;
    void* value = NULL;
    uint64_t value_size = 0;
    
    while(simdhashmap_iterate(hashmap, &it, &key, &key_size, &value, &value_size))
    {
        noop_res = cpu_rdtsc();
    }

    uint64_t noop_res2 = noop_res;

    /* Delete */

    SCOPED_PROFILE_MS_START(_simdhashmap_delete);

    MEAN_PROFILE_NS_INIT(_simdhashmap_delete);

    for(i = 0; i < SIMDHASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;
        String key = string_newf(KEY_NAME"%zu", i);

        MEAN_PROFILE_NS_START(_simdhashmap_delete);

        simdhashmap_remove(hashmap, (const void*)key, string_length(key));

        MEAN_PROFILE_NS_STOP(_simdhashmap_delete);

        string_free(key);
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

    logger_release();

    return 0;
}
