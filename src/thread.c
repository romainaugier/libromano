/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/thread.h"
#include "libromano/atomic.h"
#include "libromano/vector.h"

#include "concurrentqueue/concurrentqueue.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#if defined(ROMANO_WIN)
typedef HANDLE thread_handle;
typedef DWORD thread_id;
#elif defined(ROMANO_LINUX)
typedef pthread_t thread_handle;
typedef int thread_id;
#endif /* defined(ROMANO_WIN) */

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

mutex_t* mutex_new(void)
{
    mutex_t* new_mutex = malloc(sizeof(mutex_t));

#if defined(ROMANO_WIN)
    InitializeCriticalSection(new_mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_init(new_mutex, NULL);
#endif /* defined(ROMANO_WIN) */

    return new_mutex;
}

void mutex_init(mutex_t* mutex)
{
#if defined(ROMANO_WIN)
    InitializeCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_init(mutex, NULL);
#endif /* defined(ROMANO_WIN) */
}


void mutex_lock(mutex_t* mutex)
{
    ROMANO_ASSERT(mutex != NULL, "Mutex has not been initialized");

#if defined(ROMANO_WIN)
    EnterCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_lock(mutex);
#endif /* defined(ROMANO_WIN) */
}

void mutex_unlock(mutex_t* mutex)
{
    ROMANO_ASSERT(mutex != NULL, "Mutex has not been initialized");

#if defined(ROMANO_WIN)
    LeaveCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_unlock(mutex);
#endif /* defined(ROMANO_WIN) */
}

void mutex_release(mutex_t* mutex)
{
    ROMANO_ASSERT(mutex != NULL, "Mutex has not been initialized");

#if defined(ROMANO_WIN)
    DeleteCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_destroy(mutex);
#endif /* defined(ROMANO_WIN) */
}

void mutex_free(mutex_t* mutex)
{
    ROMANO_ASSERT(mutex != NULL, "Mutex has not been initialized");

#if defined(ROMANO_WIN)
    DeleteCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_destroy(mutex);
#endif /* defined(ROMANO_WIN) */
    
    free(mutex);
}

conditional_variable* conditional_variable_new(void)
{
    conditional_variable* new_cond_var = malloc(sizeof(conditional_variable));

#if defined(ROMANO_WIN)
    InitializeConditionVariable(new_cond_var);
#elif defined(ROMANO_LINUX)
    pthread_cond_init(new_cond_var, NULL);
#endif /* defined(ROMANO_WIN) */

    return new_cond_var;
}

void conditional_variable_init(conditional_variable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");

#if defined(ROMANO_WIN)
    InitializeConditionVariable(cond_var);
#elif defined(ROMANO_LINUX)
    pthread_cond_init(cond_var, NULL);
#endif /* defined(ROMANO_WIN) */
}

void conditional_variable_wait(conditional_variable* cond_var, mutex_t* mtx, uint32_t wait_duration_ms)
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

void conditional_variable_signal(conditional_variable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");
#if defined(ROMANO_WIN)
    WakeConditionVariable(cond_var);
#elif defined(ROMANO_LINUX)
    pthread_cond_signal(cond_var);
#endif /* defined(ROMANO_WIN) */
}

void conditional_variable_broadcast(conditional_variable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");

#if defined(ROMANO_WIN)
    WakeAllConditionVariable(cond_var);
#elif defined(ROMANO_LINUX)
    pthread_cond_broadcast(cond_var);
#endif /* defined(ROMANO_WIN) */
}

void conditional_variable_release(conditional_variable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");

#if defined(ROMANO_LINUX)
    pthread_cond_destroy(cond_var);
#endif /* defined(ROMANO_LINUX) */
}

void conditional_variable_free(conditional_variable* cond_var)
{
    ROMANO_ASSERT(cond_var != NULL, "");

#if defined(ROMANO_LINUX)
    pthread_cond_destroy(cond_var);
#endif /* defined(ROMANO_LINUX) */

    free(cond_var);
}

struct thread {
    thread_handle _thread_handle;
    thread_id _id;
#if defined(ROMANO_LINUX)
    thread_func _func;
    void* _data;
#endif /* defined(ROMANO_LINUX) */
};

thread_t* thread_create(thread_func func, void* arg)
{
    thread_t* new_thread = (thread_t*)malloc(sizeof(thread_t));

    memset(new_thread, 0, sizeof(thread_t));

#if defined(ROMANO_WIN)
    new_thread->_thread_handle = CreateThread(NULL,
                                              0,
                                              (LPTHREAD_START_ROUTINE)func,
                                              arg,
                                              CREATE_SUSPENDED,
                                              &new_thread->_id);
#elif defined(ROMANO_LINUX)
    new_thread->_func = func;
    new_thread->_data = arg;
#endif /* defined(ROMANO_WIN) */

    return new_thread;
}

void thread_start(thread_t* thread)
{
    ROMANO_ASSERT(thread != NULL, "thread has not been initialized");

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
#if defined(ROMANO_WIN)
    Sleep((DWORD)sleep_duration_ms);
#elif defined(ROMANO_LINUX)
    struct timespec wait_duration;
    wait_duration.tv_sec = sleep_duration_ms / 1000;
    wait_duration.tv_nsec = (sleep_duration_ms % 1000) * 1000000;

    nanosleep(&wait_duration, NULL);
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

void thread_detach(thread_t* thread)
{
    ROMANO_ASSERT(thread != NULL, "");

#if defined(ROMANO_WIN)
    CloseHandle(thread->_thread_handle);
#elif defined(ROMANO_LINUX)
    pthread_detach(thread->_thread_handle);
#endif /* defined(ROMANO_WIN) */

    free(thread);
}

void thread_join(thread_t* thread)
{
    if(thread == NULL)
    {
        return;
    }

#if defined(ROMANO_WIN)
    WaitForSingleObject(thread->_thread_handle, INFINITE);
    CloseHandle(thread->_thread_handle);
#elif defined(ROMANO_LINUX)
    pthread_join(thread->_thread_handle, NULL);
#endif /* defined(ROMANO_WIN) */

    free(thread);
}

struct work
{
    thread_func func;
    void* arg;
};

typedef struct work work_t;

struct threadpool
{
    MoodycamelCQHandle work_queue;

    uint32_t working_threads_count;
    uint32_t workers_count;
    uint32_t stop;
};

work_t* work_new(thread_func func, void* arg)
{
    work_t* new_work;
    
    ROMANO_ASSERT(func != NULL, "");

    new_work = malloc(sizeof(work_t));
    
    new_work->func = func;
    new_work->arg = arg;

    return new_work;
}

void work_free(work_t* work)
{
    ROMANO_ASSERT(work != NULL, "");

    free(work);
}

void* threadpool_worker_func(void* arg)
{
    threadpool_t* threadpool = (threadpool_t*)arg;
    work_t* work;

    while(1)
    {
        if(atomic_load_32((atomic32_t*)&threadpool->stop))
        {
            break;
        }

        if(moodycamel_cq_try_dequeue(threadpool->work_queue, (MoodycamelValue)&work))
        {
            atomic_add_32((atomic32_t*)&threadpool->working_threads_count, 1);

            work->func(work->arg);

            atomic_sub_32((atomic32_t*)&threadpool->working_threads_count, 1);

            work_free(work);
        }
    }

    atomic_sub_32((atomic32_t*)&threadpool->workers_count, 1);

    return NULL;
}

threadpool_t* threadpool_init(size_t workers_count)
{
    threadpool_t* threadpool;
    size_t i;
    
    workers_count = workers_count == 0 ? get_num_procs() : workers_count;
    
    threadpool = malloc(sizeof(struct threadpool));

    threadpool->workers_count = workers_count;
    threadpool->working_threads_count = 0;
    threadpool->stop = 0;

    if(!moodycamel_cq_create(&threadpool->work_queue))
    {
        return NULL;
    }

    for(i = 0; i < workers_count; i++)
    {
        thread_t* new_thread;
        
        new_thread = thread_create(threadpool_worker_func, (void*)threadpool);
        
        thread_start(new_thread);
        thread_detach(new_thread);
    }

    return threadpool;
}

int threadpool_work_add(threadpool_t* threadpool, thread_func func, void* arg)
{
    work_t* work;

    ROMANO_ASSERT(threadpool != NULL, "");

    work = work_new(func, arg);

    moodycamel_cq_enqueue(threadpool->work_queue, (MoodycamelValue)work);

    return 1;
}

void threadpool_wait(threadpool_t* threadpool)
{
    ROMANO_ASSERT(threadpool != NULL, "");

    while(1)
    {
        if(atomic_load_32((atomic32_t*)&threadpool->working_threads_count) == 0 && 
           moodycamel_cq_size_approx(threadpool->work_queue) == 0)
        {
            break;
        }
    }
}

void threadpool_release(threadpool_t* threadpool)
{
    work_t* work;
    size_t i;
    
    ROMANO_ASSERT(threadpool != NULL, "");

    atomic_store_32((atomic32_t*)&threadpool->stop, 1);

    threadpool->stop = 1;

    while(moodycamel_cq_size_approx(threadpool->work_queue) > 0)
    {
        if(moodycamel_cq_try_dequeue(threadpool->work_queue, (MoodycamelValue)&work))
        {
            work_free(work);
        }
    }

    threadpool_wait(threadpool);

    moodycamel_cq_destroy(threadpool->work_queue);

    free(threadpool);
    threadpool = NULL;
}

