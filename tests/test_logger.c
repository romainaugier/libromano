/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/filesystem.h"

static size_t count_occurrences(const char* haystack, const char* needle)
{
    size_t count = 0;

    while((haystack = strstr(haystack, needle)) != NULL)
    {
        count++;
        haystack++;
    }

    return count;
}

static void test_file_logging(void)
{
    const char* path = test_tmp_path("test_logger.log");
    FileContent content;

    fs_remove(path);

    logger_set_level(LogLevel_Warning);
    logger_disable_console();
    logger_enable_file(path);

    logger_log_debug("debug message");
    logger_log_info("info message");
    logger_log_warning("warning message %d", 1);
    logger_log_error("error message %s", "two");
    logger_log(LogLevel_Fatal, "fatal message");
    logger_log((log_level)42, "clamped level");

    logger_disable_file();
    logger_enable_console();
    logger_set_level(LogLevel_Info);

    TEST_ASSERT(fs_file_content_init(&content, path, false));

    TEST_CHECK_EQ_UINT(count_occurrences(content.content, "debug message"), 0);
    TEST_CHECK_EQ_UINT(count_occurrences(content.content, "info message"), 0);
    TEST_CHECK_EQ_UINT(count_occurrences(content.content, "[WARNING]"), 1);
    TEST_CHECK_EQ_UINT(count_occurrences(content.content, "warning message 1"), 1);
    TEST_CHECK_EQ_UINT(count_occurrences(content.content, "[ERROR]"), 1);
    TEST_CHECK_EQ_UINT(count_occurrences(content.content, "error message two"), 1);
    TEST_CHECK_EQ_UINT(count_occurrences(content.content, "[FATAL]"), 1);
    TEST_CHECK_EQ_UINT(count_occurrences(content.content, "clamped level"), 0);

    fs_file_content_release(&content);
}

static void test_levels(void)
{
    logger_set_level((log_level)99);
    logger_log_debug("visible at the debug level");
    logger_set_level(LogLevel_Info);
    logger_log_info("long message %0512d", 0);
    TEST_CHECK(true);
}

static void test_reinit_and_release(void)
{
    const char* path = test_tmp_path("test_logger_release.log");

    logger_init();
    logger_enable_file(path);
    logger_log_info("written before release");
    logger_release();

    logger_init();
    logger_disable_file();
    logger_log_info("still usable after release");

    TEST_CHECK(fs_path_exists(path));
}

TEST_MAIN(
    TEST(test_file_logging),
    TEST(test_levels),
    TEST(test_reinit_and_release),
)
