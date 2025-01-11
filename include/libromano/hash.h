/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_HASH)
#define __LIBROMANO_HASH

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

ROMANO_API uint32_t hash_fnv1a(const char* str, const size_t n);

ROMANO_API uint32_t hash_fnv1a_pippip(const char* str, const size_t n);

ROMANO_API uint32_t hash_murmur3(const void *key, const size_t len, const uint32_t seed);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_HASH) */
