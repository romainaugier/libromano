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

/* 
    Flag used to turn on an optimization similar to sso for keys and values of size less than 
    sizeof(size_t) - 1 + sizeof(void*)
    On little endian platforms we need to swap the size when it is not interned as we check the last
    byte of the size when looking for interning size
*/

#if defined(ROMANO_HASHMAP_INTERN_SMALL_VALUES)
#define __ROMANO_HASHMAP_INTERN_SMALL_VALUES 1
#else
#define __ROMANO_HASHMAP_INTERN_SMALL_VALUES 0
#endif /* defined(ROMANO_HASHMAP_INTERN_SMALL_VALUES) */

ROMANO_CPP_ENTER

struct _StringHashMap;

typedef struct _StringHashMap StringHashMap;

ROMANO_API StringHashMap* string_hashmap_new(void);

ROMANO_API size_t string_hashmap_size(StringHashMap* hashmap);

ROMANO_API size_t string_hashmap_capacity(StringHashMap* hashmap);

ROMANO_API void string_hashmap_insert(StringHashMap* hashmap,
                                      const char* key,
                                      const size_t key_len,
                                      void* value,
                                      const size_t value_size);

ROMANO_API void string_hashmap_update(StringHashMap* hashmap,
                                      const char* key,
                                      const size_t key_len,
                                      void* value,
                                      const size_t value_size);

ROMANO_API void* string_hashmap_get(StringHashMap* hashmap,
                                    const char* key,
                                    const size_t key_len,
                                    size_t* value_size);

ROMANO_API void string_hashmap_remove(StringHashMap* hashmap,
                                      const char* key,
                                      size_t key_len);

ROMANO_API void string_hashmap_free(StringHashMap* hashmap);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_HASHMAP) */
