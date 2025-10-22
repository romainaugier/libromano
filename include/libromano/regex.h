/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_REGEX)
#define __LIBROMANO_REGEX

#include "libromano/common.h"
#include "libromano/bit.h"

/*
    Very simple regular expression matching using Thompson's NFA algorithm
    Supports *, +, ., ?, | operators and alpha-numeric characters
*/

ROMANO_CPP_ENTER

typedef enum RegexFlags {
    RegexFlags_DebugCompilation = BIT(0),
} RegexFlags;

struct Regex;

typedef struct Regex Regex;

ROMANO_API Regex* regex_compile(const char* pattern, RegexFlags flags);

ROMANO_API bool regex_match(Regex* regex, const char* string);

ROMANO_API void regex_free(Regex* regex);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_REGEX) */
