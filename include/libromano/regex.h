/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_REGEX)
#define __LIBROMANO_REGEX

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

struct _Regex;

typedef struct _Regex Regex;

ROMANO_API bool regex_compile(Regex* regex, const char* pattern);

ROMANO_API bool regex_match(Regex* regex, const char* string);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_REGEX) */