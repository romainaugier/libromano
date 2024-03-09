/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hash.h"

#include <ctype.h>

/* fnv1a hash */
uint32_t hash_fnv1a(const char* str, size_t n)
{
    uint32_t result = EMPTY_HASH;
    uint8_t* s = (uint8_t*)str;
    size_t i;

#if defined(ROMANO_CLANG) || defined(ROMANO_GCC)
#pragma nounroll
#endif /* defined(ROMANO_CLANG) || defined(ROMANO_GCC) */

    for(i = 0; i < n; i++)
    {
        result ^= (uint32_t)tolower(s[i]);
        result *= (uint32_t)0x01000193u;
    }

    return result;
}

/* fnv1a_pippip hash */
#define _PADr_KAZE(x, n) (((x) << (n)) >> (n))

uint32_t hash_fnv1a_pippip(const char *str, size_t n) 
{
	const uint32_t PRIME = 591798841u; 
    uint32_t hash32; 
    uint64_t hash64 = 14695981039346656037u;
	size_t cycles, nd_head;

    if (n > 8)
    {
        cycles = ((n - 1) >> 4) + 1; 
        nd_head = n - (cycles << 3);

#if defined(ROMANO_CLANG) || defined(ROMANO_GCC)
#pragma nounroll
#endif /* defined(ROMANO_CLANG) || defined(ROMANO_GCC) */

        for(; cycles--; str += 8) 
        {
            hash64 = (hash64 ^ (*(uint64_t *)(str)) ) * PRIME;        
            hash64 = (hash64 ^ (*(uint64_t *)(str + nd_head))) * PRIME;        
        }
    }
    else
    {
        hash64 = (hash64 ^ _PADr_KAZE(*(uint64_t *)(str + 0), (8 - n) << 3)) * PRIME;        
    }

    hash32 = (uint32_t)(hash64 ^ (hash64 >> 32));
    return hash32 ^ (hash32 >> 16);
}


