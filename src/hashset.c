/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashset.h"
#include "libromano/bit.h"
#include "libromano/random.h"
#include "libromano/error.h"
#include "libromano/math/common32.h"
#include "libromano/hash.h"
#include "libromano/memory.h"

extern ErrorCode g_current_error;

/* Hashset */

#define INTERNED_SIZE ((sizeof(size_t) - 1) + 4)

typedef enum
{
    BucketFlag_KeyInterned = 0x1,
} BucketFlag;

ROMANO_PACKED_STRUCT(struct _Bucket
{
    void* key;
    uint32_t key_size;
    uint32_t hash;
    uint16_t probe_length;
    uint16_t flags;
});

typedef struct _Bucket Bucket;

ROMANO_FORCE_INLINE void bucket_set_flag(Bucket* bucket, uint32_t flag)
{
    bucket->flags |= (uint16_t)flag;
}

ROMANO_FORCE_INLINE void bucket_unset_flag(Bucket* bucket, uint32_t flag)
{
    bucket->flags &= ~(uint16_t)flag;
}

ROMANO_FORCE_INLINE bool bucket_has_flag(const Bucket* bucket, const uint32_t flag)
{
    return bucket->flags & (uint16_t)flag;
}

void hashset_bucket_new(Bucket* bucket,
                        const void* key,
                        const uint32_t key_size,
                        const uint32_t hash,
                        const uint32_t probe_length)
{
    ROMANO_ASSERT(bucket != NULL, "");

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

    bucket->hash = hash;
    bucket->probe_length = (uint16_t)probe_length;
}

ROMANO_FORCE_INLINE uint32_t bucket_get_key_size(const Bucket* bucket)
{
    if(bucket_has_flag(bucket, BucketFlag_KeyInterned))
    {
        char key_size = (((const char*)bucket)[INTERNED_SIZE]);

        return (uint32_t)key_size;
    }

#if ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN
    return mem_bswapu32(bucket->key_size);
#else
    return bucket->key_size;
#endif /* ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN */
}

ROMANO_FORCE_INLINE void* bucket_get_key(const Bucket* bucket)
{
    if(bucket_has_flag(bucket, BucketFlag_KeyInterned))
        return (void*)bucket;

    return bucket->key;
}

ROMANO_FORCE_INLINE uint32_t bucket_get_hash(const Bucket* bucket)
{
    return bucket->hash;
}

ROMANO_FORCE_INLINE void bucket_set_hash(Bucket* bucket, const uint32_t hash)
{
    bucket->hash = hash;
}

ROMANO_FORCE_INLINE uint32_t bucket_get_probe_length(const Bucket* bucket)
{
    return (uint32_t)bucket->probe_length;
}

ROMANO_FORCE_INLINE void bucket_set_probe_length(Bucket* bucket, const uint32_t probe_length)
{
    bucket->probe_length = (uint16_t)probe_length;
}

ROMANO_FORCE_INLINE bool bucket_is_empty(const Bucket* bucket)
{
    return bucket_get_key_size(bucket) == 0;
}

ROMANO_FORCE_INLINE void bucket_set_empty(Bucket* bucket)
{
    memset(bucket, 0, sizeof(Bucket));
}

ROMANO_FORCE_INLINE bool bucket_compare_key(const Bucket* bucket,
                                            const void* key,
                                            const uint32_t key_size,
                                            const uint32_t hash)
{
    return (bucket->hash == hash) &&
           (bucket_get_key_size(bucket) == key_size) &&
           (memcmp(bucket_get_key(bucket), key, key_size) == 0);
}

void hashset_bucket_free(Bucket* bucket)
{
    ROMANO_ASSERT(bucket != NULL, "bucket is NULL");

    if(bucket_is_empty(bucket))
        return;

    if(!bucket_has_flag(bucket, BucketFlag_KeyInterned))
        free(bucket_get_key(bucket));

    memset(bucket, 0, sizeof(Bucket));
}

#define HASHSET_MAX_LOAD 0.9f
#define HASHSET_INITIAL_CAPACITY 1024

struct _Hashset
{
    Bucket* buckets;
    size_t size;
    size_t capacity;
    hashset_hash_func hash_func;
    uint32_t hashkey;
    uint32_t max_probes;
};

ROMANO_FORCE_INLINE uint32_t hashset_hash(const Hashset* hashset, const void* key, const size_t key_size)
{
    return hashset->hash_func(key, key_size, hashset->hashkey);
}

ROMANO_FORCE_INLINE size_t hashset_index(const Hashset* hashset, const uint32_t hash)
{
    return hash & (hashset->capacity - 1);
}

ROMANO_FORCE_INLINE size_t hashset_get_new_capacity(Hashset* hashset)
{
    return round_u64_to_next_pow2(hashset->capacity + 1) + 1;
}

void hashset_move_entry(Hashset* hashset, Bucket* entry, const bool rehash);

void hashset_grow(Hashset* hashset,
                  const size_t capacity,
                  const bool rehash)
{
    Bucket* old_buckets;
    Bucket* bucket;

    size_t i;
    size_t old_capacity;

    ROMANO_ASSERT(hashset != NULL, "");

    old_buckets = hashset->buckets;
    old_capacity = hashset->capacity;

    hashset->buckets = (Bucket*)calloc(capacity, sizeof(Bucket));
    hashset->capacity = capacity;
    hashset->size = 0;

    if(rehash)
        hashset->hashkey ^= random_next_uint32();

    hashset->max_probes = (uint32_t)mathf_log2((float)hashset->capacity);

    if(old_buckets != NULL)
    {
        for(i = 0; i < old_capacity; i++)
        {
            bucket = &old_buckets[i];

            if(bucket_is_empty(bucket))
                continue;

            hashset_move_entry(hashset, bucket, rehash);
        }

        free(old_buckets);
    }
}

Hashset* hashset_new(size_t initial_capacity)
{
    Hashset* hashset = (Hashset*)malloc(sizeof(Hashset));

    if(hashset == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    hashset->buckets = NULL;
    hashset->hash_func = hash_wyhash32;
    hashset->size = 0;
    hashset->capacity = 0;
    hashset->hashkey ^= random_next_uint32();

    if(initial_capacity == 0)
        initial_capacity = HASHSET_INITIAL_CAPACITY;
    else
        initial_capacity = round_u64_to_next_pow2(initial_capacity + 1) + 1;

    hashset_grow(hashset,
                 initial_capacity,
                 false);

    return hashset;
}

size_t hashset_size(Hashset* hashset)
{
    return hashset->size;
}

size_t hashset_capacity(Hashset* hashset)
{
    return hashset->capacity;
}

void hashset_set_hash_func(Hashset* hashset, hashset_hash_func func)
{
    hashset->hash_func = func;
}

void hashset_move_entry(Hashset* hashset,
                        Bucket* entry,
                        const bool rehash)
{
    Bucket* bucket;
    Bucket new_entry;
    Bucket tmp;

    size_t index;

    uint32_t hash;

    memmove(&new_entry, entry, sizeof(Bucket));

    bucket_set_probe_length(&new_entry, 0);

    if(rehash)
    {
        hash = hashset_hash(hashset, bucket_get_key(&new_entry), bucket_get_key_size(&new_entry));
        bucket_set_hash(&new_entry, hash);
    }
    else
    {
        hash = bucket_get_hash(&new_entry);
    }

    index = hashset_index(hashset, hash);

    while(1)
    {
        bucket = &hashset->buckets[index];

        if(!bucket_is_empty(bucket))
        {
            if(new_entry.probe_length > bucket_get_probe_length(bucket))
            {
                tmp = new_entry;
                new_entry = *bucket;
                *bucket = tmp;
            }

            index = (index + 1) & (hashset->capacity - 1);
            new_entry.probe_length++;
        }
        else
        {
            *bucket = new_entry;

            hashset->size++;

            return;
        }
    }
}

void hashset_insert_bucket(Hashset* hashset,
                           Bucket* entry)
{
    Bucket* bucket;
    Bucket tmp;

    size_t index;
    uint32_t hash;

    if((hashset->size + 1) > hashset->capacity * HASHSET_MAX_LOAD)
        hashset_grow(hashset, hashset_get_new_capacity(hashset), false);

    hash = hashset_hash(hashset, bucket_get_key(entry), bucket_get_key_size(entry));
    bucket_set_hash(entry, hash);
    bucket_set_probe_length(entry, 0);

    index = hashset_index(hashset, hash);

    while(1)
    {
        bucket = &hashset->buckets[index];

        if(!bucket_is_empty(bucket))
        {
            if(entry->probe_length > bucket_get_probe_length(bucket))
            {
                tmp = *entry;
                *entry = *bucket;
                *bucket = tmp;
            }

            index = (index + 1) & (hashset->capacity - 1);
            entry->probe_length++;

            if(entry->probe_length >= hashset->max_probes)
            {
                hashset_grow(hashset, hashset_get_new_capacity(hashset), false);

                hashset_insert_bucket(hashset, entry);

                return;
            }
        }
        else
        {
            *bucket = *entry;

            hashset->size++;

            return;
        }
    }
}

bool hashset_add(Hashset* hashset,
                 const void* key,
                 const uint32_t key_size)
{
    Bucket* bucket;
    Bucket entry;
    Bucket tmp;

    size_t index;
    uint32_t hash;

    if((hashset->size + 1) > hashset->capacity * HASHSET_MAX_LOAD)
        hashset_grow(hashset, hashset_get_new_capacity(hashset), false);

    hash = hashset_hash(hashset, key, key_size);
    hashset_bucket_new(&entry, key, key_size, hash, 0);

    index = hashset_index(hashset, hash);

    while(1)
    {
        bucket = &hashset->buckets[index];

        if(!bucket_is_empty(bucket))
        {
            if(bucket_compare_key(bucket, key, key_size, hash))
            {
                hashset_bucket_free(&entry);
                return false;
            }

            if(entry.probe_length > bucket_get_probe_length(bucket))
            {
                tmp = entry;
                entry = *bucket;
                *bucket = tmp;
            }

            index = (index + 1) & (hashset->capacity - 1);
            entry.probe_length++;

            if(entry.probe_length >= hashset->max_probes)
            {
                hashset_grow(hashset, hashset_get_new_capacity(hashset), false);

                hashset_insert_bucket(hashset, &entry);

                return true;
            }
        }
        else
        {
            *bucket = entry;

            hashset->size++;

            return true;
        }
    }
}

bool hashset_contains(Hashset* hashset,
                      const void* key,
                      const uint32_t key_size)
{
    Bucket* bucket;

    size_t index;
    uint32_t hash;
    uint32_t probe_length;

    hash = hashset_hash(hashset, key, key_size);

    index = hashset_index(hashset, hash);

    probe_length = 0;

    while(1)
    {
        bucket = &hashset->buckets[index];

        if(bucket_compare_key(bucket, key, key_size, hash))
            return true;

        if(bucket_is_empty(bucket) || probe_length > bucket_get_probe_length(bucket))
            return false;

        index = (index + 1) & (hashset->capacity - 1);
        probe_length++;
    }
}

void hashset_remove(Hashset* hashset,
                    const void* key,
                    const uint32_t key_size)
{
    Bucket* bucket;
    Bucket* backward_shift_bucket;

    size_t index;
    uint32_t hash;
    uint32_t probe_length;

    hash = hashset_hash(hashset, key, key_size);

    index = hashset_index(hashset, hash);

    probe_length = 0;

    while(1)
    {
        bucket = &hashset->buckets[index];

        if(bucket_is_empty(bucket) || probe_length > bucket_get_probe_length(bucket))
            return;

        if(bucket_compare_key(bucket, key, key_size, hash))
        {
            hashset_bucket_free(bucket);

            hashset->size--;

            while(1)
            {
                bucket_set_empty(bucket);

                index = (index + 1) & (hashset->capacity - 1);

                backward_shift_bucket = &hashset->buckets[index];

                if(bucket_is_empty(backward_shift_bucket) ||
                   bucket_get_probe_length(backward_shift_bucket) == 0)
                    return;

                bucket_set_probe_length(backward_shift_bucket,
                                        bucket_get_probe_length(backward_shift_bucket) - 1);

                *bucket = *backward_shift_bucket;
                bucket = backward_shift_bucket;
            }
        }

        index = (index + 1) & (hashset->capacity - 1);
        probe_length++;
    }
}

bool hashset_iterate(Hashset* hashset,
                     HashsetIterator* it,
                     void** key,
                     uint32_t* key_size)
{
    uint32_t i;

    for(i = *it; i < hashset->capacity; i++)
    {
        if(bucket_is_empty(&hashset->buckets[i]))
            continue;

        if(key != NULL)
            *key = bucket_get_key(&hashset->buckets[i]);

        if(key_size != NULL)
            *key_size = bucket_get_key_size(&hashset->buckets[i]);

        *it = i + 1;

        return true;
    }

    return false;
}

void hashset_free(Hashset* hashset)
{
    size_t i;

    ROMANO_ASSERT(hashset != NULL, "");

    if(hashset->buckets != NULL)
    {
        for(i = 0; i < hashset->capacity; i++)
            hashset_bucket_free(&hashset->buckets[i]);

        free(hashset->buckets);
    }

    free(hashset);
}
