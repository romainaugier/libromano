/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/filesystem.h"
#include "libromano/memory.h"
#include "libromano/vector.h"
#include "libromano/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ROMANO_WIN)
#include <Shlwapi.h>
#include <shellapi.h>
#include <PathCch.h>
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
#include <sys/stat.h>
#if !defined(ROMANO_APPLE)
#include <linux/limits.h>
#endif /* !defined(ROMANO_APPLE) */
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <ftw.h>
#endif /* ROMANO_WIN */

extern ErrorCode g_current_error;

bool fs_file_content_init(FileContent* content,
                          const char* path,
                          bool read_binary)
{
    FILE* file_handle;
    size_t read_sz;

    ROMANO_ASSERT(content != NULL, "NULL file content");

    content->content = NULL;
    content->content_sz = 0;

    file_handle = fopen(path, read_binary ? "rb" : "r");

    if(file_handle == NULL)
    {
        g_current_error = (ErrorCode)error_get_last_from_system();
        return false;
    }

    fseek(file_handle, 0, SEEK_END);
    content->content_sz = ftell(file_handle);
    rewind(file_handle);
    content->content = (char*)calloc(content->content_sz + 1, sizeof(char));

    if(content->content == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        fclose(file_handle);
        return false;
    }

    read_sz = fread(content->content, sizeof(char), content->content_sz, file_handle);

    if(ferror(file_handle) || (read_binary && read_sz != content->content_sz))
    {
        g_current_error = error_get_last_from_system();
        fclose(file_handle);
        fs_file_content_release(content);
        return false;
    }

    /* Text mode can read less than the file size (e.g. \r\n translation on Windows) */
    content->content_sz = read_sz;
    content->content[read_sz] = '\0';

    fclose(file_handle);

    return true;
}

FileContent* fs_file_content_new(const char* path,
                                 bool read_binary)
{
    FileContent* content = (FileContent*)calloc(1, sizeof(FileContent));

    if(content == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

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
    return (bool)PathFileExistsA(path);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    struct stat sb;

    return (bool)(stat(path, &sb) == 0 && (S_ISDIR(sb.st_mode) || S_ISREG(sb.st_mode)));
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */
}

static bool fs_makedir(const char* path)
{
    if(fs_path_exists(path))
        return true;

#if defined(ROMANO_WIN)
    return (bool)CreateDirectoryA(path, NULL);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    return mkdir(path, 0755) == 0 || errno == EEXIST; /* drwxr-xr-x */
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */
}

bool fs_makedirs(const char *path)
{
    char buffer[MAX_PATH];
    size_t path_sz = strlen(path);
    size_t i;

    if(fs_path_exists(path))
        return true;

    if(path_sz == 0 || path_sz >= MAX_PATH)
        return false;

    memcpy(buffer, path, path_sz + 1);

    for(i = 1; i < path_sz; i++)
    {
        if(buffer[i] != '/' && buffer[i] != '\\')
            continue;

        if(buffer[i - 1] == ':' || buffer[i - 1] == '/' || buffer[i - 1] == '\\')
            continue;

        buffer[i] = '\0';

        if(!fs_makedir(buffer))
            return false;

        buffer[i] = path[i];
    }

    return fs_makedir(buffer);
}

size_t fs_parent_dir(const char* path)
{
    size_t path_sz = strlen(path);

    while(path_sz > 0)
    {
        path_sz--;

        if(path[path_sz] == '\\' || path[path_sz] == '/')
            return path_sz == 0 ? 1 : path_sz;
    }

    return 0;
}

char* fs_parent_dir_new(const char* path)
{
    ROMANO_ASSERT(path != NULL, "path is NULL");

    size_t parent_path_sz = fs_parent_dir(path);

    char* parent_path = (char*)malloc((parent_path_sz + 1) * sizeof(char));

    if(parent_path == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    memcpy(parent_path, path, parent_path_sz);

    parent_path[parent_path_sz] = '\0';

    return parent_path;
}

size_t fs_parent_dir_init(const char* path, char* buffer, size_t buffer_size)
{
    ROMANO_ASSERT(path != NULL, "path is NULL");

    size_t parent_path_sz = fs_parent_dir(path);

    if(parent_path_sz >= buffer_size)
        return parent_path_sz + 1;

    memcpy(buffer, path, parent_path_sz);

    buffer[parent_path_sz] = '\0';

    return 0;
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

#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
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

    if(sz == 0)
    {
        g_current_error = error_get_last_from_system();
        return false;
    }

    char* buffer = (char*)calloc(sz + 1, sizeof(char));

    if(buffer == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        *out_path = NULL;
        *out_sz = 0;
        return false;
    }

    DWORD total_sz = GetCurrentDirectoryA(sz + 1, buffer);

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
    {
        g_current_error = error_get_last_from_system();
        return false;
    }

    *out_sz = strlen(*out_path);
#elif defined(ROMANO_APPLE)
    *out_path = getcwd(NULL, MAX_PATH);

    if(*out_path == NULL)
    {
        *out_sz = 0;
        return false;
    }

    *out_sz = strlen(*out_path);
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */

    return true;
}

#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
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
            g_current_error = error_get_last_from_system();
            return false;
        }

        return true;
    }
    else
    {
        if(!DeleteFileA(path))
        {
            g_current_error = error_get_last_from_system();
            return false;
        }

        return true;
    }
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    if(fs_is_dir(path))
    {
        if(nftw(path, fs_remove_callback, 64, FTW_DEPTH | FTW_PHYS) != 0)
        {
            g_current_error = error_get_last_from_system();
            return false;
        }

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
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
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
    const DWORD attributes = GetFileAttributesA(path);

    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    struct stat stats;
    return stat(path, &stats) == 0 && S_ISDIR(stats.st_mode);
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
    const DWORD attributes = GetFileAttributesA(path);

    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    struct stat stats;

    return stat(path, &stats) == 0 && S_ISREG(stats.st_mode);
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */

    return true;
}

bool fs_walk_iterator_init(FSWalkIterator* walk_iterator)
{
    ROMANO_ASSERT(walk_iterator != NULL, "walk_iterator is NULL");

    memset(walk_iterator, 0, sizeof(FSWalkIterator));

#if defined(ROMANO_WIN)
    walk_iterator->_h_find = INVALID_HANDLE_VALUE;
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    walk_iterator->_dir = NULL;
#endif /* defined(ROMANO_WIN) */

    walk_iterator->current_path_capacity = 256;
    walk_iterator->current_path = (char*)calloc(256, sizeof(char));

    if(walk_iterator->current_path == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return false;
    }

    vector_init(&walk_iterator->_dir_queue, 128, sizeof(char*));

    walk_iterator->_first_entry = true;

    return true;
}

FSWalkIterator* fs_walk_iterator_new(void)
{
    FSWalkIterator* item = (FSWalkIterator*)malloc(sizeof(FSWalkIterator));

    if(item == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    if(!fs_walk_iterator_init(item))
    {
        g_current_error = ErrorCode_MemAllocError;
        free(item);
        return NULL;
    }

    return item;
}

void fs_walk_iterator_queue_release_cb(void* data)
{
    ROMANO_ASSERT(data != NULL, "data is NULL");

    free(*(char**)data);
}

void fs_walk_iterator_release(FSWalkIterator* walk_iterator)
{
    ROMANO_ASSERT(walk_iterator != NULL, "walk_iterator is NULL");

    if(walk_iterator->current_path != NULL)
        free(walk_iterator->current_path);

    if(walk_iterator->_current_dir != NULL)
        free(walk_iterator->_current_dir);

#if defined(ROMANO_WIN)
    if(walk_iterator->_h_find != INVALID_HANDLE_VALUE)
    {
        if(!FindClose(walk_iterator->_h_find))
        {
            g_current_error = error_get_last_from_system();
        }
    }

    walk_iterator->_h_find = INVALID_HANDLE_VALUE;
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    if(walk_iterator->_dir != NULL)
        closedir(walk_iterator->_dir);

    walk_iterator->_dir = NULL;
#endif /* defined(ROMANO_WIN) */

    vector_release_with_dtor(&walk_iterator->_dir_queue, fs_walk_iterator_queue_release_cb);
}

void fs_walk_iterator_free(FSWalkIterator* walk_iterator)
{
    ROMANO_ASSERT(walk_iterator != NULL, "walk_iterator is NULL");

    fs_walk_iterator_release(walk_iterator);
    free(walk_iterator);
}

static bool walk_set_current_path(FSWalkIterator* walk_iterator, const char* name, char separator)
{
    const size_t name_sz = strlen(name);
    const size_t current_path_sz = walk_iterator->_current_dir_sz + 1 + name_sz + 1;

    while(current_path_sz > walk_iterator->current_path_capacity)
    {
        char* new_path;

        if(walk_iterator->current_path_capacity > SIZE_MAX / 2)
        {
            g_current_error = ErrorCode_SizeOverflow;
            return false;
        }

        new_path = (char*)realloc(walk_iterator->current_path, walk_iterator->current_path_capacity * 2);

        if(new_path == NULL)
        {
            g_current_error = ErrorCode_MemAllocError;
            return false;
        }

        walk_iterator->current_path = new_path;
        walk_iterator->current_path_capacity *= 2;
    }

    memcpy(walk_iterator->current_path, walk_iterator->_current_dir, walk_iterator->_current_dir_sz);
    walk_iterator->current_path[walk_iterator->_current_dir_sz] = separator;
    memcpy(walk_iterator->current_path + walk_iterator->_current_dir_sz + 1, name, name_sz + 1);
    walk_iterator->current_path_sz = current_path_sz - 1;

    return true;
}

static bool walk_queue_current_path(FSWalkIterator* walk_iterator)
{
    char* dir_path = (char*)malloc(walk_iterator->current_path_sz + 1);

    if(dir_path == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return false;
    }

    memcpy(dir_path, walk_iterator->current_path, walk_iterator->current_path_sz + 1);
    vector_push_back(&walk_iterator->_dir_queue, &dir_path);

    return true;
}

static bool walk_next_directory(FSWalkIterator* walk_iterator)
{
    if(vector_size(&walk_iterator->_dir_queue) == 0)
        return false;

    free(walk_iterator->_current_dir);

    walk_iterator->_current_dir = *(char**)vector_at(&walk_iterator->_dir_queue, 0);
    walk_iterator->_current_dir_sz = strlen(walk_iterator->_current_dir);
    vector_pop_front(&walk_iterator->_dir_queue);

    return true;
}

static bool walk_is_dot_entry(const char* name)
{
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

bool fs_walk(const char* path,
             FSWalkIterator* walk_iterator,
             FSWalkMode mode)
{
    ROMANO_ASSERT(walk_iterator != NULL, "walk_iterator is NULL");

    if(walk_iterator->_first_entry)
    {
        size_t path_sz = strlen(path);
        char* path_copy = calloc(path_sz + 1, sizeof(char));

        if(path_copy == NULL)
        {
            g_current_error = ErrorCode_MemAllocError;
            return false;
        }

        memcpy(path_copy, path, path_sz);
        vector_push_back(&walk_iterator->_dir_queue, &path_copy);

        walk_iterator->_first_entry = false;
    }

#if defined(ROMANO_WIN)
    WIN32_FIND_DATAA find_data;

    while(true)
    {
        bool is_dir;

        if(walk_iterator->_h_find == INVALID_HANDLE_VALUE)
        {
            char* pattern;

            if(!walk_next_directory(walk_iterator))
                return false;

            pattern = (char*)malloc(walk_iterator->_current_dir_sz + 3);

            if(pattern == NULL)
            {
                g_current_error = ErrorCode_MemAllocError;
                return false;
            }

            memcpy(pattern, walk_iterator->_current_dir, walk_iterator->_current_dir_sz);
            memcpy(pattern + walk_iterator->_current_dir_sz, "\\*", 3);

            walk_iterator->_h_find = FindFirstFileA(pattern, &find_data);

            free(pattern);

            if(walk_iterator->_h_find == INVALID_HANDLE_VALUE)
                continue;
        }
        else if(!FindNextFileA(walk_iterator->_h_find, &find_data))
        {
            FindClose(walk_iterator->_h_find);
            walk_iterator->_h_find = INVALID_HANDLE_VALUE;
            continue;
        }

        if(walk_is_dot_entry(find_data.cFileName))
            continue;

        if(!walk_set_current_path(walk_iterator, find_data.cFileName, '\\'))
            return false;

        is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if(is_dir && (mode & FSWalkMode_Recursive) && !walk_queue_current_path(walk_iterator))
            return false;

        if((is_dir && (mode & FSWalkMode_YieldDirs)) || (!is_dir && (mode & FSWalkMode_YieldFiles)))
            return true;
    }
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    struct dirent* entry;

    while(true)
    {
        bool is_dir;
        bool is_file;

        if(walk_iterator->_dir == NULL)
        {
            if(!walk_next_directory(walk_iterator))
                return false;

            walk_iterator->_dir = opendir(walk_iterator->_current_dir);

            if(walk_iterator->_dir == NULL)
                continue;
        }

        entry = readdir(walk_iterator->_dir);

        if(entry == NULL)
        {
            closedir(walk_iterator->_dir);
            walk_iterator->_dir = NULL;
            continue;
        }

        if(walk_is_dot_entry(entry->d_name))
            continue;

        if(!walk_set_current_path(walk_iterator, entry->d_name, '/'))
            return false;

        is_dir = entry->d_type == DT_UNKNOWN ? fs_is_dir(walk_iterator->current_path) : entry->d_type == DT_DIR;
        is_file = entry->d_type == DT_UNKNOWN ? fs_is_file(walk_iterator->current_path) : entry->d_type == DT_REG;

        if(is_dir && (mode & FSWalkMode_Recursive) && !walk_queue_current_path(walk_iterator))
            return false;

        if((is_dir && (mode & FSWalkMode_YieldDirs)) || (is_file && (mode & FSWalkMode_YieldFiles)))
            return true;
    }
#else
#error "Unsupported platform"
#endif /* defined(ROMANO_WIN) */
}
