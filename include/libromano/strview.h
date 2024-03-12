/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_STRVIEW)
#define __LIBROMANO_STRVIEW

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

typedef struct {
    size_t size;
    char* data;
} strview_t;

#define STRVIEW_FMT "%.*s"
#define STRVIEW_ARG(str_view) (int) (str_view).size, (str_view).data
#define STRVIEW_NULL strview_new(NULL, 0);

ROMANO_API strview_t strview_new(const char* data, size_t count);

ROMANO_API int strview_cmp(const strview_t lhs, const strview_t rhs);

ROMANO_API strview_t* strview_split(const char* data, const char* separator, size_t* count);

ROMANO_API int strview_split_yield(const char* data, const char* separator, strview_t* stringview);

ROMANO_API strview_t strview_trim(const char* data);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_STRVIEW) */
