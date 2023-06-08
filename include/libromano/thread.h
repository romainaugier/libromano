/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_THREAD)
#define __LIBROMANO_THREAD

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

ROMANO_API size_t get_num_procs(void);

#if defined(ROMANO_WIN)
#include <Windows.h>
typedef CRITICAL_SECTION mutex_t;
typedef CONDITION_VARIABLE conditional_variable;
#elif defined(ROMANO_LINUX)
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t conditional_variable;
#endif /* defined(ROMANO_WIN) */

/* Creates a new mutex (memory allocated) */
ROMANO_API mutex_t* mutex_new(void);

/* Initializes a mutex */
ROMANO_API void mutex_init(mutex_t* mutex);

/* Locks the given mutex */
ROMANO_API void mutex_lock(mutex_t* mutex);

/* Unlocks the given mutex */
ROMANO_API void mutex_unlock(mutex_t* mutex);

/* Releases the given mutex */
ROMANO_API void mutex_release(mutex_t* mutex);

/* Releases and frees the given mutex */
ROMANO_API void mutex_free(mutex_t* mutex);

/* Creates a new conditional variable (memory allocated) and initializes it */
ROMANO_API conditional_variable* conditional_variable_new(void);

/* Initializes the given conditional variable */
ROMANO_API void conditional_variable_init(conditional_variable* cond_var);

/* Waits for the conditional variable to be waken, for the given time */
ROMANO_API void conditional_variable_wait(conditional_variable* cond_var, mutex_t* mutex, uint32_t wait_duration_ms);

/* Signal one conditional variable */
ROMANO_API void conditional_variable_signal(conditional_variable* cond_var);

/* Signal all conditonal variables */
ROMANO_API void conditional_variable_broadcast(conditional_variable* cond_var);

/* Releases the given conditional variable */
ROMANO_API void conditional_variable_release(conditional_variable* cond_var);

/* Releases and frees the given conditional variable */
ROMANO_API void conditional_variable_free(conditional_variable* cond_var);

struct thread;
typedef struct thread thread_t;
typedef void* (*thread_func)(void* arg);

/* Creates a new thread and launches it. The func is the function the thread will execute,  */
/* and the arg can be a pointer to anything that will be passed to the function */
ROMANO_API thread_t* thread_create(thread_func func, void* arg);

/* Starts the given thread */
ROMANO_API void thread_start(thread_t* thread);

/* Sleeps for x milliseconds */
ROMANO_API void thread_sleep(const int sleep_duration_ms);

/* Returns the current thread id */
ROMANO_API size_t thread_get_id(void);

/* Detach the given thread */
ROMANO_API void thread_detach(thread_t* thread);

/* Waits until the given thread has finished and destroy it */
ROMANO_API void thread_join(thread_t* thread);

struct threadpool;
typedef struct threadpool threadpool_t;

/* Creates a threadpool with x workers and waits for work */
ROMANO_API threadpool_t* threadpool_init(size_t workers_count);

/* Adds some work to the threadpool  */
ROMANO_API int threadpool_work_add(threadpool_t* threadpool, thread_func func, void* arg);

/* Wait for all the work to be done */
ROMANO_API void threadpool_wait(threadpool_t* threadpool);

/* Release all the workers and the threadpool */
ROMANO_API void threadpool_release(threadpool_t* threadpool);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_THREAD) */
