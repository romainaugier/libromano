/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hashmap.h"
#include "libromano/math/common32.h"
#include "libromano/bit.h"
#include "libromano/random.h"
#include "libromano/logger.h"

/* HashMap */

#define INTERNED_SIZE ((sizeof(size_t) - 1) + 4)

typedef enum
{
    BucketFlag_KeyInterned = 0x1,
} BucketFlag;

ROMANO_PACKED_STRUCT(struct _Bucket 
{
    void* key;
    uint32_t key_size;
    void* value;
    uint32_t value_size;
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

void bucket_new(Bucket* bucket,
                const void* key, 
                const uint32_t key_size,
                void* value,
                const uint32_t value_size,
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
    bucket->hash = hash;
    bucket->probe_length = (uint16_t)probe_length;
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

ROMANO_FORCE_INLINE uint32_t bucket_get_value_size(const Bucket* bucket)
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
    {
        return (void*)bucket;
    }

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

void bucket_free(Bucket* bucket)
{
    ROMANO_ASSERT(bucket != NULL, "");

    if(bucket_is_empty(bucket))
    {
        return;
    }
    
    if(!bucket_has_flag(bucket, BucketFlag_KeyInterned))
    {
        free(bucket_get_key(bucket));
    }

    if(bucket_get_value_size(bucket) > 8)
    {
        free(bucket->value);
    }

    memset(bucket, 0, sizeof(Bucket));
}

#define HASHMAP_MAX_LOAD 0.9f
#define HASHMAP_INITIAL_CAPACITY 1024

struct _HashMap {
   Bucket* buckets;
   size_t size;
   size_t capacity;
   hashmap_hash_func hash_func;
   uint32_t hashkey;
   uint32_t max_probes;
};

ROMANO_FORCE_INLINE uint32_t hashmap_hash(const HashMap* hashmap, const void* key, const size_t key_size)
{
    return hashmap->hash_func(key, key_size, hashmap->hashkey);
}

ROMANO_FORCE_INLINE size_t hashmap_index(const HashMap* hashmap, const uint32_t hash) 
{
    return hash & (hashmap->capacity - 1); 
}

ROMANO_FORCE_INLINE size_t hashmap_get_new_capacity(HashMap* hashmap)
{
    return round_u64_to_next_pow2(hashmap->capacity + 1) + 1;
}

void hashmap_move_entry(HashMap* hashmap, Bucket* entry, const bool rehash);

void hashmap_grow(HashMap* hashmap,
                  const size_t capacity,
                  const bool rehash)
{
    Bucket* old_buckets;
    Bucket* bucket;

    size_t i;
    size_t index;
    size_t old_capacity;

    ROMANO_ASSERT(hashmap != NULL, "");

    old_buckets = hashmap->buckets;
    old_capacity = hashmap->capacity;
    
    hashmap->buckets = (Bucket*)calloc(capacity, sizeof(Bucket));
    hashmap->capacity = capacity;
    hashmap->size = 0;
    
    if(rehash)
    {
        hashmap->hashkey ^= random_next_uint32();
    }

    hashmap->max_probes = (uint32_t)mathf_log2((float)hashmap->capacity);

    if(old_buckets != NULL)
    {
        for(i = 0; i < old_capacity; i++)
        {
            bucket = &old_buckets[i];

            if(bucket_is_empty(bucket))
            {
                continue;
            }

            hashmap_move_entry(hashmap, bucket, rehash);
        }

        free(old_buckets);
    }
}

HashMap* hashmap_new(size_t initial_capacity)
{
    HashMap* hashmap = (HashMap*)malloc(sizeof(HashMap));
    hashmap->buckets = NULL;
    hashmap->hash_func = hash_murmur3;
    hashmap->size = 0;
    hashmap->capacity = 0;
    hashmap->hashkey ^= random_next_uint32();

    if(initial_capacity == 0)
    {
        initial_capacity = HASHMAP_INITIAL_CAPACITY;
    }
    else
    {
        initial_capacity = round_u64_to_next_pow2(initial_capacity + 1) + 1;
    }

    hashmap_grow(hashmap,
                 initial_capacity, 
                 false);

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

void hashmap_set_hash_func(HashMap* hashmap, hashmap_hash_func func)
{
    hashmap->hash_func = func;
}

void hashmap_move_entry(HashMap* hashmap, 
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
        hash = hashmap_hash(hashmap, bucket_get_key(&new_entry), bucket_get_key_size(&new_entry));
        bucket_set_hash(&new_entry, hash);
    }
    else
    {
        hash = bucket_get_hash(&new_entry);
    }

    index = hashmap_index(hashmap, hash);

    while(1)
    {
        bucket = &hashmap->buckets[index];

        if(!bucket_is_empty(bucket))
        {
            if(new_entry.probe_length > bucket_get_probe_length(bucket))
            {
                tmp = new_entry;
                new_entry = *bucket;
                *bucket = tmp;
            }

            index = (index + 1) & (hashmap->capacity - 1);
            new_entry.probe_length++;
        }
        else
        {
            *bucket = new_entry;

            hashmap->size++;

            return;
        }
    }
}

void hashmap_insert_bucket(HashMap* hashmap,
                           Bucket* entry)
{
    Bucket* bucket;
    Bucket tmp;

    size_t index;
    uint32_t hash;

    if((hashmap->size + 1) > hashmap->capacity * HASHMAP_MAX_LOAD)
    {
        hashmap_grow(hashmap, hashmap_get_new_capacity(hashmap), false);
    }

    hash = hashmap_hash(hashmap, bucket_get_key(entry), bucket_get_key_size(entry));
    bucket_set_hash(entry, hash);
    bucket_set_probe_length(entry, 0);

    index = hashmap_index(hashmap, hash);

    while(1)
    {
        bucket = &hashmap->buckets[index];

        if(!bucket_is_empty(bucket))
        {
            if(bucket_compare_key(bucket, bucket_get_key(entry), bucket_get_key_size(entry), hash))
            {
                return;
            }

            if(entry->probe_length > bucket->probe_length)
            {
                tmp = *entry;
                *entry = *bucket;
                *bucket = tmp;
            }

            index = (index + 1) & (hashmap->capacity - 1);
            entry->probe_length++;

            if(entry->probe_length >= hashmap->max_probes)
            {
                hashmap_grow(hashmap, hashmap_get_new_capacity(hashmap), false);

                hashmap_insert_bucket(hashmap, entry);

                return;
            }
        }
        else
        {
            *bucket = *entry;

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
    uint32_t hash;

    if((hashmap->size + 1) > hashmap->capacity * HASHMAP_MAX_LOAD)
    {
        hashmap_grow(hashmap, hashmap_get_new_capacity(hashmap), false);
    }

    hash = hashmap_hash(hashmap, key, key_size);
    bucket_new(&entry, key, key_size, value, value_size, hash, 0);

    index = hashmap_index(hashmap, hash);

    while(1)
    {
        bucket = &hashmap->buckets[index];

        if(!bucket_is_empty(bucket))
        {
            if(bucket_compare_key(bucket, key, key_size, hash))
            {
                return;
            }

            if(entry.probe_length > bucket->probe_length)
            {
                tmp = entry;
                entry = *bucket;
                *bucket = tmp;
            }

            index = (index + 1) & (hashmap->capacity - 1);
            entry.probe_length++;

            if(entry.probe_length >= hashmap->max_probes)
            {
                hashmap_grow(hashmap, hashmap_get_new_capacity(hashmap), false);
                hashmap_insert_bucket(hashmap, &entry);

                return;
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
    uint32_t hash;
    uint32_t probe_length;

    probe_length = 0;

    hash = hashmap_hash(hashmap, key, key_size);

    index = hashmap_index(hashmap, hash);

    while(1)
    {
        bucket = &hashmap->buckets[index];

        if(!bucket_is_empty(bucket))
        {
            if(bucket_compare_key(bucket, key, key_size, hash))
            {
                bucket_update_value(bucket, value, value_size);
                return;
            }

            index = (index + 1) & (hashmap->capacity - 1);
            probe_length++;
        }
        else
        {
            bucket_new(bucket, key, key_size, value, value_size, hash, probe_length);

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
    uint32_t hash;
    uint32_t probe_length;

    hash = hashmap_hash(hashmap, key, key_size);

    index = hashmap_index(hashmap, hash);

    probe_length = 0;

    while(1)
    {
        bucket = &hashmap->buckets[index];

        if(bucket_compare_key(bucket, key, key_size, hash))
        {
            if(value_size != NULL)
            {
                *value_size = bucket_get_value_size(bucket);
            }

            return bucket_get_value(bucket);
        }

        if(bucket_is_empty(bucket) || probe_length > bucket_get_probe_length(bucket))
        {
            return NULL;
        }

        index = (index + 1) & (hashmap->capacity - 1);
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
    uint32_t hash;
    uint32_t probe_length;

    hash = hashmap_hash(hashmap, key, key_size);

    index = hashmap_index(hashmap, hash);

    probe_length = 0;

    while(1)
    {
        bucket = &hashmap->buckets[index];

        if(bucket_is_empty(bucket) || probe_length > bucket_get_probe_length(bucket))
        {
            return;
        }

        if(bucket_compare_key(bucket, key, key_size, hash))
        {
            bucket_free(bucket);

            hashmap->size--;

            while(1)
            {
                bucket_set_empty(bucket);

                index = (index + 1) & (hashmap->capacity - 1);

                backward_shift_bucket = &hashmap->buckets[index];

                if(bucket_is_empty(backward_shift_bucket) || bucket_get_probe_length(backward_shift_bucket) == 0)
                {
                    return;
                }

                bucket_set_probe_length(backward_shift_bucket,
                                        bucket_get_probe_length(backward_shift_bucket) - 1);

                *bucket = *backward_shift_bucket;
                bucket = backward_shift_bucket;
            }
        }

        index = (index + 1) & (hashmap->capacity - 1);
        probe_length++;
    }
}

bool hashmap_iterate(HashMap* hashmap,
                     HashMapIterator* it,
                     void** key,
                     uint32_t* key_size,
                     void** value,
                     uint32_t* value_size)
{
    uint32_t i;

    for(i = *it; i < hashmap->capacity; i++)
    {
        if(bucket_is_empty(&hashmap->buckets[i]))
        {
            continue;
        }

        *key = bucket_get_key(&hashmap->buckets[i]);
        *key_size = bucket_get_key_size(&hashmap->buckets[i]);
        *value = bucket_get_value(&hashmap->buckets[i]);
        *value_size = bucket_get_value_size(&hashmap->buckets[i]);

        *it = i + 1;

        return true;
    }

    return false;
}

void hashmap_free(HashMap* hashmap)
{
    size_t i;

    ROMANO_ASSERT(hashmap != NULL, "");

    if(hashmap->buckets != NULL)
    {
        for(i = 0; i < hashmap->capacity; i++)
        {
            if(bucket_is_empty(&hashmap->buckets[i]))
            {
                continue;
            }
            
            bucket_free(&hashmap->buckets[i]);
        }

        free(hashmap->buckets);
    }

    free(hashmap);
}

/* SimdHashMap */

#define SIMD_INTERNED_SIZE ((sizeof(size_t) - 1) + ROMANO_SIZEOF_PTR)

#define BUCKET_KEY_CAN_BE_INTERNED(__key_size) (__key_size <= (SIMD_INTERNED_SIZE))
#define BUCKET_KEY_INTERNED_SIZE(entry) (((char*)entry)[SIMD_INTERNED_SIZE] & 0xFF)
#define BUCKET_KEY_INTERNED(entry) (BUCKET_KEY_INTERNED_SIZE(entry) <= (SIMD_INTERNED_SIZE))

#define BUCKET_VALUE_CAN_BE_INTERNED(__value_size) (__value_size <= SIMD_INTERNED_SIZE)
#define BUCKET_VALUE_INTERNED_SIZE(entry) (((char*)entry)[SIMD_INTERNED_SIZE * 2] & 0xFF)
#define BUCKET_VALUE_INTERNED(entry) (BUCKET_VALUE_INTERNED_SIZE(entry) <= SIMD_INTERNED_SIZE)

#define __ROMANO_HASHMAP_INTERN_SMALL_VALUES 1

ROMANO_PACKED_STRUCT(struct _SimdBucket 
{ 
    void* key;
    uint64_t key_size;
    void* value;
    uint64_t value_size;
});

typedef struct _SimdBucket SimdBucket;

void simdbucket_new(SimdBucket* simdbucket,
                    const void* key, 
                    const uint64_t key_size,
                    void* value,
                    const uint64_t value_size)
{
    char* simdbucket_cast;

    ROMANO_ASSERT(simdbucket != NULL, "");

#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES 
    if(BUCKET_KEY_CAN_BE_INTERNED(key_size))
    {
        simdbucket_cast = (char*)&(*simdbucket);
        memcpy(&simdbucket_cast[0], key, key_size * sizeof(char));
        simdbucket_cast[SIMD_INTERNED_SIZE] = (key_size & 0xFF);
    }
    else
    {
        simdbucket->key = malloc(key_size * sizeof(char));
        memcpy(simdbucket->key, key, key_size);

#if ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN
        simdbucket->key_size = mem_bswapu64(key_size);
#else
        simdbucket->key_size = key_size;
#endif /* ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN */
    }

    if(BUCKET_VALUE_CAN_BE_INTERNED(value_size))
    {
        simdbucket_cast = (char*)&(*simdbucket);
        memcpy(&simdbucket_cast[SIMD_INTERNED_SIZE + 1], value, value_size);
        simdbucket_cast[SIMD_INTERNED_SIZE * 2] = (value_size & 0xFF);
    }
    else
    {
        simdbucket->value = malloc(value_size);
        memcpy(simdbucket->value, value, value_size);

#if ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN
        simdbucket->value_size = mem_bswapu64(value_size);
#else
        simdbucket->value_size = value_size;
#endif /* ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN */
    }

#else
    simdbucket->key = malloc(key_size);
    memcpy(simdbucket->key, key, key_size);
    simdbucket->key_size = key_size;

    simdbucket->value = malloc(value_size);
    memcpy(simdbucket->value, value, value_size);
    simdbucket->value_size = value_size;
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
}

ROMANO_FORCE_INLINE size_t simdbucket_get_value_size(const SimdBucket* simdbucket)
{
#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    size_t value_size;

    value_size = (size_t)(((char*)simdbucket)[SIMD_INTERNED_SIZE * 2]);

    if(value_size <= SIMD_INTERNED_SIZE)
    {
        return value_size;
    }
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */

#if ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN
    return mem_bswapu64(simdbucket->value_size);
#else
    return simdbucket->value_size;
#endif /* ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN */
}

ROMANO_FORCE_INLINE void* simdbucket_get_value(SimdBucket* simdbucket)
{
#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    if(simdbucket_get_value_size(simdbucket) <= SIMD_INTERNED_SIZE)
    {
        return &(simdbucket->value);
    }
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
    return simdbucket->value;
}

ROMANO_FORCE_INLINE size_t simdbucket_get_key_size(const SimdBucket* simdbucket)
{
#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    size_t simdbucket_size;

    simdbucket_size = (size_t)(((char*)simdbucket)[SIMD_INTERNED_SIZE]);

    if(simdbucket_size <= SIMD_INTERNED_SIZE)
    {
        return simdbucket_size;
    }
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */

#if ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN
    return mem_bswapu64(simdbucket->key_size);
#else
    return simdbucket->key_size;
#endif /* ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN */
}

ROMANO_FORCE_INLINE const void* simdbucket_get_key(const SimdBucket* simdbucket)
{
#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    if(simdbucket_get_key_size(simdbucket) <= SIMD_INTERNED_SIZE)
    {
        return (const void*)simdbucket;
    }
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */
    return (const void*)simdbucket->key;
}

void simdbucket_free(SimdBucket* simdbucket)
{
    ROMANO_ASSERT(simdbucket != NULL, "");
    
#if __ROMANO_HASHMAP_INTERN_SMALL_VALUES
    if(!BUCKET_KEY_INTERNED(simdbucket))
    {
        free(simdbucket->key);
    }

    if(!BUCKET_VALUE_INTERNED(simdbucket))
    {
        free(simdbucket->value);
    }
#else
    free(simdbucket->key);
    free(simdbucket->value);
#endif /* __ROMANO_HASHMAP_INTERN_SMALL_VALUES */

    simdbucket->key = NULL;
    simdbucket->key_size = 0;
    simdbucket->value = NULL;
    simdbucket->value_size = 0;
}

#if defined(__AVX2__)
#pragma message("Compiling SimdHashMap with AVX2")
#include <immintrin.h>
#define SIMD_WIDTH 32
#define SIMD_TARGET_NAME "AVX2"
typedef __m256i SimdRegister;
#define SIMD_SET1_EPI8(x) _mm256_set1_epi8(x)
#define SIMD_LOADU(ptr) _mm256_loadu_si256((__m256i const*)(ptr))
#define SIMD_CMPEQ_EPI8(a, b) _mm256_cmpeq_epi8(a, b)
#define SIMD_MOVEMASK_EPI8(a) _mm256_movemask_epi8(a)
typedef uint32_t SimdMaskType;
#elif defined(__SSE2__)
#pragma message("Compiling SimdHashMap with SSE2")
#include <emmintrin.h>
#define SIMD_WIDTH 16
#define SIMD_TARGET_NAME "SSE2"
typedef __m128i SimdRegister;
#define SIMD_SET1_EPI8(x) _mm_set1_epi8(x)
#define SIMD_LOADU(ptr) _mm_loadu_si128((__m128i const*)(ptr))
#define SIMD_CMPEQ_EPI8(a, b) _mm_cmpeq_epi8(a, b)
#define SIMD_MOVEMASK_EPI8(a) _mm_movemask_epi8(a)
typedef uint16_t SimdMaskType;
#else
#pragma message("Compiling SimdHashMap with Scalar")
#define SIMD_WIDTH 8
#define SIMD_TARGET_NAME "Scalar"
typedef struct { int8_t bytes[SIMD_WIDTH]; } SimdRegister;

inline SimdRegister SIMD_SET1_EPI8(int8_t x) {
    SimdRegister reg;
    for(int i=0; i<SIMD_WIDTH; ++i) reg.bytes[i] = x;
    return reg;
}

inline SimdRegister SIMD_LOADU(const int8_t* ptr) {
    SimdRegister reg;
    memcpy(reg.bytes, ptr, SIMD_WIDTH);
    return reg;
}

inline SimdRegister SIMD_CMPEQ_EPI8(SimdRegister a, SimdRegister b) {
    SimdRegister res;
    for(int i=0; i<SIMD_WIDTH; ++i) res.bytes[i] = (a.bytes[i] == b.bytes[i]) ? (int8_t)0xFF : 0;
    return res;
}

typedef uint32_t SimdMaskType;
inline SimdMaskType SIMD_MOVEMASK_EPI8(SimdRegister a) {
    SimdMaskType mask = 0;
    for(int i=0; i<SIMD_WIDTH; ++i) {
        if (a.bytes[i] < 0) { 
                mask |= (1u << i);
        }
    }
    return mask;
}
#endif /* defined(__AVX2__) */

#if defined(ROMANO_MSVC)
#include <intrin.h>
ROMANO_FORCE_INLINE uint32_t count_trailing_zeros(SimdMaskType mask) 
{
    unsigned long index;
    if (mask == 0) return sizeof(SimdMaskType) * 8;
    #if defined(__AVX2__) || defined(__clang__) || defined(__GNUC__)
     _BitScanForward(&index, (uint32_t)mask);
    #else
     _BitScanForward(&index, (uint16_t)mask);
    #endif
    return (uint32_t)index;
}
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
ROMANO_FORCE_INLINE uint32_t count_trailing_zeros(SimdMaskType mask) 
{
    if (mask == 0) return sizeof(SimdMaskType) * 8;
#if defined(__AVX2__) || !defined(__SSE2__)
    return (uint32_t)__builtin_ctz((uint32_t)mask);
#else
    return (uint32_t)__builtin_ctz((uint16_t)mask);
#endif
}
#else
ROMANO_FORCE_INLINE uint32_t count_trailing_zeros(SimdMaskType mask) 
{
    if (mask == 0) return sizeof(SimdMaskType) * 8;
    uint32_t count = 0;
    while ((mask & 1) == 0) {
        mask >>= 1;
        count++;
    }
    return count;
}
#endif

typedef int8_t ctrl_t;
const ctrl_t CTRL_EMPTY = -128; /* 10000000 */
const ctrl_t CTRL_DELETED = -2; /* 11111110 */
/* Any value >= 0 represents a FULL bucket with H2 hash stored (0-127) */

ROMANO_FORCE_INLINE bool ctrl_is_full(ctrl_t c) { return c >= 0; }
ROMANO_FORCE_INLINE bool ctrl_is_empty(ctrl_t c) { return c == CTRL_EMPTY; }
ROMANO_FORCE_INLINE bool ctrl_is_deleted(ctrl_t c) { return c == CTRL_DELETED; }
ROMANO_FORCE_INLINE bool ctrl_is_empty_or_deleted(ctrl_t c) { return c < 0; }

/* Get H2 hash (top 7 bits) from full hash */
ROMANO_FORCE_INLINE ctrl_t hash_to_h2(uint32_t hash) 
{
    /* Shift right by (32 - 7) = 25 bits */
    return (ctrl_t)(hash >> 25);
}

#define SIMDHASHMAP_MAX_LOAD 0.6f
#define SIMDHASHMAP_INITIAL_CAPACITY 128

const size_t GROUP_WIDTH = SIMD_WIDTH;

struct _SimdHashMap 
{
   ctrl_t* ctrl;
   SimdBucket* buckets;
   size_t size;
   size_t capacity;
   size_t growth_left;
   hashmap_hash_func hash_func;
   uint32_t hash_seed;
};

ROMANO_FORCE_INLINE size_t simdhashmap_capacity_with_padding(size_t cap) 
{
    return cap > 0 ? cap + GROUP_WIDTH - 1 : 0;
}

ROMANO_FORCE_INLINE size_t simdhashmap_index_from_hash(size_t capacity, uint32_t hash) 
{
    return (size_t)hash & (capacity - 1);
}

ROMANO_FORCE_INLINE uint32_t simdhashmap_hash(const SimdHashMap* hashmap,
                                              const void* key,
                                              const size_t key_size)
{
    uint32_t hash = hashmap->hash_func(key, key_size, hashmap->hash_seed);
    // hash ^= hash >> 16;
    // hash *= 0X85EBCA6B;
    // hash ^= hash >> 13;
    return hash;
}

void simdhashmap_clear_internal(SimdHashMap* hashmap) 
{
    if(!hashmap || hashmap->capacity == 0) 
    {
        return;
    }

    for(size_t i = 0; i < hashmap->capacity; ++i) 
    {
        if (ctrl_is_full(hashmap->ctrl[i])) 
        {
            simdbucket_free(&hashmap->buckets[i]);
        }
    }

    memset(hashmap->ctrl, CTRL_EMPTY, hashmap->capacity);
    memcpy(hashmap->ctrl + hashmap->capacity, hashmap->ctrl, GROUP_WIDTH - 1);

    hashmap->size = 0;
    size_t threshold = (size_t)(hashmap->capacity * SIMDHASHMAP_MAX_LOAD);
    hashmap->growth_left = (threshold > hashmap->size) ? (threshold - hashmap->size) : 0;
}

size_t simdhashmap_find_bucket(const SimdHashMap* hashmap,
                               const void* key,
                               const uint64_t key_size,
                               const uint32_t hash)
{
    if(hashmap->capacity == 0) 
    {
        return 0;
    }

    size_t start_index = simdhashmap_index_from_hash(hashmap->capacity, hash);
    ctrl_t target_h2 = hash_to_h2(hash);
    size_t current_offset = 0;

    while(current_offset < hashmap->capacity)
    {
        size_t group_start_index = (start_index + current_offset) & (hashmap->capacity - 1);
        const ctrl_t* group_ptr = hashmap->ctrl + group_start_index;

        SimdRegister group_ctrl = SIMD_LOADU(group_ptr);

        SimdRegister match_h2_vec = SIMD_CMPEQ_EPI8(group_ctrl, SIMD_SET1_EPI8(target_h2));
        SimdMaskType h2_mask = SIMD_MOVEMASK_EPI8(match_h2_vec);

        SimdRegister match_empty_vec = SIMD_CMPEQ_EPI8(group_ctrl, SIMD_SET1_EPI8(CTRL_EMPTY));
        SimdMaskType empty_mask = SIMD_MOVEMASK_EPI8(match_empty_vec);

        SimdMaskType h2_probe_mask = h2_mask;

        while(h2_probe_mask != 0)
        {
            uint32_t bit_pos = count_trailing_zeros(h2_probe_mask);
            size_t probe_index = (group_start_index + bit_pos) & (hashmap->capacity - 1);

            const SimdBucket* bucket = &hashmap->buckets[probe_index];

            if(simdbucket_get_key_size(bucket) == key_size &&
               memcmp(simdbucket_get_key(bucket), key, key_size) == 0)
            {
                return probe_index;
            }

            h2_probe_mask &= (h2_probe_mask - 1);
        }

        if(empty_mask != 0)
        {
            return hashmap->capacity;
        }

        current_offset += GROUP_WIDTH;
    }

    return hashmap->capacity;
}

typedef struct {
    size_t index;
    bool found;
    bool found_insert_slot;
} FindResult;

FindResult simdhashmap_find_internal(const SimdHashMap* hashmap,
                                     const void* key,
                                     const uint64_t key_size,
                                     const uint32_t hash)
{
    size_t start_index = simdhashmap_index_from_hash(hashmap->capacity, hash);
    ctrl_t target_h2 = hash_to_h2(hash);
    size_t current_offset = 0;
    FindResult result = { hashmap->capacity, false, false };

    size_t first_deleted_slot = hashmap->capacity;

    while(current_offset < hashmap->capacity) 
    {
        size_t group_start_index = (start_index + current_offset) & (hashmap->capacity - 1);
        const ctrl_t* group_ptr = hashmap->ctrl + group_start_index;

        SimdRegister group_ctrl = SIMD_LOADU(group_ptr);

        SimdRegister match_h2_vec = SIMD_CMPEQ_EPI8(group_ctrl, SIMD_SET1_EPI8(target_h2));
        SimdMaskType h2_mask = SIMD_MOVEMASK_EPI8(match_h2_vec);

        SimdRegister match_empty_vec = SIMD_CMPEQ_EPI8(group_ctrl, SIMD_SET1_EPI8(CTRL_EMPTY));
        SimdMaskType empty_mask = SIMD_MOVEMASK_EPI8(match_empty_vec);

        SimdRegister match_deleted_vec = SIMD_CMPEQ_EPI8(group_ctrl, SIMD_SET1_EPI8(CTRL_DELETED));
        SimdMaskType deleted_mask = SIMD_MOVEMASK_EPI8(match_deleted_vec);

        SimdMaskType h2_probe_mask = h2_mask;

        while(h2_probe_mask != 0) 
        {
            uint32_t bit_pos = count_trailing_zeros(h2_probe_mask);
            size_t probe_index = (group_start_index + bit_pos) & (hashmap->capacity - 1);

            const SimdBucket* bucket = &hashmap->buckets[probe_index];

            if(simdbucket_get_key_size(bucket) == key_size && memcmp(simdbucket_get_key(bucket), key, key_size) == 0) 
            {
                result.index = probe_index;
                result.found = true;
                result.found_insert_slot = true;
                return result;
            }

            h2_probe_mask &= (h2_probe_mask - 1);
        }

        if(empty_mask != 0) 
        {
            result.found = false;
            result.found_insert_slot = true;

            if(first_deleted_slot < hashmap->capacity) 
            {
                result.index = first_deleted_slot;
            } 
            else 
            {
                const uint32_t empty_bit_pos = count_trailing_zeros(empty_mask);
                result.index = (group_start_index + empty_bit_pos) & (hashmap->capacity - 1);
            }

            return result;
        }

        if(deleted_mask != 0 && first_deleted_slot == hashmap->capacity) 
        {
            const uint32_t deleted_bit_pos = count_trailing_zeros(deleted_mask);
            first_deleted_slot = (group_start_index + deleted_bit_pos) & (hashmap->capacity - 1);
        }

        current_offset += GROUP_WIDTH;
    }

    result.found = false;

    if(first_deleted_slot < hashmap->capacity) 
    {
        result.index = first_deleted_slot;
        result.found_insert_slot = true;
    } 
    else
    {
        result.index = hashmap->capacity;
        result.found_insert_slot = false;
    }

    return result;
}

size_t simdhashmap_find_first_non_full(const SimdHashMap* hashmap, uint32_t hash) 
{
    const size_t start_index = simdhashmap_index_from_hash(hashmap->capacity, hash);

    for(size_t i = 0; i < hashmap->capacity; ++i) 
    {
        const size_t probe_index = (start_index + i) & (hashmap->capacity - 1);

        if(!ctrl_is_full(hashmap->ctrl[probe_index])) 
        {
            return probe_index;
        }
    }

    /* Should never happen */
    ROMANO_ASSERT(false, "No non-full bucket found during rehash");

    return hashmap->capacity;
}

bool simdhashmap_rehash_and_grow(SimdHashMap* hashmap, const size_t new_min_capacity) 
{
    size_t old_capacity = hashmap->capacity;
    ctrl_t* old_ctrl = hashmap->ctrl;
    SimdBucket* old_buckets = hashmap->buckets;

    size_t new_capacity = (old_capacity == 0) ? GROUP_WIDTH : old_capacity;

    if(new_capacity < GROUP_WIDTH) 
    {
        new_capacity = GROUP_WIDTH;
    }

    /* Power of two */
    while(new_capacity < new_min_capacity) 
    {
        new_capacity *= 2;

        /* Happens if the capacity overflows */
        if(new_capacity == 0)
        {
            ROMANO_ASSERT(false, "capacity overflow");
            hashmap->ctrl = old_ctrl;
            hashmap->buckets = old_buckets;
            hashmap->capacity = old_capacity;
            return false;
        }
    }

    size_t ctrl_alloc_size = simdhashmap_capacity_with_padding(new_capacity);
    ctrl_t* new_ctrl = (ctrl_t*)malloc(ctrl_alloc_size);

    if(!new_ctrl) 
    {
        return false;
    }

    SimdBucket* new_buckets = (SimdBucket*)calloc(new_capacity, sizeof(SimdBucket));

    if(!new_buckets) 
    {
        free(new_ctrl);
        return false;
    }

    memset(new_ctrl, CTRL_EMPTY, new_capacity);
    memcpy(new_ctrl + new_capacity, new_ctrl, GROUP_WIDTH - 1);

    hashmap->ctrl = new_ctrl;
    hashmap->buckets = new_buckets;
    hashmap->capacity = new_capacity;
    hashmap->size = 0;

    if(old_buckets != NULL) 
    {
        for(size_t i = 0; i < old_capacity; ++i) 
        {
            if(ctrl_is_full(old_ctrl[i])) 
            {
                SimdBucket* old_bucket = &old_buckets[i];

                const uint32_t hash = simdhashmap_hash(hashmap, 
                                                       simdbucket_get_key(old_bucket), 
                                                       simdbucket_get_key_size(old_bucket));
                const ctrl_t h2 = hash_to_h2(hash);

                const size_t insert_idx = simdhashmap_find_first_non_full(hashmap, hash);

                ROMANO_ASSERT(insert_idx < new_capacity, "Failed to find insertion bucket during rehash");

                hashmap->buckets[insert_idx] = *old_bucket;

                hashmap->ctrl[insert_idx] = h2;

                if(insert_idx < GROUP_WIDTH - 1) 
                {
                    hashmap->ctrl[hashmap->capacity + insert_idx] = h2;
                }

                hashmap->size++;
            }
        }
    }

    const size_t threshold = (size_t)(hashmap->capacity * SIMDHASHMAP_MAX_LOAD);
    hashmap->growth_left = (threshold > hashmap->size) ? (threshold - hashmap->size) : 0;

    if(old_ctrl != NULL) 
    {
        free(old_ctrl);
    }

    if(old_buckets != NULL) 
    {
        free(old_buckets);
    }

    return true;
}


bool simdhashmap_reserve_for_insert(SimdHashMap* hashmap) 
{
    if(hashmap->growth_left == 0) 
    {
        return simdhashmap_rehash_and_grow(hashmap, hashmap->capacity + 1);
    }

    return true;
}

SimdHashMap* simdhashmap_new(size_t initial_capacity) 
{
    SimdHashMap* hashmap = (SimdHashMap*)calloc(1, sizeof(SimdHashMap));

    if(hashmap == NULL) 
    {
        return NULL;
    }

    hashmap->size = 0;
    hashmap->capacity = 0;
    hashmap->ctrl = NULL;
    hashmap->buckets = NULL;
    hashmap->hash_func = hash_murmur3;
    hashmap->hash_seed = random_next_uint32();

    if(initial_capacity > 0 && initial_capacity < GROUP_WIDTH) 
    {
        initial_capacity = GROUP_WIDTH;
    }

    size_t required_capacity = 0;

    if(initial_capacity > 0) 
    {
        required_capacity = (size_t)((float)initial_capacity / SIMDHASHMAP_MAX_LOAD);

        if((float)initial_capacity / SIMDHASHMAP_MAX_LOAD > (float)required_capacity) 
        {
           required_capacity++;
        }
    }

    if(required_capacity > 0) 
    {
        if(!simdhashmap_rehash_and_grow(hashmap, required_capacity)) 
        {
            free(hashmap);
            return NULL;
        }
    } 
    else 
    {
        hashmap->growth_left = 0;
    }

    return hashmap;
}

size_t simdhashmap_size(const SimdHashMap* hashmap) 
{
    return hashmap ? hashmap->size : 0;
}

size_t simdhashmap_capacity(const SimdHashMap* hashmap) 
{
    return hashmap ? hashmap->capacity : 0;
}

float simdhashmap_load_factor(const SimdHashMap* hashmap) 
{
    ROMANO_ASSERT(hashmap != NULL, "Hashmap is null");

    if(hashmap->capacity == 0) 
    {
        return 0.0f;
    }

    return (float)hashmap->size / (float)hashmap->capacity;
}

void simdhashmap_set_hash_func(SimdHashMap* hashmap, hashmap_hash_func func) 
{
    ROMANO_ASSERT(hashmap != NULL, "Hashmap is null");
    ROMANO_ASSERT(func != NULL, "func is null");
    ROMANO_ASSERT(hashmap->size == 0, "Cannot set hash function if hashmap is not empty");
    
    hashmap->hash_func = func;
}

void simdhashmap_clear(SimdHashMap* hashmap) 
{
    simdhashmap_clear_internal(hashmap);
}

bool simdhashmap_reserve(SimdHashMap* hashmap, const size_t count) 
{
    ROMANO_ASSERT(hashmap != NULL, "Hashmap is null");

    if(count == 0)
    {
        return true;
    }

    size_t required_capacity = 0;

    required_capacity = (size_t)((float)count / SIMDHASHMAP_MAX_LOAD);

    if((float)count / SIMDHASHMAP_MAX_LOAD > (float)required_capacity) 
    {
        required_capacity++;
    }

    if(required_capacity > hashmap->capacity) 
    {
        return simdhashmap_rehash_and_grow(hashmap, required_capacity);
    }

    return true;
}

bool simdhashmap_set(SimdHashMap* hashmap,
                     const void* key,
                     const uint64_t key_size,
                     void* value,
                     const uint64_t value_size)
{
    ROMANO_ASSERT(hashmap != NULL, "Hashmap is null");
    ROMANO_ASSERT(key != NULL, "key is null");
    ROMANO_ASSERT(key_size > 0, "key_size == 0");

    if(!simdhashmap_reserve_for_insert(hashmap)) 
    {
        return false;
    }

    uint32_t hash = simdhashmap_hash(hashmap, key, key_size);
    FindResult result = simdhashmap_find_internal(hashmap, key, key_size, hash);

    if(result.found) 
    {
        SimdBucket* bucket = &hashmap->buckets[result.index];

        simdbucket_free(bucket);
        simdbucket_new(bucket, key, key_size, value, value_size);

        return true;

    } 
    else if (result.found_insert_slot) 
    {
        size_t insert_idx = result.index;
        SimdBucket* bucket = &hashmap->buckets[insert_idx];

        const bool was_deleted = ctrl_is_deleted(hashmap->ctrl[insert_idx]);

        simdbucket_new(bucket, key, key_size, value, value_size);

        const ctrl_t h2 = hash_to_h2(hash);
        hashmap->ctrl[insert_idx] = h2;

        if(insert_idx < GROUP_WIDTH - 1) 
        {
            hashmap->ctrl[hashmap->capacity + insert_idx] = h2;
        }

        hashmap->size++;

        if(!was_deleted) 
        {
            hashmap->growth_left--;
        }

        return true;

    } 
    else 
    {
        return false;
    }
}

void* simdhashmap_get(const SimdHashMap* hashmap,
                      const void* key,
                      const uint64_t key_size,
                      uint64_t* out_value_size)
{
    ROMANO_ASSERT(hashmap != NULL, "Hashmap is null");
    ROMANO_ASSERT(key != NULL, "key is null");
    ROMANO_ASSERT(key_size > 0, "key_size == 0");
    ROMANO_ASSERT(hashmap->capacity > 0, "Hashmap is empty");

    const uint32_t hash = simdhashmap_hash(hashmap, key, key_size);

    const size_t index = simdhashmap_find_bucket(hashmap, key, key_size, hash);

    if(index != hashmap->capacity) 
    {
        if(out_value_size != NULL) 
        {
            *out_value_size = simdbucket_get_value_size(&hashmap->buckets[index]);
        }

        return simdbucket_get_value(&hashmap->buckets[index]);
    } 
    else 
    {
        if(out_value_size != NULL)
        {
            *out_value_size = 0;
        }

        return NULL;
    }
}

bool simdhashmap_contains(const SimdHashMap* hashmap,
                          const void* key,
                          const uint64_t key_size)
{
    ROMANO_ASSERT(hashmap != NULL, "Hashmap is null");
    ROMANO_ASSERT(key != NULL, "key is null");
    ROMANO_ASSERT(key_size > 0, "key_size == 0");
    ROMANO_ASSERT(hashmap->capacity > 0, "Hashmap is empty");

    const uint32_t hash = simdhashmap_hash(hashmap, key, key_size);

    const size_t result = simdhashmap_find_bucket(hashmap, key, key_size, hash);

    return result != hashmap->capacity;
}

bool simdhashmap_remove(SimdHashMap* hashmap,
                        const void* key,
                        const uint64_t key_size)
{
    ROMANO_ASSERT(hashmap != NULL, "Hashmap is null");
    ROMANO_ASSERT(key != NULL, "key is null");
    ROMANO_ASSERT(key_size > 0, "key_size == 0");
    ROMANO_ASSERT(hashmap->capacity > 0, "Hashmap is empty");

    const uint32_t hash = simdhashmap_hash(hashmap, key, key_size);
    const size_t index = simdhashmap_find_bucket(hashmap, key, key_size, hash);

    if(index != hashmap->capacity) 
    {
        simdbucket_free(&hashmap->buckets[index]);

        hashmap->ctrl[index] = CTRL_DELETED;

        if(index < GROUP_WIDTH - 1)
        {
            hashmap->ctrl[hashmap->capacity + index] = CTRL_DELETED;
        }

        hashmap->size--;

        return true;
    }
    else 
    {
        return false;
    }
}

bool simdhashmap_iterate(const SimdHashMap* hashmap,
                         SimdHashMapIterator* it,
                         void** out_key,
                         uint64_t* out_key_size,
                         void** out_value,
                         uint64_t* out_value_size)
{
    ROMANO_ASSERT(hashmap != NULL, "Hashmap is null");
    ROMANO_ASSERT(hashmap->capacity > 0, "Hashmap is empty");
    ROMANO_ASSERT(it != NULL, "it is null");

    if(!out_key || !out_key_size || !out_value || !out_value_size) 
    {
        return false;
    }

    for(size_t i = *it; i < hashmap->capacity; ++i) 
    {
        if(ctrl_is_full(hashmap->ctrl[i])) 
        {
            const SimdBucket* bucket = &hashmap->buckets[i];
            *out_key = (void*)simdbucket_get_key(bucket);
            *out_key_size = simdbucket_get_key_size(bucket);
            *out_value = simdbucket_get_value((SimdBucket*)bucket);
            *out_value_size = simdbucket_get_value_size(bucket);
            *it = i + 1;
            return true;
        }
    }

    *it = hashmap->capacity;

    return false;
}

void simdhashmap_free(SimdHashMap* hashmap) 
{
    ROMANO_ASSERT(hashmap != NULL, "Hashmap is null");

    simdhashmap_clear_internal(hashmap);

    free(hashmap->ctrl);
    free(hashmap->buckets);
    free(hashmap);
}