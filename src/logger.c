/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/logger.h"
#include "libromano/bit.h"
#include "libromano/atomic.h"
#include "libromano/thread.h"

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

static volatile int g_logger_lock;
static volatile int g_logger_initialized = 0;
static volatile int g_loglevel = LogLevel_Info;
static volatile int g_log_mode = LogMode_Console;
static const char* g_log_file_path = NULL;
static FILE* g_log_file = NULL;

static const char* const levels_as_str[] = { "FATAL", "ERROR", "WARNING", "INFO", "DEBUG" };

void logger_lock_acquire(void)
{
    while(!atomic_compare_exchange_32((Atomic32*)&g_logger_lock, 1, 0))
        thread_yield();
}

void logger_lock_release(void)
{
    atomic_store_32((Atomic32*)&g_logger_lock, 0);
}

void logger_init(void)
{
    logger_lock_acquire();

    if(g_logger_initialized)
    {
        return;
    }

    g_logger_initialized = 1;

    logger_lock_release();
}

void logger_set_level(const log_level level)
{
    logger_lock_acquire();

    g_loglevel = level < 5 ? level : 4;

    logger_lock_release();
}

void logger_enable_console(void)
{
    logger_lock_acquire();

    g_log_mode |= LogMode_Console;

    logger_lock_release();
}

void logger_disable_console(void)
{
    logger_lock_acquire();

    g_log_mode &= ~LogMode_Console;

    logger_lock_release();
}

void logger_enable_file(const char* file_path)
{
    logger_lock_acquire();

    g_log_mode |= LogMode_File;
    g_log_file_path = file_path;

    /* _log_file = fopen(_log_file_path, "a"); */

    logger_lock_release();
}

void logger_disable_file(void)
{
    logger_lock_acquire();

    g_log_mode &= ~LogMode_File;

    logger_lock_release();
}

void logger_log(log_level level, const char* format, ...)
{
    ROMANO_ASSERT(g_logger_initialized, "Logger has not been initialized");

    logger_lock_acquire();

    level = level < 5 ? level : 4;

    if(level <= g_loglevel)
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

        if(g_log_mode & LogMode_Console)
        {
            FILE* output_stream = stdout;

            if(level == LogLevel_Error || level == LogLevel_Fatal)
            {
                output_stream = stderr;
            }

            fprintf(output_stream,
                    "[%s] %02d:%02d:%02d:%03d : %s\n",
                    levels_as_str[level],
                    local_time->tm_hour,
                    local_time->tm_min,
                    local_time->tm_sec,
                    current_time.tv_usec / 1000,
                    buffer);
        }

        if(g_log_mode & LogMode_File)
        {
            g_log_file = fopen(g_log_file_path, "a");

            ROMANO_ASSERT(g_log_file != NULL, "Log file has not been created");

            fprintf(g_log_file,
                    "[%s] %02d:%02d:%02d:%03d : %s\n",
                    levels_as_str[level],
                    local_time->tm_hour,
                    local_time->tm_min,
                    local_time->tm_sec,
                    current_time.tv_usec / 1000,
                    buffer);

            fclose(g_log_file);
        }
    }

    logger_lock_release();
}

void logger_release(void)
{
    logger_lock_acquire();

    if(g_log_file != NULL)
    {
        /* fclose(_log_file); */
    }

    if(g_log_mode & LogMode_Console)
        fflush(stdout);

    logger_lock_release();
}
