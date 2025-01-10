/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hash.h"
#include "libromano/endian.h"

#include <ctype.h>

#define EMPTY_HASH ((uint32_t)0x811c9dc5u)

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

/*
 * murmurhash3 -- from the original code:
 *
 * "MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code."
 *
 * References:
 *	https://github.com/aappleby/smhasher/
 */

uint32_t hash_murmur3(const void *key, size_t len, uint32_t seed)
{
	const uint8_t *data = key;
	const size_t orig_len = len;
	uint32_t h = seed;

	if (ROMANO_LIKELY(((uintptr_t)key & 3) == 0)) 
    {
		while (len >= sizeof(uint32_t)) 
        {
			uint32_t k = *(const uint32_t *)(const void *)data;

			k = htole32(k);

			k *= 0xcc9e2d51;
			k = (k << 15) | (k >> 17);
			k *= 0x1b873593;

			h ^= k;
			h = (h << 13) | (h >> 19);
			h = h * 5 + 0xe6546b64;

			data += sizeof(uint32_t);
			len -= sizeof(uint32_t);
		}
	} 
    else 
    {
		while (len >= sizeof(uint32_t)) 
        {
			uint32_t k;

			k  = data[0];
			k |= data[1] << 8;
			k |= data[2] << 16;
			k |= data[3] << 24;

			k *= 0xcc9e2d51;
			k = (k << 15) | (k >> 17);
			k *= 0x1b873593;

			h ^= k;
			h = (h << 13) | (h >> 19);
			h = h * 5 + 0xe6546b64;

			data += sizeof(uint32_t);
			len -= sizeof(uint32_t);
		}
	}

	/*
	 * Handle the last few bytes of the input array.
	 */
	uint32_t k = 0;

	switch (len) 
    {
        case 3:
            k ^= data[2] << 16;
            /* FALLTHROUGH */
        case 2:
            k ^= data[1] << 8;
            /* FALLTHROUGH */
        case 1:
            k ^= data[0];
            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
	}

	/*
	 * Finalisation mix: force all bits of a hash block to avalanche.
	 */
	h ^= orig_len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}