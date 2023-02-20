// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#include "libromano/memory.h"

void* debug_malloc_override(size_t size,
                            const char* line,
                            const char* file)
{
    void* ptr = malloc(size);

    return ptr;
}

void debug_free_override(void* ptr,
                         const char* line,
                         const char* file)
{
    free(ptr);
}