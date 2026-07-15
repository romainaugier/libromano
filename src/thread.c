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
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
typedef pthread_t thread_handle;
typedef int thread_id;
#include <sched.h>
#if defined(ROMANO_APPLE)
#include <sys/sysctl.h>
#endif /* defined(ROMANO_APPLE) */
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
#elif defined(ROMANO_APPLE)
    int n_cpu = 0;
    size_t n_cpu_sz = sizeof(n_cpu);
    int res = sysctlbyname("hw.ncpu", &n_cpu, &n_cpu_sz, NULL, 0);

    ROMANO_ASSERT(res == 0, "syscall failed");

    return (size_t)n_cpu;
#endif
}

Mutex* mutex_new(void)
{
    Mutex* new_mutex = malloc(sizeof(Mutex));

#if defined(ROMANO_WIN)
    InitializeCriticalSection(new_mutex);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    pthread_mutex_init(new_mutex, NULL);
#endif /* defined(ROMANO_WIN) */

    return new_mutex;
}

void mutex_init(Mutex* mutex)
{
#if defined(ROMANO_WIN)
    InitializeCriticalSection(mutex);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    pthread_mutex_init(mutex, NULL);
#endif /* defined(ROMANO_WIN) */
}


void mutex_lock(Mutex* mutex)
{
    ROMANO_ASSERT(mutex != NULL, "Mutex has not been initialized");

#if defined(ROMANO_WIN)
    EnterCriticalSection(mutex);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    pthread_mutex_lock(mutex);
#endif /* defined(ROMANO_WIN) */
}

void mutex_unlock(Mutex* mutex)
{
    ROMANO_ASSERT(mutex != NULL, "Mutex has not been initialized");

#if defined(ROMANO_WIN)
    LeaveCriticalSection(mutex);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    pthread_mutex_unlock(mutex);
#endif /* defined(ROMANO_WIN) */
}

void mutex_release(Mutex* mutex)
{
    ROMANO_ASSERT(mutex != NULL, "Mutex has not been initialized");

#if defined(ROMANO_WIN)
    DeleteCriticalSection(mutex);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    pthread_mutex_destroy(mutex);
#endif /* defined(ROMANO_WIN) */
}

void mutex_free(Mutex* mutex)
{
    ROMANO_ASSERT(mutex != NULL, "Mutex has not been initialized");

#if defined(ROMANO_WIN)
    DeleteCriticalSection(mutex);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    pthread_mutex_destroy(mutex);
#endif /* defined(ROMANO_WIN) */

    free(mutex);
}

ConditionalVariable* conditionalvariable_new(void)
{
    ConditionalVariable* new_cond_var = malloc(sizeof(ConditionalVariable));

#if defined(ROMANO_WIN)
    InitializeConditionVariable(new_cond_var);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    pthread_cond_init(new_cond_var, NULL);
#endif /* defined(ROMANO_WIN) */

    return new_cond_var;
}

void conditionalvariable_init(ConditionalVariable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");

#if defined(ROMANO_WIN)
    InitializeConditionVariable(cond_var);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    pthread_cond_init(cond_var, NULL);
#endif /* defined(ROMANO_WIN) */
}

void conditionalvariable_wait(ConditionalVariable* cond_var, Mutex* mtx, uint32_t wait_duration_ms)
{
    ROMANO_ASSERT(cond_var != NULL && mtx != NULL, "");

#if defined(ROMANO_WIN)
    if(wait_duration_ms == 0)
    {
        wait_duration_ms = INFINITE;
    }

    SleepConditionVariableCS(cond_var, mtx, (DWORD)wait_duration_ms);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
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

void conditionalvariable_signal(ConditionalVariable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");
#if defined(ROMANO_WIN)
    WakeConditionVariable(cond_var);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    pthread_cond_signal(cond_var);
#endif /* defined(ROMANO_WIN) */
}

void conditionalvariable_broadcast(ConditionalVariable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");

#if defined(ROMANO_WIN)
    WakeAllConditionVariable(cond_var);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    pthread_cond_broadcast(cond_var);
#endif /* defined(ROMANO_WIN) */
}

void conditionalvariable_release(ConditionalVariable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");

#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    pthread_cond_destroy(cond_var);
#else
    ROMANO_UNUSED(cond_var);
#endif /* defined(ROMANO_LINUX) */
}

void conditionalvariable_free(ConditionalVariable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");

#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    pthread_cond_destroy(cond_var);
#endif /* defined(ROMANO_LINUX) */

    free(cond_var);
}

struct Thread {
    thread_handle _thread_handle;
    thread_id _id;
#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
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
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
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
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
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
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
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
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
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
#elif defined(ROMANO_APPLE)
    uint64_t id;

    if(pthread_threadid_np(NULL, &id) != 0)
        return THREAD_INVALID_ID;

    return (size_t)id;
#endif /* defined(ROMANO_WIN) */
}

void thread_detach(Thread* thread)
{
    ROMANO_ASSERT(thread != NULL, "thread is NULL");

#if defined(ROMANO_WIN)
    CloseHandle(thread->_thread_handle);
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
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
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
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

typedef struct Worker
{
    MoodycamelCQHandle queue;
    struct ThreadPool* pool;
    Thread* thread;
    size_t tid;
    uint32_t index;
    char _pad[64];
} Worker;

struct ThreadPool
{
    Worker* workers;

    uint32_t working_threads_count;
    uint32_t workers_count;
    uint32_t alive_count;
    uint32_t stop;

    uint32_t submit_rr;
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
        atomic_add_32((Atomic32*)&waiter->counter, 1, MemoryOrder_Relax);

    return new_work;
}

void work_free(Work* work)
{
    ROMANO_ASSERT(work != NULL, "");

    work->func = NULL;
    work->arg = NULL;

    if(work->waiter != NULL)
        atomic_sub_32((Atomic32*)&work->waiter->counter, 1, MemoryOrder_Relax);

    free(work);
}

Worker* threadpool_current_worker(ThreadPool* pool)
{
    size_t self = thread_get_id();
    uint32_t i;

    for(i = 0; i < pool->workers_count; i++)
    {
        if(pool->workers[i].tid == self)
            return &pool->workers[i];
    }

    return NULL;
}

void work_execute(ThreadPool* pool, Work* work)
{
    ROMANO_ASSERT(work != NULL && work->func != NULL, "Invalid work item");

    atomic_thread_fence(MemoryOrder_Acquire);

    atomic_add_32((Atomic32*)&pool->working_threads_count, 1, MemoryOrder_Relax);

    work->func(work->arg);

    atomic_sub_32((Atomic32*)&pool->working_threads_count, 1, MemoryOrder_Relax);

    work_free(work);
}

Work* threadpool_try_steal(ThreadPool* pool, Worker* self)
{
    uint32_t count = pool->workers_count;
    uint32_t start = self->index;
    uint32_t i;
    Work* work;

    for(i = 1; i < count; i++)
    {
        uint32_t victim = (start + i) % count;

        if(moodycamel_cq_try_dequeue(pool->workers[victim].queue,
                                     (MoodycamelValue*)&work))
        {
            ROMANO_TP_ACQUIRE(work);

            return work;
        }
    }

    return NULL;
}

void* threadpool_worker_func(void* arg)
{
    Worker* self = (Worker*)arg;
    ThreadPool* pool = self->pool;
    Work* work;
    uint32_t spins = 0;

    self->tid = thread_get_id();

    atomic_add_32((Atomic32*)&pool->alive_count, 1, MemoryOrder_AcqRel);

    while(1)
    {
        if(atomic_load_32((Atomic32*)&pool->stop, MemoryOrder_Relax))
            break;

        if(moodycamel_cq_try_dequeue(self->queue, (MoodycamelValue*)&work))
        {
            ROMANO_TP_ACQUIRE(work);

            work_execute(pool, work);
            spins = 0;

            continue;
        }

        work = threadpool_try_steal(pool, self);

        if(work != NULL)
        {
            work_execute(pool, work);
            spins = 0;
            continue;
        }

        if(++spins < 1024)
        {
            thread_yield();
        }
        else
        {
            thread_yield();
            spins = 1024;
        }
    }

    atomic_sub_32((Atomic32*)&pool->alive_count, 1, MemoryOrder_AcqRel);

    return NULL;
}

ThreadPoolWaiter threadpool_waiter_new(void)
{
    ThreadPoolWaiter waiter;
    waiter.counter = 0;

    return waiter;
}

ThreadPool* threadpool_init(uint32_t workers_count)
{
    ThreadPool* threadpool;
    uint32_t i;

    workers_count = workers_count == 0 ? (uint32_t)get_num_procs() : workers_count;

    threadpool = (ThreadPool*)calloc(1, sizeof(ThreadPool));

    if(threadpool == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    threadpool->workers = (Worker*)calloc(workers_count, sizeof(Worker));

    if(threadpool->workers == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        free(threadpool);
        return NULL;
    }

    threadpool->workers_count = workers_count;

    for(i = 0; i < workers_count; i++)
    {
        threadpool->workers[i].pool  = threadpool;
        threadpool->workers[i].index = i;

        if(!moodycamel_cq_create(&threadpool->workers[i].queue))
        {
            uint32_t j;

            g_current_error = ErrorCode_MemAllocError;

            for(j = 0; j < i; j++)
                moodycamel_cq_destroy(threadpool->workers[j].queue);

            free(threadpool->workers);
            free(threadpool);

            return NULL;
        }
    }

    for(i = 0; i < workers_count; i++)
    {
        threadpool->workers[i].thread = thread_create(threadpool_worker_func, (void*)&threadpool->workers[i]);

        thread_start(threadpool->workers[i].thread);
    }

    while(atomic_load_32((Atomic32*)&threadpool->alive_count, MemoryOrder_Acquire) != threadpool->workers_count)
        thread_yield();

    return threadpool;
}

bool threadpool_work_add(ThreadPool* threadpool,
                         ThreadFunc func,
                         void* arg,
                         ThreadPoolWaiter* waiter)
{
    Work* work;
    Worker* self;
    uint32_t idx;

    ROMANO_ASSERT(threadpool != NULL, "");

    work = work_new(func, arg, waiter);

    if(work == NULL)
        return false;

    self = threadpool_current_worker(threadpool);

    if(self != NULL)
    {
        ROMANO_TP_RELEASE(work);
        if(!moodycamel_cq_enqueue(self->queue, (MoodycamelValue)work))
        {
            work_free(work);
            return false;
        }

        return true;
    }

    idx = atomic_fetch_add_32((Atomic32*)&threadpool->submit_rr, 1, MemoryOrder_Relax) % threadpool->workers_count;

    ROMANO_TP_RELEASE(work);
    if(!moodycamel_cq_enqueue(threadpool->workers[idx].queue, (MoodycamelValue)work))
    {
        work_free(work);
        return false;
    }

    atomic_thread_fence(MemoryOrder_Release);

    return true;
}

void threadpool_wait(ThreadPool* threadpool)
{
    uint32_t i;
    bool all_empty;

    ROMANO_ASSERT(threadpool != NULL, "");

    while(1)
    {
        if(atomic_load_32((Atomic32*)&threadpool->working_threads_count,
                          MemoryOrder_Relax) == 0)
        {
            all_empty = true;

            for(i = 0; i < threadpool->workers_count; i++)
            {
                if(moodycamel_cq_size_approx(threadpool->workers[i].queue) != 0)
                {
                    all_empty = false;
                    break;
                }
            }

            if(all_empty)
                break;
        }

        thread_yield();
    }
}

void threadpool_waiter_wait(ThreadPoolWaiter* waiter)
{
    while(atomic_load_32((Atomic32*)&waiter->counter, MemoryOrder_Relax) != 0)
        thread_yield();
}

void threadpool_release(ThreadPool* threadpool)
{
    uint32_t workers_count;
    uint32_t i;
    Work* work;

    ROMANO_ASSERT(threadpool != NULL, "");

    workers_count = threadpool->workers_count;

    atomic_store_32((Atomic32*)&threadpool->stop, 1, MemoryOrder_SeqCst);

    for(i = 0; i < workers_count; i++)
        thread_join(threadpool->workers[i].thread);

    for(i = 0; i < workers_count; i++)
    {
        while(moodycamel_cq_size_approx(threadpool->workers[i].queue) > 0)
        {
            if(moodycamel_cq_try_dequeue(threadpool->workers[i].queue, (MoodycamelValue*)&work))
            {
                ROMANO_TP_ACQUIRE(work);

                work_free(work);
            }
        }

        moodycamel_cq_destroy(threadpool->workers[i].queue);
    }

    free(threadpool->workers);
    free(threadpool);
}
