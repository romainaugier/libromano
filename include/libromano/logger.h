/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_LOGGER)
#define __LIBROMANO_LOGGER

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

#define ROMANO_LOG_BUFFER_SIZE 1024

typedef enum 
{
    LogLevel_Fatal,
    LogLevel_Error,
    LogLevel_Warning,
    LogLevel_Info,
    LogLevel_Debug
} log_level;

/* Initializes the logger. Be careful to release the logger once your program ends, otherwise file logging won't end */
/* Best practice is to register the logger_release at exit */
ROMANO_API void logger_init(void);

/* Sets the log level */
ROMANO_API void logger_set_level(const log_level level);

/* Enable console logging, which is enabled by default */
ROMANO_API void logger_enable_console(void);

/* Disable console logging, which is enable by default */
ROMANO_API void logger_disable_console(void);

/* Sets the file the logger should output to, and enables file logging */
ROMANO_API void logger_enable_file(const char* file_path);

/* Disable file logging */
ROMANO_API void logger_disable_file(void);

/* Logs a message */
ROMANO_API void logger_log(const log_level level, const char* format, ...);

/* Releases the logger */
ROMANO_API void logger_release(void);

ROMANO_CPP_END

#endif /* __LIBROMANO_LOGGER */

