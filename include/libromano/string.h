/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_STR)
#define __LIBROMANO_STR

#include "libromano/common.h"

ROMANO_CPP_ENTER

typedef char* String;

/* Creates a new heap allocated string from existing data */
ROMANO_API String string_new(const char* data);

/* Creates a new heap allocated zero initialized string of the given size */
ROMANO_API String string_newz(const size_t length);

/* Creates a new heap allocated data with formatting */
ROMANO_API String string_newf(const char* format, ...);

/* Returns the capacity of the string*/
ROMANO_API size_t string_capacity(const String string);

/* Returns the size of the string */
ROMANO_API size_t string_length(const String string);

/* Resizes the given string */
ROMANO_API void string_resize(String* string, const size_t new_size);

/* Copies an existing string */
ROMANO_API String string_copy(const String other);

/* Sets the content of the string from existing data */
ROMANO_API void string_setc(String* string, const char* data);

/* Sets the content of the string from another string */
ROMANO_API void string_sets(String* string, const String other);

/* Sets the content of the string from formatting */
ROMANO_API void string_setf(String* string, const char* format, ...);

/* Appends existing data to the string */
ROMANO_API void string_appendc(String* string, const char* data);

/* Appends another string to the string */
ROMANO_API void string_appends(String* string, const String other);

/* Appends formatted string to the string */
ROMANO_API void string_appendf(String* string, const char* format, ...);

/* Prepends existing data to the string */
ROMANO_API void string_prependc(String* string, const char* data);

/* Prepends another string to the string */
ROMANO_API void string_prepends(String* string, const String other);

/* Prepends formatted string to the string */
ROMANO_API void string_prependf(String* string, const char* format, ...);

/* Clears the content of the string (no resize) */
ROMANO_API void string_clear(String string);

/* Splits a data into multiple ones and returns a pointer to the list of datas */
ROMANO_API String* string_splitc(char* data, const char* separator, uint32_t* count);

/* 
 * Compares two String 
 * Returns one if the two strings are equals, 0 otherwise 
 */
ROMANO_API bool string_eq(const String a, const String b);

/* Frees the given String */
ROMANO_API void string_free(String data);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_STR) */
