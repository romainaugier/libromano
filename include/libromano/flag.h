/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_FLAG)
#define __LIBROMANO_FLAG

#include "libromano/libromano.h"

typedef uint32_t flag32;
typedef uint64_t flag64;

#define BIT_FLAG(bit) 1 << bit

#define HAS_FLAG(flag, bit) flag & bit

#define SET_FLAG(flag, bit) flag |= bit 

#define UNSET_FLAG(flag, bit) flag &= ~bit

#endif  /* !defined(__LIBROMANO_FLAG) */
