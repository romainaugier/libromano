/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_STRVIEW)
#define __LIBROMANO_STRVIEW

#include "libromano/common.h"

ROMANO_CPP_ENTER

typedef struct {
    size_t size;
    char* data;
} StringView;

#define STRVIEW_FMT "%.*s"
#define STRVIEW_ARG(str_view) (int) (str_view).size, (str_view).data
#define STRVIEW_NULL strview_new(NULL, 0);

/*
 * Creates a new StringView from a character array and its length.
 * Returns a StringView struct containing the pointer and size.
 */
ROMANO_API StringView strview_new(const char* data, size_t count);

/*
 * Compares two StringView objects for equality.
 * Returns non-zero if they have the same size and content, otherwise returns zero.
 */
ROMANO_API int strview_cmp(const StringView lhs, const StringView rhs);

/*
 * Splits a string at the first occurrence of the separator character.
 * Updates the StringView to the found substring.
 * Returns 1 if a split occurred, 0 otherwise.
 */
ROMANO_API int strview_split(const char* data, const char* separator, StringView* stringview);

/*
 * Splits a string at the first occurrence of the separator substring (from the left).
 * Returns the substring before the separator and optionally updates the given StringView to the remainder.
 */
ROMANO_API StringView strview_lsplit(const char* data, const char* separator, StringView* stringview);

/*
 * Splits a string at the last occurrence of the separator substring (from the right).
 * Returns the substring after the separator and optionally updates the given StringView to the remainder.
 */
ROMANO_API StringView strview_rsplit(const char* data, const char* separator, StringView* stringview);

/*
 * Searches for a substring within the StringView.
 * Returns the offset of the first occurrence, or -1 if not found.
 */
ROMANO_API int strview_find(const StringView s, const char* substr, const int substr_len);

/*
 * Checks if the StringView starts with the given substring.
 * Returns true if it does, false otherwise.
 */
ROMANO_API bool strview_startswith(const StringView s, const char* substr, const int substr_len);

/*
 * Checks if the StringView ends with the given substring.
 * Returns true if it does, false otherwise.
 */
ROMANO_API bool strview_endswith(const StringView s, const char* substr, const int substr_len);

/*
 * Trims leading and trailing whitespace from the given string.
 * Returns a StringView of the trimmed string.
 */
ROMANO_API StringView strview_trim(const char* data);

/*
 * Parses an integer from a StringView.
 * Returns the parsed integer value, or 0 if invalid or overflows.
 */
ROMANO_API int strview_parse_int(const StringView s);

/*
 * Parses a boolean value from a StringView.
 * Recognizes "0", "1", "true", "True", "false", "False".
 * Returns false if not recognized.
 */
ROMANO_API bool strview_parse_bool(const StringView s);

/*
 * Parses a double from a StringView.
 * Returns the parsed double value, or 0.0 if invalid.
 */
ROMANO_API double strview_parse_double(const StringView s);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_STRVIEW) */
