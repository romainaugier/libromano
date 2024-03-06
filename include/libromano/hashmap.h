/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_HASHMAP)
#define __LIBROMANO_HASHMAP

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

struct _hashmap;
typedef struct _hashmap hashmap_t;

ROMANO_API hashmap_t* hashmap_new(void);

ROMANO_API void hashmap_insert(hashmap_t* hashmap, 
                               const char* key, 
                               void* value, 
                               size_t value_size);

ROMANO_API void hashmap_update(hashmap_t* hashmap,
                               const char* key,
                               void* value,
                               size_t value_size);

ROMANO_API void* hashmap_get(hashmap_t* hashmap, 
                             const char* key,
                             size_t* value_size);

ROMANO_API void hashmap_remove(hashmap_t* hashmap,
                               const char* key);

ROMANO_API void hashmap_free(hashmap_t* hashmap);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_HASHMAP) */

