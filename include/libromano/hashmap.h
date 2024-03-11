/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_HASHMAP)
#define __LIBROMANO_HASHMAP

#include "libromano/libromano.h"
#include "libromano/hash.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* 
    Flag used to turn on an optimization similar to sso for keys and values of size less than 
    sizeof(size_t) - 1 + sizeof(void*)
    Interning might cause issues on little endian platforms, it needs to be tested and investigated
    This is the only file that is header only because we want to be able to turn on/off interning
*/

#if defined(ROMANO_HASHMAP_INTERN_SMALL_VALUES)
#define __ROMANO_HASHMAP_INTERN_SMALL_VALUES 1
#else
#define __ROMANO_HASHMAP_INTERN_SMALL_VALUES 0
#endif /* defined(ROMANO_HASHMAP_INTERN_SMALL_VALUES) */

ROMANO_CPP_ENTER

#define INTERNED_SIZE ((sizeof(size_t) - 1) + ROMANO_SIZEOF_PTR)

#define ENTRY_KEY_CAN_BE_INTERNED(__key_size) (__key_size <= (INTERNED_SIZE - 1))
#define ENTRY_KEY_INTERNED_SIZE(entry) (((char*)entry)[INTERNED_SIZE] & 0xFF)
#define ENTRY_KEY_INTERNED(entry) (ENTRY_KEY_INTERNED_SIZE(entry) <= (INTERNED_SIZE - 1))

#define ENTRY_VALUE_CAN_BE_INTERNED(__value_size) (__value_size <= INTERNED_SIZE)
#define ENTRY_VALUE_INTERNED_SIZE(entry) (((char*)entry)[INTERNED_SIZE * 2] & 0xFF)
#define ENTRY_VALUE_INTERNED(entry) (ENTRY_VALUE_INTERNED_SIZE(entry) <= INTERNED_SIZE)

ROMANO_PACKED_STRUCT(struct _entry {
    char* key;
    size_t key_size;
    void* value;
    size_t value_size;
});

typedef struct _entry entry_t;

static void entry_new(entry_t* entry,
                      const char* key, 
                      void* value,
                      size_t value_size)
{
    size_t key_size;

    assert(entry != NULL);

    key_size = strlen(key);

#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES 
    if(ENTRY_KEY_CAN_BE_INTERNED(key_size))
    {
        memcpy(entry, key, key_size * sizeof(char));
        ((char*)entry)[key_size] = '\0';
        ((char*)entry)[INTERNED_SIZE] = (key_size & 0xFF);
    }
    else
    {
        entry->key = (char*)malloc((key_size + 1) * sizeof(char));
        memcpy(entry->key, key, key_size * sizeof(char));
        entry->key[key_size] = '\0';
        entry->key_size = key_size;
    }

    if(ENTRY_VALUE_CAN_BE_INTERNED(value_size))
    {
        memcpy(&((char*)entry)[INTERNED_SIZE + 1], value, value_size);
        ((char*)entry)[INTERNED_SIZE * 2] = (value_size & 0xFF);
    }
    else
    {
        entry->value = malloc(value_size);
        memcpy(entry->value, value, value_size);
        entry->value_size = value_size;
    }

#else
    entry->key = (char*)malloc((key_size + 1) * sizeof(char));
    memcpy(entry->key, key, key_size * sizeof(char));
    entry->key[key_size] = '\0';
    entry->key_size = key_size;

    entry->value = malloc(value_size);
    memcpy(entry->value, value, value_size);
    entry->value_size = value_size;
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
}

ROMANO_FORCE_INLINE static size_t entry_get_value_size(entry_t* entry)
{
#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    size_t value_size;

    value_size = (size_t)(((char*)entry)[INTERNED_SIZE * 2]);

    if(value_size <= INTERNED_SIZE)
    {
        return value_size;
    }
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
    return entry->value_size;
}

ROMANO_FORCE_INLINE static void* entry_get_value(entry_t* entry)
{
#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    if(entry_get_value_size(entry) <= INTERNED_SIZE)
    {
        return &(entry->value);
    }
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
    return entry->value;
}

ROMANO_FORCE_INLINE static size_t entry_get_key_size(entry_t* entry)
{
#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    size_t key_size = (size_t)(((char*)entry)[INTERNED_SIZE]);

    return key_size <= INTERNED_SIZE ? key_size : entry->key_size;
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
    return entry->key_size;
}

ROMANO_FORCE_INLINE static char* entry_get_key(entry_t* entry)
{
#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    if(entry_get_key_size(entry) <= INTERNED_SIZE)
    {
        return (char*)entry;
    }
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
    return entry->key;
}

static void entry_free(entry_t* entry)
{
    assert(entry != NULL);
    
#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    if(!ENTRY_KEY_INTERNED(entry))
    {
        free(entry->key);
    }

    if(!ENTRY_VALUE_INTERNED(entry))
    {
        free(entry->value);
    }
#else
    free(entry->key);
    free(entry->value);
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */

    entry->key = NULL;
    entry->key_size = 0;
    entry->value = NULL;
    entry->value_size = 0;
}


struct _hashmap;
typedef struct _hashmap hashmap_t;

#define HASHMAP_MAX_LOAD 0.75
#define TOMBSTONE 0xFFFFFFFFFFFFFFFF
#define GOLDEN_RATIO 1.61f
#define HASHMAP_INITIAL_CAPACITY 1024

struct _hashmap {
   entry_t* entries;
   size_t size;
   size_t capacity;
};

static void hashmap_grow(hashmap_t* hashmap,
                         size_t capacity);

static hashmap_t* hashmap_new(void)
{
    hashmap_t* hm = (hashmap_t*)malloc(sizeof(hashmap_t));
    hm->entries = NULL;
    hm->size = 0;
    hm->capacity = 0;

    hashmap_grow(hm, HASHMAP_INITIAL_CAPACITY);

    return hm;
}

static entry_t* entry_find(entry_t* entries,
                           size_t capacity,
                           const char* key,
                           size_t key_len)
{
    entry_t* entry;
    entry_t* tombstone;
    uint32_t index;
    size_t entry_key_len;

    assert(entries != NULL);
    assert(key != NULL);

    index = 0;
    entry = NULL;
    tombstone = NULL;
    index = hash_fnv1a(key, key_len) % capacity;

    for(;;)
    {
        entry = &entries[index];

        entry_key_len = entry_get_key_size(entry);

#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
        if(entry_key_len == 0)
#else
        if(entry->key == NULL)
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
        {
            if(entry_get_value_size(entry) != TOMBSTONE)
            {
                return tombstone != NULL ? tombstone : entry;
            }
            else
            {
                if(tombstone == NULL) tombstone = entry;
            }
        }
        else if(entry_key_len == key_len && memcmp(key, entry_get_key(entry), key_len) == 0)
        {
            return entry;
        }

        index = (index + 1) % capacity;
    }

    return NULL;
}

static entry_t* hashmap_find(hashmap_t* hashmap,
                             const char* key,
                             size_t key_len)
{
    return entry_find(hashmap->entries, hashmap->capacity, key, key_len);
}

static void hashmap_grow(hashmap_t* hashmap,
                         size_t capacity)
{
    size_t i;
    entry_t* entries;
    entry_t* entry;
    entry_t* new_entry;

    assert(hashmap != NULL);
    
    entries = (entry_t*)malloc(capacity * sizeof(entry_t));

    for(i = 0; i < capacity; i++)
    {
        entries[i].key = NULL;
        entries[i].key_size = 0;
        entries[i].value = NULL;
        entries[i].value_size = 0;
    }

    hashmap->size = 0;

    if(hashmap->entries != NULL)
    {
        for(i = 0; i < hashmap->capacity; i++)
        {
            entry = &hashmap->entries[i];

#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
            if(entry_get_key_size(entry) == 0)
#else
            if(entry->key == NULL)
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
            {
                continue;
            }

            new_entry = entry_find(entries, capacity, entry_get_key(entry), entry_get_key_size(entry));

            memmove(new_entry, entry, sizeof(entry_t));

            hashmap->size++;
        }

        free(hashmap->entries);
    }

    hashmap->entries = entries;
    hashmap->capacity = capacity;
}

static void hashmap_insert(hashmap_t* hashmap, 
                           const char* key, 
                           size_t key_len,
                           void* value, 
                           size_t value_size)
{
    size_t new_capacity;
    entry_t* entry;

    assert(hashmap != NULL);
    assert(key != NULL);
    assert(value != NULL);

    if((hashmap->size + 1) > (hashmap->capacity * HASHMAP_MAX_LOAD))
    {
        new_capacity = (size_t)((float)hashmap->capacity * GOLDEN_RATIO);
        hashmap_grow(hashmap, new_capacity);
    }

    entry = hashmap_find(hashmap, key, key_len);

#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    if(entry_get_key_size(entry) == 0)
#else
    if(entry->key == NULL)
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
    {
        hashmap->size++;
    }
    else
    {
        entry_free(entry);
    }

    entry_new(entry, key, value, value_size);
}

static void hashmap_update(hashmap_t* hashmap,
                           const char* key,
                           size_t key_len,
                           void* value,
                           size_t value_size)
{
    entry_t* entry;

    assert(hashmap != NULL);
    assert(key != NULL);
    assert(value != NULL);

    entry = hashmap_find(hashmap, key, key_len);

#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    if(entry_get_key_size(entry) == 0)
#else
    if(entry->key == NULL)
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
    {
        hashmap->size++;
    }
    else
    {
        entry_free(entry);
    }

    entry_new(entry, key, value, value_size);
}

static void* hashmap_get(hashmap_t* hashmap,
                         const char* key,
                         size_t key_len,
                         size_t* value_size)
{
    entry_t* entry;

    assert(hashmap != NULL);
    assert(key != NULL);
    assert(value_size != NULL);

    if(hashmap->size == 0)
    {
        return NULL;
    }

    entry = hashmap_find(hashmap, key, key_len);

#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    if(entry_get_key_size(entry) == 0)
#else
    if(entry->key == NULL)
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
    {
        return NULL;
    }

    *value_size = entry_get_value_size(entry);

    return entry_get_value(entry);
}

static void hashmap_remove(hashmap_t* hashmap,
                           const char* key,
                           size_t key_len)
{
    entry_t* entry;

    assert(hashmap != NULL);
    assert(key != NULL);

    if(hashmap->size == 0)
    {
        return;
    }

    entry = hashmap_find(hashmap, key, key_len);

#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    if(entry_get_key_size(entry) == 0)
#else
    if(entry->key == NULL)
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
    {
        return;
    }

    entry_free(entry);

    entry->key = NULL;
    entry->value = NULL;
    entry->value_size = TOMBSTONE;

    hashmap->size--;
}

static void hashmap_free(hashmap_t* hashmap)
{
    size_t i;

    assert(hashmap != NULL);

    for(i = 0; i < hashmap->size; i++)
    {
#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
        if(entry_get_key_size(&hashmap->entries[i]) == 0)
#else
        if(hashmap->entries[i].key == NULL)
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
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

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_HASHMAP) */
