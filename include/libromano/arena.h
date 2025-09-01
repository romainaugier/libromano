/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_ARENA)
#define __LIBROMANO_ARENA

#include "libromano/common.h"

ROMANO_CPP_ENTER

#define ARENA_GROWTH_RATE 1.6180339887f
#define ARENA_BLOCK_SIZE 16384

typedef struct ArenaBlock
{
    void* address;
    size_t capacity;
    size_t offset;
    struct ArenaBlock* previous;
    struct ArenaBlock* next;
} ArenaBlock;

typedef struct
{
    ArenaBlock* current_block;
    size_t capacity;
    size_t block_size;
} Arena;

ROMANO_API void arena_init(Arena* arena, const size_t block_size);

ROMANO_API void arena_resize(Arena* arena);

ROMANO_API void* arena_push(Arena* arena, void* data, const size_t data_size);

#define arena_emplace(arena, data_size, data_type) *(data_type*)arena_push(arena, NULL, data_size)

ROMANO_API void arena_clear(Arena* arena);

ROMANO_API void arena_destroy(Arena* arena);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_ARENA) */