/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashmap.h"
#include "libromano/logger.h"
#include "libromano/string.h"
#include "libromano/vector.h"
#include "libromano/random.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#if ROMANO_DEBUG
#define SIMDHASHMAP_LOOP_COUNT 0xFFFF
#else
#define SIMDHASHMAP_LOOP_COUNT 0xFFFFF
#endif /* ROMANO_DEBUG */

#define STRING_MIN_SIZE 6
#define STRING_MAX_SIZE 16

void set_random_string(char* string, const size_t string_size)
{
    size_t i;

    for(i = 0; i < string_size; i++)
    {
        string[i] = (char)random_next_uint32_range(32, 126);
    }
}

int main(void)
{
    logger_init();

    size_t i;
    SimdHashMap* hashmap = simdhashmap_new(0);
    Vector* keys = vector_new(SIMDHASHMAP_LOOP_COUNT, sizeof(String));

    /* Insertion */

    SCOPED_PROFILE_MS_START(_simdhashmap_set);

    MEAN_PROFILE_NS_INIT(_simdhashmap_set);

    for(i = 0; i < SIMDHASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;

        const size_t string_size = random_next_uint32_range(STRING_MIN_SIZE, STRING_MAX_SIZE);

        String key = string_newz(string_size);
        set_random_string((char*)key, string_size);

        vector_push_back(keys, &key);

        MEAN_PROFILE_NS_START(_simdhashmap_set);

        simdhashmap_set(hashmap, (const void*)key, string_size, &num, sizeof(int));

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
        int num = (int)i;

        String* key = (String*)vector_at(keys, i);

        uint64_t size;

        MEAN_PROFILE_NS_START(_simdhashmap_get);

        int* num_ptr = (int*)simdhashmap_get(hashmap, (const void*)*key, string_length(*key), &size);

        MEAN_PROFILE_NS_STOP(_simdhashmap_get);

        if(num_ptr == NULL)
        {
            logger_log(LogLevel_Error, "Cannot find value for key \"%s\"", *key);
            return 1;
        }
        else if(*num_ptr != num)
        {
            logger_log(LogLevel_Error, "Key \"%s\"", *key);
            logger_log(LogLevel_Error, "Num_ptr does not correspond to num: %d != %d", num, *num_ptr);
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
        int num = (int)i;

        String* key = (String*)vector_at(keys, i);

        MEAN_PROFILE_NS_START(_simdhashmap_delete);

        simdhashmap_remove(hashmap, (const void*)*key, string_length(*key));

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

    for(i = 0; i < vector_size(keys); i++)
    {
        String* key = (String*)vector_at(keys, i);

        string_free(*key);
    } 

    vector_destroy(keys);

    logger_release();

    return 0;
}
