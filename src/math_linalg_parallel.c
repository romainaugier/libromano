/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "math_linalg_internal.h"

typedef struct LinAlgChunk {
    LinAlgRangeFunc func;
    void* data;
    size_t begin;
    size_t end;
} LinAlgChunk;

/* 32 bytes, copied inline in the threadpool job: no allocation per chunk */
ROMANO_COMPILE_TIME_ASSERT(sizeof(LinAlgChunk) <= THREADPOOL_WORK_INLINE_ARG_SIZE);

static void* linalg_chunk_run(void* arg)
{
    const LinAlgChunk* chunk = (const LinAlgChunk*)arg;

    chunk->func(chunk->data, chunk->begin, chunk->end);

    return NULL;
}

/* Chunks per thread, gives the pool some slack to balance uneven chunks */
#define LINALG_CHUNKS_PER_THREAD 4

void linalg_parallel_for(LinAlgCtx* ctx,
                         size_t count,
                         size_t grain,
                         LinAlgRangeFunc func,
                         void* data)
{
    ThreadPool* pool = linalg_ctx_get_pool(ctx);
    ThreadPoolWaiter waiter;
    LinAlgChunk chunk;
    size_t num_chunks;
    size_t chunk_size;
    size_t begin;

    if(count == 0)
        return;

    grain = grain == 0 ? 1 : grain;

    if(pool == NULL || count <= grain)
    {
        func(data, 0, count);
        return;
    }

    num_chunks = (count + grain - 1) / grain;
    num_chunks = LINALG_MIN(num_chunks, (size_t)linalg_ctx_get_threads_count(ctx) * LINALG_CHUNKS_PER_THREAD);
    chunk_size = (count + num_chunks - 1) / num_chunks;

    waiter = threadpool_waiter_new();

    chunk.func = func;
    chunk.data = data;

    /* The first chunk is kept for the calling thread */
    for(begin = chunk_size; begin < count; begin += chunk_size)
    {
        chunk.begin = begin;
        chunk.end = LINALG_MIN(begin + chunk_size, count);

        if(!threadpool_work_add_copy(pool, linalg_chunk_run, &chunk, sizeof(LinAlgChunk), &waiter))
            func(data, chunk.begin, chunk.end);
    }

    func(data, 0, LINALG_MIN(chunk_size, count));

    threadpool_waiter_wait_help(pool, &waiter);
}
