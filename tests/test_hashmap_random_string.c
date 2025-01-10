/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashmap.h"
#include "libromano/logger.h"
#include "libromano/str.h"
#include "libromano/vector.h"
#include "libromano/random.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#if ROMANO_DEBUG
#define HASHMAP_LOOP_COUNT 0xFFFF
#else
#define HASHMAP_LOOP_COUNT 0xFFFFF
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
    HashMap* hashmap = hashmap_new();
    Vector* keys = vector_new(HASHMAP_LOOP_COUNT, sizeof(str));

    /* Insertion */

    SCOPED_PROFILE_START_MILLISECONDS(_hashmap_insert);

    MEAN_PROFILE_INIT(_hashmap_insert);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;

        const size_t string_size = random_next_uint32_range(STRING_MIN_SIZE, STRING_MAX_SIZE);

        str key = str_new_zero(string_size);
        set_random_string((char*)key, string_size);

        vector_push_back(keys, &key);

        MEAN_PROFILE_START(_hashmap_insert);

        hashmap_insert(hashmap, (const void*)key, string_size, &num, sizeof(int));

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

        str* key = (str*)vector_at(keys, i);

        uint32_t size;

        MEAN_PROFILE_START(_hashmap_get);

        int* num_ptr = (int*)hashmap_get(hashmap, (const void*)*key, str_length(*key), &size);

        MEAN_PROFILE_STOP(_hashmap_get);

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

    MEAN_PROFILE_RELEASE(_hashmap_get);

    SCOPED_PROFILE_END_MILLISECONDS(_hashmap_get);

    /* Delete */

    SCOPED_PROFILE_START_MILLISECONDS(_hashmap_delete);

    MEAN_PROFILE_INIT(_hashmap_delete);

    for(i = 0; i < HASHMAP_LOOP_COUNT; i++)
    {
        int num = (int)i;

        str* key = (str*)vector_at(keys, i);

        MEAN_PROFILE_START(_hashmap_delete);

        hashmap_remove(hashmap, (const void*)*key, str_length(*key));

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

    for(i = 0; i < vector_size(keys); i++)
    {
        str* key = (str*)vector_at(keys, i);

        str_free(*key);
    } 

    vector_free(keys);

    logger_release();

    return 0;
}
