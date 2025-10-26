/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/thread.h"
#include "libromano/atomic.h"
#include "libromano/vector.h"
#include "libromano/error.h"

#include "concurrentqueue/concurrentqueue.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#if defined(ROMANO_WIN)
typedef HANDLE thread_handle;
typedef DWORD thread_id;
#include <processthreadsapi.h>
#elif defined(ROMANO_LINUX)
typedef pthread_t thread_handle;
typedef int thread_id;
#include <sched.h>
#endif /* defined(ROMANO_WIN) */

extern ErrorCode g_current_error;

size_t get_num_procs(void)
{
#if defined(ROMANO_WIN)
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    return (size_t)sys_info.dwNumberOfProcessors;
#elif defined(ROMANO_LINUX)
    return (size_t)sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

Mutex* mutex_new(void)
{
    Mutex* new_mutex = malloc(sizeof(Mutex));

#if defined(ROMANO_WIN)
    InitializeCriticalSection(new_mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_init(new_mutex, NULL);
#endif /* defined(ROMANO_WIN) */

    return new_mutex;
}

void mutex_init(Mutex* mutex)
{
#if defined(ROMANO_WIN)
    InitializeCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_init(mutex, NULL);
#endif /* defined(ROMANO_WIN) */
}


void mutex_lock(Mutex* mutex)
{
    ROMANO_ASSERT(mutex != NULL, "Mutex has not been initialized");

#if defined(ROMANO_WIN)
    EnterCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_lock(mutex);
#endif /* defined(ROMANO_WIN) */
}

void mutex_unlock(Mutex* mutex)
{
    ROMANO_ASSERT(mutex != NULL, "Mutex has not been initialized");

#if defined(ROMANO_WIN)
    LeaveCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_unlock(mutex);
#endif /* defined(ROMANO_WIN) */
}

void mutex_release(Mutex* mutex)
{
    ROMANO_ASSERT(mutex != NULL, "Mutex has not been initialized");

#if defined(ROMANO_WIN)
    DeleteCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_destroy(mutex);
#endif /* defined(ROMANO_WIN) */
}

void mutex_free(Mutex* mutex)
{
    ROMANO_ASSERT(mutex != NULL, "Mutex has not been initialized");

#if defined(ROMANO_WIN)
    DeleteCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_destroy(mutex);
#endif /* defined(ROMANO_WIN) */

    free(mutex);
}

ConditionalVariable* ConditionalVariable_new(void)
{
    ConditionalVariable* new_cond_var = malloc(sizeof(ConditionalVariable));

#if defined(ROMANO_WIN)
    InitializeConditionVariable(new_cond_var);
#elif defined(ROMANO_LINUX)
    pthread_cond_init(new_cond_var, NULL);
#endif /* defined(ROMANO_WIN) */

    return new_cond_var;
}

void ConditionalVariable_init(ConditionalVariable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");

#if defined(ROMANO_WIN)
    InitializeConditionVariable(cond_var);
#elif defined(ROMANO_LINUX)
    pthread_cond_init(cond_var, NULL);
#endif /* defined(ROMANO_WIN) */
}

void ConditionalVariable_wait(ConditionalVariable* cond_var, Mutex* mtx, uint32_t wait_duration_ms)
{
    ROMANO_ASSERT(cond_var != NULL && mtx != NULL, "");

#if defined(ROMANO_WIN)
    if(wait_duration_ms == 0)
    {
        wait_duration_ms = INFINITE;
    }

    SleepConditionVariableCS(cond_var, mtx, (DWORD)wait_duration_ms);
#elif defined(ROMANO_LINUX)
    if(wait_duration_ms == 0)
    {
        pthread_cond_wait(cond_var, mtx);
    }
    else
    {
        struct timespec wait_duration;
        wait_duration.tv_sec = 0;
        wait_duration.tv_nsec = wait_duration_ms * 1000000;

        pthread_cond_timedwait(cond_var, mtx, &wait_duration);
    }
#endif /* defined(ROMANO_WIN) */
}

void ConditionalVariable_signal(ConditionalVariable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");
#if defined(ROMANO_WIN)
    WakeConditionVariable(cond_var);
#elif defined(ROMANO_LINUX)
    pthread_cond_signal(cond_var);
#endif /* defined(ROMANO_WIN) */
}

void ConditionalVariable_broadcast(ConditionalVariable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");

#if defined(ROMANO_WIN)
    WakeAllConditionVariable(cond_var);
#elif defined(ROMANO_LINUX)
    pthread_cond_broadcast(cond_var);
#endif /* defined(ROMANO_WIN) */
}

void ConditionalVariable_release(ConditionalVariable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");

#if defined(ROMANO_LINUX)
    pthread_cond_destroy(cond_var);
#else
    ROMANO_UNUSED(cond_var);
#endif /* defined(ROMANO_LINUX) */
}

void ConditionalVariable_free(ConditionalVariable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");

#if defined(ROMANO_LINUX)
    pthread_cond_destroy(cond_var);
#endif /* defined(ROMANO_LINUX) */

    free(cond_var);
}

struct Thread {
    thread_handle _thread_handle;
    thread_id _id;
#if defined(ROMANO_LINUX)
    ThreadFunc _func;
    void* _data;
#endif /* defined(ROMANO_LINUX) */
};

void thread_init(Thread* thread, ThreadFunc func, void* arg)
{
    ROMANO_ASSERT(thread != NULL, "thread is NULL");

#if defined(ROMANO_WIN)
    thread->_thread_handle = CreateThread(NULL,
                                              0,
                                              (LPTHREAD_START_ROUTINE)func,
                                              arg,
                                              CREATE_SUSPENDED,
                                              &thread->_id);
#elif defined(ROMANO_LINUX)
    thread->_func = func;
    thread->_data = arg;
#endif /* defined(ROMANO_WIN) */
}

Thread* thread_create(ThreadFunc func, void* arg)
{
    Thread* new_thread = (Thread*)calloc(1, sizeof(Thread));

    if(new_thread == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    thread_init(new_thread, func, arg);

    return new_thread;
}

void thread_start(Thread* thread)
{
    ROMANO_ASSERT(thread != NULL, "thread is NULL");

#if defined(ROMANO_WIN)
    ResumeThread(thread->_thread_handle);
#elif defined(ROMANO_LINUX)
    pthread_create(&thread->_thread_handle,
                   NULL,
                   thread->_func,
                   thread->_data);
#endif /* defined(ROMANO_WIN) */
}

void thread_sleep(const int sleep_duration_ms)
{
    if(sleep_duration_ms == 0)
        return;

#if defined(ROMANO_WIN)
    Sleep((DWORD)sleep_duration_ms);
#elif defined(ROMANO_LINUX)
    struct timespec wait_duration;
    wait_duration.tv_sec = sleep_duration_ms / 1000;
    wait_duration.tv_nsec = (sleep_duration_ms % 1000) * 1000000;

    nanosleep(&wait_duration, NULL);
#endif /* defined(ROMANO_WIN) */
}

void thread_yield(void)
{
#if defined(ROMANO_WIN)
    SwitchToThread();
#elif defined(ROMANO_LINUX)
    sched_yield();
#else
#error "thread_yield no implemented on current platform"
#endif /* defined(ROMANO_WIN) */
}

size_t thread_get_id(void)
{
#if defined(ROMANO_WIN)
    return (size_t)GetCurrentThreadId();
#elif defined(ROMANO_LINUX)
    return (size_t)syscall(SYS_gettid);
#endif /* defined(ROMANO_WIN) */
}

void thread_detach(Thread* thread)
{
    ROMANO_ASSERT(thread != NULL, "thread is NULL");

#if defined(ROMANO_WIN)
    CloseHandle(thread->_thread_handle);
#elif defined(ROMANO_LINUX)
    pthread_detach(thread->_thread_handle);
#endif /* defined(ROMANO_WIN) */

    free(thread);
}

void thread_join(Thread* thread)
{
    ROMANO_ASSERT(thread != NULL, "thread is NULL");

    if(thread == NULL)
        return;

#if defined(ROMANO_WIN)
    WaitForSingleObject(thread->_thread_handle, INFINITE);
    CloseHandle(thread->_thread_handle);
#elif defined(ROMANO_LINUX)
    pthread_join(thread->_thread_handle, NULL);
#endif /* defined(ROMANO_WIN) */

    free(thread);
}

struct Work
{
    ThreadFunc func;
    void* arg;
    ThreadPoolWaiter* waiter;
};

typedef struct Work Work;

struct ThreadPool
{
    MoodycamelCQHandle work_queue;
    Thread** threads;

    uint32_t working_threads_count;
    uint32_t workers_count;
    uint32_t stop;
};

Work* work_new(ThreadFunc func, void* arg, ThreadPoolWaiter* waiter)
{
    Work* new_work;

    ROMANO_ASSERT(func != NULL, "");

    new_work = malloc(sizeof(Work));

    if(new_work == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    new_work->func = func;
    new_work->arg = arg;
    new_work->waiter = waiter;

    if(waiter != NULL)
        atomic_add_32((Atomic32*)&waiter->counter, 1);

    return new_work;
}

void work_free(Work* work)
{
    ROMANO_ASSERT(work != NULL, "");

    work->func = NULL;
    work->arg = NULL;

    if(work->waiter != NULL)
        atomic_sub_32((Atomic32*)&work->waiter->counter, 1);

    free(work);
}

void* threadpool_worker_func(void* arg)
{
    ThreadPool* threadpool = (ThreadPool*)arg;
    Work* work;

    while(1)
    {
        if(atomic_load_32((Atomic32*)&threadpool->stop))
            break;

        if(moodycamel_cq_try_dequeue(threadpool->work_queue, (MoodycamelValue*)&work))
        {
            ROMANO_ASSERT(work != NULL && work->func != NULL, "Invalid work item");

            atomic_add_32((Atomic32*)&threadpool->working_threads_count, 1);

            work->func(work->arg);

            atomic_sub_32((Atomic32*)&threadpool->working_threads_count, 1);

            work_free(work);
        }
    }

    atomic_sub_32((Atomic32*)&threadpool->workers_count, 1);

    return NULL;
}

ThreadPoolWaiter threadpool_waiter_new()
{
    ThreadPoolWaiter waiter;
    waiter.counter = 0;

    return waiter;
}

ThreadPool* threadpool_init(uint32_t workers_count)
{
    ThreadPool* threadpool;
    size_t i;

    workers_count = workers_count == 0 ? (uint32_t)get_num_procs() : workers_count;

    threadpool = (ThreadPool*)calloc(1, sizeof(ThreadPool));

    if(threadpool == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    atomic_store_32((Atomic32*)&threadpool->workers_count, workers_count);

    if(!moodycamel_cq_create(&threadpool->work_queue))
    {
        g_current_error = ErrorCode_MemAllocError;

        free(threadpool);

        return NULL;
    }

    threadpool->threads = (Thread**)calloc(workers_count, sizeof(Thread*));

    if(threadpool->threads == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;

        moodycamel_cq_destroy(&threadpool->work_queue);

        free(threadpool);

        return NULL;
    }

    for(i = 0; i < workers_count; i++)
    {
        threadpool->threads[i] = thread_create(threadpool_worker_func, (void*)threadpool);
        thread_start(threadpool->threads[i]);
    }

    return threadpool;
}

bool threadpool_work_add(ThreadPool* threadpool,
                         ThreadFunc func,
                         void* arg,
                         ThreadPoolWaiter* waiter)
{
    Work* work;

    ROMANO_ASSERT(threadpool != NULL, "");

    work = work_new(func, arg, waiter);

    if(!moodycamel_cq_enqueue(threadpool->work_queue, (MoodycamelValue)work))
    {
        work_free(work);
        return false;
    }

    return true;
}

void threadpool_wait(ThreadPool* threadpool)
{
    ROMANO_ASSERT(threadpool != NULL, "");

    while(1)
    {
        if(atomic_load_32((Atomic32*)&threadpool->working_threads_count) == 0 &&
           moodycamel_cq_size_approx(threadpool->work_queue) == 0)
            break;

        thread_yield();
    }
}

void threadpool_waiter_wait(ThreadPoolWaiter* waiter)
{
    while(atomic_load_32((Atomic32*)&waiter->counter) != 0)
        thread_yield();
}

void threadpool_release(ThreadPool* threadpool)
{
    Work* work;
    uint32_t workers_count;
    uint32_t i;

    ROMANO_ASSERT(threadpool != NULL, "");

    workers_count = (uint32_t)atomic_load_32((Atomic32*)&threadpool->workers_count);

    atomic_store_32((Atomic32*)&threadpool->stop, 1);

    for(i = 0; i < workers_count; i++)
        thread_join(threadpool->threads[i]);

    while(moodycamel_cq_size_approx(threadpool->work_queue) > 0)
        if(moodycamel_cq_try_dequeue(threadpool->work_queue, (MoodycamelValue)&work))
            work_free(work);

    free(threadpool->threads);

    moodycamel_cq_destroy(threadpool->work_queue);

    free(threadpool);

    threadpool = NULL;
}
