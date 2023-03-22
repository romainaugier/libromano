/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_FILESYSTEM)
#define __LIBROMANO_FILESYSTEM

#include "libromano/libromano.h"

#if defined(ROMANO_WIN)
#define MAX_PATH 260
#elif defined(ROMANO_LINUX)
#define MAX_PATH 4096
#endif /* ROMANO_WIN */

ROMANO_CPP_ENTER

ROMANO_API int fs_path_exists(const char* path);

ROMANO_API void fs_parent_dir(const char* path,
                              char* out_path);

ROMANO_API void fs_makedirs(const char* path);

ROMANO_CPP_END

#endif /* __LIBROMANO_FILESYSTEM */
