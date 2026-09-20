/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/cli.h"
#include "libromano/error.h"

static void test_error_strings(void)
{
    int code;

    TEST_CHECK_EQ_STR(error_str(ErrorCode_NoError), "No error");
    TEST_CHECK_EQ_STR(error_str(ErrorCode_MemAllocError), "Memory allocation error");
    TEST_CHECK_EQ_STR(error_str((ErrorCode)-42), "Unknown error");

    for(code = ErrorCode_MemAllocError; code <= ErrorCode_CLIMissingRequiredArg; code++)
    {
        const char* str = error_str((ErrorCode)code);

        TEST_ASSERT(str != NULL);
        TEST_CHECK_MSG(strcmp(str, "Unknown error") != 0, "code 0x%x has no description", code);
    }

    TEST_CHECK_EQ_STR(error_str((ErrorCode)(ErrorCode_CLIMissingRequiredArg + 1)), "Unknown error");
}

static void test_last_error_and_context(void)
{
    CLIParser parser;
    char long_name[1024];
    char* argv[2];

    TEST_ASSERT(cli_parser_init(&parser));

    error_get_last();
    TEST_CHECK_EQ_INT(error_get_last(), ErrorCode_NoError);

    TEST_CHECK(!cli_parser_add_arg(&parser, "input", 0, CLI_NO_SHORT_NAME, CLIArgMode_Positional,
                                   CLIArgType_Str, CLIArgAction_Append, NULL));
    TEST_CHECK_EQ_INT(error_get_last(), ErrorCode_CLIInvalidArgumentAction);
    TEST_CHECK_EQ_INT(error_get_last(), ErrorCode_NoError);
    TEST_ASSERT(error_context_str() != NULL);
    TEST_CHECK(strstr(error_context_str(), "positional") != NULL);

    long_name[0] = '-';
    long_name[1] = '-';
    memset(long_name + 2, 'x', sizeof(long_name) - 3);
    long_name[sizeof(long_name) - 1] = '\0';

    argv[0] = "program";
    argv[1] = long_name;

    TEST_CHECK(!cli_parser_parse(&parser, 2, argv));
    TEST_CHECK_EQ_STR(error_str_get_last(), "CLI: unknown argument");
    TEST_CHECK_EQ_STR(error_str_get_last(), "No error");
    TEST_CHECK_EQ_UINT(strlen(error_context_str()), 255);
    TEST_CHECK_EQ_MEM(error_context_str(), long_name, 255);

    cli_parser_release(&parser);
}

static void test_system_error(void)
{
    FILE* file = fopen("/this/path/does/not/exist", "r");

    TEST_CHECK(file == NULL);
    TEST_CHECK(error_get_last_from_system() != 0);
}

TEST_MAIN(
    TEST(test_error_strings),
    TEST(test_last_error_and_context),
    TEST(test_system_error),
)
