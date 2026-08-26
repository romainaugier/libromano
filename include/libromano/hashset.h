/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_HASHSET)
#define __LIBROMANO_HASHSET

#include "libromano/common.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

ROMANO_CPP_ENTER

/*
 * A Hashset is a set of keys using open addressing with robin hood probing
 */
struct _Hashset;

typedef struct _Hashset Hashset;

typedef uint32_t HashsetIterator;

typedef uint32_t (*hashset_hash_func)(const void*, const size_t, const uint32_t);

/*
 * Returns NULL on failure (i.e memory allocation error)
 */
ROMANO_API Hashset* hashset_new(size_t initial_capacity);

ROMANO_API size_t hashset_size(Hashset* hashset);

ROMANO_API size_t hashset_capacity(Hashset* hashset);

ROMANO_API void hashset_set_hash_func(Hashset* hashset,
                                      hashset_hash_func func);

ROMANO_API bool hashset_add(Hashset* hashset,
                            const void* key,
                            const uint32_t key_size);

ROMANO_API bool hashset_contains(Hashset* hashset,
                                 const void* key,
                                 const uint32_t key_size);

ROMANO_API void hashset_remove(Hashset* hashset,
                               const void* key,
                               const uint32_t key_size);

ROMANO_API bool hashset_iterate(Hashset* hashset,
                                HashsetIterator* it,
                                void** key,
                                uint32_t* key_size);

ROMANO_API void hashset_free(Hashset* hashset);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_HASHSET) */
