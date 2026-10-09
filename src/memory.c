/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/memory.h"
#include "libromano/logger.h"

#include <string.h>

#if defined(ROMANO_DEBUG_MEMORY)
#undef malloc
#undef calloc
#undef free
#endif /* defined(ROMANO_DEBUG_MEMORY) */

#if defined(ROMANO_MIMALLOC)
#include "mimalloc.h"

void* romano_malloc(size_t size) { return mi_malloc(size); }
void* romano_calloc(size_t count, size_t size) { return mi_calloc(count, size); }
void* romano_realloc(void* ptr, size_t size) { return mi_realloc(ptr, size); }
void romano_free(void* ptr) { mi_free(ptr); }
void* romano_aligned_alloc(size_t size, size_t alignment) { return mi_malloc_aligned(size, alignment); }
void romano_aligned_free(void* ptr) { mi_free(ptr); }
const char* romano_allocator_name(void) { return "mimalloc"; }
#else
void* romano_malloc(size_t size) { return malloc(size); }
void* romano_calloc(size_t count, size_t size) { return calloc(count, size); }
void* romano_realloc(void* ptr, size_t size) { return realloc(ptr, size); }
void romano_free(void* ptr) { free(ptr); }
const char* romano_allocator_name(void) { return "crt"; }

#if defined(ROMANO_WIN)
void* romano_aligned_alloc(size_t size, size_t alignment) { return _aligned_malloc(size, alignment); }
void romano_aligned_free(void* ptr) { _aligned_free(ptr); }
#else
void* romano_aligned_alloc(size_t size, size_t alignment)
{
    void* ptr = NULL;

    if(posix_memalign(&ptr, alignment < sizeof(void*) ? sizeof(void*) : alignment, size) != 0)
        return NULL;

    return ptr;
}

void romano_aligned_free(void* ptr) { free(ptr); }
#endif /* defined(ROMANO_WIN) */
#endif /* defined(ROMANO_MIMALLOC) */

char* romano_strndup(const char* str, size_t len)
{
    char* copy = (char*)romano_malloc(len + 1);

    if(copy == NULL)
        return NULL;

    memcpy(copy, str, len);
    copy[len] = '\0';

    return copy;
}

#if defined(ROMANO_DEBUG_MEMORY)

void* debug_malloc_override(size_t size,
                            const char* line,
                            const char* file)
{
    void* ptr;
    
    logger_log(LogLevel_Debug, "Allocating %lu bytes of memory (%s:%s)", size, file, line);

    ptr = romano_malloc(size);

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

    ptr = romano_calloc(size, element_size);

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
    romano_free(ptr);
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
