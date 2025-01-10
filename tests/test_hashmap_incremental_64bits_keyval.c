/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashmap.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#include "libromano/logger.h"

#if ROMANO_DEBUG
#define HASHMAP_LOOP_COUNT 0xFFFF
#else
#define HASHMAP_LOOP_COUNT 0xFFFFF
#endif /* ROMANO_DEBUG */


int main(void)
{
    logger_init();

    size_t i;
    HashMap* hashmap = hashmap_new();

    /* Insertion */

    SCOPED_PROFILE_START_MILLISECONDS(_hashmap_insert);

    MEAN_PROFILE_INIT(_hashmap_insert);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;

        MEAN_PROFILE_START(_hashmap_insert);

        hashmap_insert(hashmap, (const void*)&i, sizeof(size_t), &num, sizeof(int));

        MEAN_PROFILE_STOP(_hashmap_insert);
    }

    logger_log(LogLevel_Info, "Hashmap size : %zu", hashmap_size(hashmap));

    MEAN_PROFILE_RELEASE(_hashmap_insert);

    SCOPED_PROFILE_END_MILLISECONDS(_hashmap_insert);

    /* Get */

    SCOPED_PROFILE_START_MILLISECONDS(_hashmap_get);

    MEAN_PROFILE_INIT(_hashmap_get);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;

        uint32_t size;

        MEAN_PROFILE_START(_hashmap_get);

        int* num_ptr = (int*)hashmap_get(hashmap, (const void*)&i, sizeof(size_t), &size);

        MEAN_PROFILE_STOP(_hashmap_get);

        if(num_ptr == NULL)
        {
            logger_log(LogLevel_Error, "Cannot find value for key \"%zu\"", i);
            return 1;
        }
        else if(*num_ptr != num)
        {
            logger_log(LogLevel_Error, "Num_ptr does not correspond to num: %d != %d", num, *num_ptr);
            return 1;
        }
    }

    MEAN_PROFILE_RELEASE(_hashmap_get);

    SCOPED_PROFILE_END_MILLISECONDS(_hashmap_get);

    /* Delete */

    SCOPED_PROFILE_START_MILLISECONDS(_hashmap_delete);

    MEAN_PROFILE_INIT(_hashmap_delete);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;

        MEAN_PROFILE_START(_hashmap_delete);

        hashmap_remove(hashmap, (const void*)&i, sizeof(size_t));

        MEAN_PROFILE_STOP(_hashmap_delete);
    }

    MEAN_PROFILE_RELEASE(_hashmap_delete);

    logger_log(LogLevel_Info, "Hashmap size : %zu", hashmap_size(hashmap));

    if(hashmap_size(hashmap) != 0)
    {
        logger_log(LogLevel_Error, "Hashmap is not completely empty");
        return 1;
    }

    SCOPED_PROFILE_END_MILLISECONDS(_hashmap_delete);

    hashmap_free(hashmap);

    logger_release();

    return 0;
}
