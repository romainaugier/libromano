/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashmap.h"
#include "libromano/str.h"
#include "libromano/hash.h"

#include <stdlib.h>
#include <string.h>

struct _entry {
    str key;
    void* value;
    size_t value_size;
};

typedef struct _entry entry_t;

static void entry_new(entry_t* entry,
                      const char* key, 
                      void* value,
                      size_t value_size)
{
    entry->key = str_new(key);
    entry->value = malloc(value_size);

    memcpy(entry->value, value, value_size);

    entry->value_size = value_size;
}

static void entry_free(entry_t* entry)
{
    str_free(entry->key);
    free(entry->value);
}

#define HASHMAP_MAX_LOAD 0.75
#define TOMBSTONE 0xFFFFFFFFFFFFFFFF
#define GOLDEN_RATIO 1.61f
#define HASHMAP_INITIAL_CAPACITY 1024

struct _hashmap {
   entry_t* entries;
   size_t size;
   size_t capacity;
};

void hashmap_grow(hashmap_t* hashmap, size_t capacity);

hashmap_t* hashmap_new(void)
{
    hashmap_t* hm = (hashmap_t*)malloc(sizeof(hashmap_t));
    hm->entries = NULL;
    hm->size = 0;
    hm->capacity = 0;

    hashmap_grow(hm, HASHMAP_INITIAL_CAPACITY);

    return hm;
}

entry_t* hashmap_find(hashmap_t* hashmap, const char* key)
{
    entry_t* entry;
    entry_t* tombstone;
    uint32_t index;
    size_t key_len;

    index = 0;
    entry = NULL;
    tombstone = NULL;
    key_len = strlen(key);
    index = hash_fnv1a(key, key_len) % hashmap->capacity;

    for(;;)
    {
        entry = &hashmap->entries[index];

        if(entry->key == NULL)
        {
            if(entry->value_size != TOMBSTONE)
            {
                return tombstone != NULL ? tombstone : entry;
            }
            else
            {
                if(tombstone == NULL) tombstone = entry;
            }
        }
        else if(strncmp(key, entry->key, key_len > str_length(entry->key) ? str_length(entry->key) : key_len) == 0)
        {
            return entry;
        }

        index = (index + 1) % hashmap->capacity;
    }

    return NULL;
}

void hashmap_grow(hashmap_t* hashmap, size_t capacity)
{
    size_t i;
    entry_t* entries;
    entry_t* entry;
    entry_t* new_entry;
    
    entries = (entry_t*)malloc(capacity * sizeof(entry_t));

    for(i = 0; i < capacity; i++)
    {
        entries[i].key = NULL;
        entries[i].value = NULL;
        entries[i].value_size = 0;
    }

    hashmap->size = 0;

    if(hashmap->entries != NULL)
    {
        for(i = 0; i < hashmap->capacity; i++)
        {
            entry = &hashmap->entries[i];

            if(entry->key == NULL)
            {
                continue;
            }

            new_entry = hashmap_find(hashmap, entry->key);
            new_entry->key = entry->key;
            new_entry->value = entry->value;
            new_entry->value_size = entry->value_size;

            hashmap->size++;
        }

        free(hashmap->entries);
    }

    hashmap->entries = entries;
    hashmap->capacity = capacity;
}

void hashmap_insert(hashmap_t* hashmap, 
                    const char* key, 
                    void* value, 
                    size_t value_size)
{
    size_t new_capacity;
    entry_t* entry;

    if((hashmap->size + 1) > (hashmap->capacity * HASHMAP_MAX_LOAD))
    {
        new_capacity = (size_t)((float)hashmap->capacity * GOLDEN_RATIO);
        hashmap_grow(hashmap, new_capacity);
    }

    entry = hashmap_find(hashmap, key);

    if(entry->key == NULL)
    {
        hashmap->size++;
    }
    else
    {
        entry_free(entry);
    }

    entry_new(entry, key, value, value_size);
}

void hashmap_update(hashmap_t* hashmap,
                    const char* key,
                    void* value,
                    size_t value_size)
{
    entry_t* entry;

    entry = hashmap_find(hashmap, key);

    if(entry->key == NULL)
    {
        hashmap->size++;
    }
    else
    {
        entry_free(entry);
    }

    entry_new(entry, key, value, value_size);
}

void* hashmap_get(hashmap_t* hashmap,
                  const char* key,
                  size_t* value_size)
{
    entry_t* entry;

    if(hashmap->size == 0)
    {
        return NULL;
    }

    entry = hashmap_find(hashmap, key);

    if(entry->key == NULL)
    {
        return NULL;
    }

    *value_size = entry->value_size;
    return entry->value;
}

void hashmap_remove(hashmap_t* hashmap,
                    const char* key)
{
    entry_t* entry;

    if(hashmap->size == 0)
    {
        return;
    }

    entry = hashmap_find(hashmap, key);

    if(entry->key == NULL)
    {
        return;
    }

    entry_free(entry);

    entry->key = NULL;
    entry->value = NULL;
    entry->value_size = TOMBSTONE;
}

void hashmap_free(hashmap_t* hashmap)
{
    size_t i;

    if(hashmap != NULL)
    {
        for(i = 0; i < hashmap->size; i++)
        {
            if(&hashmap->entries[i].key != NULL)
            {
                entry_free(&hashmap->entries[i]);
            }
        }

        if(hashmap->entries != NULL)
        {
            free(hashmap->entries);
        }

        free(hashmap);
    }
}
