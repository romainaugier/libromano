/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/thread.h"
#include "libromano/logger.h"
#include "libromano/profiling.h"

#include <stdio.h>
#include <stdlib.h>

#define WORK_COUNT 100

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

void tpool_func(void* data)
{
    int work_id = *(int*)data;

    logger_log(LogLevel_Info, "Hello from threadpool thread %llu and work id %i", thread_get_id(), work_id);

    thread_sleep(1);
}

int main(int argc, char** argv)
{
    size_t i = 0;

    logger_init();

    logger_log(LogLevel_Info, "Creating threads");
    thread_t* t1 = thread_create(t1_func, NULL);
    thread_t* t2 = thread_create(t2_func, NULL);

    logger_log(LogLevel_Info, "Starting threads");
    thread_start(t1);
    thread_start(t2);

    logger_log(LogLevel_Info, "Joining threads");
    thread_join(t1);
    thread_join(t2);

    logger_log(LogLevel_Info, "Initializing threadpool");
    
    threadpool_t* tp = threadpool_init(0);

    int* work_data = malloc(sizeof(int) * WORK_COUNT);

    logger_log(LogLevel_Info, "Adding work to the threadpool");

    for(i = 0; i < WORK_COUNT; i++)
    {
        work_data[i] = (int)i;

        PROFILE(threadpool_work_add(tp, tpool_func, (void*)&work_data[i]););
    }

    logger_log(LogLevel_Info, "Waiting for threadpool to complete work");

    threadpool_wait(tp);

    free(work_data);

    logger_log(LogLevel_Info, "Releasing threadpool");

    threadpool_release(tp);

    logger_release();

    return 0;
}

