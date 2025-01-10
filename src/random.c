/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/random.h"

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

    return murmur64(_state_next_uint64);
}