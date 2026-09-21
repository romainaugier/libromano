/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_THREADPOOL)
#define __LIBROMANO_THREADPOOL

/*
 * Work-stealing threadpool.
 */

#include "libromano/common.h"

ROMANO_CPP_ENTER

/* Same signature as ThreadFunc from libromano/thread.h, functions are interchangeable */
typedef void* (*ThreadPoolFunc)(void* arg);

struct ThreadPool;
typedef struct ThreadPool ThreadPool;

typedef struct ThreadPoolWaiter {
    int32_t counter;
} ThreadPoolWaiter;

/*
 * Maximum size (in bytes) of an argument that threadpool_work_add_copy stores inline in the
 * job itself. Arguments up to this size cost no allocation at all: the job node comes from a
 * per-pool free list and the argument is copied into it.
 * Bigger arguments still work, the job and its argument then share a single heap allocation.
 */
#define THREADPOOL_WORK_INLINE_ARG_SIZE 96

/*
 * Number of job nodes preallocated per pool. When all of them are in flight, new jobs fall back
 * to the heap until nodes are recycled.
 */
#define THREADPOOL_WORK_SLAB_SIZE 4096

ROMANO_API ThreadPoolWaiter threadpool_waiter_new(void);

/* Creates a threadpool with x workers and waits for work */
ROMANO_API ThreadPool* threadpool_init(uint32_t workers_count);

/* Returns the number of workers of the threadpool */
ROMANO_API uint32_t threadpool_get_workers_count(ThreadPool* threadpool);

/*
 * Returns true if the calling thread is one of the workers of the given threadpool
 */
ROMANO_API bool threadpool_is_worker_thread(ThreadPool* threadpool);

/*
 * Adds some work to the threadpool.
 * arg is passed as is to func, the caller owns it and must keep it alive until func has run.
 * If no waiter is needed, pass NULL as the ThreadPool waiter
 */
ROMANO_API bool threadpool_work_add(ThreadPool* threadpool,
                                    ThreadPoolFunc func,
                                    void* arg,
                                    ThreadPoolWaiter* waiter);

/*
 * Adds some work to the threadpool, copying arg_size bytes from arg into the job.
 * func receives a pointer to the copy, which is valid (and writable) only while func runs.
 * arg can be a stack variable, it can be released as soon as this function returns.
 * No allocation happens when arg_size <= THREADPOOL_WORK_INLINE_ARG_SIZE (and a pooled job node
 * is available). The copy is aligned for any scalar type (8 bytes), not for SIMD types.
 * If no waiter is needed, pass NULL as the ThreadPool waiter
 */
ROMANO_API bool threadpool_work_add_copy(ThreadPool* threadpool,
                                         ThreadPoolFunc func,
                                         const void* arg,
                                         size_t arg_size,
                                         ThreadPoolWaiter* waiter);

/* Wait for all the work to be done */
ROMANO_API void threadpool_wait(ThreadPool* threadpool);

/* Spins until all the work attached to the waiter is done */
ROMANO_API void threadpool_waiter_wait(ThreadPoolWaiter* waiter);

/*
 * Waits until all the work attached to the waiter is done, executing pending jobs of the
 * threadpool in the meantime instead of just spinning.
 * The calling thread participates in the work, and waiting from inside a job (nested
 * parallelism) cannot deadlock the pool, which threadpool_waiter_wait can when all the
 * workers end up waiting.
 */
ROMANO_API void threadpool_waiter_wait_help(ThreadPool* threadpool, ThreadPoolWaiter* waiter);

/* Release all the workers and the threadpool */
ROMANO_API void threadpool_release(ThreadPool* threadpool);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_THREADPOOL) */
