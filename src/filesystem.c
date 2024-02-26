/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/filesystem.h"
#include "libromano/vector.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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
    wchar_t w_path[MAX_PATH + 1];
#elif defined(ROMANO_LINUX)
    unsigned long path_len = strlen(path);
#endif /* defined(ROMANO_WIN)*/

    assert(out_path != NULL);

#if defined(ROMANO_WIN)
    MultiByteToWideChar(CP_UTF8, 0, path, (int)strlen(path), w_path, MAX_PATH);
    w_path[MAX_PATH] = L'\0';

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


fs_walk_item_t* fs_walk_item_new(const char* path)
{
    size_t path_length;
    fs_walk_item_t* item;
    
    item = (fs_walk_item_t*)malloc(sizeof(fs_walk_item_t));

    if(path != NULL)
    {
        path_length = strlen(path);

        item->path = (char*)malloc(path_length * sizeof(char));
        memcpy(item->path, path, path_length * sizeof(char));
        item->path_length = path_length;
    }
    else
    {
        item->path = NULL;
        item->path_length = 0;
    }

    return item;
}


void fs_walk_item_free(fs_walk_item_t* walk_item)
{
    if(walk_item->path != NULL)
    {
        free(walk_item->path);
    }

    free(walk_item);
}


typedef struct fs_walk_data {
#if defined(ROMANO_WIN)
    WIN32_FIND_DATAA find_data;
    HANDLE h_find;
#elif defined(ROMANO_LINUX)
#endif /* defined(ROMANO_WIN) */
    vector dir_queue;
} fs_walk_data;

static volatile fs_walk_data* search_data = NULL;

fs_walk_data* fs_walk_data_new(void)
{
    fs_walk_data* tmp_search_data;
    
    tmp_search_data = (fs_walk_data*)malloc(sizeof(fs_walk_data));
#if defined(ROMANO_WIN)
    tmp_search_data->h_find = INVALID_HANDLE_VALUE;
#endif /* defined(ROMANO_WIN) */
    tmp_search_data->dir_queue = vector_new(128, sizeof(char*));

    return tmp_search_data;
}

void fs_walk_data_free(fs_walk_data* walk_data)
{
    size_t i;

    for(i = 0; i < vector_size(walk_data->dir_queue); i++) 
    { 
        free(*(void**)vector_at(walk_data->dir_queue, i)); 
    };

    vector_free(walk_data->dir_queue);

    free(walk_data);
}

void fs_walk_push_parent_dir(fs_walk_data* data)
{
#if defined(ROMANO_WIN)
    char* parent_dir_path;
    int parent_dir_path_length;
    size_t n;
    char* dir_path;
    
    parent_dir_path = *(char**)vector_at(data->dir_queue, 0);
    parent_dir_path_length = strlen(parent_dir_path) - 3;

    n = strlen(data->find_data.cFileName);
    dir_path = (char*)malloc((parent_dir_path_length + n + 5) * sizeof(char));
    memcpy(dir_path, parent_dir_path, parent_dir_path_length);
    dir_path[parent_dir_path_length] = '\\';
    memcpy(dir_path + parent_dir_path_length + 1, data->find_data.cFileName, n);

    dir_path[n + parent_dir_path_length + 1] = '\\';
    dir_path[n + parent_dir_path_length + 2] = '\\';
    dir_path[n + parent_dir_path_length + 3] = '*';
    dir_path[n + parent_dir_path_length + 4] = '\0';
    
    vector_push_back(&data->dir_queue, &dir_path);
#endif /* defined(ROMANO_WIN) */
}

void fs_walk_push_file(fs_walk_data* data, fs_walk_item_t* item)
{
#if defined(ROMANO_WIN)
    char* parent_dir_path;
    int parent_dir_path_length;
    size_t n;
    char* file_path;

    parent_dir_path = *(char**)vector_at(data->dir_queue, 0);
    parent_dir_path_length = strlen(parent_dir_path) - 3;

    n = strlen(data->find_data.cFileName);
    file_path = (char*)malloc((parent_dir_path_length + n + 2) * sizeof(char));
    memcpy(file_path, parent_dir_path, parent_dir_path_length);
    file_path[parent_dir_path_length] = '\\';
    memcpy(file_path + parent_dir_path_length + 1, data->find_data.cFileName, n);
    file_path[parent_dir_path_length + n + 1] = '\0';

    item->path = file_path;
    item->path_length = parent_dir_path_length + 2; 
#endif /* defined(ROMANO_WIN) */
}

int fs_walk(const char* path,
            fs_walk_item_t* walk_item,
            const fs_walk_mode mode)
{
    assert(walk_item != NULL);
#if defined(ROMANO_WIN)
    if(search_data == NULL)
    {
        const size_t path_length = strlen(path);

        char* dir_path = (char*)malloc(path_length + 4 * sizeof(char));
        memcpy(dir_path, path, path_length * sizeof(char));

        dir_path[path_length] = '\\';
        dir_path[path_length + 1] = '\\';
        dir_path[path_length + 2] = '*';
        dir_path[path_length + 3] = '\0';

        search_data = fs_walk_data_new();
        
        vector_push_back(&search_data->dir_queue, &dir_path);
        
        search_data->h_find = FindFirstFileA(*(char**)vector_at(search_data->dir_queue, 0), &search_data->find_data);

        if(search_data->h_find == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();

            printf("Error during fs_walk. Error code : %d\n", err);
            
            fs_walk_data_free(search_data);
            search_data = NULL;
            return 0;
        }

        if(search_data->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if(search_data->find_data.cFileName[0] != '.')
            {
                fs_walk_push_parent_dir(search_data);
            }
        }
        else
        {
            fs_walk_push_file(search_data, walk_item);
            return 1;
        }
    }

    while(1)
    {
        if(FindNextFileA(search_data->h_find, &search_data->find_data) != 0)
        {
            if(search_data->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if(search_data->find_data.cFileName[0] != '.')
                {
                    fs_walk_push_parent_dir(search_data);
                }
            }
            else
            {
                fs_walk_push_file(search_data, walk_item);
                return 1;
            }
        }
        else
        {
            DWORD err = GetLastError();

            if(err != ERROR_NO_MORE_FILES)
            {
                printf("Error code : %d\n", err);

                fs_walk_data_free(search_data);
                search_data = NULL;
                return 0;
            }

            vector_remove(&search_data->dir_queue, 0);

            if(vector_size(search_data->dir_queue) == 0) return 0;

            search_data->h_find = FindFirstFileA(*(char**)vector_at(search_data->dir_queue, 0), &search_data->find_data);

            if(search_data->h_find == INVALID_HANDLE_VALUE)
            {
                DWORD err = GetLastError();

                printf("Error during fs walk. Error code : %d\n", err);
            
                fs_walk_data_free(search_data);
                search_data = NULL;
                return 0;
            }

            if(search_data->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if(search_data->find_data.cFileName[0] != '.')
                {
                    fs_walk_push_parent_dir(search_data);
                }
            }
            else
            {
                fs_walk_push_file(search_data, walk_item);
                return 1;
            }
        }
    }

#elif defined(ROMANO_LINUX)
    return 0;
#endif /* defined(ROMANO_WIN) */
}
