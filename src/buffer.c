/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/buffer.h"
#include "libromano/error.h"

#include <string.h>

extern ErrorCode g_current_error;

bool buffer_init(Buffer* buffer, size_t initial_capacity)
{
    ROMANO_ASSERT(buffer != NULL, "buffer is NULL");

    buffer->data = calloc(initial_capacity, sizeof(char));

    if(buffer->data == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return false;
    }

    buffer->sz = 0;
    buffer->capacity = initial_capacity;

    return true;
}

size_t buffer_size(Buffer* buffer)
{
    return buffer->sz;
}

bool buffer_resize(Buffer* buffer, size_t to_add_sz)
{
    void* new_data;
    size_t new_capacity;

    new_capacity = buffer->capacity;

    while(new_capacity < (buffer->sz + to_add_sz) &&
          new_capacity < INT64_MAX)
        new_capacity <<= 1;

    new_data = realloc(buffer->data, new_capacity);

    if(new_data == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return false;
    }

    buffer->data = new_data;
    buffer->capacity = new_capacity;

    return true;
}

ROMANO_FORCE_INLINE bool buffer_check_resize(Buffer* buffer, size_t to_add_sz)
{
    if(buffer->capacity <= (buffer->sz + to_add_sz))
    {
        return buffer_resize(buffer, to_add_sz);
    }

    return true;
}

bool buffer_prepare_emplace(Buffer* buffer, size_t data_sz)
{
    return buffer_check_resize(buffer, data_sz);
}

void buffer_emplace_size(Buffer* buffer, size_t data_sz)
{
    buffer->sz += data_sz;
}

bool buffer_append(Buffer* buffer,
                   const void* ROMANO_RESTRICT data,
                   size_t data_sz)
{
    ROMANO_ASSERT(buffer != NULL, "buffer is NULL");

    if(!buffer_check_resize(buffer, data_sz))
        return false;

    memcpy((char*)buffer->data + buffer->sz, data, data_sz);

    buffer->sz += data_sz;

    return true;
}

bool buffer_is_empty(Buffer* buffer)
{
    ROMANO_ASSERT(buffer != NULL, "buffer is NULL");

    return buffer->sz == 0;
}

void buffer_reset(Buffer* buffer)
{
    ROMANO_ASSERT(buffer != NULL, "buffer is NULL");

    buffer->sz = 0;
}

void* buffer_front(Buffer* buffer)
{
    ROMANO_ASSERT(buffer != NULL, "buffer is NULL");

    return buffer->data;
}

void* buffer_back(Buffer* buffer)
{
    ROMANO_ASSERT(buffer != NULL, "buffer is NULL");

    return (void*)((char*)buffer->data + buffer->sz);
}

void buffer_release(Buffer* buffer)
{
    ROMANO_ASSERT(buffer != NULL, "buffer is NULL");

    if(buffer->data != NULL)
        free(buffer->data);

    buffer->data = NULL;
    buffer->capacity = 0;
    buffer->sz = 0;
}
