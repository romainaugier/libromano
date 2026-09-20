/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/atomic.h"
#include "libromano/thread.h"

#define NUM_THREADS 4
#define NUM_INCREMENTS 20000

static void test_num_procs(void)
{
    TEST_CHECK(get_num_procs() > 0);
    logger_log_info("%zu processors", get_num_procs());
}

typedef struct SharedCounter {
    Mutex mutex;
    uint64_t value;
    size_t thread_ids[NUM_THREADS];
    Atomic32 next_slot;
} SharedCounter;

static void* increment_with_mutex(void* arg)
{
    SharedCounter* counter = (SharedCounter*)arg;
    int32_t slot = atomic_fetch_add_32(&counter->next_slot, 1, MemoryOrder_SeqCst) - 1;
    int i;

    counter->thread_ids[slot] = thread_get_id();

    for(i = 0; i < NUM_INCREMENTS; i++)
    {
        mutex_lock(&counter->mutex);
        counter->value++;
        mutex_unlock(&counter->mutex);

        if(i % 1000 == 0)
            thread_yield();
    }

    return NULL;
}

static void test_threads_and_mutex(void)
{
    Thread* threads[NUM_THREADS];
    SharedCounter counter;
    size_t main_id = thread_get_id();
    int i;
    int j;

    memset(&counter, 0, sizeof(counter));
    mutex_init(&counter.mutex);

    for(i = 0; i < NUM_THREADS; i++)
    {
        threads[i] = thread_create(increment_with_mutex, &counter);
        TEST_ASSERT(threads[i] != NULL);
    }

    for(i = 0; i < NUM_THREADS; i++)
        thread_start(threads[i]);

    for(i = 0; i < NUM_THREADS; i++)
        thread_join(threads[i]);

    TEST_CHECK_EQ_UINT(counter.value, (uint64_t)NUM_THREADS * NUM_INCREMENTS);

    for(i = 0; i < NUM_THREADS; i++)
    {
        TEST_CHECK(counter.thread_ids[i] != main_id);
        TEST_CHECK(counter.thread_ids[i] != THREAD_INVALID_ID);

        for(j = i + 1; j < NUM_THREADS; j++)
            TEST_CHECK(counter.thread_ids[i] != counter.thread_ids[j]);
    }

    mutex_release(&counter.mutex);
}

static void* set_flag(void* arg)
{
    atomic_store_32((Atomic32*)arg, 1, MemoryOrder_Release);
    return NULL;
}

static void test_detach(void)
{
    Atomic32 flag = 0;
    Thread* thread = thread_create(set_flag, &flag);
    uint64_t start = test_now_ms();

    TEST_ASSERT(thread != NULL);

    thread_start(thread);
    thread_detach(thread);

    while(atomic_load_32(&flag, MemoryOrder_Acquire) == 0 && test_now_ms() - start < 5000)
        thread_yield();

    TEST_CHECK_EQ_INT(atomic_load_32(&flag, MemoryOrder_Acquire), 1);

    thread_sleep(10);
}

static void test_sleep(void)
{
    uint64_t start = test_now_ms();

    thread_sleep(50);

    TEST_CHECK(test_now_ms() - start >= 45);
}

typedef struct Mailbox {
    Mutex* mutex;
    ConditionalVariable* cond;
    int value;
    int ready;
    int consumed;
} Mailbox;

static void* consumer(void* arg)
{
    Mailbox* mailbox = (Mailbox*)arg;

    mutex_lock(mailbox->mutex);

    while(!mailbox->ready)
        conditionalvariable_wait(mailbox->cond, mailbox->mutex, 0);

    mailbox->consumed += mailbox->value;

    mutex_unlock(mailbox->mutex);

    return NULL;
}

static void test_condition_variable(void)
{
    Thread* threads[NUM_THREADS];
    Mailbox mailbox;
    int i;

    mailbox.mutex = mutex_new();
    mailbox.cond = conditionalvariable_new();
    mailbox.value = 0;
    mailbox.ready = 0;
    mailbox.consumed = 0;

    TEST_ASSERT(mailbox.mutex != NULL && mailbox.cond != NULL);

    for(i = 0; i < NUM_THREADS; i++)
    {
        threads[i] = thread_create(consumer, &mailbox);
        thread_start(threads[i]);
    }

    thread_sleep(20);

    mutex_lock(mailbox.mutex);
    mailbox.value = 7;
    mailbox.ready = 1;
    conditionalvariable_signal(mailbox.cond);
    conditionalvariable_broadcast(mailbox.cond);
    mutex_unlock(mailbox.mutex);

    for(i = 0; i < NUM_THREADS; i++)
        thread_join(threads[i]);

    TEST_CHECK_EQ_INT(mailbox.consumed, 7 * NUM_THREADS);

    conditionalvariable_free(mailbox.cond);
    mutex_free(mailbox.mutex);
}

static void test_timed_wait(void)
{
    ConditionalVariable cond;
    Mutex mutex;
    static const uint32_t durations[] = { 30, 1200 };
    size_t i;

    mutex_init(&mutex);
    conditionalvariable_init(&cond);

    for(i = 0; i < sizeof(durations) / sizeof(durations[0]); i++)
    {
        uint64_t start;
        uint64_t elapsed;

        mutex_lock(&mutex);
        start = test_now_ms();
        conditionalvariable_wait(&cond, &mutex, durations[i]);
        elapsed = test_now_ms() - start;
        mutex_unlock(&mutex);

        TEST_CHECK_MSG(elapsed + 5 >= durations[i] && elapsed < durations[i] + 2000,
                       "waited %llu ms instead of %u ms", (unsigned long long)elapsed, durations[i]);
    }

    conditionalvariable_release(&cond);
    mutex_release(&mutex);
}

typedef struct PoolState {
    ThreadPool* pool;
    ThreadPoolWaiter* waiter;
    Atomic64 counter;
    Atomic64 nested_counter;
} PoolState;

static void* pool_increment(void* arg)
{
    PoolState* state = (PoolState*)arg;
    volatile int spin;

    for(spin = 0; spin < 100; spin++);

    atomic_add_64(&state->counter, 1, MemoryOrder_Relax);

    return NULL;
}

static void* pool_nested(void* arg)
{
    PoolState* state = (PoolState*)arg;
    int i;

    atomic_add_64(&state->nested_counter, 1, MemoryOrder_Relax);

    for(i = 0; i < 4; i++)
        threadpool_work_add(state->pool, pool_increment, state, state->waiter);

    return NULL;
}

static void test_threadpool_waiter(void)
{
    ThreadPoolWaiter waiter = threadpool_waiter_new();
    PoolState state;
    int i;

    state.pool = threadpool_init(0);
    state.waiter = &waiter;
    state.counter = 0;
    state.nested_counter = 0;

    TEST_ASSERT(state.pool != NULL);

    for(i = 0; i < 1000; i++)
        TEST_ASSERT(threadpool_work_add(state.pool, i % 10 == 0 ? pool_nested : pool_increment, &state, &waiter));

    threadpool_waiter_wait(&waiter);

    TEST_CHECK_EQ_INT(atomic_load_64(&state.nested_counter, MemoryOrder_Acquire), 100);
    TEST_CHECK_EQ_INT(atomic_load_64(&state.counter, MemoryOrder_Acquire), 900 + 100 * 4);

    threadpool_release(state.pool);
}

static void test_threadpool_wait(void)
{
    PoolState state;
    int round;
    int i;

    state.pool = threadpool_init(2);
    state.waiter = NULL;
    state.counter = 0;

    TEST_ASSERT(state.pool != NULL);

    for(round = 1; round <= 200; round++)
    {
        for(i = 0; i < 16; i++)
            threadpool_work_add(state.pool, pool_increment, &state, NULL);

        threadpool_wait(state.pool);

        TEST_ASSERT_EQ_INT(atomic_load_64(&state.counter, MemoryOrder_Acquire), round * 16);
    }

    threadpool_release(state.pool);
}

static void test_threadpool_release_with_pending_work(void)
{
    ThreadPool* pool = threadpool_init(1);
    PoolState state;
    int i;

    state.pool = pool;
    state.waiter = NULL;
    state.counter = 0;

    TEST_ASSERT(pool != NULL);

    for(i = 0; i < 10000; i++)
        threadpool_work_add(pool, pool_increment, &state, NULL);

    threadpool_release(pool);

    TEST_CHECK(atomic_load_64(&state.counter, MemoryOrder_Acquire) <= 10000);
}

TEST_MAIN(
    TEST(test_num_procs),
    TEST(test_threads_and_mutex),
    TEST(test_detach),
    TEST(test_sleep),
    TEST(test_condition_variable),
    TEST(test_timed_wait),
    TEST(test_threadpool_waiter),
    TEST(test_threadpool_wait),
    TEST(test_threadpool_release_with_pending_work),
)
