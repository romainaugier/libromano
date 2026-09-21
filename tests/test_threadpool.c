/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/atomic.h"
#include "libromano/threadpool.h"
#include "libromano/thread.h"

#include <time.h>

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

typedef struct CopyArg {
    Atomic64* sum;
    int64_t value;
    uint32_t index;
} CopyArg;

static void* pool_copy_sum(void* arg)
{
    CopyArg* copy = (CopyArg*)arg;

    atomic_add_64(copy->sum, copy->value, MemoryOrder_Relax);

    /* The copy belongs to the job and is writable */
    copy->value = -1;

    return NULL;
}

typedef struct BigArg {
    Atomic64* sum;
    uint8_t payload[THREADPOOL_WORK_INLINE_ARG_SIZE * 3];
} BigArg;

static void* pool_big_sum(void* arg)
{
    const BigArg* big = (const BigArg*)arg;
    int64_t sum = 0;
    size_t i;

    for(i = 0; i < sizeof(big->payload); i++)
        sum += big->payload[i];

    atomic_add_64(big->sum, sum, MemoryOrder_Relax);

    return NULL;
}

/* Arguments are copied: the source can be reused/overwritten right after the call */
static void test_threadpool_work_add_copy(void)
{
    ThreadPool* pool = threadpool_init(0);
    ThreadPoolWaiter waiter = threadpool_waiter_new();
    Atomic64 sum = 0;
    int64_t expected = 0;
    CopyArg arg;
    BigArg big;
    int i;

    TEST_ASSERT(pool != NULL);

    TEST_CHECK(sizeof(CopyArg) <= THREADPOOL_WORK_INLINE_ARG_SIZE);

    /* More jobs than slab nodes, exercises the heap fallback too */
    arg.sum = &sum;

    for(i = 0; i < THREADPOOL_WORK_SLAB_SIZE * 3; i++)
    {
        arg.value = i;
        arg.index = (uint32_t)i;
        expected += i;

        TEST_ASSERT(threadpool_work_add_copy(pool, pool_copy_sum, &arg, sizeof(arg), &waiter));
    }

    threadpool_waiter_wait(&waiter);

    TEST_CHECK_EQ_INT(atomic_load_64(&sum, MemoryOrder_Acquire), expected);

    /* Arguments bigger than the inline storage */
    sum = 0;
    big.sum = &sum;

    for(i = 0; i < (int)sizeof(big.payload); i++)
        big.payload[i] = (uint8_t)(i & 0x7F);

    expected = 0;

    for(i = 0; i < (int)sizeof(big.payload); i++)
        expected += big.payload[i];

    for(i = 0; i < 100; i++)
        TEST_ASSERT(threadpool_work_add_copy(pool, pool_big_sum, &big, sizeof(big), &waiter));

    threadpool_waiter_wait(&waiter);

    TEST_CHECK_EQ_INT(atomic_load_64(&sum, MemoryOrder_Acquire), expected * 100);

    threadpool_release(pool);
}

typedef struct NestedWait {
    ThreadPool* pool;
    Atomic64* counter;
    int depth;
} NestedWait;

static void* pool_nested_wait(void* arg)
{
    NestedWait* nested = (NestedWait*)arg;
    ThreadPoolWaiter waiter = threadpool_waiter_new();
    NestedWait child;
    int i;

    atomic_add_64(nested->counter, 1, MemoryOrder_Relax);

    if(nested->depth == 0)
        return NULL;

    child = *nested;
    child.depth--;

    for(i = 0; i < 4; i++)
        threadpool_work_add_copy(nested->pool, pool_nested_wait, &child, sizeof(child), &waiter);

    /* Every worker ends up here: a plain spin would deadlock the 2 workers pool */
    threadpool_waiter_wait_help(nested->pool, &waiter);

    return NULL;
}

static void test_threadpool_wait_help_nested(void)
{
    ThreadPool* pool = threadpool_init(2);
    ThreadPoolWaiter waiter = threadpool_waiter_new();
    Atomic64 counter = 0;
    NestedWait root;
    int i;

    TEST_ASSERT(pool != NULL);
    TEST_CHECK(!threadpool_is_worker_thread(pool));
    TEST_CHECK_EQ_INT(threadpool_get_workers_count(pool), 2);

    root.pool = pool;
    root.counter = &counter;
    root.depth = 3;

    for(i = 0; i < 4; i++)
        TEST_ASSERT(threadpool_work_add_copy(pool, pool_nested_wait, &root, sizeof(root), &waiter));

    threadpool_waiter_wait_help(pool, &waiter);

    /* 4 + 16 + 64 + 256 */
    TEST_CHECK_EQ_INT(atomic_load_64(&counter, MemoryOrder_Acquire), 340);

    threadpool_release(pool);
}

/* Idle workers must park (not burn CPU) and wake up for new work */
static void test_threadpool_idle_sleep(void)
{
    ThreadPool* pool = threadpool_init(4);
    PoolState state;
    clock_t cpu_start;
    double cpu_seconds;
    int round;
    int i;

    TEST_ASSERT(pool != NULL);

    state.pool = pool;
    state.waiter = NULL;
    state.counter = 0;
    state.nested_counter = 0;

    /* Let the workers go through their spin phase and fall asleep */
    thread_sleep(100);

    cpu_start = clock();
    thread_sleep(300);
    cpu_seconds = (double)(clock() - cpu_start) / CLOCKS_PER_SEC;

    /* 4 spinning workers would burn ~300ms per core during the sleep */
    TEST_CHECK_MSG(cpu_seconds < 0.1, "idle pool used %.3fs of CPU in 0.3s", cpu_seconds);

    /* Wake-ups after sleeping, repeatedly */
    for(round = 1; round <= 20; round++)
    {
        ThreadPoolWaiter waiter = threadpool_waiter_new();

        state.waiter = &waiter;

        for(i = 0; i < 8; i++)
            TEST_ASSERT(threadpool_work_add(pool, pool_increment, &state, &waiter));

        threadpool_waiter_wait(&waiter);

        TEST_ASSERT_EQ_INT(atomic_load_64(&state.counter, MemoryOrder_Acquire), round * 8);

        if(round % 5 == 0)
            thread_sleep(50);
    }

    threadpool_release(pool);
}

TEST_MAIN(
    TEST(test_threadpool_waiter),
    TEST(test_threadpool_wait),
    TEST(test_threadpool_release_with_pending_work),
    TEST(test_threadpool_work_add_copy),
    TEST(test_threadpool_wait_help_nested),
    TEST(test_threadpool_idle_sleep),
)
