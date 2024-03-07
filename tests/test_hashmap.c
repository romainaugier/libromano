/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#define ROMANO_ENABLE_PROFILING

#include "libromano/hashmap.h"
#include "libromano/profiling.h"
#include "libromano/logger.h"
#include "libromano/str.h"


#define HASHMAP_LOOP_COUNT 0xFFFFFF


int main(void)
{
    logger_init();

    size_t i;
    hashmap_t* hashmap = hashmap_new();

    SCOPED_PROFILE_START("HASHMAP INSERTION/GET 0xFFFFFF times")

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;
        str key = str_new_fmt("key%d", i);

        hashmap_insert(hashmap, key, &num, sizeof(int));

        size_t size;
        int* num_ptr = (int*)hashmap_get(hashmap, key, &size);

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

    SCOPED_PROFILE_END

    hashmap_free(hashmap);

    logger_release();

    return 0;
}
