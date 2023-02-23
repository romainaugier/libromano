// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#include "libromano/thread.h"
#include "libromano/libromano.h"

#include <stdlib.h>
#include <assert.h>

#if defined(ROMANO_WIN)
#include <Windows.h>
typedef HANDLE thread_handle;
typedef DWORD thread_id;
typedef CRITICAL_SECTION mutex;
typedef CONDITION_VARIABLE conditional_variable;
#elif defined(ROMANO_LINUX)
#include <pthread.h>
#include <unistd.h>
typedef pthread_t thread_handle;
typedef int thread_id;
typedef pthread_mutex_t mutex;
typedef pthread_cond_t cconditional_variable;
#endif // defined(ROMANO_WIN)

size_t get_num_procs()
{
#if defined(ROMANO_WIN)
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    return (size_t)sys_info.dwNumberOfProcessors;
#elif defined(ROMANO_LINUX)
    return (size_t)sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

mutex* mutex_new()
{
#if defined(ROMANO_WIN)
    mutex* new_mutex = malloc(sizeof(mutex));

    InitializeCriticalSection(new_mutex);
#elif defined(ROMANO_LINUX)
#endif // defined(ROMANO_WIN)

    return new_mutex;
}

void mutex_init(mutex* mutex)
{
#if defined(ROMANO_WIN)
    InitializeCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
#endif // defined(ROMANO_WIN)
}


void mutex_lock(mutex* mutex)
{
    assert(mutex != NULL);

#if defined(ROMANO_WIN)
    EnterCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
#endif // defined(ROMANO_WIN)
}

void mutex_unlock(mutex* mutex)
{
    assert(mutex != NULL);

#if defined(ROMANO_WIN)
    LeaveCriticalSection(mutex);
#elif defined(ROMANO_LINUX)
#endif // defined(ROMANO_WIN)
}

void mutex_release(mutex* mutex)
{
    assert(mutex != NULL);

#if defined(ROMANO_WIN)
    DeleteCriticalSection(mutex);
#elif defined(ROMANO_LINUX)

#endif // defined(ROMANO_WIN)
}

void mutex_free(mutex* mutex)
{
    assert(mutex != NULL);

#if defined(ROMANO_WIN)
    DeleteCriticalSection(mutex);
    free(mutex);
#elif defined(ROMANO_LINUX)
#endif // defined(ROMANO_WIN)
}

conditional_variable* conditional_variable_new()
{
    conditional_variable* new_cond_var = malloc(sizeof(conditional_variable));

#if defined(ROMANO_WIN)
    InitializeConditionVariable(new_cond_var);
#else if defined(ROMANO_LINUX)
#endif // defined(ROMANO_WIN)

    return new_cond_var;
}

void conditional_variable_init(conditional_variable* cond_var)
{
    assert(cond_var != NULL);

#if defined(ROMANO_WIN)
    InitializeConditionVariable(cond_var);
#else if defined(ROMANO_LINUX)
#endif // defined(ROMANO_WIN)
}

void conditional_variable_wait(conditional_variable* cond_var, mutex* mtx, uint32_t wait_duration_ms)
{
    assert(cond_var != NULL && mtx != NULL);

#if defined(ROMANO_WIN)
    SleepConditionVariableCS(cond_var, mtx, (DWORD)wait_duration_ms);
#elif defined(ROMANO_LINUX)
#endif // defined(ROMANO_WIN)
}

void conditional_variable_signal(conditional_variable* cond_var)
{
    assert(cond_var != NULL);
#if defined(ROMANO_WIN)
    WakeConditionVariable(cond_var);
#elif defined(ROMANO_LINUX)
#endif // defined(ROMANO_WIN)
}

void conditional_variable_broadcast(conditional_variable* cond_var)
{
    assert(cond_var != NULL);

#if defined(ROMANO_WIN)
    WakeAllConditionVariable(cond_var);
#elif defined(ROMANO_LINUX)
#endif // defined(ROMANO_WIN)
}

void conditional_variable_release(conditional_variable* cond_var)
{
    assert(cond_var != NULL);
}

void conditional_variable_free(conditional_variable* cond_var)
{
    assert(cond_var != NULL);

    free(cond_var);
}

struct _thread {
    thread_handle _thread_handle;
    thread_id _id;
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
#endif // defined(ROMANO_WIN)

    return new_thread;
}

void thread_start(thread* thread)
{
    assert(thread != NULL);

#if defined(ROMANO_WIN)
    ResumeThread(thread->_thread_handle);
#elif defined(ROMANO_LINUX)
    pthread_create(&new_thread->_thread_handle,
                   NULL,
                   func,
                   arg)
#endif // defined(ROMANO_WIN)
}

void thread_sleep(int sleep_duration_ms)
{
#if defined(ROMANO_WIN)
    Sleep((DWORD)sleep_duration_ms);
#elif defined(ROMANO_LINUX)
    nanosleep(sleep_duration_ms * 1000000);
#endif // defined(ROMANO_WIN)
}

void thread_detach(thread* thread)
{
    assert(thread != NULL);

#if defined(ROMANO_WIN)
    CloseHandle(thread->_thread_handle);
#elif defined(ROMANO_LINUX)
#endif // defined(ROMANO_WIN)

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
#endif // defined(ROMANO_WIN)

    free(thread);
}

struct _work
{
    thread_func func;
    void* arg;
    void* next;
};

typedef struct _work work;

work* threadpool_work_create(thread_func func, void* arg)
{
    assert(func != NULL);

    work* new_work = malloc(sizeof(work));
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

}

void* threadpool_worker_func(void* arg)
{
    threadpool* threadpool = arg;
    work* work;

    while(1)
    {
        
    }
}

struct _threadpool
{
    work* first;
    work* last;
    
    mutex work_mutex;
    
    conditional_variable work_cond_var;
    conditional_variable working_cond_var;
    
    size_t working_count;
    size_t workers_count;

    uint32_t stop : 1;
};

typedef struct _threadpool threadpool;

threadpool* threadpool_init(size_t workers_count)
{
    workers_count = workers_count == 0 ? get_num_procs() : workers_count;

    threadpool* threadpool= malloc(sizeof(threadpool));

    threadpool->workers_count = workers_count;
    threadpool->working_count = 0;

    mutex_init(&threadpool->work_mutex);

    conditional_variable_init(&threadpool->work_cond_var);
    conditional_variable_init(&threadpool->working_cond_var);

    threadpool->first = NULL;
    threadpool->last = NULL;

    for(size_t i = 0; i < workers_count; i++)
    {
        thread* new_thread = thread_create(NULL, threadpool_worker_func);
        thread_detach(new_thread);
    }

    return threadpool;
}

uint32_t threadpool_add_work(threadpool* threadpool, thread_func func, void* arg)
{

}

void threadpool_wait(threadpool* threadpool)
{

}

void threadpool_release(threadpool* threadpool)
{
    mutex_release(&threadpool->work_mutex);
}