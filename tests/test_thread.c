// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#include "libromano/thread.h"
#include "libromano/logger.h"

#include <stdio.h>

void t1_func(void* data)
{
    logger_log(LogLevel_Info, "Hello from thread 1");

    logger_log(LogLevel_Info, "Thread 1 sleeping 2 seconds");

    thread_sleep(2000);

    logger_log(LogLevel_Info, "Thread 1 joining");
}

void t2_func(void* data)
{
    logger_log(LogLevel_Info, "Hello from thread 2");

    logger_log(LogLevel_Info, "Thread 2 sleeping 2 seconds");

    thread_sleep(2000);

    logger_log(LogLevel_Info, "Thread 2 joining");
}

int main(int argc, char** argv)
{
    logger_init();

    logger_log(LogLevel_Info, "Creating threads");
    thread* t1 = thread_create(t1_func, NULL);
    thread* t2 = thread_create(t2_func, NULL);

    logger_log(LogLevel_Info, "Starting threads");
    thread_start(t1);
    thread_start(t2);

    logger_log(LogLevel_Info, "Joining threads");
    thread_join(t1);
    thread_join(t2);

    logger_release();

    return 0;
}