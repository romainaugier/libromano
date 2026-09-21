/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/thread.h"
#include "libromano/atomic.h"
#include "libromano/error.h"


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
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);

        deadline.tv_sec += (time_t)(wait_duration_ms / 1000);
        deadline.tv_nsec += (long)(wait_duration_ms % 1000) * 1000000L;

        if(deadline.tv_nsec >= 1000000000L)
        {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000L;
        }

        pthread_cond_timedwait(cond_var, mtx, &deadline);
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
