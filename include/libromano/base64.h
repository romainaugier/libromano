/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_BASE64)
#define __LIBROMANO_BASE64

#include "libromano/common.h"

ROMANO_CPP_ENTER

ROMANO_API char* base64_encode(const void* ROMANO_RESTRICT data,
                               size_t data_sz,
                               size_t* out_sz);

ROMANO_API void* base64_decode(const char* ROMANO_RESTRICT data,
                               size_t data_siz,
                               size_t* out_sz);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_BASE64) */
