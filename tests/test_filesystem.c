  /* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/filesystem.h"
#include "libromano/logger.h"

int main(int argc, char** argv)
{
    logger_init();

    logger_log(LogLevel_Info, "Verifying the existence of this file : "__FILE__);
    int exists;

    exists = fs_path_exists(__FILE__);

    logger_log(LogLevel_Info, "File \""__FILE__"\" exists : %d", exists);

    char dir_path[MAX_PATH];
    fs_parent_dir(__FILE__, dir_path);

    logger_log(LogLevel_Info, "Directory of "__FILE__" is %s", dir_path);

    logger_release();

    return 0;
}

