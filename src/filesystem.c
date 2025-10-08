/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/filesystem.h"
#include "libromano/memory.h"
#include "libromano/vector.h"

#include <assert.h>
#include <fileapi.h>
#include <handleapi.h>
#include <minwinbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winnt.h>

#if defined(ROMANO_WIN)
#include <Shlwapi.h>
#include <shellapi.h>
#include <PathCch.h>
#elif defined(ROMANO_LINUX)
#include <sys/stat.h>
#include <linux/limits.h>
#endif /* ROMANO_WIN */

bool fs_file_content_init(FileContent* content,
                          const char* path,
                          bool read_binary)
{
    FILE* file_handle;

    ROMANO_ASSERT(content != NULL, "NULL file content");

    content->content = NULL;
    content->content_sz = 0;

    if(read_binary)
        file_handle = fopen(path, "r");
    else
        file_handle = fopen(path, "rb");

    if(file_handle == NULL)
        return false;

    fseek(file_handle, 0, SEEK_END);
    content->content_sz = ftell(file_handle);
    rewind(file_handle);
    content->content = (char*)calloc(content->content_sz + 1, sizeof(char));
    fread(content->content, sizeof(char), content->content_sz, file_handle);
    fclose(file_handle);

    return true;
}

FileContent* fs_file_content_new(const char* path,
                                 bool read_binary)
{
    FileContent* content = (FileContent*)calloc(1, sizeof(FileContent));

    if(content == NULL)
        return NULL;

    if(!fs_file_content_init(content, path, read_binary))
    {
        free(content);
        return NULL;
    }

    return content;
}

void fs_file_content_release(FileContent* content)
{
    ROMANO_ASSERT(content != NULL, "content is NULL");

    if(content->content != NULL)
    {
        free(content->content);
        content->content = NULL;
        content->content_sz = 0;
    }
}

void fs_file_content_free(FileContent* content)
{
    ROMANO_ASSERT(content != NULL, "content is NULL");

    fs_file_content_release(content);
    free(content);
}

bool fs_path_exists(const char *path)
{
    ROMANO_ASSERT(path != NULL, "path is NULL");

#if defined(ROMANO_WIN)
    return (bool) PathFileExistsA(path);
#elif defined(ROMANO_LINUX)
    struct stat sb;

    return (bool)(stat(path, &sb) == 0 && (S_ISDIR(sb.st_mode) || S_ISREG(sb.st_mode)));
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */
}

bool fs_makedirs(const char *path)
{
    ROMANO_ASSERT(path != NULL, "path is NULL");

    if(fs_path_exists(path))
        return true;

#if defined(ROMANO_WIN)
    return (bool)CreateDirectoryA(path, NULL);
#elif defined(ROMANO_LINUX)
    return mkdir(path, 0755) == 0; /* drwxr-xr-x */
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */
}

size_t fs_parent_dir(const char* path)
{
    ROMANO_ASSERT(path != NULL, "path is NULL");

    size_t path_sz = strlen(path);

    while(path_sz >= 0)
    {
        char current = path[path_sz--];

        if(current == '\\' || current == '/')
        {
            break;
        }
    }

    return path_sz;
}

bool fs_chmod(const char* path,
              FsCHMod mode)
{
    ROMANO_ASSERT(path != NULL, "path is NULL");

    if(!fs_path_exists(path))
        return false;

#if defined(ROMANO_WIN)
    DWORD attrs = GetFileAttributesA(path);

    if(attrs == INVALID_FILE_ATTRIBUTES)
        return false;
    
    /* TODO: add more attributes */
    if(mode & FsChMod_Owner_Write)
        attrs &= ~FILE_ATTRIBUTE_READONLY;
    else
        attrs |= FILE_ATTRIBUTE_READONLY;
    
    if(!SetFileAttributesA(path, attrs))
        return false;
    
#elif defined(ROMANO_LINUX)
    if(chmod(path, (mode_t)(mode & 0x01FF)) != 0)
        return false;
    
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */

    return true;
}

bool fs_get_cwd(char** out_path, size_t* out_sz)
{
#if defined(ROMANO_WIN)
    DWORD sz = GetCurrentDirectoryA(0, NULL);

    char* buffer = (char*)calloc(sz, sizeof(char));

    DWORD total_sz = GetCurrentDirectoryA(sz, buffer);

    if(total_sz == 0)
    {
        free(buffer);
        *out_path = NULL;
        *out_sz = 0;
        return false;
    }

    *out_path = buffer;
    *out_sz = (size_t)sz;
#elif defined(ROMANO_LINUX)
    *out_path = get_current_dir_name();

    if(*out_path == NULL)
        return false;

    *out_sz = strlen(*out_path);
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */

    return true;
}

#if defined(ROMANO_LINUX)
/* 
 * Used by nftw in fs_remove 
 */
int fs_remove_callback(const char* fpath, const struct stat* sb, int typeflag, struct FTW* ftwbuf)
{
    (void)sb;
    (void)typeflag;
    (void)ftwbuf;
    
    int rv = remove(fpath);

    if(rv != 0)
    {
        return rv;
    }

    return 0;
}
#endif /* defined(ROMANO_LINUX) */

bool fs_remove(const char* path)
{
    ROMANO_ASSERT(path != NULL, "path is NULL");

#if defined(ROMANO_WIN)
    if(fs_is_dir(path))
    {
        size_t path_sz = strlen(path);
        char* path_buffer = (char*)mem_alloca(path_sz + 2);
        memset(path_buffer, 0, path_sz + 2);
        memcpy(path_buffer, path, path_sz);

        SHFILEOPSTRUCTA file_op;
        memset(&file_op, 0, sizeof(SHFILEOPSTRUCTA));

        file_op.wFunc = FO_DELETE;
        file_op.pFrom = path_buffer;
        file_op.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION | FOF_SILENT;

        if(SHFileOperationA(&file_op) != 0)
        {
            /* TODO: better error handling here */
            return false;
        }

        return true;
    }
    else 
    {
        return (bool)DeleteFileA(path);
    }
#elif defined(ROMANO_LINUX)
    if(fs_is_dir(path))
    {
        if(nftw(path, fs_remove_callback, 64, FTW_DEPTH | FTW_PHYS) != 0)
            return false;

        return true;
    }
    else
    {
        return (bool)(remove(path) == 0);
    }
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */

    return true;
}

bool fs_move(const char* path,
             const char* new_path)
{
    ROMANO_ASSERT(path != NULL, "path is NULL");

#if defined(ROMANO_WIN)
    return (bool)MoveFileA(path, new_path);
#elif defined(ROMANO_LINUX)
    return rename(path, new_path) == 0;
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */

    return true;
}

bool fs_is_dir(const char* path)
{
    ROMANO_ASSERT(path != NULL, "path is NULL");

    if(!fs_path_exists(path))
        return false;

#if defined(ROMANO_WIN)
    return GetFileAttributesA(path) & FILE_ATTRIBUTE_DIRECTORY;
#elif defined(ROMANO_LINUX)
    struct stat stats;
    stat(path, &stats);
    return (bool)S_ISREG(stats.st_mode);
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */

    return true;
}

bool fs_is_file(const char* path)
{
    ROMANO_ASSERT(path != NULL, "path is NULL");

    if(!fs_path_exists(path))
        return false;

#if defined(ROMANO_WIN)
    return GetFileAttributesA(path) & ~FILE_ATTRIBUTE_DIRECTORY;
#elif defined(ROMANO_LINUX)
    struct stat stats;
    stat(path, &stats);
    return (bool)S_ISDIR(stats.st_mode);
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */

    return true;
}

void fs_walk_iterator_init(FSWalkIterator* walk_iterator)
{
    size_t path_sz;

    memset(walk_iterator, 0, sizeof(FSWalkIterator));

#if defined(ROMANO_WIN)
    walk_iterator->_h_find = INVALID_HANDLE_VALUE;
#elif defined(ROMANO_LINUX)
    walk_iterator->_dir = NULL;
#endif /* defined(ROMANO_WIN) */

    vector_init(&walk_iterator->_dir_queue, 128, sizeof(char*));

    walk_iterator->_first_entry = true;
}

FSWalkIterator* fs_walk_iterator_new()
{
    FSWalkIterator* item = (FSWalkIterator*)malloc(sizeof(FSWalkIterator));

    fs_walk_iterator_init(item);

    return item;
}

void fs_walk_iterator_queue_release_cb(void* data)
{
    free(data);
}

void fs_walk_iterator_release(FSWalkIterator* walk_iterator)
{
    if(walk_iterator->current_path != NULL)
        free(walk_iterator->current_path);

    if(walk_iterator->_current_dir != NULL)
        free(walk_iterator->_current_dir);

#if defined(ROMANO_WIN)
    FindClose(walk_iterator->_h_find);
    walk_iterator->_h_find = INVALID_HANDLE_VALUE;
#elif defined(ROMANO_LINUX)
    closedir(walk_iterator->_dir);
    walk_iterator->_dir = NULL;
#endif /* defined(ROMANO_WIN) */

    vector_release_with_dtor(&walk_iterator->_dir_queue, fs_walk_iterator_queue_release_cb);
}

void fs_walk_iterator_free(FSWalkIterator* walk_iterator)
{
    fs_walk_iterator_release(walk_iterator);
    free(walk_iterator);
}

#if defined(ROMANO_WIN)
bool should_skip_entry(FSWalkMode mode,
                       const char* path,
                       DWORD attrs)
{
    return false;
}
#elif defined(ROMANO_LINUX)
bool should_skip_entry(FSWalkMode mode,
                       const char* path)
{
    return false;
}
#else
#error "Platform not supported"
#endif /* defined(ROMANO_WIN) */

bool fs_walk(const char* path,
             FSWalkIterator* walk_iterator,
             FSWalkMode mode)
{
    ROMANO_ASSERT(walk_iterator != NULL, "walk_iterator is NULL");

    return false;

#if defined(ROMANO_WIN)
    WIN32_FIND_DATAA find_data;

    if(walk_iterator->_first_entry)
    {
        walk_iterator->_current_dir = (char*)path;
        walk_iterator->_current_dir_sz = strlen(path);

        char* search_path = mem_alloca(walk_iterator->_current_dir_sz + 3);
        memcpy(search_path, walk_iterator->_current_dir, walk_iterator->_current_dir_sz);

        search_path[walk_iterator->_current_dir_sz] = '\\';
        search_path[walk_iterator->_current_dir_sz + 1] = '*';
        search_path[walk_iterator->_current_dir_sz + 2] = '\0';

        walk_iterator->_h_find = FindFirstFileA(search_path, &find_data);

        if(walk_iterator->_h_find == INVALID_HANDLE_VALUE)
        {
            return false;
        }
    }
    else 
    {
        if(!FindNextFileA(walk_iterator->_h_find, &find_data))
            return false;
    }

    while(true)
    {
        if(should_skip_entry(mode, find_data.cFileName, find_data.dwFileAttributes))
        {
            if(!FindNextFileA(walk_iterator->_h_find, &find_data))
                break;

            continue;
        }

        size_t c_file_name_sz = strlen(find_data.cFileName);

        size_t current_path_sz = walk_iterator->_current_dir_sz + 1 + c_file_name_sz + 1;
        char* current_path = (char*)calloc(current_path_sz, sizeof(char));

        snprintf(current_path,
                 current_path_sz,
                 "%s\\%s",
                 walk_iterator->_current_dir,
                 find_data.cFileName);

        current_path[current_path_sz - 1] = '\0';

        bool is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if(is_dir && (mode & FSWalkMode_Recursive))
            vector_push_back(&walk_iterator->_dir_queue, current_path);
    }

#elif defined(ROMANO_LINUX)
#endif /* defined(ROMANO_WIN) */

    return false;
}
