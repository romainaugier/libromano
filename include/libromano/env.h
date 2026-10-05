/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_ENV)
#define __LIBROMANO_ENV

#include "libromano/common.h"

ROMANO_CPP_ENTER

/*
 * Returns the value of the environment variable, NULL if not set.
 * The pointer is valid until the next env_set/env_unset call
 */
ROMANO_API const char* env_get(const char* name);

ROMANO_API bool env_set(const char* name, const char* value);

ROMANO_API bool env_unset(const char* name);

/*
 * Returns a heap-allocated NULL-terminated array of "KEY=VALUE" strings, to free with env_list_free
 */
ROMANO_API char** env_list_new(void);

ROMANO_API void env_list_free(char** list);

/* Separator of the PATH variable entries (';' on Windows, ':' elsewhere) */
ROMANO_API char env_path_separator(void);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_ENV) */
