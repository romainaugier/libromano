/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashmap.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#include "libromano/logger.h"
#include "libromano/str.h"

#if ROMANO_DEBUG
#define HASHMAP_LOOP_COUNT 0xFFFF
#else
#define HASHMAP_LOOP_COUNT 0xFFFFF
#endif /* ROMANO_DEBUG */
#define KEY_NAME "long_key"


int main(void)
{
    logger_init();

    size_t i;
    HashMap* hashmap = hashmap_new(HASHMAP_LOOP_COUNT);

    /* Insertion */

    SCOPED_PROFILE_MS_START(_hashmap_insert);

    MEAN_PROFILE_NS_INIT(_hashmap_insert);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;
        str key = str_new_fmt(KEY_NAME"%zu", i);

        MEAN_PROFILE_NS_START(_hashmap_insert);

        hashmap_insert(hashmap, (const void*)key, str_length(key), &num, sizeof(int));

        MEAN_PROFILE_NS_STOP(_hashmap_insert);

        str_free(key);
    }

    logger_log(LogLevel_Info, "Hashmap size : %zu", hashmap_size(hashmap));

    MEAN_PROFILE_NS_RELEASE(_hashmap_insert);

    SCOPED_PROFILE_MS_END(_hashmap_insert);

    /* Get */

    SCOPED_PROFILE_MS_START(_hashmap_get);

    MEAN_PROFILE_NS_INIT(_hashmap_get);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;
        str key = str_new_fmt(KEY_NAME"%zu", i);

        uint32_t size;

        MEAN_PROFILE_NS_START(_hashmap_get);

        int* num_ptr = (int*)hashmap_get(hashmap, (const void*)key, str_length(key), &size);

        MEAN_PROFILE_NS_STOP(_hashmap_get);

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

        str_free(key);
    }

    MEAN_PROFILE_NS_RELEASE(_hashmap_get);

    SCOPED_PROFILE_MS_END(_hashmap_get);

    /* Delete */

    SCOPED_PROFILE_MS_START(_hashmap_delete);

    MEAN_PROFILE_NS_INIT(_hashmap_delete);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;
        str key = str_new_fmt(KEY_NAME"%zu", i);

        MEAN_PROFILE_NS_START(_hashmap_delete);

        hashmap_remove(hashmap, (const void*)key, str_length(key));

        MEAN_PROFILE_NS_STOP(_hashmap_delete);

        str_free(key);
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

    logger_release();

    return 0;
}
