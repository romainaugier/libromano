// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#ifndef __ROMANO_STR
#define __ROMANO_STR

#include "libromano/libromano.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef char* str;

#define GET_STR_PTR(ptr) ((char*)ptr + sizeof(size_t))
#define GET_RAW_PTR(ptr) ((char*)ptr - sizeof(size_t))

// Creates a new heap allocated string
ROMANO_API str str_new(const char* string);

// Creates a new heap allocated string with formatting
ROMANO_API str str_new_fmt(const char* format, ...);

// Frees the given string
ROMANO_API void str_free(str string);

// Returns the length of the string
ROMANO_API size_t str_len(str string);

// Splits a string into multiple ones
ROMANO_API str* str_split(const char* string, char separator);

#ifdef __cplusplus
}
#endif

#endif // __ROMANO_STR