/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/arena.h"
#include "libromano/logger.h"

#if ROMANO_DEBUG
#define NUM_LOOPS 100000
#else
#define NUM_LOOPS 1000000
#endif /* ROMANO_DEBUG */

int main(void)
{
    logger_init();

    Arena arena;
    arena_init(&arena, ARENA_BLOCK_SIZE);

    for(size_t i = 0; i < NUM_LOOPS; i++)
    {
        float f = (float)i;
        arena_push(&arena, &f, sizeof(float));
    }

    arena_release(&arena);

    logger_release();

    return 0;
}
