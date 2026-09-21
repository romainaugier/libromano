/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_MATH_LINALG_CTX)
#define __LIBROMANO_MATH_LINALG_CTX

#include "libromano/common.h"
#include "libromano/threadpool.h"

ROMANO_CPP_ENTER

/*
 * Execution context passed to the linalg functions.
 *
 * Passing a NULL context (or a context with a NULL pool) runs everything on the calling thread.
 * With a pool, the work is split in jobs pushed to that pool, and the calling thread also
 * executes jobs while waiting, so the effective parallelism is workers_count + 1.
 *
 * The context does not own the pool. Calling a linalg function from inside a job of the same
 * pool is fine (the wait helps executing jobs instead of blocking a worker).
 */
typedef struct LinAlgCtx {
    ThreadPool* pool;
} LinAlgCtx;

ROMANO_FORCE_INLINE LinAlgCtx linalg_ctx_new(ThreadPool* pool)
{
    LinAlgCtx ctx;
    ctx.pool = pool;

    return ctx;
}

ROMANO_FORCE_INLINE ThreadPool* linalg_ctx_get_pool(const LinAlgCtx* ctx)
{
    return ctx != NULL ? ctx->pool : NULL;
}

/* Number of threads that can work on a linalg call with this context (at least 1) */
ROMANO_FORCE_INLINE uint32_t linalg_ctx_get_threads_count(const LinAlgCtx* ctx)
{
    ThreadPool* pool = linalg_ctx_get_pool(ctx);

    return pool != NULL ? threadpool_get_workers_count(pool) + 1 : 1;
}

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_MATH_LINALG_CTX) */
