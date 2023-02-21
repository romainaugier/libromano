// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#pragma once

#if !defined(__LIBROMANO_SOCKET)
#define __LIBROMANO_SOCKET

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

struct _thread;
typedef struct _thread thread;
typedef void (*thread_func)(void* arg);

// Creates a new thread and launches it. The func is the function the thread will execute, 
// and the arg can be a pointer to anything that will be passed to the function
ROMANO_API thread* thread_create(thread_func func, void* arg);

// Starts the given thread
ROMANO_API void thread_start(thread* thread);

// Sleeps for x milliseconds
ROMANO_API void thread_sleep(int milliseconds);

// Waits until the given thread has finished and destroy it
ROMANO_API void thread_join(thread* thread);

struct _threadpool;
typedef struct _threadpool threadpool;

// Creates a threadpool with x workers and waits for work
ROMANO_API threadpool* threadpool_init(size_t workers_count);

// Adds some work to the threadpool 
ROMANO_API uint32_t threadpool_add_work(threadpool* threadpool, thread_func func, void* arg);

//
ROMANO_API void threadpool_wait(threadpool* threadpool);

ROMANO_API void threadpool_release(threadpool* threadpool);

ROMANO_CPP_END

#endif // !defined(__LIBROMANO_SOCKET)