/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_BIT)
#define __LIBROMANO_BIT

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

#define BIT(bit) (size_t)1 << bit

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_BIT) */

