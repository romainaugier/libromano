/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/backtrace.h"

#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif /* defined(ROMANO_LINUX) || defined(ROMANO_APPLE) */

#define MAX_FRAMES 32

static ROMANO_NO_INLINE uint32_t capture_symbols(uint32_t skip, char** symbols, void** addresses)
{
    return backtrace_call_stack_symbols(skip, MAX_FRAMES, symbols, addresses);
}

static ROMANO_NO_INLINE uint32_t capture_nested(uint32_t skip, char** symbols, void** addresses)
{
    return capture_symbols(skip, symbols, addresses);
}

static void test_call_stack(void)
{
    void* addresses[MAX_FRAMES];
    uint32_t count = backtrace_call_stack(0, MAX_FRAMES, addresses);
    uint32_t i;

    TEST_ASSERT(count > 0 && count <= MAX_FRAMES);

    for(i = 0; i < count; i++)
        TEST_CHECK(addresses[i] != NULL);

    TEST_CHECK(backtrace_call_stack(0, 2, addresses) <= 2);
}

static void test_call_stack_symbols(void)
{
    void* addresses[MAX_FRAMES];
    char* symbols[MAX_FRAMES];
    uint32_t count = capture_nested(0, symbols, addresses);
    uint32_t skipped = capture_nested(1, symbols + count, addresses + count) ;
    uint32_t i;

    TEST_ASSERT(count > 2);
    TEST_CHECK(skipped < count);

    for(i = 0; i < count + skipped && i < MAX_FRAMES; i++)
    {
        TEST_CHECK(symbols[i] != NULL);
        logger_log_debug("#%u %p : %s", i, addresses[i], symbols[i]);
        free(symbols[i]);
    }
}

#if defined(ROMANO_LINUX)
static void test_signal_handler(void)
{
    pid_t pid = fork();
    int status = 0;

    TEST_ASSERT(pid >= 0);

    if(pid == 0)
    {
        backtrace_install_signal_handler();
        raise(SIGFPE);
        _exit(0);
    }

    TEST_ASSERT(waitpid(pid, &status, 0) == pid);
    TEST_CHECK(WIFEXITED(status));
    TEST_CHECK_EQ_INT(WEXITSTATUS(status), 1);
}
#endif /* defined(ROMANO_LINUX) */

#if defined(ROMANO_LINUX)
#define PLATFORM_TESTS TEST(test_signal_handler),
#else
#define PLATFORM_TESTS
#endif /* defined(ROMANO_LINUX) */

TEST_MAIN(
    TEST(test_call_stack),
    TEST(test_call_stack_symbols),
    PLATFORM_TESTS
)
