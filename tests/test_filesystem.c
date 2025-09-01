/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/filesystem.h"
#include "libromano/logger.h"

int main(void)
{
    FSWalkItem* walk_item;

    logger_init();
    logger_set_level(LogLevel_Debug);

    logger_log(LogLevel_Info, "Verifying the existence of this file : "__FILE__);
    int exists;

    exists = fs_path_exists(__FILE__);

    logger_log(LogLevel_Info, "File \""__FILE__"\" exists : %d", exists);

    FileContent content;
    fs_file_content_new(__FILE__, &content);

    printf(__FILE__"\ncontent:\n%.*s\n", (int)content.content_length, content.content);

    fs_file_content_free(&content);

    char dir_path[MAX_PATH];
    fs_parent_dir(__FILE__, dir_path);

    logger_log(LogLevel_Info, "Directory of "__FILE__" is %s", dir_path);

    logger_log(LogLevel_Info, "Starting filesystem walk");

    char walk_dir_path[MAX_PATH];
    fs_parent_dir(dir_path, walk_dir_path);

    walk_item = fs_walk_item_new(NULL);

    while(fs_walk(walk_dir_path, walk_item, 0) != 0)
    {
        logger_log(LogLevel_Info, "%s", walk_item->path);
    }

    fs_walk_item_free(walk_item);

    logger_log(LogLevel_Info, "Finished first filesystem walk");

    walk_item = fs_walk_item_new(NULL);

    while(fs_walk(walk_dir_path, walk_item, 0) != 0)
    {
        logger_log(LogLevel_Info, "%s", walk_item->path);
    }

    fs_walk_item_free(walk_item);

    logger_log(LogLevel_Info, "Finished filesystem walk");

    logger_release();

    return 0;
}

