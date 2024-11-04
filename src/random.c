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

uint32_t random_next_int_range(const uint32_t low, const uint32_t high)
{
    _state++;

    return random_int_range(_state, low, high);
}