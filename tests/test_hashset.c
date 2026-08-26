/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashset.h"

#include "libromano/logger.h"
#include "libromano/string.h"
#include "libromano/hash.h"

#include <stdlib.h>
#include <string.h>

#if ROMANO_DEBUG
#define HASHSET_LOOP_COUNT 0xFFFF
#else
#define HASHSET_LOOP_COUNT 0xFFFFF
#endif /* ROMANO_DEBUG */

#define KEY_NAME "long_key"
#define KEY_NAME_SIZE 8

#define SHORT_KEY_COUNT 1024

uint32_t test_hash_fnv1a(const void* key,
                         const size_t key_size,
                         const uint32_t hashkey)
{
    return hash_fnv1a((const char*)key, key_size) ^ hashkey;
}

int main(void)
{
    logger_init();

    size_t i;
    Hashset* hashset = hashset_new(0);

    /* Insertion */

    for(i = 0; i < HASHSET_LOOP_COUNT; i++)
    {
        String key = string_newf(KEY_NAME"%zu", i);

        if(!hashset_add(hashset, (const void*)key, string_length(key)))
        {
            logger_log(LogLevel_Error, "Cannot add key \"%s\"", key);
            return 1;
        }

        string_free(key);
    }

    logger_log(LogLevel_Info, "Hashset size : %zu", hashset_size(hashset));

    if(hashset_size(hashset) != HASHSET_LOOP_COUNT)
    {
        logger_log(LogLevel_Error, "Hashset size does not match : %zu", hashset_size(hashset));
        return 1;
    }

    /* Duplicate insertion */

    for(i = 0; i < HASHSET_LOOP_COUNT; i += 8)
    {
        String key = string_newf(KEY_NAME"%zu", i);

        if(hashset_add(hashset, (const void*)key, string_length(key)))
        {
            logger_log(LogLevel_Error, "Key \"%s\" was added twice", key);
            return 1;
        }

        string_free(key);
    }

    if(hashset_size(hashset) != HASHSET_LOOP_COUNT)
    {
        logger_log(LogLevel_Error, "Hashset size changed after duplicate insertion : %zu", hashset_size(hashset));
        return 1;
    }

    /* Contains */

    for(i = 0; i < HASHSET_LOOP_COUNT; i++)
    {
        String key = string_newf(KEY_NAME"%zu", i);

        if(!hashset_contains(hashset, (const void*)key, string_length(key)))
        {
            logger_log(LogLevel_Error, "Cannot find key \"%s\"", key);
            return 1;
        }

        string_free(key);
    }

    {
        String missing_key = string_newf(KEY_NAME"%zu", HASHSET_LOOP_COUNT + 1);

        if(hashset_contains(hashset, (const void*)missing_key, string_length(missing_key)))
        {
            logger_log(LogLevel_Error, "Missing key \"%s\" was found", missing_key);
            return 1;
        }

        string_free(missing_key);
    }

    /* Iterate */

    bool* seen = (bool*)calloc(HASHSET_LOOP_COUNT, sizeof(bool));

    if(seen == NULL)
    {
        return 1;
    }

    HashsetIterator it = 0;

    void* key = NULL;
    uint32_t key_size = 0;

    size_t entry_count = 0;

    while(hashset_iterate(hashset, &it, &key, &key_size))
    {
        entry_count++;

        if(key_size < KEY_NAME_SIZE + 1 || memcmp(key, KEY_NAME, KEY_NAME_SIZE) != 0)
        {
            logger_log(LogLevel_Error, "Invalid key \"%.*s\"", (int)key_size, (const char*)key);
            free(seen);
            return 1;
        }

        long num = strtol((const char*)key + KEY_NAME_SIZE, NULL, 10);

        if(num < 0 || num >= (long)HASHSET_LOOP_COUNT || seen[(size_t)num])
        {
            logger_log(LogLevel_Error, "Unexpected key \"%.*s\"", (int)key_size, (const char*)key);
            free(seen);
            return 1;
        }

        seen[(size_t)num] = true;
    }

    if(entry_count != HASHSET_LOOP_COUNT)
    {
        logger_log(LogLevel_Error, "Iterated %zu entries instead of %zu", entry_count, (size_t)HASHSET_LOOP_COUNT);
        free(seen);
        return 1;
    }

    for(i = 0; i < HASHSET_LOOP_COUNT; i++)
    {
        if(!seen[i])
        {
            logger_log(LogLevel_Error, "Key %zu was not iterated", i);
            free(seen);
            return 1;
        }
    }

    free(seen);

    /* Removal */

    for(i = 0; i < HASHSET_LOOP_COUNT; i++)
    {
        String key = string_newf(KEY_NAME"%zu", i);

        hashset_remove(hashset, (const void*)key, string_length(key));

        string_free(key);
    }

    logger_log(LogLevel_Info, "Hashset size : %zu", hashset_size(hashset));

    if(hashset_size(hashset) != 0)
    {
        logger_log(LogLevel_Error, "Hashset is not completely empty");
        return 1;
    }

    for(i = 0; i < HASHSET_LOOP_COUNT; i += 8)
    {
        String key = string_newf(KEY_NAME"%zu", i);

        if(hashset_contains(hashset, (const void*)key, string_length(key)))
        {
            logger_log(LogLevel_Error, "Key \"%s\" was not removed", key);
            return 1;
        }

        string_free(key);
    }

    hashset_free(hashset);

    /* Short and numeric keys (interned bucket path) */

    Hashset* short_hashset = hashset_new(0);

    for(i = 0; i < SHORT_KEY_COUNT; i++)
    {
        String key = string_newf("s%zu", i);

        hashset_add(short_hashset, (const void*)key, string_length(key));

        string_free(key);

        int64_t num = (int64_t)i;

        hashset_add(short_hashset, &num, sizeof(num));
    }

    if(hashset_size(short_hashset) != SHORT_KEY_COUNT * 2)
    {
        logger_log(LogLevel_Error, "Short hashset size does not match : %zu", hashset_size(short_hashset));
        return 1;
    }

    for(i = 0; i < SHORT_KEY_COUNT; i++)
    {
        int64_t num = (int64_t)i;

        if(hashset_add(short_hashset, &num, sizeof(num)))
        {
            logger_log(LogLevel_Error, "Key %lld was added twice", (long long)num);
            return 1;
        }

        if(!hashset_contains(short_hashset, &num, sizeof(num)))
        {
            logger_log(LogLevel_Error, "Cannot find key %lld", (long long)num);
            return 1;
        }
    }

    for(i = 0; i < SHORT_KEY_COUNT; i++)
    {
        int64_t num = (int64_t)i;

        hashset_remove(short_hashset, &num, sizeof(num));
    }

    if(hashset_size(short_hashset) != SHORT_KEY_COUNT)
    {
        logger_log(LogLevel_Error, "Short hashset size does not match : %zu", hashset_size(short_hashset));
        return 1;
    }

    for(i = 0; i < SHORT_KEY_COUNT; i++)
    {
        String key = string_newf("s%zu", i);

        if(!hashset_contains(short_hashset, (const void*)key, string_length(key)))
        {
            logger_log(LogLevel_Error, "Cannot find key \"%s\"", key);
            return 1;
        }

        hashset_remove(short_hashset, (const void*)key, string_length(key));

        string_free(key);
    }

    if(hashset_size(short_hashset) != 0)
    {
        logger_log(LogLevel_Error, "Short hashset is not completely empty");
        return 1;
    }

    hashset_free(short_hashset);

    /* Custom hash function */

    Hashset* fnv_hashset = hashset_new(0);

    hashset_set_hash_func(fnv_hashset, test_hash_fnv1a);

    for(i = 0; i < SHORT_KEY_COUNT; i++)
    {
        String key = string_newf("k%zu", i);

        if(!hashset_add(fnv_hashset, (const void*)key, string_length(key)))
        {
            logger_log(LogLevel_Error, "Cannot add key \"%s\"", key);
            string_free(key);
            return 1;
        }

        string_free(key);
    }

    if(hashset_size(fnv_hashset) != SHORT_KEY_COUNT)
    {
        logger_log(LogLevel_Error, "Fnv hashset size does not match : %zu", hashset_size(fnv_hashset));
        return 1;
    }

    for(i = 0; i < SHORT_KEY_COUNT; i++)
    {
        String key = string_newf("k%zu", i);

        if(!hashset_contains(fnv_hashset, (const void*)key, string_length(key)))
        {
            logger_log(LogLevel_Error, "Cannot find key \"%s\"", key);
            string_free(key);
            return 1;
        }

        hashset_remove(fnv_hashset, (const void*)key, string_length(key));

        if(hashset_contains(fnv_hashset, (const void*)key, string_length(key)))
        {
            logger_log(LogLevel_Error, "Key \"%s\" was not removed", key);
            string_free(key);
            return 1;
        }

        string_free(key);
    }

    if(hashset_size(fnv_hashset) != 0)
    {
        logger_log(LogLevel_Error, "Fnv hashset is not completely empty");
        return 1;
    }

    hashset_free(fnv_hashset);

    logger_release();

    return 0;
}
