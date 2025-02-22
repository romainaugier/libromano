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

typedef struct {
    char* content;
    size_t content_length;
} FileContent;

ROMANO_API bool fs_file_content_new(const char* path,
                                    FileContent* content);

ROMANO_API void fs_file_content_free(FileContent* content);

ROMANO_API int fs_path_exists(const char* path);

ROMANO_API void fs_makedirs(const char* path);

ROMANO_API void fs_parent_dir(const char* path,
                              char* out_path);

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

/* File system walk yields all files in a directory and its subdirectory if in recursive mode */
typedef enum 
{
    FsWalkMode_YieldDirs = 0x1,
    FsWalkMode_YieldFiles = 0x2,
    FsWalkMode_Recursive = 0x4
} fs_walk_mode; 

struct fs_walk_item {
    char* path;
    size_t path_length;
};

typedef struct fs_walk_item fs_walk_item_t;

ROMANO_API fs_walk_item_t* fs_walk_item_new(const char* path);

ROMANO_API void fs_walk_item_free(fs_walk_item_t* walk_item);

ROMANO_API int fs_walk(const char* path,
                       fs_walk_item_t* walk_item,
                       const fs_walk_mode mode);

ROMANO_CPP_END

#endif /* __LIBROMANO_FILESYSTEM */
