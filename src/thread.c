/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/thread.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>

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

mutex* mutex_new(void)
{
    mutex* new_mutex = malloc(sizeof(mutex));

#if defined(ROMANO_WIN)
    InitializeCriticalSection(new_mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_init(new_mutex, NULL);
#endif /* defined(ROMANO_WIN) */

    return new_mutex;
}

void mutex_init(mutex* mutex)
{
#if defined(ROMANO_WIN)
    InitializeCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_init(mutex, NULL);
#endif /* defined(ROMANO_WIN) */
}


void mutex_lock(mutex* mutex)
{
    assert(mutex != NULL);

#if defined(ROMANO_WIN)
    EnterCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_lock(mutex);
#endif /* defined(ROMANO_WIN) */
}

void mutex_unlock(mutex* mutex)
{
    assert(mutex != NULL);

#if defined(ROMANO_WIN)
    LeaveCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_unlock(mutex);
#endif /* defined(ROMANO_WIN) */
}

void mutex_release(mutex* mutex)
{
    assert(mutex != NULL);

#if defined(ROMANO_WIN)
    DeleteCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
    pthread_mutex_destroy(mutex);
#endif /* defined(ROMANO_WIN) */
}

void mutex_free(mutex* mutex)
{
    assert(mutex != NULL);

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
    assert(cond_var != NULL);

#if defined(ROMANO_WIN)
    InitializeConditionVariable(cond_var);
#elif defined(ROMANO_LINUX)
    pthread_cond_init(cond_var, NULL);
#endif /* defined(ROMANO_WIN) */
}

void conditional_variable_wait(conditional_variable* cond_var, mutex* mtx, uint32_t wait_duration_ms)
{
    assert(cond_var != NULL && mtx != NULL);

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
    assert(cond_var != NULL);
#if defined(ROMANO_WIN)
    WakeConditionVariable(cond_var);
#elif defined(ROMANO_LINUX)
    pthread_cond_signal(cond_var);
#endif /* defined(ROMANO_WIN) */
}

void conditional_variable_broadcast(conditional_variable* cond_var)
{
    assert(cond_var != NULL);

#if defined(ROMANO_WIN)
    WakeAllConditionVariable(cond_var);
#elif defined(ROMANO_LINUX)
    pthread_cond_broadcast(cond_var);
#endif /* defined(ROMANO_WIN) */
}

void conditional_variable_release(conditional_variable* cond_var)
{
    assert(cond_var != NULL);

#if defined(ROMANO_LINUX)
    pthread_cond_destroy(cond_var);
#endif /* defined(ROMANO_LINUX) */
}

void conditional_variable_free(conditional_variable* cond_var)
{
    assert(cond_var != NULL);

#if defined(ROMANO_LINUX)
    pthread_cond_destroy(cond_var);
#endif /* defined(ROMANO_LINUX) */

    free(cond_var);
}

struct _thread {
    thread_handle _thread_handle;
    thread_id _id;
#if defined(ROMANO_LINUX)
    thread_func _func;
    void* _data;
#endif /* defined(ROMANO_LINUX) */
};

thread* thread_create(thread_func func, void* arg)
{
    thread* new_thread = (thread*)malloc(sizeof(thread));

    memset(new_thread, 0, sizeof(thread));

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

void thread_start(thread* thread)
{
    assert(thread != NULL);

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

void thread_detach(thread* thread)
{
    assert(thread != NULL);

#if defined(ROMANO_WIN)
    CloseHandle(thread->_thread_handle);
#elif defined(ROMANO_LINUX)
    pthread_detach(thread->_thread_handle);
#endif /* defined(ROMANO_WIN) */

    free(thread);
}

void thread_join(thread* thread)
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

struct _work
{
    thread_func func;
    void* arg;
    struct _work* next;
};

typedef struct _work work;

struct _threadpool
{
    work* first;
    work* last;
    
    mutex work_mutex;
    
    conditional_variable work_cond_var;
    conditional_variable working_cond_var;
    
    size_t working_count;
    size_t workers_count;

    size_t stop;
};

work* threadpool_work_new(thread_func func, void* arg)
{
    work* new_work;
    
    assert(func != NULL);

    new_work = malloc(sizeof(work));
    
    new_work->func = func;
    new_work->arg = arg;
    new_work->next = NULL;

    return new_work;
}

void threadpool_work_free(work* work)
{
    assert(work != NULL);

    free(work);
}

work* threadpool_work_get(threadpool* threadpool)
{
    work* work;
    
    assert(threadpool != NULL);
    
    work = threadpool->first;

    if(work == NULL)
    {
        return NULL;
    }

    if(work->next == NULL)
    {
        threadpool->first = NULL;
        threadpool->last = NULL;
    }
    else
    {
        threadpool->first = work->next;
    }

    return work;
}

void* threadpool_worker_func(void* arg)
{
    threadpool* threadpool = arg;
    work* work = NULL;

    while(1)
    {
        mutex_lock(&(threadpool->work_mutex));
    
        while(threadpool->first == NULL && !threadpool->stop)
        {
            conditional_variable_wait(&(threadpool->work_cond_var), 
                                      &(threadpool->work_mutex), 
                                      0);
        }

        if(threadpool->stop)
        {
            break;
        }

        work = threadpool_work_get(threadpool);
        threadpool->working_count++;
        mutex_unlock(&(threadpool->work_mutex));

        if(work != NULL)
        {
            work->func(work->arg);
            threadpool_work_free(work);
        }

        mutex_lock(&(threadpool->work_mutex));
        threadpool->working_count--;

        if(!threadpool->stop && threadpool->working_count == 0 && threadpool->first == NULL)
        {
            conditional_variable_signal(&(threadpool->working_cond_var));
        }


        mutex_unlock(&(threadpool->work_mutex));
    }

    threadpool->workers_count--;
    conditional_variable_signal(&(threadpool->working_cond_var));
    
    mutex_unlock(&(threadpool->work_mutex));

    return NULL;
}

threadpool* threadpool_init(size_t workers_count)
{
    threadpool* threadpool;
    size_t i;
    
    workers_count = workers_count == 0 ? get_num_procs() : workers_count;
    
    threadpool = malloc(sizeof(struct _threadpool));

    threadpool->workers_count = workers_count;
    threadpool->working_count = 0;

    mutex_init(&threadpool->work_mutex);

    conditional_variable_init(&threadpool->work_cond_var);
    conditional_variable_init(&threadpool->working_cond_var);

    threadpool->first = NULL;
    threadpool->last = NULL;


    for(i = 0; i < workers_count; i++)
    {
        thread* new_thread;
        
        new_thread = thread_create(threadpool_worker_func, (void*)threadpool);
        
        thread_start(new_thread);
        thread_detach(new_thread);
    }

    return threadpool;
}

int threadpool_work_add(threadpool* threadpool, thread_func func, void* arg)
{
    work* work;

    assert(threadpool != NULL);
    
    work = threadpool_work_new(func, arg);

    if(work == NULL)
    {
        return 0;
    }

    mutex_lock(&(threadpool->work_mutex));
    
    if(threadpool->first == NULL)
    {
        threadpool->first = work;
        threadpool->last = threadpool->first;
    }
    else
    {
        threadpool->last->next = work;
        threadpool->last = work;
    }
    
    conditional_variable_broadcast(&(threadpool->work_cond_var));
    mutex_unlock(&(threadpool->work_mutex));

    return 1;
}

void threadpool_wait(threadpool* threadpool)
{
    assert(threadpool != NULL);

    mutex_lock(&(threadpool->work_mutex));

    while(1)
    {
        if((!threadpool->stop && threadpool->working_count != 0) || (threadpool->stop && threadpool->workers_count != 0))
        {
            conditional_variable_wait(&(threadpool->working_cond_var), &(threadpool->work_mutex), 0);
        }
        else
        {
            break;
        }
    }

    mutex_unlock(&(threadpool->work_mutex));
}

void threadpool_release(threadpool* threadpool)
{
    work* work1;
    work* work2;
    
    assert(threadpool != NULL);

    mutex_lock(&(threadpool->work_mutex));

    work1 = threadpool->first;

    while(work1 != NULL)
    {
        work2 = work1->next;
        threadpool_work_free(work1);
        work1 = work2;
    }

    threadpool->stop = 1;

    conditional_variable_broadcast(&(threadpool->work_cond_var));
    mutex_unlock(&(threadpool->work_mutex));

    threadpool_wait(threadpool);

    mutex_release(&(threadpool->work_mutex));
    conditional_variable_release(&(threadpool->work_cond_var));
    conditional_variable_release(&(threadpool->working_cond_var));

    free(threadpool);
}

