/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/filesystem.h"
#include "libromano/logger.h"

#include <string.h>

int main(void)
{
    logger_init();
    logger_set_level(LogLevel_Debug);

    logger_log(LogLevel_Info, "Verifying the existence of this file : "__FILE__);
    int exists;

    exists = fs_path_exists(__FILE__);

    logger_log(LogLevel_Info, "File \""__FILE__"\" exists : %d", exists);

    FileContent content;
    fs_file_content_init(&content, __FILE__, false);

    printf(__FILE__"\ncontent:\n%.*s\n", (int)content.content_sz, content.content);

    fs_file_content_release(&content);

    size_t dir_sz = fs_parent_dir(__FILE__);

    char dir_path[MAX_PATH];
    memset(dir_path, 0, MAX_PATH * sizeof(char));
    memcpy(dir_path, __FILE__, dir_sz);

    logger_log(LogLevel_Info, "Directory of "__FILE__" is %.*s", dir_sz, dir_path);

    logger_log(LogLevel_Info, "Starting filesystem walk");

    char walk_dir_path[MAX_PATH];
    size_t walk_dir_path_sz = fs_parent_dir(dir_path);

    memcpy(walk_dir_path, dir_path, walk_dir_path_sz * sizeof(char));
    walk_dir_path[walk_dir_path_sz] = '\0';

    FSWalkIterator* walk_iterator = fs_walk_iterator_new();

    while(fs_walk(walk_dir_path, walk_iterator, FSWalkMode_Recursive | 
                                                FSWalkMode_YieldFiles | 
                                                FSWalkMode_YieldDirs))
    {
        logger_log(LogLevel_Info, "%s", walk_iterator->current_path);
    }

    fs_walk_iterator_free(walk_iterator);

    logger_log(LogLevel_Info, "Finished filesystem walk");

    logger_release();

    return 0;
}

