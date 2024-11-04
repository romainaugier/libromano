/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_RANDOM)
#define __LIBROMANO_RANDOM

#include "libromano/libromano.h"

/* Very fast pseudo random generator with a good distribution */

ROMANO_FORCE_INLINE uint32_t wang_hash(uint32_t seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return 1u + seed;
}

ROMANO_FORCE_INLINE uint32_t xorshift32(uint32_t state)
{
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return state;
}

ROMANO_FORCE_INLINE float random_float_01(uint32_t state)
{
    uint32_t tofloat = 0x2f800004u;

    const uint32_t x = xorshift32(wang_hash(state));

    return (float)x * *(float *)&tofloat;
}

ROMANO_FORCE_INLINE uint32_t random_int_range(const uint32_t state, const uint32_t low, const uint32_t high)
{
    return (uint32_t)(random_float_01(state) * ((float)high - (float)low)) + low;
}

/* Non-Thread safe random generators, use atomics to make it thread-safe, or use thread-local _state */

ROMANO_API float random_next_float_01();

ROMANO_API uint32_t random_next_int_range(const uint32_t low, const uint32_t high);

#endif /* !defined(__LIBROMANO_RANDOM) */