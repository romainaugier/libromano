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

/* CityHash */

#define k0 0xc3a5c85c97cb3127ULL
#define k1 0xb492b66fbe98f273ULL
#define k2 0x9ae16a3b2f90404fULL
#define k3 0xc949d7c7509e6557ULL

#define c1 0xcc9e2d51U
#define c2 0x1b873593U

static ROMANO_FORCE_INLINE uint64_t uload64(const uint8_t* p)
{
    uint64_t r;
    memcpy(&r, p, sizeof(r));
    return r;
}

static ROMANO_FORCE_INLINE uint32_t uload32(const uint8_t* p)
{
    uint32_t r;
    memcpy(&r, p, sizeof(r));
    return r;
}

static ROMANO_FORCE_INLINE uint64_t fetch64(const uint8_t* p)
{
    return htole64(uload64(p));
}

static ROMANO_FORCE_INLINE uint32_t fetch32(const uint8_t* p)
{
    return htole32(uload32(p));
}

/* Rotation / mixing */

static ROMANO_FORCE_INLINE uint64_t rotate64(uint64_t v, int shift)
{
    return shift == 0 ? v : ((v >> shift) | (v << (64 - shift)));
}

static ROMANO_FORCE_INLINE uint32_t rotate32(uint32_t v, int shift)
{
    return shift == 0 ? v : ((v >> shift) | (v << (32 - shift)));
}

static ROMANO_FORCE_INLINE uint64_t shift_mix(uint64_t v)
{
    return v ^ (v >> 47);
}

static ROMANO_FORCE_INLINE void swap64(uint64_t* a, uint64_t* b)
{
    uint64_t t = *a;
    *a = *b;
    *b = t;
}

static ROMANO_FORCE_INLINE void swap32(uint32_t* a, uint32_t* b)
{
    uint32_t t = *a;
    *a = *b;
    *b = t;
}

/* Portable short-input helpers */

static ROMANO_FORCE_INLINE uint64_t hash_len_16(uint64_t u, uint64_t v)
{
    const uint64_t mul = 0x9ddfea08eb382d69ULL;
    uint64_t a;
    uint64_t b;

    a = (u ^ v) * mul;
    a ^= (a >> 47);
    b = (v ^ a) * mul;
    b ^= (b >> 47);
    b *= mul;

    return b;
}

static ROMANO_FORCE_INLINE uint64_t hash_len_0_to_16(const uint8_t* s, size_t len)
{
    if(ROMANO_LIKELY(len >= 8))
    {
        uint64_t mul = k2 + len * 2;
        uint64_t a = fetch64(s) + k2;
        uint64_t b = fetch64(s + len - 8);
        uint64_t c = rotate64(b, 37) * mul + a;
        uint64_t d = (rotate64(a, 25) + b) * mul;
        return hash_len_16(c, d);
    }

    if(ROMANO_LIKELY(len >= 4))
    {
        uint64_t mul = k2 + len * 2;
        uint64_t a = fetch32(s);
        return hash_len_16(len + (a << 3), fetch32(s + len - 4));
    }

    if(ROMANO_LIKELY(len > 0))
    {
        uint8_t a = s[0];
        uint8_t b = s[len >> 1];
        uint8_t c = s[len - 1];
        uint32_t y = (uint32_t)a + ((uint32_t)b << 8);
        uint32_t z = (uint32_t)len + ((uint32_t)c << 2);
        return shift_mix(y * k2 ^ z * k0) * k2;
    }

    return k2;
}

static ROMANO_FORCE_INLINE uint64_t hash_len_17_to_32(const uint8_t* s, size_t len)
{
    uint64_t mul = k2 + len * 2;
    uint64_t a = fetch64(s) * k1;
    uint64_t b = fetch64(s + 8);
    uint64_t c = fetch64(s + len - 8) * mul;
    uint64_t d = fetch64(s + len - 16) * k2;

    return hash_len_16(rotate64(a + b, 43) + rotate64(c, 30) + d,
                       a + rotate64(b + k2, 18) + c);
}

static ROMANO_FORCE_INLINE uint64_t hash_len_33_to_64(const uint8_t* s, size_t len)
{
    uint64_t mul = k2 + len * 2;
    uint64_t a = fetch64(s) * k2;
    uint64_t b = fetch64(s + 8);
    uint64_t c = fetch64(s + len - 8) * mul;
    uint64_t d = fetch64(s + len - 16) * k2;
    uint64_t y = rotate64(a + b, 43) + rotate64(c, 30) + d;
    uint64_t z = hash_len_16(y, a + rotate64(b + k2, 18) + c);
    uint64_t e = fetch64(s + 16) * mul;
    uint64_t f = fetch64(s + 24);
    uint64_t g = (y + fetch64(s + len - 32)) * mul;
    uint64_t h = (z + fetch64(s + len - 24)) * mul;

    return hash_len_16(rotate64(e + f, 43) + rotate64(g, 30) + h,
                       e + rotate64(f + a, 18) + g);
}

/* Portable CityHash64 */

uint64_t hash_city64(const uint8_t* buf, size_t len)
{
    if(ROMANO_LIKELY(len <= 32))
    {
        if(ROMANO_LIKELY(len <= 16))
            return hash_len_0_to_16(buf, len);

        return hash_len_17_to_32(buf, len);
    }

    if(ROMANO_LIKELY(len <= 64))
        return hash_len_33_to_64(buf, len);

    {
        uint64_t x = fetch64(buf);
        uint64_t y = fetch64(buf + len - 16) ^ k1;
        uint64_t z = fetch64(buf + len - 56) ^ k0;
        uint64_t v[2];
        uint64_t w[2];

        v[0] = rotate64(y + k1, 49) * k0 + x;
        v[1] = rotate64(y, 42) * k0 + x;
        w[0] = rotate64(z + k2, 37) * k1 + y;
        w[1] = rotate64(z, 42) * k1 + y;
        x = x * k1 + fetch64(buf + 8);

        len = (len - 1) & ~(size_t)63;

        do
        {
            x = rotate64(x + y + v[0] + fetch64(buf + 16), 37) * k1;
            y = rotate64(y + v[1] + fetch64(buf + 48), 42) * k1;
            x ^= w[1];
            y ^= v[0];
            z = rotate64(z ^ w[0], 33);
            v[0] = rotate64(v[0] * k1, 31);
            v[1] = rotate64(v[1] * k2, 19);
            w[0] = rotate64(w[0] + fetch64(buf + 32), 43);
            w[1] = rotate64(w[1] + fetch64(buf + 40), 39);
            buf += 64;
            len -= 64;
        } while(ROMANO_LIKELY(len != 0));

        return hash_len_16(hash_len_16(v[0], w[0]) + shift_mix(y) * k1 + z,
                           hash_len_16(v[1], w[1]) + x);
    }
}

uint64_t hash_city64_with_seeds(const uint8_t* buf, size_t len,
                                uint64_t seed0, uint64_t seed1)
{
    if(ROMANO_LIKELY(len <= 32))
    {
        if(ROMANO_LIKELY(len <= 16))
            return hash_len_16(fetch64(buf) ^ seed0, fetch64(buf + len - 8) ^ seed1);

        return hash_len_16(fetch64(buf + len - 16) ^ seed0,
                           fetch64(buf + len - 8) ^ seed1);
    }

    /* Reuse the portable path; the seeded variant just injects at the ends. */
    return hash_city64_with_seeds(buf, 32, seed0, seed1) ^
           hash_city64(buf + 32, len - 32) * k1;
}

uint64_t hash_city64_with_seed(const uint8_t* buf, size_t len, uint64_t seed)
{
    return hash_city64_with_seeds(buf, len, seed, k2);
}

/* Portable CityHash32 */

static ROMANO_FORCE_INLINE uint32_t fmix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6bU;
    h ^= h >> 13;
    h *= 0xc2b2ae35U;
    h ^= h >> 16;
    return h;
}

static ROMANO_FORCE_INLINE uint32_t mur(uint32_t a, uint32_t h)
{
    a *= c1;
    a = rotate32(a, 17);
    a *= c2;
    h ^= a;
    h = rotate32(h, 19);
    return h * 5 + 0xe6546b64U;
}

uint32_t hash_city32(const uint8_t* buf, size_t len)
{
    if(ROMANO_LIKELY(len <= 24))
    {
        if(ROMANO_LIKELY(len <= 12))
        {
            if(ROMANO_LIKELY(len >= 4))
            {
                uint32_t h = (uint32_t)len;
                uint32_t a = fetch32(buf);
                uint32_t b = fetch32(buf + len - 4);
                return fmix32(mur(b, mur(a, h)));
            }

            if(ROMANO_LIKELY(len > 0))
            {
                uint32_t h = (uint32_t)len;
                uint32_t a = buf[0];
                uint32_t b = buf[len >> 1];
                uint32_t c = buf[len - 1];
                return fmix32(mur(c, mur(b, mur(a, h))));
            }

            return 0;
        }

        {
            uint32_t h = (uint32_t)len;
            uint32_t a = fetch32(buf);
            uint32_t b = fetch32(buf + 4);
            uint32_t c = fetch32(buf + len - 8);
            uint32_t d = fetch32(buf + len - 4);
            return fmix32(mur(d, mur(c, mur(b, mur(a, h)))));
        }
    }

    {
        uint32_t h = (uint32_t)len;
        uint32_t a = fetch32(buf);
        uint32_t b = fetch32(buf + 4);
        uint32_t c = fetch32(buf + 8);
        uint32_t d = fetch32(buf + 12);
        uint32_t e = fetch32(buf + 16);
        uint32_t f = fetch32(buf + 20);
        uint32_t g = fetch32(buf + 24);
        uint32_t hv = fetch32(buf + 28);

        h = mur(c, h) + a;
        h = mur(e, h) + b;
        h = mur(f, h) + c;
        h = mur(g, h) + d;
        h = mur(hv, h) + e;

        buf += 32;
        len -= 32;

        while(ROMANO_LIKELY(len >= 32))
        {
            a = fetch32(buf);
            b = fetch32(buf + 4);
            c = fetch32(buf + 8);
            d = fetch32(buf + 12);
            e = fetch32(buf + 16);
            f = fetch32(buf + 20);
            g = fetch32(buf + 24);
            hv = fetch32(buf + 28);

            h = mur(c, h) + a;
            h = mur(e, h) + b;
            h = mur(f, h) + c;
            h = mur(g, h) + d;
            h = mur(hv, h) + e;

            buf += 32;
            len -= 32;
        }

        switch(len)
        {
            case 24:
                h = mur(fetch32(buf + 20), h) + fetch32(buf + 16);
                /* FALLTHROUGH */
            case 20:
                h = mur(fetch32(buf + 16), h) + fetch32(buf + 12);
                /* FALLTHROUGH */
            case 16:
                h = mur(fetch32(buf + 12), h) + fetch32(buf + 8);
                /* FALLTHROUGH */
            case 12:
                h = mur(fetch32(buf + 8), h) + fetch32(buf + 4);
                /* FALLTHROUGH */
            case 8:
                h = mur(fetch32(buf + 4), h) + fetch32(buf);
                /* FALLTHROUGH */
            case 4:
                h = mur(fetch32(buf), h) + (uint32_t)len;
                /* FALLTHROUGH */
            default:
                h = fmix32(h);
                break;
        }

        return h;
    }
}

/* Portable CityHash128 */

static ROMANO_FORCE_INLINE hash_uint128_t weak_hash_len_32_with_seeds_vals(uint64_t w,
                                                                           uint64_t x,
                                                                           uint64_t y,
                                                                           uint64_t z,
                                                                           uint64_t a,
                                                                           uint64_t b)
{
    hash_uint128_t r;
    a += w;
    b = rotate64(b + a + z, 21);
    uint64_t c = a;
    a += x;
    a += y;
    b += rotate64(a, 44);
    r.lo = a + z;
    r.hi = b + c;
    return r;
}

static ROMANO_FORCE_INLINE hash_uint128_t weak_hash_len_32_with_seeds(const uint8_t* s,
                                                                      uint64_t a,
                                                                      uint64_t b)
{
    return weak_hash_len_32_with_seeds_vals(fetch64(s), fetch64(s + 8),
                                            fetch64(s + 16), fetch64(s + 24),
                                            a, b);
}

static ROMANO_FORCE_INLINE hash_uint128_t city_murmur(const uint8_t* s,
                                                      size_t len,
                                                      hash_uint128_t seed)
{
    uint64_t a = fetch64(s);
    uint64_t b = fetch64(s + 8);
    uint64_t c = fetch64(s + len - 8);
    uint64_t d = fetch64(s + len - 16);
    uint64_t y = rotate64(a + b, 43) + rotate64(c, 30) + d;
    uint64_t z = hash_len_16(y, a + rotate64(b + k2, 18) + c);
    uint64_t e = fetch64(s + 16) * k2;
    uint64_t f = fetch64(s + 24);
    uint64_t g = (y + fetch64(s + len - 32)) * k2;
    uint64_t h = (z + fetch64(s + len - 24)) * k2;
    uint64_t u = rotate64(e + f, 43) + rotate64(g, 30) + h;
    uint64_t v = e + rotate64(f + a, 18) + g;

    return weak_hash_len_32_with_seeds_vals(fetch64(s + len - 64), fetch64(s + len - 56),
                                            fetch64(s + len - 48) + seed.lo, fetch64(s + len - 40) + seed.hi,
                                            u,
                                            v);
}

hash_uint128_t hash_city128_with_seed(const uint8_t* s, size_t len, hash_uint128_t seed)
{
    if(ROMANO_LIKELY(len < 128))
        return city_murmur(s, len, seed);

    {
        uint64_t x = seed.lo;
        uint64_t y = seed.hi;
        uint64_t z = len * k1;
        uint64_t v[2];
        uint64_t w[2];
        hash_uint128_t vv;

        v[0] = rotate64(y ^ k1, 49) * k1 + fetch64(s);
        v[1] = rotate64(v[0], 42) * k1 + fetch64(s + 8);
        w[0] = rotate64(y + z, 35) * k1 + x;
        w[1] = rotate64(x + fetch64(s + 88), 53) * k1;

        do
        {
            x = rotate64(x + y + v[0] + fetch64(s + 8), 37) * k1;
            y = rotate64(y + v[1] + fetch64(s + 48), 42) * k1;
            x ^= w[1];
            y += v[0] + fetch64(s + 40);
            z = rotate64(z + w[0], 33) * k1;
            vv = weak_hash_len_32_with_seeds(s, v[1] * k1, x + w[0]);
            v[0] = vv.lo; v[1] = vv.hi;
            vv = weak_hash_len_32_with_seeds(s + 32, z + w[1], y + fetch64(s + 16));
            w[0] = vv.lo; w[1] = vv.hi;
            swap64(&z, &x);
            s += 64;

            x = rotate64(x + y + v[0] + fetch64(s + 8), 37) * k1;
            y = rotate64(y + v[1] + fetch64(s + 48), 42) * k1;
            x ^= w[1];
            y += v[0] + fetch64(s + 40);
            z = rotate64(z + w[0], 33) * k1;
            vv = weak_hash_len_32_with_seeds(s, v[1] * k1, x + w[0]);
            v[0] = vv.lo; v[1] = vv.hi;
            vv = weak_hash_len_32_with_seeds(s + 32, z + w[1], y + fetch64(s + 16));
            w[0] = vv.lo; w[1] = vv.hi;
            swap64(&z, &x);
            s += 64;

            len -= 128;
        } while(ROMANO_LIKELY(len >= 128));

        x += rotate64(v[0] + z, 49) * k0;
        y = y * k0 + rotate64(w[1], 37);
        z = z * k0 + rotate64(w[0], 27);
        w[0] *= 9;
        v[0] *= k0;

        for(size_t tail_done = 0; tail_done < len; )
        {
            tail_done += 32;
            y = rotate64(x + y, 42) * k0 + v[1];
            w[0] += fetch64(s + len - tail_done + 16);
            x = x * k0 + w[0];
            z += w[1] + fetch64(s + len - tail_done);
            w[1] += v[0];
            vv = weak_hash_len_32_with_seeds(s + len - tail_done, v[0] + z, v[1]);
            v[0] = vv.lo; v[1] = vv.hi;
            v[0] *= k0;
        }

        x = hash_len_16(x, v[0]);
        y = hash_len_16(y + z, w[0]);

        return hash_uint128_make(hash_len_16(x + v[1], w[1]) + y,
                                 hash_len_16(x + w[1], y + v[1]));
    }
}

hash_uint128_t hash_city128(const uint8_t* s, size_t len)
{
    if(ROMANO_LIKELY(len >= 16))
        return hash_city128_with_seed(s + 16, len - 16,
                                      hash_uint128_make(fetch64(s), fetch64(s + 8) + k0));

    return hash_city128_with_seed(s, len, hash_uint128_make(k0, k1));
}

/* Hardware CRC-accelerated path */

/* Runtime dispatch helper. We resolve once and cache. */
static int g_hash_city_crc_available = -1;

int hash_city_crc_available(void)
{
    if(ROMANO_UNLIKELY(g_hash_city_crc_available < 0))
    {
#if defined(ROMANO_X86_64) || defined(ROMANO_X86) 
        g_hash_city_crc_available = cpu_has_feature(CPUFeature_SSE4_2) ? 1 : 0;
#elif defined(ROMANO_AARCH64)
        g_hash_city_crc_available = cpu_has_feature(CPUFeature_CRC32) ? 1 : 0;
#else
        g_hash_city_crc_available = 0;
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */
    }

    return g_hash_city_crc_available;
}

/* CRC32-C(u64) primitives. Both x86 SSE4.2 and ARMv8 CRC32 compute the
 * same CRC-32C polynomial; only the intrinsic names differ. */
#if defined(ROMANO_X86_64)
#   include <nmmintrin.h>
    static ROMANO_FORCE_INLINE uint64_t crc_u64(uint64_t crc, uint64_t v)
    {
        return (uint64_t)_mm_crc32_u64(crc, v);
    }
#elif defined(ROMANO_AARCH64)
#   include <arm_acle.h>
    static ROMANO_FORCE_INLINE uint64_t crc_u64(uint64_t crc, uint64_t v)
    {
        return (uint64_t)__crc32cd((uint32_t)crc, v);
    }
#else
    static ROMANO_FORCE_INLINE uint64_t crc_u64(uint64_t crc, uint64_t v)
    {
        (void)crc; (void)v;
        return 0; /* unreachable: guarded by hash_city_crc_available() */
    }
#endif

/* CRC-accelerated CityHash128/256 over long inputs */

/* Requires len >= 900 in the original; we relax the requirement and call
 * the portable path for shorter buffers so the API is uniform. */
static void hash_city_crc256_long(const uint8_t* s,
                                  size_t len,
                                  hash_uint128_t seed,
                                  uint64_t result[4])
{
    uint64_t a = fetch64(s + 56) + k0;
    uint64_t b = fetch64(s + 96) + k0;
    uint64_t c = result[0] = hash_len_0_to_16(s + 16, 16) + k0;
    uint64_t d = result[1] = hash_len_17_to_32(s + 48, 16) + k1;

    uint64_t v[4];
    uint64_t w[4];

    v[0] = fetch64(s + 112) * k0;
    v[1] = fetch64(s + 120) * k1;
    v[2] = fetch64(s + 128) * k2;
    v[3] = fetch64(s + 136) * k3;
    w[0] = fetch64(s + 8) * k1;
    w[1] = fetch64(s + 24) * k2;
    w[2] = fetch64(s + 40) * k3;
    w[3] = fetch64(s + 64) * k0;

    uint64_t x = result[0];
    uint64_t y = result[1];
    uint64_t z = len * k1;

    v[0] ^= fetch64(s + 168) * k0;
    v[1] ^= fetch64(s + 176) * k1;
    v[2] ^= fetch64(s + 184) * k2;
    v[3] ^= fetch64(s + 192) * k3;
    w[0] ^= fetch64(s + 200) * k0;
    w[1] ^= fetch64(s + 208) * k1;
    w[2] ^= fetch64(s + 216) * k2;
    w[3] ^= fetch64(s + 224) * k3;

    (void)seed;

    len = (len - 1) & ~(size_t)255;

    do
    {
        x = crc_u64(x, fetch64(s + 16));
        y = crc_u64(y, fetch64(s + 24));
        z = crc_u64(z, fetch64(s + 32));
        a = crc_u64(a, fetch64(s + 40));
        b = crc_u64(b, fetch64(s + 48));
        c = crc_u64(c, fetch64(s + 56));
        d = crc_u64(d, fetch64(s + 64));
        result[3] = crc_u64(v[0], fetch64(s + 72));
        result[2] = crc_u64(v[1], fetch64(s + 80));
        result[1] = crc_u64(v[2], fetch64(s + 88));
        result[0] = crc_u64(v[3], fetch64(s + 96));

        /* 128-bit-wide shuffle of the 4-word state. */
        {
            uint64_t tmp[4];
            tmp[0] = x; tmp[1] = y; tmp[2] = z; tmp[3] = a;
            x = b; y = c; z = d; a = tmp[0];
            b = tmp[1]; c = tmp[2]; d = tmp[3];
        }

        x = crc_u64(x, fetch64(s + 104));
        y = crc_u64(y, fetch64(s + 112));
        z = crc_u64(z, fetch64(s + 120));
        a = crc_u64(a, fetch64(s + 128));
        b = crc_u64(b, fetch64(s + 136));
        c = crc_u64(c, fetch64(s + 144));
        d = crc_u64(d, fetch64(s + 152));
        result[3] = crc_u64(v[0], fetch64(s + 160));
        result[2] = crc_u64(v[1], fetch64(s + 168));
        result[1] = crc_u64(v[2], fetch64(s + 176));
        result[0] = crc_u64(v[3], fetch64(s + 184));

        {
            uint64_t tmp[4];
            tmp[0] = x; tmp[1] = y; tmp[2] = z; tmp[3] = a;
            x = b; y = c; z = d; a = tmp[0];
            b = tmp[1]; c = tmp[2]; d = tmp[3];
        }

        /* Rotate the 4x128-bit vectors via the 32-byte chunk */
        {
            uint64_t tmp[4];
            tmp[0] = v[0]; tmp[1] = v[1]; tmp[2] = v[2]; tmp[3] = v[3];
            v[0] = w[0]; v[1] = w[1]; v[2] = w[2]; v[3] = w[3];
            w[0] = tmp[0]; w[1] = tmp[1]; w[2] = tmp[2]; w[3] = tmp[3];
        }

        s += 256;
        len -= 256;
    } while(len != 0);

    /* Fold the 32 bytes at the tail of the buffer. */
    a = crc_u64(a, fetch64(s + 16));
    b = crc_u64(b, fetch64(s + 24));
    c = crc_u64(c, fetch64(s + 32));
    d = crc_u64(d, fetch64(s + 40));
    x = crc_u64(x, fetch64(s + 48));
    y = crc_u64(y, fetch64(s + 56));
    z = crc_u64(z, fetch64(s + 64));
    result[3] = crc_u64(v[0], fetch64(s + 72));
    result[2] = crc_u64(v[1], fetch64(s + 80));
    result[1] = crc_u64(v[2], fetch64(s + 88));
    result[0] = crc_u64(v[3], fetch64(s + 96));

    result[0] ^= a;
    result[1] ^= b;
    result[2] ^= c;
    result[3] ^= d;
    result[0] ^= x;
    result[1] ^= y;
    result[2] ^= z;
}

hash_uint128_t hash_city_crc128_with_seed(const uint8_t* buf, size_t len,
                                     hash_uint128_t seed)
{
    if(ROMANO_LIKELY(!hash_city_crc_available() || len <= 900))
    {
        return hash_city128_with_seed(buf, len, seed);
    }

    {
        uint64_t result[4];
        hash_city_crc256_long(buf, len, seed, result);
        return hash_uint128_make(result[0], result[1]);
    }
}

hash_uint128_t hash_city_crc128(const uint8_t* buf, size_t len)
{
    if(ROMANO_LIKELY(!hash_city_crc_available() || len <= 900))
    {
        return hash_city128(buf, len);
    }

    {
        uint64_t result[4];
        hash_city_crc256_long(buf, len, hash_uint128_make(0, 0), result);
        return hash_uint128_make(result[0], result[1]);
    }
}

void hash_city_crc256(const uint8_t* buf, size_t len, uint64_t result[4])
{
    if(ROMANO_UNLIKELY(!hash_city_crc_available() || len <= 900))
    {
        /* Portable fallback: two independent 128-bit hashes. */
        hash_uint128_t h0 = hash_city128(buf, len);
        hash_uint128_t h1 = hash_city128_with_seed(buf, len,
                                              hash_uint128_make(h0.lo ^ k0, h0.hi ^ k1));
        result[0] = h0.lo;
        result[1] = h0.hi;
        result[2] = h1.lo;
        result[3] = h1.hi;
        return;
    }

    hash_city_crc256_long(buf, len, hash_uint128_make(0, 0), result);
}