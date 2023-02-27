/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/memory.h"
#include "libromano/logger.h"

#if defined(ROMANO_DEBUG_MEMORY)

void* debug_malloc_override(size_t size,
                            const char* line,
                            const char* file)
{
    void* ptr;
    
    logger_log(LogLevel_Debug, "Allocating %lu bytes of memory at L.%s in file \"%s\"", size, line, file);

    ptr = malloc(size);

    if(ptr == NULL)
    {
        logger_log(LogLevel_Error, "Allocation failed (L.%s in F:%s)", line, file);
        return NULL;
    }

    return ptr;
}

void debug_free_override(void* ptr,
                         const char* line,
                         const char* file)
{
    logger_log(LogLevel_Debug, "Freeing memory at L.%s in file \"%s\"", line, file);
    free(ptr);
}

#endif /* defined(ROMANO_DEBUG_MEMORY) */