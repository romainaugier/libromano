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
        atomic_add_32((Atomic32*)data, 1);
    }

    return NULL;
}

void* cmpxchg_func1(void* data)
{
    size_t id;
    id = thread_get_id();

    while(1)
    {
        if(atomic_compare_exchange_32((Atomic32*)data, 2, 1))
        {
            logger_log(LogLevel_Info, "Atomic set to 2 from tid: %zu", id);
            break;
        }
    }

    

    return NULL;
}

void* cmpxchg_func2(void* data)
{
    size_t id;
    id = thread_get_id();

    thread_sleep(250);

    atomic_compare_exchange_32((Atomic32*)data, 1, 0);
    logger_log(LogLevel_Info, "Atomic set to 1 from tid: %zu", id);

    while(1)
    {
        if(atomic_compare_exchange_32((Atomic32*)data, 3, 2))
        {
            logger_log(LogLevel_Info, "Atomic set to 3 from tid: %zu", id);
            break;
        }
    }

    return NULL;
}

void* xchg_func(void* data)
{
    size_t i;

    thread_sleep(10);

    for(i = 0; i < 1000; i++)
    {
        atomic_exchange_32((Atomic32*)data, i);
    }

    return NULL;
}

int main(void)
{
    Atomic32 counter = 0;
    Atomic32 cmpxchg = 0;
    Atomic32 xchg = 0;

    logger_init();

    logger_log(LogLevel_Info, "Starting atomics tests");

    Thread* t1 = thread_create(func, (void*)&counter);
    Thread* t2 = thread_create(func, (void*)&counter);

    thread_start(t1);
    thread_start(t2);

    thread_join(t1);
    thread_join(t2);

    logger_log(LogLevel_Info, "Counter: %d", counter);

    logger_log(LogLevel_Info, "Testing compare exchange atomics");

    Thread* t3 = thread_create(cmpxchg_func1, (void*)&cmpxchg);
    Thread* t4 = thread_create(cmpxchg_func2, (void*)&cmpxchg);

    thread_start(t3);
    thread_start(t4);

    thread_join(t3);
    thread_join(t4);

    logger_log(LogLevel_Info, "Testing exchange atomics");

    Thread* t5 = thread_create(xchg_func, (void*)&xchg);
    Thread* t6 = thread_create(xchg_func, (void*)&xchg);

    thread_start(t5);
    thread_start(t6);

    thread_join(t5);
    thread_join(t6);

    logger_log(LogLevel_Info, "Finished atomics test");

    logger_release();

    return 0;
}