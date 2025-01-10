/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashmap.h"
#include "libromano/math/common32.h"
#include "libromano/bit.h"
#include "libromano/random.h"
#include "libromano/logger.h"

#define INTERNED_SIZE ((sizeof(size_t) - 1) + 4)

typedef enum
{
    BucketFlag_KeyInterned = 0x1
} BucketFlag;

ROMANO_PACKED_STRUCT(struct _Bucket {
    void* key;
    uint32_t key_size;
    void* value;
    uint32_t value_size;
    uint32_t probe_length;
    uint32_t flags;
});

typedef struct _Bucket Bucket;

ROMANO_FORCE_INLINE void bucket_set_flag(Bucket* bucket, uint32_t flag)
{
    bucket->flags |= flag;
}

ROMANO_FORCE_INLINE void bucket_unset_flag(Bucket* bucket, uint32_t flag)
{
    bucket->flags &= ~flag;
}

ROMANO_FORCE_INLINE bool bucket_has_flag(Bucket* bucket, uint32_t flag)
{
    return bucket->flags & flag;
}

void bucket_new(Bucket* bucket,
                const void* key, 
                const uint32_t key_size,
                void* value,
                const uint32_t value_size,
                const uint32_t probe_length)
{
    assert(bucket != NULL);

    memset(bucket, 0, sizeof(Bucket));

    if(key_size < (INTERNED_SIZE))
    {
        memcpy(bucket, key, key_size * sizeof(char));
        ((char*)bucket)[INTERNED_SIZE] = (key_size & 0xFF);
        bucket_set_flag(bucket, BucketFlag_KeyInterned);
    }
    else
    {
        bucket->key = malloc((key_size) * sizeof(char));
        memcpy(bucket->key, key, key_size * sizeof(char));
#if ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN
        bucket->key_size = mem_bswapu32(key_size);
#else
        bucket->key_size = key_size;
#endif /* ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN */
        bucket_unset_flag(bucket, BucketFlag_KeyInterned);
    }

    if(value_size == 0)
    {
        bucket->value = value;
    }
    else
    {
        if(value_size <= 8)
        {
            memcpy(&bucket->value, value, value_size);
        }
        else
        {
            bucket->value = malloc(value_size);
            memcpy(bucket->value, value, value_size);
        }
    }

    bucket->value_size = value_size;

    bucket->probe_length = probe_length;
}

ROMANO_FORCE_INLINE void* bucket_get_value(Bucket* bucket)
{
    if(bucket->value_size <= 8)
    {
        return &bucket->value;
    }
    else
    {
        return bucket->value;
    }
}

ROMANO_FORCE_INLINE uint32_t bucket_get_value_size(Bucket* bucket)
{
    return bucket->value_size;
}

ROMANO_FORCE_INLINE void bucket_update_value(Bucket* bucket, 
                                             void* value,
                                             const uint32_t value_size)
{
    memset(&bucket->value, 0, sizeof(void*));

    if(bucket->value_size > 0)
    {
        if(value_size > 8)
        {
            if(value_size == bucket->value_size)
            {
                memcpy(bucket->value, value, value_size);
            }
            else if(value_size == 0)
            {
                free(bucket->value);
                bucket->value = value;
            }
            else
            {
                bucket->value = realloc(bucket->value, value_size);
                memcpy(bucket->value, value, value_size);
            }
        }
        else
        {
            if(value_size == bucket->value_size)
            {
                memcpy(&bucket->value, value, value_size);
            }
            else if(bucket->value_size > 8)
            {
                free(bucket->value);
                memcpy(&bucket->value, value, value_size);
            }
            else
            {
                memcpy(&bucket->value, value, value_size);
            }
        }
    }
    else if(value_size > 0)
    {
        if(value_size > 8)
        {
            bucket->value = malloc(value_size);
            memcpy(bucket->value, value, value_size);
        }
        else
        {
            memcpy(&bucket->value, value, value_size);
        }
    }
    else
    {
        bucket->value = value;
    }
    
    bucket->value_size = value_size;
}

ROMANO_FORCE_INLINE uint32_t bucket_get_key_size(Bucket* bucket)
{
    if(bucket_has_flag(bucket, BucketFlag_KeyInterned))
    {
        char key_size = (((char*)bucket)[INTERNED_SIZE]);

        return (uint32_t)key_size;
    }

#if ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN
    return mem_bswapu32(bucket->key_size);
#else
    return bucket->key_size;
#endif /* ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN */
}

ROMANO_FORCE_INLINE void* bucket_get_key(Bucket* bucket)
{
    if(bucket_has_flag(bucket, BucketFlag_KeyInterned))
    {
        return (void*)bucket;
    }

    return bucket->key;
}

ROMANO_FORCE_INLINE uint32_t bucket_get_probe_length(Bucket* bucket)
{
    return bucket->probe_length;
}

ROMANO_FORCE_INLINE bool bucket_is_empty(Bucket* bucket)
{
    return bucket_get_key_size(bucket) == 0;
}

ROMANO_FORCE_INLINE void bucket_set_empty(Bucket* bucket)
{
    memset(bucket, 0, sizeof(Bucket));
}

void bucket_free(Bucket* bucket)
{
    assert(bucket != NULL);
    
    if(!(bucket_get_key_size(bucket) < INTERNED_SIZE))
    {
        free(bucket->key);
    }

    if(bucket_get_value_size(bucket) > 8)
    {
        free(bucket->value);
    }

    memset(bucket, 0, sizeof(Bucket));
}

#define HASHMAP_MAX_LOAD 0.95f
#define MAX_PROBES_CAP_LOG_BASE 6.0f
#define HASHMAP_GROWTH_RATE 2.0f
#define HASHMAP_INITIAL_CAPACITY 1024

struct _HashMap {
   Bucket* buckets;
   size_t size;
   size_t capacity;
   uint32_t hashkey;
};

ROMANO_FORCE_INLINE uint32_t hashmap_hash(const HashMap* hashmap, const void* key, const size_t key_size)
{
    return hash_murmur3(key, key_size, hashmap->hashkey);
}

size_t hashmap_get_new_capacity(HashMap* hashmap)
{
    return round_u64_to_next_pow2(hashmap->capacity + 1) + 1;
}

void hashmap_move_entry(HashMap* hashmap, Bucket* entry);

void hashmap_grow(HashMap* hashmap,
                  size_t capacity)
{
    Bucket* old_buckets;
    Bucket* bucket;

    size_t i;
    size_t index;
    size_t old_capacity;

    assert(hashmap != NULL);

    old_buckets = hashmap->buckets;
    old_capacity = hashmap->capacity;
    
    hashmap->buckets = (Bucket*)calloc(capacity, sizeof(Bucket));
    hashmap->capacity = capacity;
    hashmap->size = 0;
    hashmap->hashkey ^= random_next_uint32();

    if(old_buckets != NULL)
    {
        for(i = 0; i < old_capacity; i++)
        {
            bucket = &old_buckets[i];

            if(bucket_is_empty(bucket))
            {
                continue;
            }

            hashmap_move_entry(hashmap, bucket);

            /*
            hashmap_insert(hashmap, 
                           bucket_get_key(bucket),
                           bucket_get_key_size(bucket),
                           bucket_get_value(bucket),
                           bucket_get_value_size(bucket));

            bucket_free(bucket);
            */
        }

        free(old_buckets);
    }
}

HashMap* hashmap_new(void)
{
    HashMap* hashmap = (HashMap*)malloc(sizeof(HashMap));
    hashmap->buckets = NULL;
    hashmap->size = 0;
    hashmap->capacity = 0;
    hashmap->hashkey ^= random_next_uint32();

    hashmap_grow(hashmap, HASHMAP_INITIAL_CAPACITY);

    return hashmap;
}

size_t hashmap_size(HashMap* hashmap)
{
    return hashmap->size;
}

size_t hashmap_capacity(HashMap* hashmap)
{
    return hashmap->capacity;
}

void hashmap_move_entry(HashMap* hashmap, Bucket* entry)
{
    Bucket* bucket;
    Bucket new_entry;
    Bucket tmp;

    size_t index;
    uint32_t max_probes;
    bool has_swapped;

    if((hashmap->size + 1) > hashmap->capacity * HASHMAP_MAX_LOAD)
    {
        hashmap_grow(hashmap, hashmap_get_new_capacity(hashmap));
    }

    has_swapped = false;

    start:

    max_probes = (uint32_t)mathf_logN(hashmap->capacity, MAX_PROBES_CAP_LOG_BASE);

    if(!has_swapped)
    {
        memmove(&new_entry, entry, sizeof(Bucket));
    }
    
    new_entry.probe_length = 0;

    index = hashmap_hash(hashmap, bucket_get_key(&new_entry), bucket_get_key_size(&new_entry)) % hashmap->capacity;

    while(1)
    {
        bucket = &hashmap->buckets[index];

        if(!bucket_is_empty(bucket))
        {
            if(new_entry.probe_length > bucket->probe_length)
            {
                tmp = new_entry;
                new_entry = *bucket;
                *bucket = tmp;

                has_swapped = true;
            }

            index = (index + 1) % hashmap->capacity;
            new_entry.probe_length++;

            if(new_entry.probe_length >= max_probes)
            {
                hashmap_grow(hashmap, hashmap_get_new_capacity(hashmap));
                goto start;
            }
        }
        else
        {
            *bucket = new_entry;

            hashmap->size++;

            return;
        }
    }
}

void hashmap_insert(HashMap* hashmap,
                    const void* key,
                    const uint32_t key_size,
                    void* value,
                    const uint32_t value_size)
{
    Bucket* bucket;
    Bucket entry;
    Bucket tmp;

    size_t index;
    uint32_t max_probes;
    bool has_swapped;

    if((hashmap->size + 1) > hashmap->capacity * HASHMAP_MAX_LOAD)
    {
        hashmap_grow(hashmap, hashmap_get_new_capacity(hashmap));
    }

    has_swapped = false;

    start:

    max_probes = (uint32_t)mathf_logN(hashmap->capacity, MAX_PROBES_CAP_LOG_BASE);

    if(!has_swapped)
    {
        bucket_new(&entry, key, key_size, value, value_size, 0);
    }
    else
    {
        entry.probe_length = 0;
    }

    index = hashmap_hash(hashmap, bucket_get_key(&entry), bucket_get_key_size(&entry)) % hashmap->capacity;

    while(1)
    {
        bucket = &hashmap->buckets[index];

        if(!bucket_is_empty(bucket))
        {
            if(bucket_get_key_size(bucket) == key_size && memcmp(bucket_get_key(bucket), key, (size_t)key_size) == 0)
            {
                return;
            }

            if(entry.probe_length > bucket->probe_length)
            {
                tmp = entry;
                entry = *bucket;
                *bucket = tmp;

                has_swapped = true;
            }

            index = (index + 1) % hashmap->capacity;
            entry.probe_length++;

            if(entry.probe_length >= max_probes)
            {
                hashmap_grow(hashmap, hashmap_get_new_capacity(hashmap));
                goto start;
            }
        }
        else
        {
            *bucket = entry;

            hashmap->size++;

            return;
        }
    }
}

void hashmap_update(HashMap* hashmap,
                    const void* key,
                    const uint32_t key_size,
                    void* value,
                    const uint32_t value_size)
{
    Bucket* bucket;

    size_t index;
    uint32_t probe_length;

    probe_length = 0;

    index = hashmap_hash(hashmap, key, key_size) % hashmap->capacity;

    while(1)
    {
        bucket = &hashmap->buckets[index];

        if(!bucket_is_empty(bucket))
        {
            if((size_t)bucket_get_key_size(bucket) == key_size && memcmp(bucket_get_key(bucket), key, (size_t)key_size) == 0)
            {
                bucket_update_value(bucket, value, value_size);
                return;
            }

            index = (index + 1) % hashmap->capacity;
            probe_length++;
        }
        else
        {
            bucket_new(bucket, key, key_size, value, value_size, probe_length);

            hashmap->size++;

            return;
        }
    }
}

void* hashmap_get(HashMap* hashmap,
                  const void* key,
                  const uint32_t key_size,
                  uint32_t* value_size)
{
    Bucket* bucket;

    size_t index;
    uint32_t probe_length;

    index = hashmap_hash(hashmap, key, key_size) % hashmap->capacity;
    probe_length = 0;

    while(1)
    {
        bucket = &hashmap->buckets[index];

        if(bucket_get_key_size(bucket) == key_size && memcmp(bucket_get_key(bucket), key, (size_t)key_size) == 0)
        {
            *value_size = bucket_get_value_size(bucket);

            return bucket_get_value(bucket);
        }

        if(bucket_is_empty(bucket) || probe_length > bucket_get_probe_length(bucket))
        {
            return NULL;
        }

        index = (index + 1) % hashmap->capacity;
        probe_length++;
    }
}

void hashmap_remove(HashMap* hashmap,
                    const void* key,
                    const uint32_t key_size)
{
    Bucket* bucket;
    Bucket* backward_shift_bucket;

    size_t index;
    uint32_t probe_length;

    index = hashmap_hash(hashmap, key, key_size) % hashmap->capacity;
    probe_length = 0;

    while(1)
    {
        bucket = &hashmap->buckets[index];

        if(bucket_is_empty(bucket) || probe_length > bucket_get_probe_length(bucket))
        {
            return;
        }

        if(bucket_get_key_size(bucket) == key_size && memcmp(bucket_get_key(bucket), key, (size_t)key_size) == 0)
        {
            bucket_free(bucket);

            hashmap->size--;

            while(1)
            {
                bucket_set_empty(bucket);

                index = (index + 1) % hashmap->capacity;

                backward_shift_bucket = &hashmap->buckets[index];

                if(bucket_is_empty(backward_shift_bucket) || bucket_get_probe_length(backward_shift_bucket) == 0)
                {
                    return;
                }

                backward_shift_bucket->probe_length--;
                *bucket = *backward_shift_bucket;
                bucket = backward_shift_bucket;
            }
        }

        index = (index + 1) % hashmap->capacity;
        probe_length++;
    }
}

void hashmap_free(HashMap* hashmap)
{
    size_t i;

    assert(hashmap != NULL);

    if(hashmap->buckets != NULL)
    {
        for(i = 0; i < hashmap->size; i++)
        {
            if(!bucket_is_empty(&hashmap->buckets[i]))
            {
                bucket_free(&hashmap->buckets[i]);
            }
        }

        free(hashmap->buckets);
    }

    free(hashmap);
}