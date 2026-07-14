/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_MEMORY)
#define __LIBROMANO_MEMORY

#include "libromano/common.h"

#if defined(ROMANO_X86_64)
#include <immintrin.h>
#endif /* defined(ROMANO_X86_64) */

#if defined(ROMANO_MSVC)
#include <stdlib.h>
#endif /* defined(ROMANO_MSVC) */

ROMANO_CPP_ENTER

#if defined(ROMANO_DEBUG_MEMORY)

/* Memory debug utility function, just malloc with a log of where it happens */
ROMANOAPI void* debug_malloc_override(size_t size,
                                      const char* line,
                                      const char* file);

/* Memory debug utility function, just calloc with a log of where it happens */
ROMANOAPI void* debug_calloc_override(size_t size,
                                      size_t element_size,
                                      const char* line,
                                      const char* file);

/* Memory debug utility function, just free with a log of where it happens */
ROMANOAPI void debug_free_override(void* ptr,
                                   const char* line,
                                   const char* file);

#define malloc(size) debug_malloc_override(size, __LINE__, __FILE__)
#define calloc(size, element_size) debug_calloc_override(size, element_size, __LINE__, __FILE__)
#define free(ptr) debug_free_override(ptr, __LINE__, __FILE__)

#endif /* defined(ROMANO_DEBUG_MEMORY) */

#if defined(ROMANO_X86_64)
static ROMANO_FORCE_INLINE void* mem_aligned_alloc(const size_t size, const size_t alignment) { return _mm_malloc(size, alignment); }
static ROMANO_FORCE_INLINE void mem_aligned_free(void* ptr) { _mm_free(ptr); }
#elif defined(ROMANO_AARCH64)
static ROMANO_FORCE_INLINE void* mem_aligned_alloc(const size_t size, const size_t alignment) { void* ptr = NULL; if(posix_memalign(&ptr, alignment, size) != 0) return NULL; return ptr; }
static ROMANO_FORCE_INLINE void mem_aligned_free(void* ptr) { free(ptr); }
#endif /* defined(ROMANO_X86_64) */

#if defined(ROMANO_MSVC)
#define mem_alloca(size) _malloca((size))
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
#define mem_alloca(size) alloca((size))
#endif /* defined(ROMANO_MSVC) */

typedef enum
{
    Endianness_Little = 0,
    Endianness_Big = 1
} Endianness;

#define ENDIANNESS_STR(en) en == 1 ? "Big" : "Little"

void mem_check_endianness(void);

ROMANO_API Endianness mem_get_endianness(void);

#if defined(ROMANO_MSVC)
ROMANO_FORCE_INLINE uint16_t mem_bswapu16(uint16_t x) { return _byteswap_ushort(x); }
ROMANO_FORCE_INLINE uint32_t mem_bswapu32(uint32_t x) { return _byteswap_ulong(x); }
ROMANO_FORCE_INLINE uint64_t mem_bswapu64(uint64_t x) { return _byteswap_uint64(x); }
#elif defined(ROMANO_GCC) || defined(ROMANO_CLANG)
ROMANO_FORCE_INLINE uint16_t mem_bswapu16(uint16_t x) { return __builtin_bswap16(x); }
ROMANO_FORCE_INLINE uint32_t mem_bswapu32(uint32_t x) { return __builtin_bswap32(x); }
ROMANO_FORCE_INLINE uint64_t mem_bswapu64(uint64_t x) { return __builtin_bswap64(x); }
#else
#error "No implementation for byte swap functions using this compiler"
#endif /* defined(ROMANO_MSVC) */

ROMANO_API void mem_swap(void *m1, void *m2, const size_t n);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_MEMORY) */
