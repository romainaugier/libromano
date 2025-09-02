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
#define HASHMAP_LOOP_COUNT 0xFFFF
#else
#define HASHMAP_LOOP_COUNT 0xFFFFF
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

    size_t i;
    HashMap* hashmap = hashmap_new(57381);
    Vector* keys = vector_new(HASHMAP_LOOP_COUNT, STRING_SIZE * sizeof(char));

    /* Insertion */

    SCOPED_PROFILE_MS_START(_hashmap_insert);

    MEAN_PROFILE_NS_INIT(_hashmap_insert);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        char key[STRING_SIZE];
        set_random_string(key);

        vector_push_back(keys, key);

        MEAN_PROFILE_NS_START(_hashmap_insert);

        hashmap_insert(hashmap, (const void*)key, STRING_SIZE, &i, sizeof(size_t));

        MEAN_PROFILE_NS_STOP(_hashmap_insert);
    }

    logger_log(LogLevel_Info, "Hashmap size : %zu", hashmap_size(hashmap));

    MEAN_PROFILE_NS_RELEASE(_hashmap_insert);

    SCOPED_PROFILE_MS_END(_hashmap_insert);

    /* Get */

    SCOPED_PROFILE_MS_START(_hashmap_get);

    MEAN_PROFILE_NS_INIT(_hashmap_get);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        uint32_t size;

        MEAN_PROFILE_NS_START(_hashmap_get);

        size_t* num_ptr = (size_t*)hashmap_get(hashmap, (const void*)vector_at(keys, i), STRING_SIZE, &size);

        MEAN_PROFILE_NS_STOP(_hashmap_get);

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

    MEAN_PROFILE_NS_RELEASE(_hashmap_get);

    SCOPED_PROFILE_MS_END(_hashmap_get);

    /* Delete */

    SCOPED_PROFILE_MS_START(_hashmap_delete);

    MEAN_PROFILE_NS_INIT(_hashmap_delete);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        MEAN_PROFILE_NS_START(_hashmap_delete);

        hashmap_remove(hashmap, (const void*)vector_at(keys, i), STRING_SIZE);

        MEAN_PROFILE_NS_STOP(_hashmap_delete);
    }

    MEAN_PROFILE_NS_RELEASE(_hashmap_delete);

    logger_log(LogLevel_Info, "Hashmap size : %zu", hashmap_size(hashmap));

    if(hashmap_size(hashmap) != 0)
    {
        logger_log(LogLevel_Error, "Hashmap is not completely empty");
        return 1;
    }

    SCOPED_PROFILE_MS_END(_hashmap_delete);

    hashmap_free(hashmap);
    vector_destroy(keys);

    logger_release();

    return 0;
}
