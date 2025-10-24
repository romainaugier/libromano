/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_THREAD)
#define __LIBROMANO_THREAD

#include "libromano/common.h"

ROMANO_CPP_ENTER

ROMANO_API size_t get_num_procs(void);

#if defined(ROMANO_WIN)
#include <Windows.h>
typedef CRITICAL_SECTION Mutex;
typedef CONDITION_VARIABLE ConditionalVariable;
#elif defined(ROMANO_LINUX)
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
typedef pthread_mutex_t Mutex;
typedef pthread_cond_t ConditionalVariable;
#endif /* defined(ROMANO_WIN) */

/* Creates a new mutex (memory allocated) */
ROMANO_API Mutex* mutex_new(void);

/* Initializes a mutex */
ROMANO_API void mutex_init(Mutex* mutex);

/* Locks the given mutex */
ROMANO_API void mutex_lock(Mutex* mutex);

/* Unlocks the given mutex */
ROMANO_API void mutex_unlock(Mutex* mutex);

/* Releases the given mutex */
ROMANO_API void mutex_release(Mutex* mutex);

/* Releases and frees the given mutex */
ROMANO_API void mutex_free(Mutex* mutex);

/* Creates a new conditional variable (memory allocated) and initializes it */
ROMANO_API ConditionalVariable* ConditionalVariable_new(void);

/* Initializes the given conditional variable */
ROMANO_API void ConditionalVariable_init(ConditionalVariable* cond_var);

/* Waits for the conditional variable to be waken, for the given time */
ROMANO_API void ConditionalVariable_wait(ConditionalVariable* cond_var, Mutex* mutex, uint32_t wait_duration_ms);

/* Signal one conditional variable */
ROMANO_API void ConditionalVariable_signal(ConditionalVariable* cond_var);

/* Signal all conditonal variables */
ROMANO_API void ConditionalVariable_broadcast(ConditionalVariable* cond_var);

/* Releases the given conditional variable */
ROMANO_API void ConditionalVariable_release(ConditionalVariable* cond_var);

/* Releases and frees the given conditional variable */
ROMANO_API void ConditionalVariable_free(ConditionalVariable* cond_var);

struct Thread;
typedef struct Thread Thread;
typedef void* (*ThreadFunc)(void* arg);

/* Creates a new thread and launches it. The func is the function the thread will execute,  */
/* and the arg can be a pointer to anything that will be passed to the function */
ROMANO_API Thread* thread_create(ThreadFunc func, void* arg);

/* Starts the given thread */
ROMANO_API void thread_start(Thread* thread);

/* Sleeps for x milliseconds */
ROMANO_API void thread_sleep(const int sleep_duration_ms);

/*
 *
 */
ROMANO_API void thread_yield(void);

/* Returns the current thread id */
ROMANO_API size_t thread_get_id(void);

/* Detach the given thread */
ROMANO_API void thread_detach(Thread* thread);

/* Waits until the given thread has finished and destroy it */
ROMANO_API void thread_join(Thread* thread);

struct ThreadPool;
typedef struct ThreadPool ThreadPool;

typedef struct ThreadPoolWaiter {
    int32_t counter;
} ThreadPoolWaiter;

ROMANO_API ThreadPoolWaiter threadpool_waiter_new();

/* Creates a threadpool with x workers and waits for work */
ROMANO_API ThreadPool* threadpool_init(uint32_t workers_count);

/*
 * Adds some work to the threadpool.
 * If no waiter is needed, pass NULL as the ThreadPool waiter
 */
ROMANO_API bool threadpool_work_add(ThreadPool* threadpool,
                                    ThreadFunc func,
                                    void* arg,
                                    ThreadPoolWaiter* waiter);

/* Wait for all the work to be done */
ROMANO_API void threadpool_wait(ThreadPool* threadpool);

ROMANO_API void threadpool_waiter_wait(ThreadPoolWaiter* waiter);

/* Release all the workers and the threadpool */
ROMANO_API void threadpool_release(ThreadPool* threadpool);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_THREAD) */
