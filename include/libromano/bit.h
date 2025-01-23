/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_BIT)
#define __LIBROMANO_BIT

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

#define BIT(bit) (size_t)1 << bit

ROMANO_FORCE_INLINE uint32_t round_u32_to_next_pow2(uint32_t x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x++;
}

ROMANO_FORCE_INLINE uint64_t round_u64_to_next_pow2(uint64_t x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x++;
}

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_BIT) */
