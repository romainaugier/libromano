/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_HASH)
#define __LIBROMANO_HASH

#include "libromano/libromano.h"

#define EMPTY_HASH ((uint32_t)0x811c9dc5)

ROMANO_CPP_ENTER

ROMANO_API uint32_t hash_fnv1a(const char* str);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_HASH) */
