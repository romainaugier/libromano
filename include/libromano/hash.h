/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_HASH)
#define __LIBROMANO_HASH

#include "libromano/common.h"

ROMANO_CPP_ENTER

ROMANO_API uint32_t hash_fnv1a(const char* str, const size_t n);

ROMANO_API uint32_t hash_fnv1a_pippip(const char* str, const size_t n);

ROMANO_API uint32_t hash_murmur3(const void *key, const size_t len, const uint32_t seed);

ROMANO_API uint64_t hash_wyhash64(const void *key, size_t len, uint64_t seed);

ROMANO_API uint32_t hash_wyhash32(const void *key, size_t len, uint32_t seed);

typedef struct _hash_uint128_t
{
    uint64_t lo;
    uint64_t hi;
} hash_uint128_t;

static ROMANO_FORCE_INLINE uint64_t hash_128_to_64(const hash_uint128_t x)
{
    const uint64_t kmul = 0x9ddfea08eb382d69ULL;
    uint64_t a;
    uint64_t b;

    a = (x.lo ^ x.hi) * kmul;
    a ^= (a >> 47);
    b = (x.hi ^ a) * kmul;
    b ^= (b >> 47);
    b *= kmul;

    return b;
}

static ROMANO_FORCE_INLINE hash_uint128_t hash_uint128_make(uint64_t lo, uint64_t hi)
{
    hash_uint128_t r;
    r.lo = lo;
    r.hi = hi;
    return r;
}

ROMANO_API uint64_t hash_city64(const uint8_t* buf,
                                size_t len);

ROMANO_API uint64_t hash_city64_with_seed(const uint8_t* buf,
                                          size_t len,
                                          uint64_t seed);

ROMANO_API uint64_t hash_city64_with_seeds(const uint8_t* buf,
                                           size_t len,
                                           uint64_t seed0,
                                           uint64_t seed1);

ROMANO_API uint32_t hash_city32(const uint8_t* buf,
                                size_t len);

ROMANO_API hash_uint128_t hash_city128(const uint8_t* buf,
                                       size_t len);

ROMANO_API hash_uint128_t hash_city128_with_seed(const uint8_t* buf,
                                                 size_t len,
                                                 hash_uint128_t seed);

/* Use CityHashCrc128 when the CRC path is available, otherwise fall back
 * to portable CityHash128. These are safe to call on any platform. */
ROMANO_API hash_uint128_t hash_city_crc128(const uint8_t* buf, size_t len);

ROMANO_API hash_uint128_t hash_city_crc128_with_seed(const uint8_t* buf, size_t len,
                                                     hash_uint128_t seed);

/* Produces a 256-bit hash (result[0..3]). Requires a CRC-capable CPU;
 * otherwise falls back to hashing twice with the portable implementation. */
ROMANO_API void hash_city_crc256(const uint8_t* buf, size_t len,
                                 uint64_t result[4]);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_HASH) */
