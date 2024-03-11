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
    size_t id;

    while(!atomic_compare_exchange_32((atomic32_t*)data, 2, 1))
    {
        id = thread_get_id();
    }

    logger_log(LogLevel_Info, "Atomic set to 2 from tid: %zu", id);

    return NULL;
}

void* cmpxchg_func2(void* data)
{
    thread_sleep(250);

    atomic_compare_exchange_32((atomic32_t*)data, 1, 0);

    logger_log(LogLevel_Info, "Atomic set to 1");

    return NULL;
}

void* xchg_func(void* data)
{
    size_t i;

    thread_sleep(10);

    for(i = 0; i < 1000; i++)
    {
        atomic_exchange_32((atomic32_t*)data, i);
    }

    return NULL;
}

int main(void)
{
    atomic32_t counter = 0;
    atomic32_t cmpxchg = 0;
    atomic32_t xchg = 0;

    logger_init();

    logger_log(LogLevel_Info, "Starting atomics tests");

    thread_t* t1 = thread_create(func, (void*)&counter);
    thread_t* t2 = thread_create(func, (void*)&counter);

    thread_start(t1);
    thread_start(t2);

    thread_join(t1);
    thread_join(t2);

    logger_log(LogLevel_Info, "Counter: %d", counter);

    logger_log(LogLevel_Info, "Testing compare exchange atomics");

    thread_t* t3 = thread_create(cmpxchg_func1, (void*)&cmpxchg);
    thread_t* t4 = thread_create(cmpxchg_func2, (void*)&cmpxchg);

    thread_start(t3);
    thread_start(t4);

    thread_join(t3);
    thread_join(t4);

    logger_log(LogLevel_Info, "Testing exchange atomics");

    thread_t* t5 = thread_create(xchg_func, (void*)&xchg);
    thread_t* t6 = thread_create(xchg_func, (void*)&xchg);

    thread_start(t5);
    thread_start(t6);

    thread_join(t5);
    thread_join(t6);

    logger_log(LogLevel_Info, "Finished atomics test");

    logger_release();

    return 0;
}