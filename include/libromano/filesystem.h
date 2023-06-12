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

ROMANO_API void fs_chmod(const char* path, 
                         const uint32_t mode);

ROMANO_API void fs_get_cwd(char* out_path);

ROMANO_API void fs_list_dir(const char* dir_path,
                           char** out_paths,
                           uint32_t* count);

ROMANO_API void fs_remove(const char* path);

ROMANO_API void fs_rename(const char* path,
                          const char* new_name);

ROMANO_API void fs_rm_dir(const char* path);

ROMANO_API uint32_t fs_is_dir(const char* path);

ROMANO_API uint32_t fs_is_file(const char* path);

typedef enum 
{
    FsWalkMode_YieldDirs = 0,
    FsWalkMode_YieldFiles = 1 << 0
} fs_walk_mode; 

ROMANO_API void fs_walk(const char* path,
                        const fs_walk_mode mode);

ROMANO_CPP_END

#endif /* __LIBROMANO_FILESYSTEM */
