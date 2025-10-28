/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_ARENA)
#define __LIBROMANO_ARENA

#include "libromano/common.h"

/*
 * Basic memory arena structure, be careful if you want to push objects that have a size larger
 * than the block size you set
 */

ROMANO_CPP_ENTER

#define ARENA_GROWTH_RATE 1.6180339887f
#define ARENA_BLOCK_SIZE 16384

/*
 * Memory block used internally by the arena
 */
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

/*
 * Initializes a new arena
 */
ROMANO_API bool arena_init(Arena* arena, const size_t block_size);

/*
 * Creates a new heap-allocated Arena and initializes it. Returns NULL on failure
 */
ROMANO_API Arena* arena_new(const size_t block_size);

/*
 * Pushes a new element to the Arena, returns the adress for that element.
 * Returns NULL on failure (memory allocation failure)
 */
ROMANO_API void* arena_push(Arena* arena, void* data, const size_t data_size);

/*
 * Emplaces a new element to the Arena, returns the adress for that element
 */
#define arena_emplace(arena, data_size, data_type) *(data_type*)arena_push(arena, NULL, data_size)

/*
 * Clears the arena and reset everything inside
 */
ROMANO_API void arena_clear(Arena* arena);

/*
 * Clears and releases the Arena
 */
ROMANO_API void arena_release(Arena* arena);

/*
 * Releases and frees a heap-allocated arena
 */
ROMANO_API void arena_free(Arena* arena);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_ARENA) */
