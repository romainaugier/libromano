/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/arena.h"

#include <stdlib.h>
#include <string.h>

ArenaBlock* arena_block_init(const size_t block_size)
{
    const size_t total_size = block_size + sizeof(ArenaBlock);

    void* addr = malloc(total_size);

    ROMANO_ASSERT(addr != NULL, "Error during arena reallocation");

    if(addr == NULL)
    {
        return NULL;
    }

    void* block_addr = (char*)addr + sizeof(ArenaBlock);

    ArenaBlock* block = (ArenaBlock*)addr;
    block->address = block_addr;
    block->capacity = block_size;
    block->offset = 0;
    block->previous = NULL;
    block->next = NULL;

    return block;
}

void arena_init(Arena* arena, const size_t block_size)
{
    arena->current_block = arena_block_init(block_size);
    arena->block_size = block_size;
    arena->capacity = block_size;
}

ROMANO_FORCE_INLINE bool arena_check_resize(Arena* arena, 
                                            const size_t new_size)
{
    return (arena->current_block->offset + new_size) >= arena->current_block->capacity;
}

void arena_resize(Arena* arena)
{
    ArenaBlock* new_block = arena_block_init(arena->block_size);

    new_block->previous = arena->current_block;
    arena->current_block = new_block;

    arena->capacity += arena->block_size;
}

void* arena_push(Arena* arena, void* data, const size_t data_size)
{
    if(arena_check_resize(arena, data_size))
    {
        arena_resize(arena);
    }

    void* data_address = (void*)((char*)arena->current_block->address + arena->current_block->offset);
    
    if(data != NULL)
    {
        memcpy(data_address, data, data_size);
    }

    arena->current_block->offset += data_size;

    return data_address;
}

void arena_clear(Arena* arena)
{
    ArenaBlock* current = arena->current_block;
    current->offset = 0;

    while(current->previous != NULL)
    {
        current = current->previous;
        current->offset = 0;
    }

    arena->current_block = current;
}

void arena_destroy(Arena* arena)
{
    if(arena->current_block != NULL)
    {
        ArenaBlock* prev_block = arena->current_block->previous;

        while(prev_block != NULL)
        {
            ArenaBlock* prev_prev_block = prev_block->previous;

            free(prev_block);

            prev_block = prev_prev_block;
        }

        ArenaBlock* next_block = arena->current_block->next;

        while(next_block != NULL)
        {
            ArenaBlock* next_next_block = next_block->next;

            free(next_block);

            next_block = next_next_block;
        }

        free(arena->current_block);

        arena->current_block = NULL;
    }

    arena->capacity = 0;
}