/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/random.h"

#if !defined(ROMANO_MSVC)
static uint64_t wyhash64_x = 1; 
#endif /* !defined(ROMANO_MSVC) */

uint64_t random_wyhash_64() 
{
#if defined(ROMANO_MSVC)
#pragma message("[WARNING]: uint128_t is not supported by MSVC, disabling random_wyhash_64")
    ROMANO_NOT_IMPLEMENTED;
    return 0;
#else
    wyhash64_x += 0x60bee2bee120fc15;
    __uint128_t tmp;
    tmp = (__uint128_t) wyhash64_x * 0xa3b195354a39b70d;
    uint64_t m1 = (tmp >> 64) ^ tmp;
    tmp = (__uint128_t)m1 * 0x1b03738712fad5c9;
    uint64_t m2 = (tmp >> 64) ^ tmp;
    return m2;
#endif /* defined(ROMANO_MSVC) */
}

#if !defined(ROMANO_MSVC)
static __uint128_t g_lehmer64_state = 1;
#endif /* !defined(ROMANO_MSVC) */

uint64_t random_lehmer_64() 
{
#if defined(ROMANO_MSVC)
#pragma message("[WARNING]: uint128_t is not supported by MSVC, disabling random_lehmer_64")
    ROMANO_NOT_IMPLEMENTED;
    return 0;
#else
    g_lehmer64_state *= 0xda942042e4dd58b5;
    return g_lehmer64_state >> 64;
#endif /* defined(ROMANO_MSVC) */
}

static uint32_t _state = 0;

float random_next_float_01()
{
    _state++;

    return random_float_01(_state);
}

uint32_t random_next_uint32_range(const uint32_t low, const uint32_t high)
{
    _state++;

    return random_uint32_range(_state, low, high);
}

static uint32_t _state_next_uint32 = 0;

uint32_t random_next_uint32()
{
    _state_next_uint32++;

    return xorshift32(wang_hash(_state_next_uint32));
}

static uint64_t _state_next_uint64 = 0;

uint64_t random_next_uint64()
{
    _state_next_uint64++;

    return murmur_64(_state_next_uint64);
}