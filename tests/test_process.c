/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/process.h"
#include "libromano/env.h"
#include "libromano/os.h"
#include "libromano/time.h"

#if defined(ROMANO_WIN)
#define SHELL "cmd", "/c"
#else
#define SHELL "/bin/sh", "-c"
#endif /* defined(ROMANO_WIN) */

static void test_process_capture(void)
{
    const char* argv[] = { SHELL, "echo hello && echo oops 1>&2", NULL };
    ProcessOptions options;
    ProcessResult result;

    memset(&options, 0, sizeof(ProcessOptions));
    options.argv = argv;
    options.flags = ProcessFlag_CaptureStdout | ProcessFlag_CaptureStderr;

    TEST_CHECK(process_run(&options, &result));
    TEST_CHECK_EQ_INT(result.exit_code, 0);
    TEST_CHECK(strncmp(result.out, "hello", 5) == 0);
    TEST_CHECK(strncmp(result.err, "oops", 4) == 0);

    process_result_release(&result);
}

static void test_process_exit_code(void)
{
    const char* argv[] = { SHELL, "exit 3", NULL };
    ProcessOptions options;
    ProcessResult result;

    memset(&options, 0, sizeof(ProcessOptions));
    options.argv = argv;

    TEST_CHECK(process_run(&options, &result));
    TEST_CHECK_EQ_INT(result.exit_code, 3);
    TEST_CHECK(result.out == NULL);

    process_result_release(&result);
}

static void test_process_not_found(void)
{
    const char* argv[] = { "romano_this_command_does_not_exist", NULL };
    ProcessOptions options;
    ProcessResult result;

    memset(&options, 0, sizeof(ProcessOptions));
    options.argv = argv;

    TEST_CHECK(!process_run(&options, &result));
}

static void test_process_large_output(void)
{
#if defined(ROMANO_WIN)
    const char* argv[] = { SHELL, "for /L %i in (1,1,20000) do @echo 0123456789", NULL };
#else
    const char* argv[] = { SHELL, "i=0; while [ $i -lt 20000 ]; do echo 0123456789; echo err 1>&2; i=$((i+1)); done", NULL };
#endif /* defined(ROMANO_WIN) */
    ProcessOptions options;
    ProcessResult result;

    memset(&options, 0, sizeof(ProcessOptions));
    options.argv = argv;
    options.flags = ProcessFlag_CaptureStdout | ProcessFlag_CaptureStderr;

    TEST_CHECK(process_run(&options, &result));
    TEST_CHECK(result.out_sz >= 20000 * 11);

    process_result_release(&result);
}

static void test_process_env_cwd(void)
{
#if defined(ROMANO_WIN)
    const char* argv[] = { SHELL, "echo %ROMANO_TEST_VAR%", NULL };
    const char* env[] = { "ROMANO_TEST_VAR=bob", "SystemRoot=C:\\Windows", NULL };
#else
    const char* argv[] = { SHELL, "echo $ROMANO_TEST_VAR; pwd", NULL };
    const char* env[] = { "ROMANO_TEST_VAR=bob", NULL };
#endif /* defined(ROMANO_WIN) */
    ProcessOptions options;
    ProcessResult result;
    char tmp[1024];

    TEST_CHECK(os_temp_dir(tmp, sizeof(tmp)));

    memset(&options, 0, sizeof(ProcessOptions));
    options.argv = argv;
    options.env = env;
    options.cwd = tmp;
    options.flags = ProcessFlag_CaptureStdout;

    TEST_CHECK(process_run(&options, &result));
    TEST_CHECK(strncmp(result.out, "bob", 3) == 0);

    process_result_release(&result);
}

static void test_process_timeout(void)
{
#if defined(ROMANO_WIN)
    const char* argv[] = { "powershell", "-NoProfile", "-Command", "Start-Sleep -Seconds 5", NULL };
#else
    const char* argv[] = { SHELL, "echo started; sleep 5; echo never", NULL };
#endif /* defined(ROMANO_WIN) */
    ProcessOptions options;
    ProcessResult result;
    uint64_t start;

    memset(&options, 0, sizeof(ProcessOptions));
    options.argv = argv;
    options.flags = ProcessFlag_CaptureStdout;
    options.timeout_ms = 200;

    start = time_monotonic_ns();
    TEST_CHECK(process_run(&options, &result));
    TEST_CHECK(result.timed_out);
    TEST_CHECK(time_monotonic_ns() - start < 3000000000ULL);
    process_result_release(&result);

    options.timeout_ms = 5000;
    options.argv = (const char* const[]){ SHELL, "exit 0", NULL };
    TEST_CHECK(process_run(&options, &result));
    TEST_CHECK(!result.timed_out);
    TEST_CHECK_EQ_INT(result.exit_code, 0);
    process_result_release(&result);
}

static void test_env(void)
{
    char** list;
    bool found = false;
    size_t i;

    TEST_CHECK(env_set("ROMANO_ENV_TEST", "42"));
    TEST_CHECK_EQ_STR(env_get("ROMANO_ENV_TEST"), "42");

    list = env_list_new();
    TEST_CHECK(list != NULL);

    for(i = 0; list[i] != NULL; i++)
        if(strcmp(list[i], "ROMANO_ENV_TEST=42") == 0)
            found = true;

    TEST_CHECK(found);
    env_list_free(list);

    TEST_CHECK(env_unset("ROMANO_ENV_TEST"));
    TEST_CHECK(env_get("ROMANO_ENV_TEST") == NULL || env_get("ROMANO_ENV_TEST")[0] == '\0');
}

static void test_os(void)
{
    char buffer[1024];
    uint64_t t0;

    TEST_CHECK(os_hostname(buffer, sizeof(buffer)));
    TEST_CHECK(os_version(buffer, sizeof(buffer)));
    TEST_CHECK(os_exe_path(buffer, sizeof(buffer)));
    TEST_CHECK(os_home_dir(buffer, sizeof(buffer)));
    TEST_CHECK(os_memory_total() > 0);
    TEST_CHECK(os_memory_available() <= os_memory_total());

    t0 = time_monotonic_ns();
    TEST_CHECK(time_monotonic_ns() >= t0);
}

TEST_MAIN(
    TEST(test_process_capture),
    TEST(test_process_exit_code),
    TEST(test_process_not_found),
    TEST(test_process_large_output),
    TEST(test_process_env_cwd),
    TEST(test_process_timeout),
    TEST(test_env),
    TEST(test_os),
)
