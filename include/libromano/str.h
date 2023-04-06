/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#if !defined(__LIBROMANO_STR)
#define __LIBROMANO_STR

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

typedef char* str;

/* Creates a new heap allocated data */
ROMANO_API str str_new(const char* data);

/* Creates a new heap allocated data with formatting */
ROMANO_API str str_new_fmt(const char* format, ...);

/* Frees the given data */
ROMANO_API void str_free(str data);

/* Returns the length of the data */
ROMANO_API size_t str_length(str data);

/* Splits a data into multiple ones and returns a pointer to the list of datas */
ROMANO_API str* str_split(char* data, const char* separator, uint32_t* count);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_STR) */

