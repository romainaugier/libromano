/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_STRING_ALGO)
#define __LIBROMANO_STRING_ALGO

#include "libromano/common.h"

ROMANO_CPP_ENTER

ROMANO_API const char* strstrn(const char* haystack,
                               size_t haystack_sz,
                               const char* needle,
                               size_t needle_sz);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_STRING_ALGO) */
