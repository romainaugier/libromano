/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashmap.h"
#include "libromano/logger.h"
#include "libromano/random.h"
#include "libromano/vector.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#if ROMANO_DEBUG
#define SIMDHASHMAP_LOOP_COUNT 0xFFFF
#else
#define SIMDHASHMAP_LOOP_COUNT 0xFFFFF
#endif /* ROMANO_DEBUG */

#define STRING_SIZE 64

void set_random_string(char* string)
{
    size_t i;

    for(i = 0; i < STRING_SIZE; i++)
    {
        string[i] = (char)random_next_uint32_range(32, 126);
    }
}

int main(void)
{
    logger_init();

    return 0;

    size_t i;
    SimdHashMap* hashmap = simdhashmap_new(57381);
    Vector* keys = vector_new(SIMDHASHMAP_LOOP_COUNT, STRING_SIZE * sizeof(char));

    /* Insertion */

    SCOPED_PROFILE_MS_START(_simdhashmap_set);

    MEAN_PROFILE_NS_INIT(_simdhashmap_set);

    for(i = 0; i < SIMDHASHMAP_LOOP_COUNT; i++)
    {
        char key[STRING_SIZE];
        set_random_string(key);

        vector_push_back(keys, key);

        MEAN_PROFILE_NS_START(_simdhashmap_set);

        simdhashmap_set(hashmap, (const void*)key, STRING_SIZE, &i, sizeof(size_t));

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
        uint64_t size;

        MEAN_PROFILE_NS_START(_simdhashmap_get);

        size_t* num_ptr = (size_t*)simdhashmap_get(hashmap, (const void*)vector_at(keys, i), STRING_SIZE, &size);

        MEAN_PROFILE_NS_STOP(_simdhashmap_get);

        if(num_ptr == NULL)
        {
            logger_log(LogLevel_Error, "Cannot find value for key \"%.*s\"", STRING_SIZE, (char*)vector_at(keys, i));
            return 1;
        }
        else if(*num_ptr != i)
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
        MEAN_PROFILE_NS_START(_simdhashmap_delete);

        simdhashmap_remove(hashmap, (const void*)vector_at(keys, i), STRING_SIZE);

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
