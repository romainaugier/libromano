/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#define ROMANO_HASHMAP_INTERN_SMALL_VALUES
#include "libromano/hashmap.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#include "libromano/logger.h"
#include "libromano/str.h"


#define HASHMAP_LOOP_COUNT 0xFFFFF


int main(void)
{
    logger_init();

    size_t i;
    hashmap_t* hashmap = hashmap_new();

    /* Insertion */

    SCOPED_PROFILE_START_MILLISECONDS(_hashmap_insert);

    MEAN_PROFILE_INIT(_hashmap_insert);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;
        str key = str_new_fmt("key%d", i);

        MEAN_PROFILE_START(_hashmap_insert);

        hashmap_insert(hashmap, key, str_length(key), &num, sizeof(int));

        MEAN_PROFILE_STOP(_hashmap_insert);

        str_free(key);
    }

    logger_log(LogLevel_Info, "Hashmap size : %zu", hashmap->size);

    MEAN_PROFILE_RELEASE(_hashmap_insert);

    SCOPED_PROFILE_END_MILLISECONDS(_hashmap_insert);

    /* Get */

    SCOPED_PROFILE_START_MILLISECONDS(_hashmap_get);

    MEAN_PROFILE_INIT(_hashmap_get);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;
        str key = str_new_fmt("key%d", i);

        size_t size;

        MEAN_PROFILE_START(_hashmap_get);

        int* num_ptr = (int*)hashmap_get(hashmap, key, str_length(key), &size);

        MEAN_PROFILE_STOP(_hashmap_get);

        if(num_ptr == NULL)
        {
            logger_log(LogLevel_Info, "Cannot find value for key \"%s\"", key);
        }
        else if(*num_ptr != num)
        {
            logger_log(LogLevel_Error, "Num_ptr does not correspond to num: %d != %d", num, *num_ptr);
        }

        str_free(key);
    }

    MEAN_PROFILE_RELEASE(_hashmap_get);

    SCOPED_PROFILE_END_MILLISECONDS(_hashmap_get);

    /* Delete */

    SCOPED_PROFILE_START_MILLISECONDS(_hashmap_delete);

    MEAN_PROFILE_INIT(_hashmap_delete);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;
        str key = str_new_fmt("key%d", i);

        MEAN_PROFILE_START(_hashmap_delete);

        hashmap_remove(hashmap, key, str_length(key));

        MEAN_PROFILE_STOP(_hashmap_delete);

        str_free(key);
    }

    MEAN_PROFILE_RELEASE(_hashmap_delete);

    logger_log(LogLevel_Info, "Hashmap size : %zu", hashmap->size);

    SCOPED_PROFILE_END_MILLISECONDS(_hashmap_delete);

    hashmap_free(hashmap);

    logger_release();

    return 0;
}
