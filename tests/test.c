/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/filesystem.h"

#include <setjmp.h>
#include <stdarg.h>
#include <time.h>

#if defined(ROMANO_WIN)
#include <Windows.h>
#endif /* defined(ROMANO_WIN) */

#if !defined(TESTS_TMP_DIR)
#define TESTS_TMP_DIR "tests_tmp"
#endif /* !defined(TESTS_TMP_DIR) */

#define TEST_MESSAGE_SIZE 512
#define TEST_TMP_PATHS 8

static jmp_buf g_abort_jump;
static bool g_case_running = false;
static uint64_t g_case_checks = 0;
static uint64_t g_case_failures = 0;
static uint64_t g_total_checks = 0;

static void log_fuzz_context(void)
{
    const FuzzRunInfo* run = fuzz_current_run();

    if(run == NULL || run->minimizing)
        return;

    logger_log_error("    while fuzzing '%s' at iteration %llu (ROMANO_FUZZ_REPLAY=0x%016llx)",
                     run->name,
                     (unsigned long long)run->iteration,
                     (unsigned long long)run->iteration_seed);
}

static bool record(bool ok)
{
    g_case_checks++;

    if(!ok)
        g_case_failures++;

    return ok;
}

bool test_check(bool ok, const char* file, int line, const char* expr)
{
    if(!record(ok))
    {
        logger_log_error("%s:%d: check failed: %s", file, line, expr);
        log_fuzz_context();
    }

    return ok;
}

static void log_vmessage(const char* format, va_list args)
{
    char message[TEST_MESSAGE_SIZE];

    vsnprintf(message, sizeof(message), format, args);
    logger_log_error("    %s", message);
}

bool test_checkf(bool ok, const char* file, int line, const char* expr, const char* format, ...)
{
    va_list args;

    if(record(ok))
        return true;

    logger_log_error("%s:%d: check failed: %s", file, line, expr);

    va_start(args, format);
    log_vmessage(format, args);
    va_end(args);

    log_fuzz_context();

    return false;
}

bool test_check_int(int64_t a, int64_t b, const char* file, int line, const char* expr)
{
    bool ok = a == b;

    if(!record(ok))
    {
        logger_log_error("%s:%d: check failed: %s (%lld vs %lld)", file, line, expr, (long long)a, (long long)b);
        log_fuzz_context();
    }

    return ok;
}

bool test_check_uint(uint64_t a, uint64_t b, const char* file, int line, const char* expr)
{
    bool ok = a == b;

    if(!record(ok))
    {
        logger_log_error("%s:%d: check failed: %s (%llu vs %llu, 0x%llx vs 0x%llx)",
                         file, line, expr,
                         (unsigned long long)a, (unsigned long long)b,
                         (unsigned long long)a, (unsigned long long)b);
        log_fuzz_context();
    }

    return ok;
}

bool test_check_near(double a, double b, double epsilon, const char* file, int line, const char* expr)
{
    bool ok = fabs(a - b) <= epsilon || (isnan(a) && isnan(b)) || (isinf(a) && a == b);

    if(!record(ok))
    {
        logger_log_error("%s:%d: check failed: %s (%.17g vs %.17g)", file, line, expr, a, b);
        log_fuzz_context();
    }

    return ok;
}

bool test_check_str(const char* a, const char* b, const char* file, int line, const char* expr)
{
    bool ok = (a == NULL && b == NULL) || (a != NULL && b != NULL && strcmp(a, b) == 0);

    if(!record(ok))
    {
        logger_log_error("%s:%d: check failed: %s (\"%s\" vs \"%s\")",
                         file, line, expr,
                         a != NULL ? a : "(null)",
                         b != NULL ? b : "(null)");
        log_fuzz_context();
    }

    return ok;
}

bool test_check_mem(const void* a, const void* b, size_t size, const char* file, int line, const char* expr)
{
    bool ok = size == 0 || (a != NULL && b != NULL && memcmp(a, b, size) == 0);

    if(!record(ok))
    {
        logger_log_error("%s:%d: check failed: %s (%zu bytes differ)", file, line, expr, size);

        if(a != NULL && b != NULL)
        {
            fuzz_log_input((const uint8_t*)a, size, 64);
            fuzz_log_input((const uint8_t*)b, size, 64);
        }

        log_fuzz_context();
    }

    return ok;
}

void test_abort(void)
{
    if(g_case_running)
        longjmp(g_abort_jump, 1);

    abort();
}

bool test_fuzz_failure(const char* file, int line, const char* expr, const char* format, ...)
{
    va_list args;
    const FuzzRunInfo* run = fuzz_current_run();

    if(run != NULL && run->minimizing)
        return false;

    logger_log_error("%s:%d: fuzz check failed: %s", file, line, expr);

    if(format != NULL)
    {
        va_start(args, format);
        log_vmessage(format, args);
        va_end(args);
    }

    return false;
}

bool test_fuzz_property(const char* name, uint64_t iterations, FuzzPropertyFunc property, void* user_data)
{
    FuzzOptions options;

    fuzz_options_init(&options, name);
    options.iterations = test_scaled(iterations);

    return test_fuzz_property_with(&options, property, user_data);
}

bool test_fuzz_property_with(const FuzzOptions* options, FuzzPropertyFunc property, void* user_data)
{
    FuzzReport report;
    bool ok = fuzz_run_property(options, property, user_data, &report);

    g_total_checks += report.iterations;
    test_checkf(ok, __FILE__, __LINE__, "fuzz_run_property", "property '%s' failed", options->name);

    fuzz_report_release(&report);

    return ok;
}

bool test_fuzz_input(const FuzzOptions* options, FuzzInputFunc target, void* user_data)
{
    FuzzReport report;
    bool ok = fuzz_run_input(options, target, user_data, &report);

    g_total_checks += report.iterations;
    test_checkf(ok, __FILE__, __LINE__, "fuzz_run_input", "target '%s' failed", options->name);

    fuzz_report_release(&report);

    return ok;
}

const char* test_tmp_path(const char* name)
{
    static char paths[TEST_TMP_PATHS][MAX_PATH];
    static size_t next = 0;
    char* path = paths[next++ % TEST_TMP_PATHS];

    if(!fs_path_exists(TESTS_TMP_DIR))
        fs_makedirs(TESTS_TMP_DIR);

    snprintf(path, MAX_PATH, "%s/%s", TESTS_TMP_DIR, name);

#if defined(ROMANO_WIN)
    {
        char* c;

        for(c = path; *c != '\0'; c++)
            if(*c == '/')
                *c = '\\';
    }
#endif /* defined(ROMANO_WIN) */

    return path;
}

uint64_t test_now_ms(void)
{
#if defined(ROMANO_WIN)
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif /* defined(ROMANO_WIN) */
}

void test_setenv(const char* name, const char* value)
{
#if defined(ROMANO_WIN)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif /* defined(ROMANO_WIN) */
}

void test_unsetenv(const char* name)
{
#if defined(ROMANO_WIN)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif /* defined(ROMANO_WIN) */
}

uint64_t test_scaled(uint64_t count)
{
#if ROMANO_DEBUG
    return count > 4 ? count / 4 : 1;
#else
    return count;
#endif /* ROMANO_DEBUG */
}

static bool case_selected(const char* name, int argc, char** argv)
{
    bool has_filter = false;
    int i;

    for(i = 1; i < argc; i++)
    {
        if(argv[i][0] == '-')
            continue;

        has_filter = true;

        if(strstr(name, argv[i]) != NULL)
            return true;
    }

    return !has_filter;
}

static bool has_flag(int argc, char** argv, const char* flag)
{
    int i;

    for(i = 1; i < argc; i++)
        if(strcmp(argv[i], flag) == 0)
            return true;

    return false;
}

static bool run_case(const TestCase* test_case)
{
    clock_t start = clock();
    double elapsed;

    g_case_checks = 0;
    g_case_failures = 0;
    g_case_running = true;

    logger_log_debug("[ RUN  ] %s", test_case->name);

    if(setjmp(g_abort_jump) == 0)
        test_case->func();
    else
        logger_log_error("%s: aborted", test_case->name);

    g_case_running = false;
    fuzz_reset();

    elapsed = (double)(clock() - start) / (double)CLOCKS_PER_SEC;
    g_total_checks += g_case_checks;

    if(g_case_failures > 0)
    {
        logger_log_error("[ FAIL ] %s (%llu/%llu checks failed)",
                         test_case->name,
                         (unsigned long long)g_case_failures,
                         (unsigned long long)g_case_checks);
        return false;
    }

    logger_log_info("[  OK  ] %s (%llu checks, %.3fs)",
                    test_case->name,
                    (unsigned long long)g_case_checks,
                    elapsed);

    return true;
}

int test_main(int argc, char** argv, const TestCase* cases, size_t count)
{
    size_t run = 0;
    size_t failed = 0;
    size_t i;

    logger_init();
    logger_set_level(has_flag(argc, argv, "-v") ? LogLevel_Debug : LogLevel_Info);

    if(has_flag(argc, argv, "--list"))
    {
        for(i = 0; i < count; i++)
            logger_log_info("%s", cases[i].name);

        logger_release();
        return 0;
    }

    for(i = 0; i < count; i++)
    {
        if(!case_selected(cases[i].name, argc, argv))
            continue;

        run++;

        if(!run_case(&cases[i]))
            failed++;
    }

    if(failed > 0)
        logger_log_error("%zu/%zu test cases failed", failed, run);
    else
        logger_log_info("%zu test cases passed (%llu checks)", run, (unsigned long long)g_total_checks);

    logger_release();

    return failed > 0 || run == 0 ? 1 : 0;
}
