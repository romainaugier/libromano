/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/logger.h"

int main(int argc, char** argv)
{
    logger_init();

    logger_set_level(LogLevel_Debug);
    logger_enable_file("./test_log_file.txt");

    logger_log(LogLevel_Debug, "Debug message !!");
    logger_log(LogLevel_Info, "Info message !!");
    logger_log(LogLevel_Warning, "Warning message !!");
    logger_log(LogLevel_Error, "Error message !!");
    logger_log(LogLevel_Fatal, "Fatal message !!");

    logger_release();
    
    return 0;
}

