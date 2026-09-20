/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/cpu.h"
#include "libromano/endian.h"
#include "libromano/hash.h"

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

ROMANO_FORCE_INLINE uint64_t pippip_load64(const char* p)
{
    uint64_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

uint32_t hash_fnv1a_pippip(const char* str, const size_t n)
{
    const uint32_t PRIME = 591798841u;
    uint32_t hash32;
    uint64_t hash64 = 14695981039346656037u;
    size_t cycles, nd_head;

    if(n > 8)
    {
        cycles = ((n - 1) >> 4) + 1;
        nd_head = n - (cycles << 3);

        for(; cycles--; str += 8)
        {
            hash64 = (hash64 ^ pippip_load64(str)) * PRIME;
            hash64 = (hash64 ^ pippip_load64(str + nd_head)) * PRIME;
        }
    }
    else
    {
        uint64_t tail = 0;

        if(n > 0)
            memcpy(&tail, str, n);

        hash64 = (hash64 ^ tail) * PRIME;
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
			k |= (uint32_t)data[1] << 8;
			k |= (uint32_t)data[2] << 16;
			k |= (uint32_t)data[3] << 24;

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
            k ^= (uint32_t)data[2] << 16;
            /* FALLTHROUGH */
        case 2:
            k ^= (uint32_t)data[1] << 8;
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

ROMANO_FORCE_INLINE uint32_t hash64_to_32(uint64_t h)
{
    return (uint32_t)(h ^ (h >> 32));
}

uint32_t hash_wyhash32(const void *key, size_t len, uint32_t seed)
{
    return hash64_to_32(wyhash(key, len, (uint64_t)seed, wyhash_secret));
}

/* CityHash v1.1, port of the reference implementation (https://github.com/google/cityhash) */

#define k0 0xc3a5c85c97cb3127ULL
#define k1 0xb492b66fbe98f273ULL
#define k2 0x9ae16a3b2f90404fULL

#define c1 0xcc9e2d51U
#define c2 0x1b873593U

typedef struct CityPair {
    uint64_t first;
    uint64_t second;
} CityPair;

ROMANO_FORCE_INLINE uint64_t fetch64(const uint8_t* p)
{
    uint64_t r;
    memcpy(&r, p, sizeof(r));
    return le64toh(r);
}

ROMANO_FORCE_INLINE uint32_t fetch32(const uint8_t* p)
{
    uint32_t r;
    memcpy(&r, p, sizeof(r));
    return le32toh(r);
}

ROMANO_FORCE_INLINE uint64_t rotate64(uint64_t v, int shift)
{
    return shift == 0 ? v : ((v >> shift) | (v << (64 - shift)));
}

ROMANO_FORCE_INLINE uint32_t rotate32(uint32_t v, int shift)
{
    return shift == 0 ? v : ((v >> shift) | (v << (32 - shift)));
}

ROMANO_FORCE_INLINE uint32_t bswap32(uint32_t x)
{
    return ((x & 0xFF000000u) >> 24) | ((x & 0x00FF0000u) >> 8) | ((x & 0x0000FF00u) << 8) | ((x & 0x000000FFu) << 24);
}

ROMANO_FORCE_INLINE uint64_t bswap64(uint64_t x)
{
    return ((uint64_t)bswap32((uint32_t)x) << 32) | (uint64_t)bswap32((uint32_t)(x >> 32));
}

ROMANO_FORCE_INLINE uint64_t shift_mix(uint64_t v)
{
    return v ^ (v >> 47);
}

#define PERMUTE3(T, a, b, c) do { T tmp_ = (a); (a) = (b); (b) = tmp_; tmp_ = (a); (a) = (c); (c) = tmp_; } while(0)
#define SWAP(T, a, b) do { T tmp_ = (a); (a) = (b); (b) = tmp_; } while(0)

ROMANO_FORCE_INLINE uint64_t hash_len_16(uint64_t u, uint64_t v)
{
    return hash_128_to_64(hash_uint128_make(u, v));
}

ROMANO_FORCE_INLINE uint64_t hash_len_16_mul(uint64_t u, uint64_t v, uint64_t mul)
{
    uint64_t a = (u ^ v) * mul;
    uint64_t b;

    a ^= (a >> 47);
    b = (v ^ a) * mul;
    b ^= (b >> 47);
    b *= mul;

    return b;
}

/* CityHash32 */

ROMANO_FORCE_INLINE uint32_t fmix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6bU;
    h ^= h >> 13;
    h *= 0xc2b2ae35U;
    h ^= h >> 16;
    return h;
}

ROMANO_FORCE_INLINE uint32_t mur(uint32_t a, uint32_t h)
{
    a *= c1;
    a = rotate32(a, 17);
    a *= c2;
    h ^= a;
    h = rotate32(h, 19);
    return h * 5 + 0xe6546b64U;
}

static uint32_t hash32_len_13_to_24(const uint8_t* s, size_t len)
{
    const uint32_t a = fetch32(s - 4 + (len >> 1));
    const uint32_t b = fetch32(s + 4);
    const uint32_t c = fetch32(s + len - 8);
    const uint32_t d = fetch32(s + (len >> 1));
    const uint32_t e = fetch32(s);
    const uint32_t f = fetch32(s + len - 4);
    const uint32_t h = (uint32_t)len;

    return fmix32(mur(f, mur(e, mur(d, mur(c, mur(b, mur(a, h)))))));
}

static uint32_t hash32_len_0_to_4(const uint8_t* s, size_t len)
{
    uint32_t b = 0;
    uint32_t c = 9;
    size_t i;

    for(i = 0; i < len; i++)
    {
        const signed char v = (signed char)s[i];
        b = b * c1 + (uint32_t)(int32_t)v;
        c ^= b;
    }

    return fmix32(mur(b, mur((uint32_t)len, c)));
}

static uint32_t hash32_len_5_to_12(const uint8_t* s, size_t len)
{
    uint32_t a = (uint32_t)len;
    uint32_t b = (uint32_t)len * 5;
    uint32_t c = 9;
    const uint32_t d = b;

    a += fetch32(s);
    b += fetch32(s + len - 4);
    c += fetch32(s + ((len >> 1) & 4));

    return fmix32(mur(c, mur(b, mur(a, d))));
}

uint32_t hash_city32(const uint8_t* s, size_t len)
{
    uint32_t h, g, f;
    uint32_t a0, a1, a2, a3, a4;
    size_t iters;

    if(len <= 24)
    {
        return len <= 12 ?
               (len <= 4 ? hash32_len_0_to_4(s, len) : hash32_len_5_to_12(s, len)) :
               hash32_len_13_to_24(s, len);
    }

    h = (uint32_t)len;
    g = c1 * (uint32_t)len;
    f = g;

    a0 = rotate32(fetch32(s + len - 4) * c1, 17) * c2;
    a1 = rotate32(fetch32(s + len - 8) * c1, 17) * c2;
    a2 = rotate32(fetch32(s + len - 16) * c1, 17) * c2;
    a3 = rotate32(fetch32(s + len - 12) * c1, 17) * c2;
    a4 = rotate32(fetch32(s + len - 20) * c1, 17) * c2;

    h ^= a0;
    h = rotate32(h, 19);
    h = h * 5 + 0xe6546b64U;
    h ^= a2;
    h = rotate32(h, 19);
    h = h * 5 + 0xe6546b64U;
    g ^= a1;
    g = rotate32(g, 19);
    g = g * 5 + 0xe6546b64U;
    g ^= a3;
    g = rotate32(g, 19);
    g = g * 5 + 0xe6546b64U;
    f += a4;
    f = rotate32(f, 19);
    f = f * 5 + 0xe6546b64U;

    iters = (len - 1) / 20;

    do
    {
        const uint32_t b0 = rotate32(fetch32(s) * c1, 17) * c2;
        const uint32_t b1 = fetch32(s + 4);
        const uint32_t b2 = rotate32(fetch32(s + 8) * c1, 17) * c2;
        const uint32_t b3 = rotate32(fetch32(s + 12) * c1, 17) * c2;
        const uint32_t b4 = fetch32(s + 16);

        h ^= b0;
        h = rotate32(h, 18);
        h = h * 5 + 0xe6546b64U;
        f += b1;
        f = rotate32(f, 19);
        f = f * c1;
        g += b2;
        g = rotate32(g, 18);
        g = g * 5 + 0xe6546b64U;
        h ^= b3 + b1;
        h = rotate32(h, 19);
        h = h * 5 + 0xe6546b64U;
        g ^= b4;
        g = bswap32(g) * 5;
        h += b4 * 5;
        h = bswap32(h);
        f += b0;
        PERMUTE3(uint32_t, f, h, g);
        s += 20;
    } while(--iters != 0);

    g = rotate32(g, 11) * c1;
    g = rotate32(g, 17) * c1;
    f = rotate32(f, 11) * c1;
    f = rotate32(f, 17) * c1;
    h = rotate32(h + g, 19);
    h = h * 5 + 0xe6546b64U;
    h = rotate32(h, 17) * c1;
    h = rotate32(h + f, 19);
    h = h * 5 + 0xe6546b64U;
    h = rotate32(h, 17) * c1;

    return h;
}

/* CityHash64 */

static uint64_t hash_len_0_to_16(const uint8_t* s, size_t len)
{
    if(len >= 8)
    {
        const uint64_t mul = k2 + len * 2;
        const uint64_t a = fetch64(s) + k2;
        const uint64_t b = fetch64(s + len - 8);
        const uint64_t c = rotate64(b, 37) * mul + a;
        const uint64_t d = (rotate64(a, 25) + b) * mul;
        return hash_len_16_mul(c, d, mul);
    }

    if(len >= 4)
    {
        const uint64_t mul = k2 + len * 2;
        const uint64_t a = fetch32(s);
        return hash_len_16_mul(len + (a << 3), fetch32(s + len - 4), mul);
    }

    if(len > 0)
    {
        const uint8_t a = s[0];
        const uint8_t b = s[len >> 1];
        const uint8_t c = s[len - 1];
        const uint32_t y = (uint32_t)a + ((uint32_t)b << 8);
        const uint32_t z = (uint32_t)len + ((uint32_t)c << 2);
        return shift_mix(y * k2 ^ z * k0) * k2;
    }

    return k2;
}

static uint64_t hash_len_17_to_32(const uint8_t* s, size_t len)
{
    const uint64_t mul = k2 + len * 2;
    const uint64_t a = fetch64(s) * k1;
    const uint64_t b = fetch64(s + 8);
    const uint64_t c = fetch64(s + len - 8) * mul;
    const uint64_t d = fetch64(s + len - 16) * k2;

    return hash_len_16_mul(rotate64(a + b, 43) + rotate64(c, 30) + d,
                           a + rotate64(b + k2, 18) + c,
                           mul);
}

ROMANO_FORCE_INLINE CityPair weak_hash_len_32_with_seeds_values(uint64_t w, uint64_t x, uint64_t y, uint64_t z,
                                                                uint64_t a, uint64_t b)
{
    CityPair result;
    uint64_t c;

    a += w;
    b = rotate64(b + a + z, 21);
    c = a;
    a += x;
    a += y;
    b += rotate64(a, 44);

    result.first = a + z;
    result.second = b + c;

    return result;
}

ROMANO_FORCE_INLINE CityPair weak_hash_len_32_with_seeds(const uint8_t* s, uint64_t a, uint64_t b)
{
    return weak_hash_len_32_with_seeds_values(fetch64(s), fetch64(s + 8), fetch64(s + 16), fetch64(s + 24), a, b);
}

static uint64_t hash_len_33_to_64(const uint8_t* s, size_t len)
{
    const uint64_t mul = k2 + len * 2;
    uint64_t a = fetch64(s) * k2;
    uint64_t b = fetch64(s + 8);
    const uint64_t c = fetch64(s + len - 24);
    const uint64_t d = fetch64(s + len - 32);
    const uint64_t e = fetch64(s + 16) * k2;
    const uint64_t f = fetch64(s + 24) * 9;
    const uint64_t g = fetch64(s + len - 8);
    const uint64_t h = fetch64(s + len - 16) * mul;
    const uint64_t u = rotate64(a + g, 43) + (rotate64(b, 30) + c) * 9;
    const uint64_t v = ((a + g) ^ d) + f + 1;
    const uint64_t w = bswap64((u + v) * mul) + h;
    const uint64_t x = rotate64(e + f, 42) + c;
    const uint64_t y = (bswap64((v + w) * mul) + g) * mul;
    const uint64_t z = e + f + c;

    a = bswap64((x + z) * mul + y) + b;
    b = shift_mix((z + a) * mul + d + h) * mul;

    return b + x;
}

uint64_t hash_city64(const uint8_t* s, size_t len)
{
    uint64_t x, y, z;
    CityPair v, w;

    if(len <= 32)
        return len <= 16 ? hash_len_0_to_16(s, len) : hash_len_17_to_32(s, len);

    if(len <= 64)
        return hash_len_33_to_64(s, len);

    x = fetch64(s + len - 40);
    y = fetch64(s + len - 16) + fetch64(s + len - 56);
    z = hash_len_16(fetch64(s + len - 48) + len, fetch64(s + len - 24));
    v = weak_hash_len_32_with_seeds(s + len - 64, len, z);
    w = weak_hash_len_32_with_seeds(s + len - 32, y + k1, x);
    x = x * k1 + fetch64(s);

    len = (len - 1) & ~(size_t)63;

    do
    {
        x = rotate64(x + y + v.first + fetch64(s + 8), 37) * k1;
        y = rotate64(y + v.second + fetch64(s + 48), 42) * k1;
        x ^= w.second;
        y += v.first + fetch64(s + 40);
        z = rotate64(z + w.first, 33) * k1;
        v = weak_hash_len_32_with_seeds(s, v.second * k1, x + w.first);
        w = weak_hash_len_32_with_seeds(s + 32, z + w.second, y + fetch64(s + 16));
        SWAP(uint64_t, z, x);
        s += 64;
        len -= 64;
    } while(len != 0);

    return hash_len_16(hash_len_16(v.first, w.first) + shift_mix(y) * k1 + z,
                       hash_len_16(v.second, w.second) + x);
}

uint64_t hash_city64_with_seeds(const uint8_t* buf, size_t len, uint64_t seed0, uint64_t seed1)
{
    return hash_len_16(hash_city64(buf, len) - seed0, seed1);
}

uint64_t hash_city64_with_seed(const uint8_t* buf, size_t len, uint64_t seed)
{
    return hash_city64_with_seeds(buf, len, k2, seed);
}

/* CityHash128 */

static hash_uint128_t city_murmur(const uint8_t* s, size_t len, hash_uint128_t seed)
{
    uint64_t a = seed.lo;
    uint64_t b = seed.hi;
    uint64_t c;
    uint64_t d;

    if(len <= 16)
    {
        a = shift_mix(a * k1) * k1;
        c = b * k1 + hash_len_0_to_16(s, len);
        d = shift_mix(a + (len >= 8 ? fetch64(s) : c));
    }
    else
    {
        size_t remaining = len - 16;

        c = hash_len_16(fetch64(s + len - 8) + k1, a);
        d = hash_len_16(b + len, c + fetch64(s + len - 16));
        a += d;

        do
        {
            a ^= shift_mix(fetch64(s) * k1) * k1;
            a *= k1;
            b ^= a;
            c ^= shift_mix(fetch64(s + 8) * k1) * k1;
            c *= k1;
            d ^= c;
            s += 16;
            remaining = remaining > 16 ? remaining - 16 : 0;
        } while(remaining > 0);
    }

    a = hash_len_16(a, c);
    b = hash_len_16(d, b);

    return hash_uint128_make(a ^ b, hash_len_16(b, a));
}

hash_uint128_t hash_city128_with_seed(const uint8_t* s, size_t len, hash_uint128_t seed)
{
    CityPair v, w;
    uint64_t x, y, z;
    size_t tail_done;

    if(len < 128)
        return city_murmur(s, len, seed);

    x = seed.lo;
    y = seed.hi;
    z = len * k1;

    v.first = rotate64(y ^ k1, 49) * k1 + fetch64(s);
    v.second = rotate64(v.first, 42) * k1 + fetch64(s + 8);
    w.first = rotate64(y + z, 35) * k1 + x;
    w.second = rotate64(x + fetch64(s + 88), 53) * k1;

    do
    {
        int round;

        for(round = 0; round < 2; round++)
        {
            x = rotate64(x + y + v.first + fetch64(s + 8), 37) * k1;
            y = rotate64(y + v.second + fetch64(s + 48), 42) * k1;
            x ^= w.second;
            y += v.first + fetch64(s + 40);
            z = rotate64(z + w.first, 33) * k1;
            v = weak_hash_len_32_with_seeds(s, v.second * k1, x + w.first);
            w = weak_hash_len_32_with_seeds(s + 32, z + w.second, y + fetch64(s + 16));
            SWAP(uint64_t, z, x);
            s += 64;
        }

        len -= 128;
    } while(ROMANO_LIKELY(len >= 128));

    x += rotate64(v.first + z, 49) * k0;
    y = y * k0 + rotate64(w.second, 37);
    z = z * k0 + rotate64(w.first, 27);
    w.first *= 9;
    v.first *= k0;

    for(tail_done = 0; tail_done < len;)
    {
        tail_done += 32;
        y = rotate64(x + y, 42) * k0 + v.second;
        w.first += fetch64(s + len - tail_done + 16);
        x = x * k0 + w.first;
        z += w.second + fetch64(s + len - tail_done);
        w.second += v.first;
        v = weak_hash_len_32_with_seeds(s + len - tail_done, v.first + z, v.second);
        v.first *= k0;
    }

    x = hash_len_16(x, v.first);
    y = hash_len_16(y + z, w.first);

    return hash_uint128_make(hash_len_16(x + v.second, w.second) + y,
                             hash_len_16(x + w.second, y + v.second));
}

hash_uint128_t hash_city128(const uint8_t* s, size_t len)
{
    return len >= 16 ?
           hash_city128_with_seed(s + 16, len - 16, hash_uint128_make(fetch64(s), fetch64(s + 8) + k0)) :
           hash_city128_with_seed(s, len, hash_uint128_make(k0, k1));
}

/* CityHashCrc, CRC32-C accelerated when the cpu supports it, software CRC32-C otherwise */

#if defined(ROMANO_X86_64) && (defined(__SSE4_2__) || defined(ROMANO_MSVC))
#include <nmmintrin.h>
#define ROMANO_HASH_HW_CRC 1
#define hw_crc_u64(crc, v) ((uint64_t)_mm_crc32_u64((crc), (v)))
#elif defined(ROMANO_AARCH64) && defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>
#define ROMANO_HASH_HW_CRC 1
#define hw_crc_u64(crc, v) ((uint64_t)__crc32cd((uint32_t)(crc), (v)))
#else
#define ROMANO_HASH_HW_CRC 0
#endif /* defined(ROMANO_X86_64) && (defined(__SSE4_2__) || defined(ROMANO_MSVC)) */

static uint64_t sw_crc_u64(uint64_t crc, uint64_t v)
{
    uint32_t c = (uint32_t)crc;
    int i;
    int bit;

    for(i = 0; i < 8; i++)
    {
        c ^= (uint32_t)(v >> (8 * i)) & 0xFF;

        for(bit = 0; bit < 8; bit++)
            c = (c >> 1) ^ (0x82F63B78u & (0u - (c & 1u)));
    }

    return c;
}

int hash_city_crc_available(void)
{
#if ROMANO_HASH_HW_CRC && defined(ROMANO_X86_64)
    return cpu_has_feature(CPUFeature_SSE4_2) ? 1 : 0;
#elif ROMANO_HASH_HW_CRC && defined(ROMANO_AARCH64)
    return cpu_has_feature(CPUFeature_CRC32) ? 1 : 0;
#else
    return 0;
#endif /* ROMANO_HASH_HW_CRC && defined(ROMANO_X86_64) */
}

#define CRC256_CHUNK(crc, r)                        \
    do                                              \
    {                                               \
        PERMUTE3(uint64_t, x, z, y);                \
        b += fetch64(s);                            \
        c += fetch64(s + 8);                        \
        d += fetch64(s + 16);                       \
        e += fetch64(s + 24);                       \
        f += fetch64(s + 32);                       \
        a += b;                                     \
        h += f;                                     \
        b += c;                                     \
        f += d;                                     \
        g += e;                                     \
        e += z;                                     \
        g += x;                                     \
        z = crc(z, b + g);                          \
        y = crc(y, e + h);                          \
        x = crc(x, f + a);                          \
        e = rotate64(e, r);                         \
        c += e;                                     \
        s += 40;                                    \
    } while(0)

#define DEFINE_CRC256_LONG(name, crc)                                               \
static void name(const uint8_t* s, size_t len, uint32_t seed, uint64_t* result)     \
{                                                                                   \
    uint64_t a = fetch64(s + 56) + k0;                                              \
    uint64_t b = fetch64(s + 96) + k0;                                              \
    uint64_t c = result[0] = hash_len_16(b, len);                                   \
    uint64_t d = result[1] = fetch64(s + 120) * k0 + len;                           \
    uint64_t e = fetch64(s + 184) + seed;                                           \
    uint64_t f = 0;                                                                 \
    uint64_t g = 0;                                                                 \
    uint64_t h = c + d;                                                             \
    uint64_t x = seed;                                                              \
    uint64_t y = 0;                                                                 \
    uint64_t z = 0;                                                                 \
    size_t iters = len / 240;                                                       \
                                                                                    \
    len -= iters * 240;                                                             \
                                                                                    \
    do                                                                              \
    {                                                                               \
        CRC256_CHUNK(crc, 0); PERMUTE3(uint64_t, a, h, c);                          \
        CRC256_CHUNK(crc, 33); PERMUTE3(uint64_t, a, h, f);                         \
        CRC256_CHUNK(crc, 0); PERMUTE3(uint64_t, b, h, f);                          \
        CRC256_CHUNK(crc, 42); PERMUTE3(uint64_t, b, h, d);                         \
        CRC256_CHUNK(crc, 0); PERMUTE3(uint64_t, b, h, e);                          \
        CRC256_CHUNK(crc, 33); PERMUTE3(uint64_t, a, h, e);                         \
    } while(--iters > 0);                                                           \
                                                                                    \
    while(len >= 40)                                                                \
    {                                                                               \
        CRC256_CHUNK(crc, 29);                                                      \
        e ^= rotate64(a, 20);                                                       \
        h += rotate64(b, 30);                                                       \
        g ^= rotate64(c, 40);                                                       \
        f += rotate64(d, 34);                                                       \
        PERMUTE3(uint64_t, c, h, g);                                                \
        len -= 40;                                                                  \
    }                                                                               \
                                                                                    \
    if(len > 0)                                                                     \
    {                                                                               \
        s = s + len - 40;                                                           \
        CRC256_CHUNK(crc, 33);                                                      \
        e ^= rotate64(a, 43);                                                       \
        h += rotate64(b, 42);                                                       \
        g ^= rotate64(c, 41);                                                       \
        f += rotate64(d, 40);                                                       \
    }                                                                               \
                                                                                    \
    result[0] ^= h;                                                                 \
    result[1] ^= g;                                                                 \
    g += h;                                                                         \
    a = hash_len_16(a, g + z);                                                      \
    x += y << 32;                                                                   \
    b += x;                                                                         \
    c = hash_len_16(c, z) + h;                                                      \
    d = hash_len_16(d, e + result[0]);                                              \
    g += e;                                                                         \
    h += hash_len_16(x, f);                                                         \
    e = hash_len_16(a, d) + g;                                                      \
    z = hash_len_16(b, c) + a;                                                      \
    y = hash_len_16(g, h) + c;                                                      \
    result[0] = e + z + y + x;                                                      \
    a = shift_mix((a + y) * k0) * k0 + b;                                           \
    result[1] += a + result[0];                                                     \
    a = shift_mix(a * k0) * k0 + c;                                                 \
    result[2] = a + result[1];                                                      \
    a = shift_mix((a + e) * k0) * k0;                                               \
    result[3] = a + result[2];                                                      \
}

DEFINE_CRC256_LONG(hash_city_crc256_long_sw, sw_crc_u64)

#if ROMANO_HASH_HW_CRC
DEFINE_CRC256_LONG(hash_city_crc256_long_hw, hw_crc_u64)
#endif /* ROMANO_HASH_HW_CRC */

static void hash_city_crc256_long(const uint8_t* s, size_t len, uint32_t seed, uint64_t* result)
{
#if ROMANO_HASH_HW_CRC
    if(hash_city_crc_available())
    {
        hash_city_crc256_long_hw(s, len, seed, result);
        return;
    }
#endif /* ROMANO_HASH_HW_CRC */

    hash_city_crc256_long_sw(s, len, seed, result);
}

void hash_city_crc256(const uint8_t* buf, size_t len, uint64_t result[4])
{
    if(ROMANO_LIKELY(len >= 240))
    {
        hash_city_crc256_long(buf, len, 0, result);
    }
    else
    {
        uint8_t padded[240];

        if(len > 0)
            memcpy(padded, buf, len);

        memset(padded + len, 0, 240 - len);

        hash_city_crc256_long(padded, 240, ~(uint32_t)len, result);
    }
}

hash_uint128_t hash_city_crc128_with_seed(const uint8_t* buf, size_t len, hash_uint128_t seed)
{
    uint64_t result[4];
    uint64_t u;
    uint64_t v;

    if(len <= 900)
        return hash_city128_with_seed(buf, len, seed);

    hash_city_crc256(buf, len, result);

    u = seed.hi + result[0];
    v = seed.lo + result[1];

    return hash_uint128_make(hash_len_16(u, v + result[2]),
                             hash_len_16(rotate64(v, 32), u * k0 + result[3]));
}

hash_uint128_t hash_city_crc128(const uint8_t* buf, size_t len)
{
    uint64_t result[4];

    if(len <= 900)
        return hash_city128(buf, len);

    hash_city_crc256(buf, len, result);

    return hash_uint128_make(result[2], result[3]);
}
