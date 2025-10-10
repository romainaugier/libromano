/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/filesystem.h"
#include "libromano/memory.h"
#include "libromano/vector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ROMANO_WIN)
#include <Shlwapi.h>
#include <shellapi.h>
#include <PathCch.h>
#elif defined(ROMANO_LINUX)
#include <sys/stat.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <ftw.h>
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

    while(path_sz > 0)
    {
        char current = path[path_sz--];

        if(current == '\\' || current == '/')
        {
            break;
        }
    }

    return path_sz + 1;
}

char* fs_parent_dir_new(const char* path)
{
    ROMANO_ASSERT(path != NULL, "path is NULL");

    size_t parent_path_sz = fs_parent_dir(path);

    char* parent_path = (char*)malloc((parent_path_sz + 1) * sizeof(char));

    if(parent_path == NULL)
        return NULL;

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
    memset(walk_iterator, 0, sizeof(FSWalkIterator));

#if defined(ROMANO_WIN)
    walk_iterator->_h_find = INVALID_HANDLE_VALUE;
#elif defined(ROMANO_LINUX)
    walk_iterator->_dir = NULL;
#endif /* defined(ROMANO_WIN) */

    walk_iterator->current_path_capacity = 256;
    walk_iterator->current_path = (char*)calloc(256, sizeof(char));

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
    free(*(char**)data);
}

void fs_walk_iterator_release(FSWalkIterator* walk_iterator)
{
    if(walk_iterator->current_path != NULL)
        free(walk_iterator->current_path);

    if(walk_iterator->_current_dir != NULL)
        free(walk_iterator->_current_dir);

#if defined(ROMANO_WIN)
    if(walk_iterator->_h_find != INVALID_HANDLE_VALUE)
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
bool walk_should_skip_entry(FSWalkMode mode,
                            const char* entry_name,
                            DWORD attrs)
{
    if((attrs & FILE_ATTRIBUTE_DIRECTORY) && (mode & FSWalkMode_YieldDirs) == 0) 
        return true;

    if((attrs & ~FILE_ATTRIBUTE_DIRECTORY) && (mode & FSWalkMode_YieldFiles) == 0) 
        return true;

    if(strcmp(entry_name, ".") == 0 || strcmp(entry_name, "..") == 0)
        return true;

    return false;
}
#elif defined(ROMANO_LINUX)
bool walk_should_skip_entry(FSWalkMode mode,
                            const char* entry_name,
                            unsigned char entry_type /* struct dirent->d_type */)
{
    /* TODO: maybe handle DT_UNKNOWN correctly ? */

    switch(entry_type)
    {
        case DT_REG:
            return mode & FSWalkMode_YieldFiles;
        case DT_DIR:
            return mode & FSWalkMode_YieldDirs;
        default:
            return true;
    }

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

    if(walk_iterator->_first_entry)
    {
        size_t path_sz = strlen(path);
        char* path_copy = calloc(path_sz + 1, sizeof(char));
        memcpy(path_copy, path, path_sz);

        vector_push_back(&walk_iterator->_dir_queue, &path_copy);

        walk_iterator->_first_entry = false;
    }

#if defined(ROMANO_WIN)
    WIN32_FIND_DATAA find_data;

    while(true)
    {
        if(walk_iterator->_h_find == INVALID_HANDLE_VALUE)
        {
            if(vector_size(&walk_iterator->_dir_queue) == 0)
                return false;

            char* search_path = *(char**)vector_at(&walk_iterator->_dir_queue, 0);
            vector_pop_front(&walk_iterator->_dir_queue);

            size_t search_path_sz = strlen(search_path);

            search_path = realloc(search_path, search_path_sz + 3);

            if(search_path == NULL)
                return false;

            search_path[search_path_sz] = '\\';
            search_path[search_path_sz + 1] = '*';
            search_path[search_path_sz + 2] = '\0';

            walk_iterator->_h_find = FindFirstFileA(search_path, &find_data);

            if(walk_iterator->_h_find == INVALID_HANDLE_VALUE)
                return false;

            walk_iterator->_current_dir = search_path;
            walk_iterator->_current_dir_sz = search_path_sz;

            break;
        }
        else 
        {
            if(!FindNextFileA(walk_iterator->_h_find, &find_data))
            {
                FindClose(walk_iterator->_h_find);
                walk_iterator->_h_find = INVALID_HANDLE_VALUE;

                DWORD err = GetLastError();

                if(err != ERROR_NO_MORE_FILES)
                {
                    return false;
                }
            }
            else
            {
                break;
            }
        }
    }

    while(true)
    {
        if(walk_should_skip_entry(mode, find_data.cFileName, find_data.dwFileAttributes))
        {
            if(!FindNextFileA(walk_iterator->_h_find, &find_data))
                break;

            continue;
        }

        size_t c_file_name_sz = strlen(find_data.cFileName);

        size_t current_path_sz = walk_iterator->_current_dir_sz + 1 + c_file_name_sz + 1;

        if(current_path_sz > walk_iterator->current_path_capacity)
        {
            walk_iterator->current_path_capacity <<= 1;
            walk_iterator->current_path = realloc(walk_iterator->current_path,
                                                  walk_iterator->current_path_capacity);

            if(walk_iterator->current_path == NULL)
                return false;
        }

        snprintf(walk_iterator->current_path,
                 current_path_sz,
                 "%.*s\\%s",
                 (int)walk_iterator->_current_dir_sz,
                 walk_iterator->_current_dir,
                 find_data.cFileName);

        walk_iterator->current_path[current_path_sz - 1] = '\0';

        bool is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if(is_dir && (mode & FSWalkMode_Recursive))
        {
            char* dir_path = (char*)calloc(current_path_sz, sizeof(char));
            memcpy(dir_path, walk_iterator->current_path, current_path_sz * sizeof(char));
            vector_push_back(&walk_iterator->_dir_queue, &dir_path);
        }

        return true;
    }
#elif defined(ROMANO_LINUX)
    struct dirent* entry;

    while(true)
    {
        if(walk_iterator->_dir == NULL)
        {
            if(vector_size(&walk_iterator->_dir_queue) == 0)
                return false;

            char* search_path = *(char**)vector_at(&walk_iterator->_dir_queue, 0);
            vector_pop_front(&walk_iterator->_dir_queue);

            size_t search_path_sz = strlen(search_path);

            walk_iterator->_dir = opendir(search_path);

            if(walk_iterator->_dir == NULL)
            {
                return false;
            }

            walk_iterator->_current_dir = search_path;
            walk_iterator->_current_dir_sz = search_path_sz;

            break;
        }
        else
        {
            int old_errno = errno;

            entry = readdir(walk_iterator->_dir);

            if(entry == NULL)
            {
                closedir(walk_iterator->_dir);

                if(old_errno != errno)
                    return false;
            }
            else
            {
                break;
            }
        }
    }

    while(true)
    {
        if(walk_should_skip_entry(mode, entry->d_name, entry->d_type))
        {
            entry = readdir(walk_iterator->_dir);

            if(entry == NULL)
                break;

            continue;
        }

        size_t c_file_name_sz = strlen(entry->d_name);

        size_t current_path_sz = walk_iterator->_current_dir_sz + 1 + c_file_name_sz + 1;

        if(current_path_sz > walk_iterator->current_path_capacity)
        {
            walk_iterator->current_path_capacity <<= 1;
            walk_iterator->current_path = realloc(walk_iterator->current_path,
                                                  walk_iterator->current_path_capacity);

            if(walk_iterator->current_path == NULL)
                return false;
        }

        snprintf(walk_iterator->current_path,
                 current_path_sz,
                 "%.*s\\%s",
                 (int)walk_iterator->_current_dir_sz,
                 walk_iterator->_current_dir,
                 find_data.cFileName);

        walk_iterator->current_path[current_path_sz - 1] = '\0';

        bool is_dir = entry->d_type == DT_UNKNOWN ? fs_is_dir(walk_iterator->current_path) :
                                                    entry->d_type == DT_DIR;

        if(is_dir && (mode & FSWalkMode_Recursive))
        {
            char* dir_path = (char*)calloc(current_path_sz, sizeof(char));
            memcpy(dir_path, walk_iterator->current_path, current_path_sz * sizeof(char));
            vector_push_back(&walk_iterator->_dir_queue, &dir_path);
        }

        return true;
    }

#endif /* defined(ROMANO_WIN) */

    return false;
}
