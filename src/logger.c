// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#include "libromano/logger.h"
#include "libromano/bit.h"

#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>

// Log mode

typedef enum 
{
    LogMode_Console = BIT(0),
    LogMode_File = BIT(1)
} log_mode;

// Global variables

static int _logger_initialized = 0;
static int _log_level = LogLevel_Info;
static int _log_mode = LogMode_Console;
static const char* _log_file_path = NULL;
static FILE* _log_file = NULL;

static const char* _levels_as_str[] = { "FATAL", "ERROR", "WARNING", "INFO", "DEBUG" };

// API

void logger_init()
{
    if(_logger_initialized)
    {
        return;
    }

    _logger_initialized = 1;
}

void logger_set_level(log_level level)
{
    _log_level = level;
}

void logger_enable_console()
{
    _log_mode |= LogMode_Console;
}

void logger_disable_console()
{
    _log_mode &= LogMode_Console;
}

void logger_enable_file(const char* file_path)
{
    _log_mode |= LogMode_File;
    _log_file_path = file_path;

    // _log_file = fopen(_log_file_path, "a");
}

void logger_disable_file()
{
    _log_mode &= LogMode_File;
}

void logger_log(log_level level, const char* format, ...)
{
    assert(_logger_initialized);

    if(level <= _log_level)
    {
        time_t raw_time;
        time(&raw_time);

        struct tm* local_time = localtime(&raw_time);

        char buffer[ROMANO_LOG_BUFFER_SIZE];

        va_list args;
        va_start(args, format);
        vsnprintf(buffer, ROMANO_LOG_BUFFER_SIZE, format, args);
        va_end(args);

        if(_log_mode & LogMode_Console)
        {
            FILE* output_stream = stdout;

            if(level == LogLevel_Error || level == LogLevel_Fatal) 
            {
                output_stream = stderr;
            }

            fprintf(output_stream,
                    "[%s] %02d:%02d:%02d : %s\n", 
                    _levels_as_str[level],
                    local_time->tm_hour,
                    local_time->tm_min,
                    local_time->tm_sec,
                    buffer);
        }

        if(_log_mode & LogMode_File)
        {
            _log_file = fopen(_log_file_path, "a");

            assert(_log_file != NULL);

            fprintf(_log_file,
                    "[%s] %02d:%02d:%02d : %s\n",
                    _levels_as_str[level],
                    local_time->tm_hour,
                    local_time->tm_min,
                    local_time->tm_sec,
                    buffer);

            fclose(_log_file);
        }
    }
}

void logger_release()
{
    if(_log_file != NULL)
    {
        // fclose(_log_file);
    }
}