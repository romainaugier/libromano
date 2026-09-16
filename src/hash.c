/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hash.h"
#include "libromano/endian.h"

#include <ctype.h>
#include <string.h>

#define EMPTY_HASH ((uint32_t)0x811c9dc5u)

/* fnv1a hash */
uint32_t hash_fnv1a(const char* str, const size_t n)
{
    uint32_t result = EMPTY_HASH;
    char* s = (char*)str;
    size_t i;

    for(i = 0; i < n; i++)
    {
        result ^= (uint32_t)s[i];
        result *= (uint32_t)0x01000193UL;
    }

    return result;
}

/* fnv1a_pippip hash */
#define _PADr_KAZE(x, n) (((x) << (n)) >> (n))

uint32_t hash_fnv1a_pippip(const char *str, const size_t n)
{
	const uint32_t PRIME = 591798841u;
    uint32_t hash32;
    uint64_t hash64 = 14695981039346656037u;
	size_t cycles, nd_head;

    if (n > 8)
    {
        cycles = ((n - 1) >> 4) + 1;
        nd_head = n - (cycles << 3);

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

uint32_t hash_murmur3(const void *key, const size_t len, const uint32_t seed)
{
	size_t len2 = len;
	const uint8_t *data = key;
	const size_t orig_len = len;
	uint32_t h = seed;

	if(ROMANO_LIKELY(((uintptr_t)key & 3) == 0))
    {
		while(len2 >= sizeof(uint32_t))
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
			len2 -= sizeof(uint32_t);
		}
	}
    else
    {
		while(len2 >= sizeof(uint32_t))
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
			len2 -= sizeof(uint32_t);
		}
	}

	/*
	 * Handle the last few bytes of the input array.
	 */
	uint32_t k = 0;

	switch(len2)
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

/*
 * wyhash final version 4.2, portable C99, no dependencies.
 *
 * Algorithm by Wang Yi <https://github.com/wangyi-fudan/wyhash>
 * This file is released into the public domain (Unlicense / CC0).
 */

#define WYHASH_VERSION 4 /* final version 4.2 */

#if defined(ROMANO_MSVC) && defined(_M_X64) && !defined(__clang__)
#include <intrin.h>
#pragma intrinsic(_umul128)
#define WYHASH_HAVE_UMUL128 1
#endif

/* 128-bit multiply: *A <- low 64 bits, *B <- high 64 bits. */
static inline void wyhash_mum(uint64_t *A, uint64_t *B)
{
#if defined(__SIZEOF_INT128__)
	__uint128_t r = (__uint128_t)(*A) * (__uint128_t)(*B);
	*A = (uint64_t)r;
	*B = (uint64_t)(r >> 64);
#elif defined(WYHASH_HAVE_UMUL128)
	uint64_t hi;
	uint64_t lo = _umul128(*A, *B, &hi);
	*A = lo;
	*B = hi;
#else
	uint64_t ha = *A >> 32, hb = *B >> 32;
	uint64_t la = (uint32_t)*A, lb = (uint32_t)*B;
	uint64_t rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb;
	uint64_t t  = rl + (rm0 << 32);
	uint64_t lo = t + (rm1 << 32);
	uint64_t hi = rh + (rm0 >> 32) + (rm1 >> 32) + (t < rl) + (lo < t);
	*A = lo;
	*B = hi;
#endif // defined(__SIZEOF_INT128__)
}

static inline uint64_t wyhash_mix(uint64_t A, uint64_t B)
{
    wyhash_mum(&A, &B);
    return A ^ B;
}

static inline uint64_t wyhash_r8(const uint8_t *p)
{
    uint64_t v;
    memcpy(&v, p, 8);
    return v;
}

static inline uint64_t wyhash_r4(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, 4);
    return (uint64_t)v;
}

static inline uint64_t wyhash_r3(const uint8_t *p, size_t k)
{
    return ((uint64_t)p[0] << 16) | ((uint64_t)p[k >> 1] << 8) | (uint64_t)p[k - 1];
}

static const uint64_t wyhash_secret[4] = {
    0xa0761d6478bd642full,
    0xe7037ed1a0b428dbull,
    0x8ebc6af09c88c6e3ull,
    0x589965cc75374cc3ull
};

static inline uint64_t wyhash(const void *key,
							  size_t len,
							  uint64_t seed,
                              const uint64_t *secret)
{
    const uint8_t *p = (const uint8_t *)key;
    uint64_t a, b;

    seed ^= wyhash_mix(seed ^ secret[0], secret[1]);

    if(ROMANO_LIKELY(len <= 16)) 
	{
        if(ROMANO_LIKELY(len >= 4)) 
		{
            a = (wyhash_r4(p) << 32) | wyhash_r4(p + ((len >> 3) << 2));
            b = (wyhash_r4(p + len - 4) << 32) | wyhash_r4(p + len - 4 - ((len >> 3) << 2));
		}
		else if(ROMANO_LIKELY(len > 0)) 
		{
            a = wyhash_r3(p, len);
            b = 0;
        }
		else
		{
            a = b = 0;
        }
    } 
	else 
	{
        size_t i = len;

        if(ROMANO_UNLIKELY(i > 48)) 
		{
            uint64_t see1 = seed, see2 = seed;

            do {
                seed = wyhash_mix(wyhash_r8(p)      ^ secret[1],
                                  wyhash_r8(p +  8) ^ seed);
                see1 = wyhash_mix(wyhash_r8(p + 16) ^ secret[2],
                                  wyhash_r8(p + 24) ^ see1);
                see2 = wyhash_mix(wyhash_r8(p + 32) ^ secret[3],
                                  wyhash_r8(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (ROMANO_LIKELY(i > 48));

            seed ^= see1 ^ see2;
        }

        while(i > 16) 
		{
            seed = wyhash_mix(wyhash_r8(p) ^ secret[1],
                              wyhash_r8(p + 8) ^ seed);
            i -= 16;
            p += 16;
        }

        a = wyhash_r8(p + i - 16);
        b = wyhash_r8(p + i - 8);
    }

    a ^= secret[1];
    b ^= seed;
    wyhash_mum(&a, &b);

    return wyhash_mix(a ^ secret[0] ^ (uint64_t)len, b ^ secret[1]);
}

uint64_t hash_wyhash64(const void *key, size_t len, uint64_t seed)
{
    return wyhash(key, len, seed, wyhash_secret);
}

static ROMANO_FORCE_INLINE uint32_t hash64_to_32(uint64_t h)
{
    return (uint32_t)(h ^ (h >> 32));
}

uint32_t hash_wyhash32(const void *key, size_t len, uint32_t seed)
{
    return hash64_to_32(wyhash(key, len, (uint64_t)seed, wyhash_secret));
}