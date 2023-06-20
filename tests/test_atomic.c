/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/atomic.h"
#include "libromano/thread.h"
#include "libromano/logger.h"

void* func(void* data)
{
    size_t i;

    for(i = 0; i < 10000; i++)
    {
        atomic_add_32((atomic32_t*)data, 1);
    }

    return NULL;
}

void* cmpxchg_func1(void* data)
{
    while(!atomic_compare_exchange_32((atomic32_t*)data, 2, 1))
    {
        logger_log(LogLevel_Info, "Sleeping 10ms");
        thread_sleep(10);
    }

    logger_log(LogLevel_Info, "Atomic set to 2");

    return NULL;
}

void* cmpxchg_func2(void* data)
{
    thread_sleep(250);

    atomic_compare_exchange_32((atomic32_t*)data, 1, 0);

    logger_log(LogLevel_Info, "Atomic set to 1");

    return NULL;
}

int main(void)
{
    atomic32_t counter = 0;
    atomic32_t cmpxchg = 0;

    logger_init();

    thread_t* t1 = thread_create(func, (void*)&counter);
    thread_t* t2 = thread_create(func, (void*)&counter);

    thread_start(t1);
    thread_start(t2);

    thread_join(t1);
    thread_join(t2);

    logger_log(LogLevel_Info, "Counter: %d", counter);

    thread_t* t3 = thread_create(cmpxchg_func1, (void*)&cmpxchg);
    thread_t* t4 = thread_create(cmpxchg_func2, (void*)&cmpxchg);

    thread_start(t3);
    thread_start(t4);

    thread_join(t3);
    thread_join(t4);

    logger_release();

    return 0;
}