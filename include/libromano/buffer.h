/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_BUFFER)
#define __LIBROMANO_BUFFER

#include "libromano/common.h"

ROMANO_CPP_ENTER

typedef struct Buffer {
    void* data;
    size_t capacity;
    size_t sz;
} Buffer;

ROMANO_API bool buffer_init(Buffer* buffer, size_t initial_capacity);

ROMANO_API size_t buffer_size(Buffer* buffer);

/*
 *
 */
ROMANO_API bool buffer_prepare_emplace(Buffer* buffer, size_t data_sz);

ROMANO_API void buffer_emplace_size(Buffer* buffer, size_t data_sz);

ROMANO_API bool buffer_append(Buffer* buffer,
                              const void* ROMANO_RESTRICT data,
                              size_t data_sz);

ROMANO_API bool buffer_is_empty(Buffer* buffer);

ROMANO_API void buffer_reset(Buffer* buffer);

ROMANO_API void* buffer_front(Buffer* buffer);

ROMANO_API void* buffer_back(Buffer* buffer);

ROMANO_API void buffer_release(Buffer* buffer);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_BUFFER) */
