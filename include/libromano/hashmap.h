/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_HASHMAP)
#define __LIBROMANO_HASHMAP

#include "libromano/libromano.h"
#include "libromano/hash.h"
#include "libromano/memory.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

ROMANO_CPP_ENTER

typedef enum
{
    HashMapFlags_NoValueCopy = 0x1,
} HashMapFlags;

struct _HashMap;

typedef struct _HashMap HashMap;

typedef uint32_t (*hashmap_hash_func)(const void*, const size_t, const uint32_t);

ROMANO_API HashMap* hashmap_new(size_t initial_capacity);

ROMANO_API size_t hashmap_size(HashMap* hashmap);

ROMANO_API size_t hashmap_capacity(HashMap* hashmap);

ROMANO_API void hashmap_set_hash_func(HashMap* hashmap, 
                                      hashmap_hash_func func);

ROMANO_API void hashmap_insert(HashMap* hashmap,
                               const void* key,
                               const uint32_t key_size,
                               void* value,
                               const uint32_t value_size);

ROMANO_API void hashmap_update(HashMap* hashmap,
                               const void* key,
                               const uint32_t key_size,
                               void* value,
                               const uint32_t value_size);

ROMANO_API void* hashmap_get(HashMap* hashmap,
                             const void* key,
                             const uint32_t key_size,
                             uint32_t* value_size);

ROMANO_API void hashmap_remove(HashMap* hashmap,
                               const void* key,
                               const uint32_t key_size);

ROMANO_API void hashmap_free(HashMap* hashmap);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_HASHMAP) */
