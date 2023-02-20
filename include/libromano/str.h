// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#ifndef __LIBROMANO_STR
#define __LIBROMANO_STR

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

typedef char* str;

// Creates a new heap allocated string
ROMANO_API str str_new(const char* string);

// Creates a new heap allocated string with formatting
ROMANO_API str str_new_fmt(const char* format, ...);

// Frees the given string
ROMANO_API void str_free(str string);

// Returns the length of the string
ROMANO_API size_t str_length(str string);

// Splits a string into multiple ones and returns a pointer to the list of strings
ROMANO_API str* str_split(char* string, const char* separator, uint32_t* count);

ROMANO_CPP_END

#endif // __LIBROMANO_STR