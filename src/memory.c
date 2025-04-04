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
    
    logger_log(LogLevel_Debug, "Allocating %lu bytes of memory (%s:%s)", size, file, line);

    ptr = malloc(size);

    if(ptr == NULL)
    {
        logger_log(LogLevel_Error, "Allocation failed (%s:%s)", file, line);
        return NULL;
    }

    return ptr;
}

void* debug_calloc_override(size_t size,
                            size_t element_size,
                            const char* line,
                            const char* file)
{
    void* ptr;
    
    logger_log(LogLevel_Debug, "Allocating %lu bytes of memory (%s:%s)", size, file, line);

    ptr = calloc(size, element_size);

    if(ptr == NULL)
    {
        logger_log(LogLevel_Error, "Allocation failed (%s:%s)", file, line);
        return NULL;
    }

    return ptr;
}

void debug_free_override(void* ptr,
                         const char* line,
                         const char* file)
{
    logger_log(LogLevel_Debug, "Freeing memory (%s:%s)", file, line);
    free(ptr);
}

#endif /* defined(ROMANO_DEBUG_MEMORY) */

ROMANO_FORCE_INLINE bool is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } e = { 0x01000000 };

    return e.c[0];
}

static Endianness _endianness = 0;

void mem_check_endianness(void)
{
    _endianness = is_big_endian() ? Endianness_Big : Endianness_Little;
}

Endianness mem_get_endianness(void)
{
    return _endianness;
}

void mem_swap(void *m1, void *m2, const size_t n)
{
    char*  p     = m1;
    char*  p_end = p + n;
    char*  q     = m2;

    while (p < p_end) {
        char  tmp = *p;
        *p = *q;
        *q = tmp;
        p++;
        q++;
    }
}
