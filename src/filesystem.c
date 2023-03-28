/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/filesystem.h"

#include <assert.h>

#if defined(ROMANO_WIN)
#include <Shlwapi.h>
#include <PathCch.h>
#include <Windows.h>
#elif defined(ROMANO_LINUX)
#include <sys/stat.h>
#include <linux/limits.h>
#include <string.h>
#endif /* ROMANO_WIN */

int fs_path_exists(const char *path)
{
#if defined(ROMANO_WIN)
    return PathFileExistsA(path);
#elif defined(ROMANO_LINUX)
    struct stat sb;

    return stat(path, &sb) == 0 && (S_ISDIR(sb.st_mode) || 
                                               S_ISREG(sb.st_mode));

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
    mkdir(path, 0755); /* drwxr-xr-x */
#endif /* ROMANO_WIN */
}

void fs_parent_dir(const char* path,
                   char* out_path)
{
#if defined(ROMANO_WIN)
    wchar_t w_path[MAX_PATH];
#elif defined(ROMANO_LINUX)
    unsigned long path_len = strlen(path);
#endif /* defined(ROMANO_WIN)*/

    assert(out_path != NULL);

#if defined(ROMANO_WIN)
    MultiByteToWideChar(CP_UTF8, 0, path, strlen(path), w_path, MAX_PATH);
    w_path[MAX_PATH - 1] = L'\0';

    PathCchRemoveFileSpec(w_path, MAX_PATH);

    WideCharToMultiByte(CP_UTF8, 0, w_path, MAX_PATH, out_path, MAX_PATH, NULL, NULL);
    out_path[MAX_PATH - 1] = '\0';
#elif defined(ROMANO_LINUX)
    while(path_len >= 0)
    {
        char current = path[path_len--];

        if(current == '/')
        {
            break;
        }
    }

    out_path = memcpy(out_path, path, path_len);
    out_path[path_len + 1] = '\0';
#endif /* ROMANO_WIN */
}
