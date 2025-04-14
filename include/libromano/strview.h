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
} StringView;

#define STRVIEW_FMT "%.*s"
#define STRVIEW_ARG(str_view) (int) (str_view).size, (str_view).data
#define STRVIEW_NULL strview_new(NULL, 0);

ROMANO_API StringView strview_new(const char* data, size_t count);

ROMANO_API int strview_cmp(const StringView lhs, const StringView rhs);

ROMANO_API int strview_split(const char* data, const char* separator, StringView* stringview);

ROMANO_API StringView strview_lsplit(const char* data, const char* separator, StringView* stringview);

ROMANO_API StringView strview_rsplit(const char* data, const char* separator, StringView* stringview);

ROMANO_API int strview_find(const StringView s, const char* substr, const int substr_len);

ROMANO_API int strview_startswith(const StringView s, const char* substr, const int substr_len);

ROMANO_API int strview_endswith(const StringView s, const char* substr, const int substr_len);

ROMANO_API StringView strview_trim(const char* data);

ROMANO_API int strview_parse_int(const StringView s);

ROMANO_API int strview_parse_bool(const StringView s);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_STRVIEW) */
