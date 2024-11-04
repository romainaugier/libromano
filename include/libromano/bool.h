/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_BOOL)
#define __LIBROMANO_BOOL

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

typedef int bool;

#define true 1
#define false 0

ROMANO_CPP_END

#endif /* #define __LIBROMANO_BOOL */