/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/threadpool.h"
#include "libromano/thread.h"
#include "libromano/atomic.h"
#include "libromano/cpu.h"
#include "libromano/error.h"

#include "concurrentqueue/concurrentqueue.h"

#include <stdlib.h>
#include <string.h>

extern ErrorCode g_current_error;

/* TSan annotations for the concurrent queue acquire/release */
#if defined(__SANITIZE_THREAD__)
#define ROMANO_TP_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define ROMANO_TP_TSAN 1
#endif
#endif /* defined(__SANITIZE_THREAD__) */

#if defined(ROMANO_TP_TSAN)
void __tsan_acquire(void* addr);
void __tsan_release(void* addr);
#define ROMANO_TP_RELEASE(p) __tsan_release(p)
#define ROMANO_TP_ACQUIRE(p) __tsan_acquire(p)
#else
#define ROMANO_TP_RELEASE(p) ((void)0)
#define ROMANO_TP_ACQUIRE(p) ((void)0)
#endif /* defined(TP_TSAN) */

/*
 * Job node. The argument is either a user pointer (threadpool_work_add), or a copy stored inline
 * in the node (threadpool_work_add_copy). Nodes come from a per-pool slab recycled through a
 * lock-free free list, so submitting a small job does not touch the allocator at all.
 * Nodes that do not fit (big arguments, or slab exhausted) are heap allocated in one block.
 */

#define WORK_NIL 0xFFFFFFFFu
#define WORK_ORIGIN_SLAB 0u
#define WORK_ORIGIN_HEAP 1u

typedef struct Work
{
    ThreadPoolFunc func;
    void* arg;
    ThreadPoolWaiter* waiter;
    Atomic32 next; /* free list link (slab index), slab nodes only */
    uint32_t origin;

    /* The union aligns the inline storage for any scalar type */
    union {
        void* p;
        double d;
        int64_t i;
        unsigned char bytes[THREADPOOL_WORK_INLINE_ARG_SIZE];
    } storage;
} Work;

typedef struct Worker
{
    MoodycamelCQHandle queue;
    struct ThreadPool* pool;
    Thread* thread;
    uint32_t index;
    char _pad[ROMANO_CACHE_LINE_SIZE];
} Worker;

struct ThreadPool
{
    Worker* workers;
    Work* slab;

    uint32_t workers_count;
    uint32_t alive_count;
    uint32_t stop;
    uint32_t submit_rr;

    char _pad0[ROMANO_CACHE_LINE_SIZE];

    /* Free list head: low 32 bits = slab index, high 32 bits = ABA tag */
    Atomic64 free_head;

    char _pad1[ROMANO_CACHE_LINE_SIZE];

    uint32_t working_threads_count;
    uint32_t pending_count;

    char _pad2[ROMANO_CACHE_LINE_SIZE];

    /*
     * Idle workers park on sleep_cv. work_epoch is bumped by every submission, a worker only
     * sleeps if the epoch did not move since before its last (empty) scan of the queues.
     */
    Atomic32 work_epoch;
    Atomic32 sleepers;
    Mutex sleep_mutex;
    ConditionalVariable sleep_cv;
};

/* Number of empty scans (each followed by a yield) before an idle worker goes to sleep */
#define THREADPOOL_SPINS_BEFORE_SLEEP 1024

/* Worker the calling thread belongs to, if any (avoids a gettid syscall per submission) */
static ROMANO_THREAD_LOCAL Worker* tls_worker = NULL;

#define FREE_HEAD_TAG_MASK 0xFFFFFFFF00000000ull
#define FREE_HEAD_TAG_ONE 0x0000000100000000ull

ROMANO_FORCE_INLINE Atomic64 free_head_make(Atomic64 old_head, uint32_t index)
{
    const uint64_t tag = ((uint64_t)old_head & FREE_HEAD_TAG_MASK) + FREE_HEAD_TAG_ONE;
    return (Atomic64)(tag | (uint64_t)index);
}

static Work* work_slab_pop(ThreadPool* pool)
{
    Atomic64 head;
    Atomic64 new_head;
    uint32_t index;
    uint32_t next;

    while(1)
    {
        head = atomic_load_64(&pool->free_head, MemoryOrder_Acquire);
        index = (uint32_t)((uint64_t)head & 0xFFFFFFFFull);

        if(index == WORK_NIL)
            return NULL;

        /*
         * The node may be popped (and even pushed back) by another thread between this load and
         * the CAS: the value read is then stale, but the tag makes the CAS fail. The slab memory
         * is never freed while the pool lives, so the read itself is always valid.
         */
        next = (uint32_t)atomic_load_32(&pool->slab[index].next, MemoryOrder_Relax);
        new_head = free_head_make(head, next);

        if(atomic_compare_exchange_weak_64(&pool->free_head, new_head, head, MemoryOrder_Acquire))
            return &pool->slab[index];
    }
}

static void work_slab_push(ThreadPool* pool, Work* work)
{
    const uint32_t index = (uint32_t)(work - pool->slab);
    Atomic64 head;

    while(1)
    {
        head = atomic_load_64(&pool->free_head, MemoryOrder_Relax);

        atomic_store_32(&work->next,
                        (Atomic32)(uint32_t)((uint64_t)head & 0xFFFFFFFFull),
                        MemoryOrder_Relax);

        /* Release: publishes the recycled node to the next popper */
        if(atomic_compare_exchange_weak_64(&pool->free_head,
                                           free_head_make(head, index),
                                           head,
                                           MemoryOrder_Release))
            return;
    }
}

static Work* work_alloc(ThreadPool* pool, size_t arg_size)
{
    Work* work = NULL;
    size_t alloc_size;

    if(arg_size <= THREADPOOL_WORK_INLINE_ARG_SIZE)
    {
        work = work_slab_pop(pool);

        if(work != NULL)
        {
            work->origin = WORK_ORIGIN_SLAB;
            return work;
        }

        alloc_size = sizeof(Work);
    }
    else
    {
        alloc_size = offsetof(Work, storage) + arg_size;
        alloc_size = alloc_size < sizeof(Work) ? sizeof(Work) : alloc_size;
    }

    work = (Work*)malloc(alloc_size);

    if(work == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    work->origin = WORK_ORIGIN_HEAP;

    return work;
}

static void work_recycle(ThreadPool* pool, Work* work)
{
    if(work->origin == WORK_ORIGIN_SLAB)
        work_slab_push(pool, work);
    else
        free(work);
}

/* Signals the waiter and gives the node back, without running it */
static void work_drop(ThreadPool* pool, Work* work)
{
    ThreadPoolWaiter* waiter = work->waiter;

    work_recycle(pool, work);

    if(waiter != NULL)
        atomic_sub_32((Atomic32*)&waiter->counter, 1, MemoryOrder_Release);
}

static Worker* threadpool_current_worker(ThreadPool* pool)
{
    Worker* worker = tls_worker;

    return (worker != NULL && worker->pool == pool) ? worker : NULL;
}

static void work_execute(ThreadPool* pool, Work* work)
{
    ThreadPoolWaiter* waiter;

    ROMANO_ASSERT(work != NULL && work->func != NULL, "Invalid work item");

    atomic_thread_fence(MemoryOrder_Acquire);

    atomic_add_32((Atomic32*)&pool->working_threads_count, 1, MemoryOrder_Relax);

    work->func(work->arg);

    atomic_sub_32((Atomic32*)&pool->working_threads_count, 1, MemoryOrder_Relax);

    /* Read before recycling: the node may be reused right after */
    waiter = work->waiter;

    work_recycle(pool, work);

    if(waiter != NULL)
        atomic_sub_32((Atomic32*)&waiter->counter, 1, MemoryOrder_Release);

    atomic_sub_32((Atomic32*)&pool->pending_count, 1, MemoryOrder_Release);
}

/* Tries to dequeue a job from any worker queue, starting at start and skipping skip */
static Work* threadpool_find_work(ThreadPool* pool, uint32_t start, uint32_t skip)
{
    const uint32_t count = pool->workers_count;
    uint32_t i;
    Work* work;

    for(i = 0; i < count; i++)
    {
        const uint32_t victim = (start + i) % count;

        if(victim == skip)
            continue;

        if(moodycamel_cq_try_dequeue(pool->workers[victim].queue, (MoodycamelValue*)&work))
        {
            ROMANO_TP_ACQUIRE(work);
            return work;
        }
    }

    return NULL;
}

/*
 * Sleeps until a job is submitted after the given epoch was read (or the pool stops).
 *
 * No lost wakeup: the worker increments sleepers then reads work_epoch, the submitter
 * increments work_epoch then reads sleepers, all sequentially consistent RMW/loads. Either the
 * submitter sees the sleeper and signals under the mutex, or the worker sees the new epoch and
 * does not wait.
 */
static void threadpool_worker_sleep(ThreadPool* pool, Atomic32 epoch)
{
    mutex_lock(&pool->sleep_mutex);

    atomic_add_32(&pool->sleepers, 1, MemoryOrder_SeqCst);

    while(atomic_load_32(&pool->work_epoch, MemoryOrder_SeqCst) == epoch &&
          !atomic_load_32((Atomic32*)&pool->stop, MemoryOrder_SeqCst))
        conditionalvariable_wait(&pool->sleep_cv, &pool->sleep_mutex, 0);

    atomic_sub_32(&pool->sleepers, 1, MemoryOrder_SeqCst);

    mutex_unlock(&pool->sleep_mutex);
}

static void threadpool_wake_one(ThreadPool* pool)
{
    atomic_add_32(&pool->work_epoch, 1, MemoryOrder_SeqCst);

    if(atomic_load_32(&pool->sleepers, MemoryOrder_SeqCst) > 0)
    {
        mutex_lock(&pool->sleep_mutex);
        conditionalvariable_signal(&pool->sleep_cv);
        mutex_unlock(&pool->sleep_mutex);
    }
}

static void* threadpool_worker_func(void* arg)
{
    Worker* self = (Worker*)arg;
    ThreadPool* pool = self->pool;
    Work* work;
    Atomic32 epoch;
    uint32_t idle_spins = 0;

    tls_worker = self;

    atomic_add_32((Atomic32*)&pool->alive_count, 1, MemoryOrder_AcqRel);

    while(1)
    {
        if(atomic_load_32((Atomic32*)&pool->stop, MemoryOrder_Relax))
            break;

        /* Read before scanning: anything submitted after this bumps it */
        epoch = atomic_load_32(&pool->work_epoch, MemoryOrder_SeqCst);

        if(moodycamel_cq_try_dequeue(self->queue, (MoodycamelValue*)&work))
        {
            ROMANO_TP_ACQUIRE(work);
            work_execute(pool, work);
            idle_spins = 0;
            continue;
        }

        work = threadpool_find_work(pool, self->index + 1, self->index);

        if(work != NULL)
        {
            work_execute(pool, work);
            idle_spins = 0;
            continue;
        }

        if(++idle_spins < THREADPOOL_SPINS_BEFORE_SLEEP)
        {
            thread_yield();
            continue;
        }

        idle_spins = 0;
        threadpool_worker_sleep(pool, epoch);
    }

    tls_worker = NULL;

    atomic_sub_32((Atomic32*)&pool->alive_count, 1, MemoryOrder_AcqRel);

    return NULL;
}

ThreadPoolWaiter threadpool_waiter_new(void)
{
    ThreadPoolWaiter waiter;
    waiter.counter = 0;

    return waiter;
}

ThreadPool* threadpool_init(uint32_t workers_count)
{
    ThreadPool* threadpool;
    uint32_t i;

    workers_count = workers_count == 0 ? (uint32_t)get_num_procs() : workers_count;

    threadpool = (ThreadPool*)calloc(1, sizeof(ThreadPool));

    if(threadpool == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    threadpool->workers = (Worker*)calloc(workers_count, sizeof(Worker));
    threadpool->slab = (Work*)malloc(THREADPOOL_WORK_SLAB_SIZE * sizeof(Work));

    if(threadpool->workers == NULL || threadpool->slab == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        free(threadpool->workers);
        free(threadpool->slab);
        free(threadpool);
        return NULL;
    }

    /* Chain all the slab nodes in the free list */
    for(i = 0; i < THREADPOOL_WORK_SLAB_SIZE; i++)
    {
        threadpool->slab[i].next = (Atomic32)((i + 1) < THREADPOOL_WORK_SLAB_SIZE ? (i + 1) : WORK_NIL);
        threadpool->slab[i].origin = WORK_ORIGIN_SLAB;
    }

    threadpool->free_head = 0; /* index 0, tag 0 */

    mutex_init(&threadpool->sleep_mutex);
    conditionalvariable_init(&threadpool->sleep_cv);

    threadpool->workers_count = workers_count;

    for(i = 0; i < workers_count; i++)
    {
        threadpool->workers[i].pool  = threadpool;
        threadpool->workers[i].index = i;

        if(!moodycamel_cq_create(&threadpool->workers[i].queue))
        {
            uint32_t j;

            g_current_error = ErrorCode_MemAllocError;

            for(j = 0; j < i; j++)
                moodycamel_cq_destroy(threadpool->workers[j].queue);

            conditionalvariable_release(&threadpool->sleep_cv);
            mutex_release(&threadpool->sleep_mutex);
            free(threadpool->workers);
            free(threadpool->slab);
            free(threadpool);

            return NULL;
        }
    }

    for(i = 0; i < workers_count; i++)
    {
        threadpool->workers[i].thread = thread_create(threadpool_worker_func, (void*)&threadpool->workers[i]);

        thread_start(threadpool->workers[i].thread);
    }

    while(atomic_load_32((Atomic32*)&threadpool->alive_count, MemoryOrder_Acquire) != threadpool->workers_count)
        thread_yield();

    return threadpool;
}

uint32_t threadpool_get_workers_count(ThreadPool* threadpool)
{
    ROMANO_ASSERT(threadpool != NULL, "");

    return threadpool->workers_count;
}

bool threadpool_is_worker_thread(ThreadPool* threadpool)
{
    return threadpool_current_worker(threadpool) != NULL;
}

/* Pushes an initialized job, the waiter must already have been incremented */
static bool threadpool_submit(ThreadPool* threadpool, Work* work)
{
    Worker* self;
    MoodycamelCQHandle queue;
    uint32_t idx;

    atomic_add_32((Atomic32*)&threadpool->pending_count, 1, MemoryOrder_Relax);

    self = threadpool_current_worker(threadpool);

    if(self != NULL)
    {
        queue = self->queue;
    }
    else
    {
        idx = (uint32_t)atomic_fetch_add_32((Atomic32*)&threadpool->submit_rr, 1, MemoryOrder_Relax);
        queue = threadpool->workers[idx % threadpool->workers_count].queue;
    }

    ROMANO_TP_RELEASE(work);

    if(!moodycamel_cq_enqueue(queue, (MoodycamelValue)work))
    {
        work_drop(threadpool, work);
        atomic_sub_32((Atomic32*)&threadpool->pending_count, 1, MemoryOrder_Relax);
        return false;
    }

    threadpool_wake_one(threadpool);

    return true;
}

bool threadpool_work_add(ThreadPool* threadpool,
                         ThreadPoolFunc func,
                         void* arg,
                         ThreadPoolWaiter* waiter)
{
    Work* work;

    ROMANO_ASSERT(threadpool != NULL, "");
    ROMANO_ASSERT(func != NULL, "");

    work = work_alloc(threadpool, 0);

    if(work == NULL)
        return false;

    work->func = func;
    work->arg = arg;
    work->waiter = waiter;

    if(waiter != NULL)
        atomic_add_32((Atomic32*)&waiter->counter, 1, MemoryOrder_Relax);

    return threadpool_submit(threadpool, work);
}

bool threadpool_work_add_copy(ThreadPool* threadpool,
                              ThreadPoolFunc func,
                              const void* arg,
                              size_t arg_size,
                              ThreadPoolWaiter* waiter)
{
    Work* work;

    ROMANO_ASSERT(threadpool != NULL, "");
    ROMANO_ASSERT(func != NULL, "");
    ROMANO_ASSERT(arg != NULL || arg_size == 0, "");

    work = work_alloc(threadpool, arg_size);

    if(work == NULL)
        return false;

    if(arg_size > 0)
        memcpy(work->storage.bytes, arg, arg_size);

    work->func = func;
    work->arg = (void*)work->storage.bytes;
    work->waiter = waiter;

    if(waiter != NULL)
        atomic_add_32((Atomic32*)&waiter->counter, 1, MemoryOrder_Relax);

    return threadpool_submit(threadpool, work);
}

void threadpool_wait(ThreadPool* threadpool)
{
    ROMANO_ASSERT(threadpool != NULL, "");

    while(atomic_load_32((Atomic32*)&threadpool->pending_count, MemoryOrder_Acquire) != 0)
        thread_yield();
}

void threadpool_waiter_wait(ThreadPoolWaiter* waiter)
{
    while(atomic_load_32((Atomic32*)&waiter->counter, MemoryOrder_Acquire) != 0)
        thread_yield();
}

void threadpool_waiter_wait_help(ThreadPool* threadpool, ThreadPoolWaiter* waiter)
{
    Worker* self;
    Work* work;
    uint32_t start;

    ROMANO_ASSERT(threadpool != NULL && waiter != NULL, "");

    self = threadpool_current_worker(threadpool);

    while(atomic_load_32((Atomic32*)&waiter->counter, MemoryOrder_Acquire) != 0)
    {
        work = NULL;

        if(self != NULL && moodycamel_cq_try_dequeue(self->queue, (MoodycamelValue*)&work))
        {
            ROMANO_TP_ACQUIRE(work);
        }
        else
        {
            start = self != NULL ? self->index + 1 : 0;
            work = threadpool_find_work(threadpool, start, self != NULL ? self->index : WORK_NIL);
        }

        if(work != NULL)
            work_execute(threadpool, work);
        else
            thread_yield();
    }
}

void threadpool_release(ThreadPool* threadpool)
{
    uint32_t workers_count;
    uint32_t i;
    Work* work;

    ROMANO_ASSERT(threadpool != NULL, "");

    workers_count = threadpool->workers_count;

    atomic_store_32((Atomic32*)&threadpool->stop, 1, MemoryOrder_SeqCst);

    mutex_lock(&threadpool->sleep_mutex);
    conditionalvariable_broadcast(&threadpool->sleep_cv);
    mutex_unlock(&threadpool->sleep_mutex);

    for(i = 0; i < workers_count; i++)
        thread_join(threadpool->workers[i].thread);

    for(i = 0; i < workers_count; i++)
    {
        while(moodycamel_cq_size_approx(threadpool->workers[i].queue) > 0)
        {
            if(moodycamel_cq_try_dequeue(threadpool->workers[i].queue, (MoodycamelValue*)&work))
            {
                ROMANO_TP_ACQUIRE(work);

                work_drop(threadpool, work);
            }
        }

        moodycamel_cq_destroy(threadpool->workers[i].queue);
    }

    conditionalvariable_release(&threadpool->sleep_cv);
    mutex_release(&threadpool->sleep_mutex);

    free(threadpool->slab);
    free(threadpool->workers);
    free(threadpool);
}
