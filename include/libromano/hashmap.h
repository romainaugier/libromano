/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_HASHMAP)
#define __LIBROMANO_HASHMAP

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

struct _hashmap;
typedef struct _hashmap hashmap;

ROMANO_API hashmap* hashmap_new(void);

ROMANO_API void hashmap_insert(hashmap* hashmap, 
                               const char* key, 
                               void* value, 
                               size_t value_size);

ROMANO_API void hashmap_update(hashmap* hashmap,
                               const char* key,
                               void* value,
                               size_t value_size);

ROMANO_API void* hashmap_get(hashmap* hashmap, 
                             const char* key);

ROMANO_API void hashmap_free(hashmap* hashmap);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_HASHMAP) */

