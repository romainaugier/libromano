/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/logger.h"
#include "libromano/bit.h"

#if defined(ROMANO_WIN)
#include "libromano/time.h"
#elif defined(ROMANO_LINUX)
#include <sys/time.h>
#endif /* defined(ROMANO_WIN) */

#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>

typedef enum 
{
    LogMode_Console = BIT(0),
    LogMode_File = BIT(1)
} log_mode;

static int _logger_initialized = 0;
static int _loglevel = LogLevel_Info;
static int _log_mode = LogMode_Console;
static const char* _log_file_path = NULL;
static FILE* _log_file = NULL;

static const char* const levels_as_str[] = { "FATAL", "ERROR", "WARNING", "INFO", "DEBUG" };

void logger_init(void)
{
    if(_logger_initialized)
    {
        return;
    }

    _logger_initialized = 1;
}

void logger_set_level(const log_level level)
{
    _loglevel = level < 5 ? level : 4;

}

void logger_enable_console(void)
{
    _log_mode |= LogMode_Console;
}

void logger_disable_console(void)
{
    _log_mode &= LogMode_Console;
}

void logger_enable_file(const char* file_path)
{
    _log_mode |= LogMode_File;
    _log_file_path = file_path;

    /* _log_file = fopen(_log_file_path, "a"); */
}

void logger_disable_file(void)
{
    _log_mode &= LogMode_File;
}

void logger_log(log_level level, const char* format, ...)
{
    assert(_logger_initialized);

    level = level < 5 ? level : 4;

    if(level <= _loglevel)
    {
        time_t raw_time;
        struct timeval current_time;
        struct tm* local_time;
        char buffer[ROMANO_LOG_BUFFER_SIZE];
        va_list args;
        
        time(&raw_time);

        gettimeofday(&current_time, NULL);

        local_time = localtime(&raw_time);

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
                    "[%s] %02d:%02d:%02d:%03ld : %s\n", 
                    levels_as_str[level],
                    local_time->tm_hour,
                    local_time->tm_min,
                    local_time->tm_sec,
                    current_time.tv_usec / 1000,
                    buffer);
        }

        if(_log_mode & LogMode_File)
        {
            _log_file = fopen(_log_file_path, "a");

            assert(_log_file != NULL);

            fprintf(_log_file,
                    "[%s] %02d:%02d:%02d:%03ld : %s\n",
                    levels_as_str[level],
                    local_time->tm_hour,
                    local_time->tm_min,
                    local_time->tm_sec,
                    current_time.tv_usec / 1000,
                    buffer);

            fclose(_log_file);
        }
    }
}

void logger_release(void)
{
    if(_log_file != NULL)
    {
        /* fclose(_log_file); */
    }
}

