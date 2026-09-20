/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/atomic.h"
#include "libromano/thread.h"

/* Keep the counts modest so the suite stays fast under ThreadSanitizer */
#define TEST_NUM_THREADS 4
#define TEST_ITERATIONS 50000
#define TEST_ITERATIONS_SMALL 10000

#define test_report(name, ok) TEST_CHECK_MSG((ok), "%s", (name))

static void run_threads_n(int count, void* (*fn)(void*), void* data)
{
    Thread* threads[TEST_NUM_THREADS];
    int i;

    for(i = 0; i < count; i++)
    {
        threads[i] = thread_create(fn, data);
    }

    for(i = 0; i < count; i++)
    {
        thread_start(threads[i]);
    }

    for(i = 0; i < count; i++)
    {
        thread_join(threads[i]);
    }
}

static void test_load_store_32(void)
{
    Atomic32 v = 0;

    atomic_store_32(&v, 42, MemoryOrder_Relax);
    test_report("atomic_store_32/atomic_load_32 (Relax)",
                atomic_load_32(&v, MemoryOrder_Relax) == 42);

    atomic_store_32(&v, -7, MemoryOrder_SeqCst);
    test_report("atomic_store_32/atomic_load_32 (SeqCst, negative)",
                atomic_load_32(&v, MemoryOrder_SeqCst) == -7);

    atomic_store_32(&v, 0, MemoryOrder_Release);
    test_report("atomic_store_32/atomic_load_32 (Release/Acquire, zero)",
                atomic_load_32(&v, MemoryOrder_Acquire) == 0);

    atomic_store_32(&v, 12345, MemoryOrder_Relax);
    test_report("atomic_load_32 (Relax)",
                atomic_load_32(&v, MemoryOrder_Relax) == 12345);

    atomic_store_32(&v, -12345, MemoryOrder_Relax);
    test_report("atomic_load_32 (negative)",
                atomic_load_32(&v, MemoryOrder_Relax) == -12345);
}

static void test_load_store_64(void)
{
    Atomic64 v = 0;

    atomic_store_64(&v, (Atomic64)0x123456789ABCDEF0LL, MemoryOrder_Relax);
    test_report("atomic_store_64/atomic_load_64 (Relax)",
                atomic_load_64(&v, MemoryOrder_Relax) ==
                (Atomic64)0x123456789ABCDEF0LL);

    atomic_store_64(&v, (Atomic64)-1, MemoryOrder_SeqCst);
    test_report("atomic_store_64/atomic_load_64 (SeqCst, negative)",
                atomic_load_64(&v, MemoryOrder_SeqCst) == (Atomic64)-1);

    atomic_store_64(&v, 0, MemoryOrder_Release);
    test_report("atomic_store_64/atomic_load_64 (Release/Acquire, zero)",
                atomic_load_64(&v, MemoryOrder_Acquire) == 0);

    atomic_store_64(&v, (Atomic64)0x7FFFFFFFFFFFFFFFLL, MemoryOrder_Relax);
    test_report("atomic_load_64 (max positive)",
                atomic_load_64(&v, MemoryOrder_Relax) ==
                (Atomic64)0x7FFFFFFFFFFFFFFFLL);
}

static void test_add_sub_32(void)
{
    Atomic32 v = 0;

    atomic_add_32(&v, 5, MemoryOrder_Relax);
    test_report("atomic_add_32 (single)",
                atomic_load_32(&v, MemoryOrder_Relax) == 5);

    atomic_add_32(&v, 10, MemoryOrder_Relax);
    test_report("atomic_add_32 (accumulate)",
                atomic_load_32(&v, MemoryOrder_Relax) == 15);

    atomic_sub_32(&v, 3, MemoryOrder_Relax);
    test_report("atomic_sub_32 (single)",
                atomic_load_32(&v, MemoryOrder_Relax) == 12);

    atomic_sub_32(&v, 2, MemoryOrder_Relax);
    test_report("atomic_sub_32 (accumulate)",
                atomic_load_32(&v, MemoryOrder_Relax) == 10);

    atomic_sub_32(&v, 100, MemoryOrder_Relax);
    test_report("atomic_sub_32 (negative result)",
                atomic_load_32(&v, MemoryOrder_Relax) == -90);

    v = 0;
    (void)atomic_fetch_add_32(&v, 7, MemoryOrder_Relax);
    test_report("atomic_fetch_add_32 (stored value)",
                atomic_load_32(&v, MemoryOrder_Relax) == 7);

    v = 10;
    (void)atomic_fetch_sub_32(&v, 4, MemoryOrder_Relax);
    test_report("atomic_fetch_sub_32 (stored value)",
                atomic_load_32(&v, MemoryOrder_Relax) == 6);
}

static void test_add_sub_64(void)
{
    Atomic64 v = 0;

    atomic_add_64(&v, 5, MemoryOrder_Relax);
    test_report("atomic_add_64 (single)",
                atomic_load_64(&v, MemoryOrder_Relax) == 5);

    atomic_add_64(&v, 10000000000LL, MemoryOrder_Relax);
    test_report("atomic_add_64 (large)",
                atomic_load_64(&v, MemoryOrder_Relax) == 10000000005LL);

    atomic_sub_64(&v, 5, MemoryOrder_Relax);
    test_report("atomic_sub_64 (single)",
                atomic_load_64(&v, MemoryOrder_Relax) == 10000000000LL);

    atomic_sub_64(&v, 20000000000LL, MemoryOrder_Relax);
    test_report("atomic_sub_64 (negative result)",
                atomic_load_64(&v, MemoryOrder_Relax) == -10000000000LL);

    v = 0;
    (void)atomic_fetch_add_64(&v, 7, MemoryOrder_Relax);
    test_report("atomic_fetch_add_64 (stored value)",
                atomic_load_64(&v, MemoryOrder_Relax) == 7);

    v = 10;
    (void)atomic_fetch_sub_64(&v, 4, MemoryOrder_Relax);
    test_report("atomic_fetch_sub_64 (stored value)",
                atomic_load_64(&v, MemoryOrder_Relax) == 6);
}

static void test_bitwise_32(void)
{
    Atomic32 v;

    v = 0x0F0F0F0F;
    atomic_and_32(&v, 0x00FF00FF, MemoryOrder_Relax);
    test_report("atomic_and_32",
                atomic_load_32(&v, MemoryOrder_Relax) == 0x000F000F);

    v = (Atomic32)0xFFFFFFFF;
    atomic_and_32(&v, 0x0F0F0F0F, MemoryOrder_Relax);
    test_report("atomic_and_32 (clear bits)",
                atomic_load_32(&v, MemoryOrder_Relax) == 0x0F0F0F0F);

    v = 0x0F0F0F0F;
    (void)atomic_fetch_and_32(&v, 0x00FF00FF, MemoryOrder_Relax);
    test_report("atomic_fetch_and_32 (stored value)",
                atomic_load_32(&v, MemoryOrder_Relax) == 0x000F000F);

    v = 0x0F0F0F0F;
    atomic_or_32(&v, 0x70707070, MemoryOrder_Relax);
    test_report("atomic_or_32",
                atomic_load_32(&v, MemoryOrder_Relax) == 0x7F7F7F7F);

    v = 0x0F0F0F0F;
    (void)atomic_fetch_or_32(&v, 0x70707070, MemoryOrder_Relax);
    test_report("atomic_fetch_or_32 (stored value)",
                atomic_load_32(&v, MemoryOrder_Relax) == 0x7F7F7F7F);

    v = 0x0F0F0F0F;
    atomic_xor_32(&v, (Atomic32)0xFFFFFFFF, MemoryOrder_Relax);
    test_report("atomic_xor_32",
                atomic_load_32(&v, MemoryOrder_Relax) == (Atomic32)0xF0F0F0F0);

    v = 0x0F0F0F0F;
    (void)atomic_fetch_xor_32(&v, (Atomic32)0xFFFFFFFF, MemoryOrder_Relax);
    test_report("atomic_fetch_xor_32 (stored value)",
                atomic_load_32(&v, MemoryOrder_Relax) == (Atomic32)0xF0F0F0F0);
}

static void test_bitwise_64(void)
{
    Atomic64 v;

    v = (Atomic64)0x0F0F0F0F0F0F0F0FLL;
    atomic_and_64(&v, (Atomic64)0x00FF00FF00FF00FFLL, MemoryOrder_Relax);
    test_report("atomic_and_64",
                atomic_load_64(&v, MemoryOrder_Relax) ==
                (Atomic64)0x000F000F000F000FLL);

    v = (Atomic64)0x0F0F0F0F0F0F0F0FLL;
    (void)atomic_fetch_and_64(&v, (Atomic64)0x00FF00FF00FF00FFLL,
                              MemoryOrder_Relax);
    test_report("atomic_fetch_and_64 (stored value)",
                atomic_load_64(&v, MemoryOrder_Relax) ==
                (Atomic64)0x000F000F000F000FLL);

    v = (Atomic64)0x0F0F0F0F0F0F0F0FLL;
    atomic_or_64(&v, (Atomic64)0x7070707070707070LL, MemoryOrder_Relax);
    test_report("atomic_or_64",
                atomic_load_64(&v, MemoryOrder_Relax) ==
                (Atomic64)0x7F7F7F7F7F7F7F7FLL);

    v = (Atomic64)0x0F0F0F0F0F0F0F0FLL;
    (void)atomic_fetch_or_64(&v, (Atomic64)0x7070707070707070LL,
                             MemoryOrder_Relax);
    test_report("atomic_fetch_or_64 (stored value)",
                atomic_load_64(&v, MemoryOrder_Relax) ==
                (Atomic64)0x7F7F7F7F7F7F7F7FLL);

    v = (Atomic64)0x0F0F0F0F0F0F0F0FLL;
    atomic_xor_64(&v, (Atomic64)0x0F0F0F0F0F0F0F0FLL, MemoryOrder_Relax);
    test_report("atomic_xor_64",
                atomic_load_64(&v, MemoryOrder_Relax) == 0);

    v = (Atomic64)0x0F0F0F0F0F0F0F0FLL;
    (void)atomic_fetch_xor_64(&v, (Atomic64)0x0F0F0F0F0F0F0F0FLL,
                              MemoryOrder_Relax);
    test_report("atomic_fetch_xor_64 (stored value)",
                atomic_load_64(&v, MemoryOrder_Relax) == 0);
}

static void test_compare_exchange_32(void)
{
    Atomic32 v;

    /* Strong, success */
    v = 1;
    test_report("atomic_compare_exchange_strong_32 (success)",
                atomic_compare_exchange_strong_32(&v, 2, 1,
                                                  MemoryOrder_SeqCst) &&
                atomic_load_32(&v, MemoryOrder_Relax) == 2);

    /* Strong, failure */
    v = 5;
    test_report("atomic_compare_exchange_strong_32 (failure)",
                !atomic_compare_exchange_strong_32(&v, 2, 1,
                                                   MemoryOrder_SeqCst) &&
                atomic_load_32(&v, MemoryOrder_Relax) == 5);

    /* Weak, success */
    v = 1;
    test_report("atomic_compare_exchange_weak_32 (success)",
                atomic_compare_exchange_weak_32(&v, 2, 1,
                                                MemoryOrder_AcqRel) &&
                atomic_load_32(&v, MemoryOrder_Relax) == 2);

    /* Weak, failure */
    v = 5;
    test_report("atomic_compare_exchange_weak_32 (failure)",
                !atomic_compare_exchange_weak_32(&v, 2, 1,
                                                 MemoryOrder_AcqRel) &&
                atomic_load_32(&v, MemoryOrder_Relax) == 5);

    /* CAS on matching value with Relax order */
    v = 42;
    test_report("atomic_compare_exchange_strong_32 (Relax)",
                atomic_compare_exchange_strong_32(&v, 43, 42,
                                                  MemoryOrder_Relax) &&
                atomic_load_32(&v, MemoryOrder_Relax) == 43);
}

static void test_compare_exchange_64(void)
{
    Atomic64 v;

    v = 1;
    test_report("atomic_compare_exchange_strong_64 (success)",
                atomic_compare_exchange_strong_64(&v, 2, 1,
                                                  MemoryOrder_SeqCst) &&
                atomic_load_64(&v, MemoryOrder_Relax) == 2);

    v = 5;
    test_report("atomic_compare_exchange_strong_64 (failure)",
                !atomic_compare_exchange_strong_64(&v, 2, 1,
                                                   MemoryOrder_SeqCst) &&
                atomic_load_64(&v, MemoryOrder_Relax) == 5);

    v = 1;
    test_report("atomic_compare_exchange_weak_64 (success)",
                atomic_compare_exchange_weak_64(&v, 2, 1,
                                                MemoryOrder_AcqRel) &&
                atomic_load_64(&v, MemoryOrder_Relax) == 2);

    v = 5;
    test_report("atomic_compare_exchange_weak_64 (failure)",
                !atomic_compare_exchange_weak_64(&v, 2, 1,
                                                 MemoryOrder_AcqRel) &&
                atomic_load_64(&v, MemoryOrder_Relax) == 5);

    v = (Atomic64)0x100000000LL;
    test_report("atomic_compare_exchange_strong_64 (large value)",
                atomic_compare_exchange_strong_64(&v, (Atomic64)0x200000000LL,
                                                  (Atomic64)0x100000000LL,
                                                  MemoryOrder_SeqCst) &&
                atomic_load_64(&v, MemoryOrder_Relax) ==
                (Atomic64)0x200000000LL);
}

static void test_exchange_32(void)
{
    Atomic32 v;

    v = 10;
    test_report("atomic_exchange_32 (returns old value)",
                atomic_exchange_32(&v, 20, MemoryOrder_Relax) == 10);
    test_report("atomic_exchange_32 (stores new value)",
                atomic_load_32(&v, MemoryOrder_Relax) == 20);

    v = -5;
    test_report("atomic_exchange_32 (negative, returns old)",
                atomic_exchange_32(&v, 5, MemoryOrder_AcqRel) == -5);
    test_report("atomic_exchange_32 (negative, stored)",
                atomic_load_32(&v, MemoryOrder_Relax) == 5);
}

static void test_exchange_64(void)
{
    Atomic64 v;

    v = 10;
    test_report("atomic_exchange_64 (returns old value)",
                atomic_exchange_64(&v, 20, MemoryOrder_Relax) == 10);
    test_report("atomic_exchange_64 (stores new value)",
                atomic_load_64(&v, MemoryOrder_Relax) == 20);

    v = (Atomic64)0x100000000LL;
    test_report("atomic_exchange_64 (large, returns old)",
                atomic_exchange_64(&v, (Atomic64)0x200000000LL,
                                   MemoryOrder_AcqRel) ==
                (Atomic64)0x100000000LL);
    test_report("atomic_exchange_64 (large, stored)",
                atomic_load_64(&v, MemoryOrder_Relax) ==
                (Atomic64)0x200000000LL);
}

static void test_fence(void)
{
    /* Simply make sure each memory order is accepted by the fence. */
    atomic_thread_fence(MemoryOrder_Relax);
    atomic_thread_fence(MemoryOrder_Consume);
    atomic_thread_fence(MemoryOrder_Acquire);
    atomic_thread_fence(MemoryOrder_Release);
    atomic_thread_fence(MemoryOrder_AcqRel);
    atomic_thread_fence(MemoryOrder_SeqCst);

    test_report("atomic_thread_fence (all memory orders)", 1);
}

/*
 * Each worker increments 'data' TEST_ITERATIONS times using the operation
 * named after it. Together they check that every increment is applied
 * exactly once (no lost updates) under concurrency.
 */

static void* worker_add_32(void* data)
{
    int i;
    for(i = 0; i < TEST_ITERATIONS; i++)
    {
        atomic_add_32((Atomic32*)data, 1, MemoryOrder_Relax);
    }
    return NULL;
}

static void* worker_fetch_add_32(void* data)
{
    int i;
    for(i = 0; i < TEST_ITERATIONS; i++)
    {
        (void)atomic_fetch_add_32((Atomic32*)data, 1, MemoryOrder_Relax);
    }
    return NULL;
}

static void* worker_sub_32(void* data)
{
    int i;
    for(i = 0; i < TEST_ITERATIONS; i++)
    {
        atomic_sub_32((Atomic32*)data, 1, MemoryOrder_Relax);
    }
    return NULL;
}

static void* worker_fetch_sub_32(void* data)
{
    int i;
    for(i = 0; i < TEST_ITERATIONS; i++)
    {
        (void)atomic_fetch_sub_32((Atomic32*)data, 1, MemoryOrder_Relax);
    }
    return NULL;
}

static void* worker_add_64(void* data)
{
    int i;
    for(i = 0; i < TEST_ITERATIONS; i++)
    {
        atomic_add_64((Atomic64*)data, 1, MemoryOrder_Relax);
    }
    return NULL;
}

static void* worker_fetch_add_64(void* data)
{
    int i;
    for(i = 0; i < TEST_ITERATIONS; i++)
    {
        (void)atomic_fetch_add_64((Atomic64*)data, 1, MemoryOrder_Relax);
    }
    return NULL;
}

static void test_mt_add_32(void)
{
    Atomic32 counter = 0;
    run_threads_n(TEST_NUM_THREADS, worker_add_32, &counter);
    test_report("atomic_add_32 (concurrent)",
                atomic_load_32(&counter, MemoryOrder_Relax) ==
                (Atomic32)(TEST_NUM_THREADS * TEST_ITERATIONS));
}

static void test_mt_fetch_add_32(void)
{
    Atomic32 counter = 0;
    run_threads_n(TEST_NUM_THREADS, worker_fetch_add_32, &counter);
    test_report("atomic_fetch_add_32 (concurrent)",
                atomic_load_32(&counter, MemoryOrder_Relax) ==
                (Atomic32)(TEST_NUM_THREADS * TEST_ITERATIONS));
}

static void test_mt_sub_32(void)
{
    Atomic32 counter = 0;
    run_threads_n(TEST_NUM_THREADS, worker_sub_32, &counter);
    test_report("atomic_sub_32 (concurrent)",
                atomic_load_32(&counter, MemoryOrder_Relax) ==
                (Atomic32)(-(TEST_NUM_THREADS * TEST_ITERATIONS)));
}

static void test_mt_fetch_sub_32(void)
{
    Atomic32 counter = 0;
    run_threads_n(TEST_NUM_THREADS, worker_fetch_sub_32, &counter);
    test_report("atomic_fetch_sub_32 (concurrent)",
                atomic_load_32(&counter, MemoryOrder_Relax) ==
                (Atomic32)(-(TEST_NUM_THREADS * TEST_ITERATIONS)));
}

static void test_mt_add_64(void)
{
    Atomic64 counter = 0;
    run_threads_n(TEST_NUM_THREADS, worker_add_64, &counter);
    test_report("atomic_add_64 (concurrent)",
                atomic_load_64(&counter, MemoryOrder_Relax) ==
                (Atomic64)(TEST_NUM_THREADS * TEST_ITERATIONS));
}

static void test_mt_fetch_add_64(void)
{
    Atomic64 counter = 0;
    run_threads_n(TEST_NUM_THREADS, worker_fetch_add_64, &counter);
    test_report("atomic_fetch_add_64 (concurrent)",
                atomic_load_64(&counter, MemoryOrder_Relax) ==
                (Atomic64)(TEST_NUM_THREADS * TEST_ITERATIONS));
}

/*
 * Spinlock built on top of atomic_compare_exchange_weak_32.
 * The critical section deliberately uses non-atomic load/store on the
 * counter: if the lock is correct, the increment is safe.
 */
typedef struct {
    Atomic32 lock;
    Atomic32 counter;
} Spinlock;

static void* worker_spinlock(void* data)
{
    Spinlock* sl = (Spinlock*)data;
    int i;

    for(i = 0; i < TEST_ITERATIONS_SMALL; i++)
    {
        /* Acquire */
        while(!atomic_compare_exchange_weak_32(&sl->lock, 1, 0,
                                               MemoryOrder_Acquire))
        {
            /* spin */
        }

        /* Non-atomic read-modify-write protected by the lock */
        {
            Atomic32 v = atomic_load_32(&sl->counter, MemoryOrder_Relax);
            atomic_store_32(&sl->counter, v + 1, MemoryOrder_Relax);
        }

        /* Release */
        atomic_store_32(&sl->lock, 0, MemoryOrder_Release);
    }

    return NULL;
}

static void test_mt_spinlock(void)
{
    Spinlock sl;
    sl.lock = 0;
    sl.counter = 0;

    run_threads_n(TEST_NUM_THREADS, worker_spinlock, &sl);

    test_report("atomic_compare_exchange_weak_32 (spinlock correctness)",
                atomic_load_32(&sl.counter, MemoryOrder_Relax) ==
                (Atomic32)(TEST_NUM_THREADS * TEST_ITERATIONS_SMALL));
}

/*
 * Concurrent exchange: many threads race to publish their iteration index.
 * The final value must be one that was actually stored, i.e. within
 * [0, TEST_ITERATIONS_SMALL).
 */
static void* worker_exchange_32(void* data)
{
    int i;
    for(i = 0; i < TEST_ITERATIONS_SMALL; i++)
    {
        (void)atomic_exchange_32((Atomic32*)data, i, MemoryOrder_Relax);
    }
    return NULL;
}

static void test_mt_exchange(void)
{
    Atomic32 xchg = 0;
    Atomic32 final_value;

    run_threads_n(TEST_NUM_THREADS, worker_exchange_32, &xchg);

    final_value = atomic_load_32(&xchg, MemoryOrder_Relax);

    test_report("atomic_exchange_32 (concurrent)",
                final_value >= 0 && final_value < TEST_ITERATIONS_SMALL);
}

/*
 * Producer/consumer pair using acquire/release ordering. The producer
 * writes 'data' then publishes 'ready'; the consumer waits for 'ready'
 * with an acquire load and then reads 'data'.
 */
typedef struct {
    Atomic32 data;
    Atomic32 ready;
    Atomic32 observed;
} ProducerConsumer;

static void* worker_producer(void* data)
{
    ProducerConsumer* pc = (ProducerConsumer*)data;

    atomic_store_32(&pc->data, 42, MemoryOrder_Relax);
    atomic_store_32(&pc->ready, 1, MemoryOrder_Release);

    return NULL;
}

static void* worker_consumer(void* data)
{
    ProducerConsumer* pc = (ProducerConsumer*)data;

    while(atomic_load_32(&pc->ready, MemoryOrder_Acquire) == 0)
    {
        /* spin until producer signals */
    }

    /* With acquire semantics, the store to 'data' must be visible. */
    atomic_store_32(&pc->observed,
                    atomic_load_32(&pc->data, MemoryOrder_Relax),
                    MemoryOrder_Relax);

    return NULL;
}

static void test_mt_producer_consumer(void)
{
    ProducerConsumer pc;
    Thread* producer;
    Thread* consumer;

    pc.data = 0;
    pc.ready = 0;
    pc.observed = 0;

    producer = thread_create(worker_producer, &pc);
    consumer = thread_create(worker_consumer, &pc);

    thread_start(producer);
    thread_start(consumer);

    thread_join(producer);
    thread_join(consumer);

    test_report("acquire/release producer-consumer visibility",
                atomic_load_32(&pc.observed, MemoryOrder_Relax) == 42);
}

/*
 * Same idea, but using explicit fences instead of acquire/release on
 * the loads and stores. Exercises atomic_thread_fence.
 */
typedef struct {
    Atomic32 data;
    Atomic32 ready;
    Atomic32 observed;
} FenceTest;

static void* worker_fence_producer(void* data)
{
    FenceTest* ft = (FenceTest*)data;

    atomic_store_32(&ft->data, 99, MemoryOrder_Relax);
    atomic_thread_fence(MemoryOrder_Release);
    atomic_store_32(&ft->ready, 1, MemoryOrder_Relax);

    return NULL;
}

static void* worker_fence_consumer(void* data)
{
    FenceTest* ft = (FenceTest*)data;

    while(atomic_load_32(&ft->ready, MemoryOrder_Relax) == 0)
    {
        /* spin */
    }

    atomic_thread_fence(MemoryOrder_Acquire);
    atomic_store_32(&ft->observed,
                    atomic_load_32(&ft->data, MemoryOrder_Relax),
                    MemoryOrder_Relax);

    return NULL;
}

static void test_mt_fence(void)
{
    FenceTest ft;
    Thread* producer;
    Thread* consumer;

    ft.data = 0;
    ft.ready = 0;
    ft.observed = 0;

    producer = thread_create(worker_fence_producer, &ft);
    consumer = thread_create(worker_fence_consumer, &ft);

    thread_start(producer);
    thread_start(consumer);

    thread_join(producer);
    thread_join(consumer);

    test_report("atomic_thread_fence (release/acquire visibility)",
                atomic_load_32(&ft.observed, MemoryOrder_Relax) == 99);
}

/* The fetch functions are documented as returning the new value */
static void test_fetch_return_values(void)
{
    Atomic32 v32 = 10;
    Atomic64 v64 = 10;

    TEST_CHECK_EQ_INT(atomic_fetch_add_32(&v32, 5, MemoryOrder_SeqCst), 15);
    TEST_CHECK_EQ_INT(atomic_fetch_sub_32(&v32, 3, MemoryOrder_SeqCst), 12);
    TEST_CHECK_EQ_INT(atomic_fetch_and_32(&v32, 0x8, MemoryOrder_SeqCst), 8);
    TEST_CHECK_EQ_INT(atomic_fetch_or_32(&v32, 0x3, MemoryOrder_SeqCst), 11);
    TEST_CHECK_EQ_INT(atomic_fetch_xor_32(&v32, 0x1, MemoryOrder_SeqCst), 10);

    TEST_CHECK_EQ_INT(atomic_fetch_add_64(&v64, 5, MemoryOrder_SeqCst), 15);
    TEST_CHECK_EQ_INT(atomic_fetch_sub_64(&v64, 3, MemoryOrder_SeqCst), 12);
    TEST_CHECK_EQ_INT(atomic_fetch_and_64(&v64, 0x8, MemoryOrder_SeqCst), 8);
    TEST_CHECK_EQ_INT(atomic_fetch_or_64(&v64, 0x3, MemoryOrder_SeqCst), 11);
    TEST_CHECK_EQ_INT(atomic_fetch_xor_64(&v64, 0x1, MemoryOrder_SeqCst), 10);
}

TEST_MAIN(
    TEST(test_fetch_return_values),
    TEST(test_load_store_32),
    TEST(test_load_store_64),
    TEST(test_add_sub_32),
    TEST(test_add_sub_64),
    TEST(test_bitwise_32),
    TEST(test_bitwise_64),
    TEST(test_compare_exchange_32),
    TEST(test_compare_exchange_64),
    TEST(test_exchange_32),
    TEST(test_exchange_64),
    TEST(test_fence),
    TEST(test_mt_add_32),
    TEST(test_mt_fetch_add_32),
    TEST(test_mt_sub_32),
    TEST(test_mt_fetch_sub_32),
    TEST(test_mt_add_64),
    TEST(test_mt_fetch_add_64),
    TEST(test_mt_spinlock),
    TEST(test_mt_exchange),
    TEST(test_mt_producer_consumer),
    TEST(test_mt_fence),
)
