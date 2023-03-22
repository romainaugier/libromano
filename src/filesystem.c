/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/filesystem.h"

#include <assert.h>

#if defined(ROMANO_WIN)
#include <Shlwapi.h>
#include <PathCch.h>
#include <Windows.h>
#endif /* ROMANO_WIN */

int fs_path_exists(const char *path)
{
#if defined(ROMANO_WIN)
    return PathFileExistsA(path);
#elif defined(ROMANO_LINUX)
#error "fs_path_exist() not implemented on this platform"
#endif /* ROMANO_WIN */
}

void fs_makedirs(const char *path)
{
    if(fs_path_exists(path))
    {
        return;
    }

#if defined(ROMANO_WIN)
    CreateDirectory(path, NULL);
#elif defined(ROMANO_LINUX)
#error "fs_makedirs() not implemented on this platform"
#endif /* ROMANO_WIN */
}

void fs_parent_dir(const char* path,
                   char* out_path)
{
    assert(out_path != NULL);

#if defined(ROMANO_WIN)
    wchar_t w_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path, strlen(path), w_path, MAX_PATH);
    w_path[MAX_PATH - 1] = L'\0';

    PathCchRemoveFileSpec(w_path, MAX_PATH);

    WideCharToMultiByte(CP_UTF8, 0, w_path, MAX_PATH, out_path, MAX_PATH, NULL, NULL);
    out_path[MAX_PATH - 1] = '\0';
#elif defined(ROMANO_LINUX)
#error "fs_path_dirname() not implemented on this platform"
#endif /* ROMANO_WIN */
}
