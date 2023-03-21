/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashmap.h"
#include "libromano/str.h"

struct _entry {
    str key;
    void* value;
};

typedef struct _entry entry;

static entry* entry_new(const char* key, 
                         void* value,
                         size_t value_size)
{
    entry* new_entry;

    new_entry = malloc(sizeof(entry));

    new_entry->key = str_new(key);
    new_entry->value = malloc(value_size);

    memcpy(value, new_entry->value, value_size);

    return new_entry;
}

static void entry_free(entry* entry)
{
    str_free(entry->key);
    free(entry->value);

    free(entry);
}

struct _hashmap {
   entry* entries;
   size_t size;
   size_t capacity;
};

hashmap* hashmap_new()
{
    hashmap* hashmap = malloc(sizeof(hashmap));

    return hashmap;
}

void hashmap_insert(hashmap* hashmap, 
                    const char* key, 
                    void* value, 
                    size_t value_size)
{
}

void* hashmap_get(hashmap* hashmap,
                  const char* key)
{
}

void hashmap_free(hashmap* hashmap)
{
    size_t i;

    if(hashmap != NULL)
    {
        for(i = 0; i < hashmap->size; i++)
        {
            entry_free(&hashmap->entries[i]);
        }

        free(hashmap);
    }
}
