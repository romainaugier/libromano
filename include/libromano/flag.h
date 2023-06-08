/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#if !defined(__LIBROMANOFLAG)
#define __LIBROMANOFLAG

#include "libromano/libromano.h"

typedef uint32_t flag32;
typedef uint64_t flag64;

#define BIT_FLAG(bit) 1 << bit

#define HAS_FLAG(flag, bit) flag & bit

#define SET_FLAG(flag, bit) flag |= bit 

#define UNSET_FLAG(flag, bit) flag &= ~bit

#endif  /* !defined(__LIBROMANOFLAG) */
