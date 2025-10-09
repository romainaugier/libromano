/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_FILESYSTEM)
#define __LIBROMANO_FILESYSTEM

#include "libromano/vector.h"

#if defined(ROMANO_WIN)
#define MAX_PATH 260
#include <Windows.h>
#elif defined(ROMANO_LINUX)
#define MAX_PATH 4096
#include <dirent.h>
#endif /* ROMANO_WIN */

ROMANO_CPP_ENTER

typedef struct {
    char* content;
    size_t content_sz;
} FileContent;

/*
 * Initializes a FileContent object.
 * If the content of the file cannot be read, returns false, otherwise true
 */
ROMANO_API bool fs_file_content_init(FileContent* content,
                                     const char* file_path,
                                     bool read_binary);

/*
 * Creates a heap-alloacated FileContent object.
 * If the content of the file cannot be read it will return NULL
 */
ROMANO_API FileContent* fs_file_content_new(const char* path,
                                            bool read_binary);

/*
 * Releases the FileContent
 */
ROMANO_API void fs_file_content_release(FileContent* content);

/*
 * Releases and frees the FileContent
 */
ROMANO_API void fs_file_content_free(FileContent* content);

/*
 * Returns true if the given path exists, false otherwise
 */
ROMANO_API bool fs_path_exists(const char* path);

/*
 * Creates the given directory structure. Returns true on success, false otherwise
 */
ROMANO_API bool fs_makedirs(const char* path);

/*
 * Returns the size, from the start of path, of the parent directory path.
 * Should be used as a stringview later, or copied using memcpy
 */
ROMANO_API size_t fs_parent_dir(const char* path);

/*
 * Returns a malloced null-terminated string containing the parent dir path
 */
ROMANO_API char* fs_parent_dir_new(const char* path);

/*
 * Returns zero on success and copies the parent dir path in buffer with a null terminator
 * If buffer_size is not large enough, it returns the needed size (including the null byte)
 */
ROMANO_API size_t fs_parent_dir_init(const char* path, char* buffer, size_t buffer_size);

typedef enum FsCHMode
{
    FsChMod_NOne = 0,
    
    FsChMod_Owner_Read    = 0x0100,  /* 0400 */
    FsChMod_Owner_Write   = 0x0080,  /* 0200 */
    FsChMod_Owner_Exec    = 0x0040,  /* 0100 */
    FsChMod_Owner_Rwx     = 0x01C0,  /* 0700 */
    
    FsChMod_Group_Read    = 0x0020,  /* 0040 */
    FsChMod_Group_Write   = 0x0010,  /* 0020 */
    FsChMod_Group_Exec    = 0x0008,  /* 0010 */
    FsChMod_Group_Rwx     = 0x0038,  /* 0070 */
    
    FsChMod_Other_Read    = 0x0004,  /* 0004 */
    FsChMod_Other_Write   = 0x0002,  /* 0002 */
    FsChMod_Other_Exec    = 0x0001,  /* 0001 */
    FsChMod_Other_Rwx     = 0x0007,  /* 0007 */
    
    FsChMod_Rwx_All       = 0x01FF,  /* 0777 */
    FsChMod_Rw_Owner      = 0x0180,  /* 0600 */
    FsChMod_Rw_All        = 0x01B6,  /* 0666 */
    FsChMod_Rx_All        = 0x0124   /* 0555 */
} FsCHMod;

/*
 * Applies permissions on the given file/directory
 */
ROMANO_API bool fs_chmod(const char* path, 
                         FsCHMod mode);

/*
 * Returns the current working directory.
 * out_path will be heap-allocated and must be freed by the user
 * Returns false if the function fails
 */
ROMANO_API bool fs_get_cwd(char** out_path, size_t* out_len);

/*
 * Removes the item at the given path. 
 * If the item is a directory, it will be removed recursively.
 * Returns true on success.
 */
ROMANO_API bool fs_remove(const char* path);

/*
 * Moves the item from path to new_path
 * Returns true on success
 */
ROMANO_API bool fs_move(const char* path,
                        const char* new_path);

/*
 * Returns true if the item at the given path exists and is a directory
 */
ROMANO_API bool fs_is_dir(const char* path);

/*
 * Returns true if the item at the given path exists and is a file
 */
ROMANO_API bool fs_is_file(const char* path);

typedef enum 
{
    FSWalkMode_YieldDirs = 0x1,
    FSWalkMode_YieldFiles = 0x2,
    FSWalkMode_Recursive = 0x4
} FSWalkMode; 

typedef struct FSWalkIterator {
    char* current_path;
    size_t current_path_sz;
    size_t current_path_capacity;

    char* _current_dir;
    size_t _current_dir_sz;

    Vector _dir_queue;

    bool _is_end;
    bool _first_entry;

#if defined(ROMANO_WIN)
    HANDLE _h_find;
#elif defined(ROMANO_LINUX)
    DIR* _dir;
#endif /* defined(ROMANO_WIN) */
} FSWalkIterator;

ROMANO_API void fs_walk_iterator_init(FSWalkIterator* walk_iterator);

ROMANO_API FSWalkIterator* fs_walk_iterator_new();

ROMANO_API void fs_walk_iterator_release(FSWalkIterator* walk_iterator);

ROMANO_API void fs_walk_iterator_free(FSWalkIterator* walk_iterator);

ROMANO_API bool fs_walk(const char* path,
                        FSWalkIterator* walk_iterator,
                        FSWalkMode mode);

ROMANO_CPP_END

#endif /* __LIBROMANO_FILESYSTEM */
