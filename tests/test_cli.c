/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/cli.h"
#include "libromano/error.h"

#define ARGV(...) (char*[]){ "program", __VA_ARGS__, NULL }
#define ARGC(argv) test_argc(argv)

static int test_argc(char** argv)
{
    int argc = 0;

    while(argv[argc] != NULL)
        argc++;

    return argc;
}

static void setup_parser(CLIParser* parser)
{
    TEST_ASSERT(cli_parser_init(parser));

    cli_parser_set_program_info(parser, "program", "Test application demonstrating CLI parsing");

    TEST_ASSERT(cli_parser_add_arg(parser, "input", 0, CLI_NO_SHORT_NAME, CLIArgMode_Positional, CLIArgType_Str, CLIArgAction_Store, "Input file path"));
    TEST_ASSERT(cli_parser_add_arg(parser, "output", 0, CLI_NO_SHORT_NAME, CLIArgMode_Positional, CLIArgType_Str, CLIArgAction_Store, "Output file path"));
    TEST_ASSERT(cli_parser_add_arg(parser, "num-iterations", 0, 'n', CLIArgMode_Optional, CLIArgType_Int, CLIArgAction_Store, "Number of iterations"));
    TEST_ASSERT(cli_parser_add_arg(parser, "threshold", 0, 't', CLIArgMode_Optional, CLIArgType_Float, CLIArgAction_Store, "Threshold"));
    TEST_ASSERT(cli_parser_add_arg(parser, "verbose", 0, 'v', CLIArgMode_Optional, CLIArgType_Bool, CLIArgAction_StoreTrue, "Verbose output"));
    TEST_ASSERT(cli_parser_add_arg(parser, "quiet", 0, 'q', CLIArgMode_Optional, CLIArgType_Bool, CLIArgAction_StoreFalse, "Quiet output"));
    TEST_ASSERT(cli_parser_add_arg(parser, "level", 0, 'l', CLIArgMode_Optional, CLIArgType_Int, CLIArgAction_Count, "Level"));
    TEST_ASSERT(cli_parser_add_arg(parser, "enable-feature", 0, CLI_NO_SHORT_NAME, CLIArgMode_Optional, CLIArgType_Bool, CLIArgAction_Store, NULL));
    TEST_ASSERT(cli_parser_add_arg(parser, "name", 0, CLI_NO_SHORT_NAME, CLIArgMode_Optional, CLIArgType_Str, CLIArgAction_Store, "Name"));
}

static void test_parse_all_kinds(void)
{
    char** argv = ARGV("input.txt", "output.txt", "--verbose", "-n=10", "--threshold", "0.5",
                       "--enable-feature:true", "-l", "--level", "-l", "--name", "\"quoted\"", "-q");
    CLIParser parser;
    size_t size = 0;

    setup_parser(&parser);

    TEST_ASSERT(cli_parser_parse(&parser, ARGC(argv), argv));

    TEST_CHECK_EQ_STR(cli_parser_arg_get_str(&parser, "input", 0, &size), "input.txt");
    TEST_CHECK_EQ_UINT(size, 9);
    TEST_CHECK_EQ_STR(cli_parser_arg_get_str(&parser, "output", 0, NULL), "output.txt");
    TEST_CHECK_EQ_STR(cli_parser_arg_get_str(&parser, "name", 4, NULL), "quoted");

    TEST_ASSERT(cli_parser_has_arg(&parser, "num-iterations", 0));
    TEST_CHECK_EQ_INT(*cli_parser_arg_get_i64(&parser, "num-iterations", 0), 10);
    TEST_CHECK_NEAR(*cli_parser_arg_get_f64(&parser, "threshold", 0), 0.5, 0.0);
    TEST_CHECK(*cli_parser_arg_get_bool(&parser, "verbose", 0));
    TEST_CHECK(!*cli_parser_arg_get_bool(&parser, "quiet", 0));
    TEST_CHECK(*cli_parser_arg_get_bool(&parser, "enable-feature", 0));
    TEST_CHECK_EQ_INT(*cli_parser_arg_get_i64(&parser, "level", 0), 3);

    TEST_CHECK(cli_parser_arg_get_i64(&parser, "threshold", 0) == NULL);
    TEST_CHECK(cli_parser_arg_get_f64(&parser, "verbose", 0) == NULL);
    TEST_CHECK(cli_parser_arg_get_str(&parser, "level", 0, NULL) == NULL);
    TEST_CHECK(cli_parser_arg_get_bool(&parser, "input", 0) == NULL);
    TEST_CHECK(cli_parser_arg_get_i64(&parser, "missing", 0) == NULL);
    TEST_CHECK(cli_parser_arg_get_f64(&parser, "missing", 0) == NULL);
    TEST_CHECK(cli_parser_arg_get_str(&parser, "missing", 0, NULL) == NULL);
    TEST_CHECK(cli_parser_arg_get_bool(&parser, "missing", 0) == NULL);
    TEST_CHECK(!cli_parser_has_arg(&parser, "missing", 0));

    cli_parser_print_help(&parser);
    cli_parser_release(&parser);
}

static void test_bool_values(void)
{
    static const char* const values[] = { "1", "0", "true", "false", "True", "False" };
    static const bool expected[] = { true, false, true, false, true, false };
    size_t i;

    for(i = 0; i < sizeof(values) / sizeof(values[0]); i++)
    {
        char argument[64];
        char** argv = ARGV("in", "out", argument);
        CLIParser parser;

        snprintf(argument, sizeof(argument), "--enable-feature=%s", values[i]);

        setup_parser(&parser);
        TEST_ASSERT(cli_parser_parse(&parser, ARGC(argv), argv));
        TEST_CHECK_MSG(*cli_parser_arg_get_bool(&parser, "enable-feature", 0) == expected[i], "value %s", values[i]);
        cli_parser_release(&parser);
    }
}

static void test_no_positional_arguments(void)
{
    char** argv = ARGV("--count=3");
    CLIParser parser;

    TEST_ASSERT(cli_parser_init(&parser));
    TEST_ASSERT(cli_parser_add_arg(&parser, "count", 0, 'c', CLIArgMode_Optional, CLIArgType_Int, CLIArgAction_Store, NULL));

    TEST_CHECK(cli_parser_parse(&parser, ARGC(argv), argv));
    TEST_CHECK_EQ_INT(*cli_parser_arg_get_i64(&parser, "count", 0), 3);

    cli_parser_release(&parser);
}

static void test_repeated_string_argument(void)
{
    char** argv = ARGV("in", "out", "--name", "first", "--name=second");
    CLIParser parser;

    setup_parser(&parser);
    TEST_ASSERT(cli_parser_parse(&parser, ARGC(argv), argv));
    TEST_CHECK_EQ_STR(cli_parser_arg_get_str(&parser, "name", 0, NULL), "second");
    cli_parser_release(&parser);
}

static void test_required_named_argument(void)
{
    char** with = ARGV("--config", "file.cfg");
    char** without = ARGV("--other=1");
    CLIParser parser;

    TEST_ASSERT(cli_parser_init(&parser));
    TEST_ASSERT(cli_parser_add_arg(&parser, "config", 0, CLI_NO_SHORT_NAME, CLIArgMode_Named, CLIArgType_Str, CLIArgAction_Store, NULL));
    TEST_ASSERT(cli_parser_add_arg(&parser, "other", 0, CLI_NO_SHORT_NAME, CLIArgMode_Optional, CLIArgType_Int, CLIArgAction_Store, NULL));
    TEST_CHECK(cli_parser_parse(&parser, ARGC(with), with));
    cli_parser_release(&parser);

    TEST_ASSERT(cli_parser_init(&parser));
    TEST_ASSERT(cli_parser_add_arg(&parser, "config", 0, CLI_NO_SHORT_NAME, CLIArgMode_Named, CLIArgType_Str, CLIArgAction_Store, NULL));
    TEST_ASSERT(cli_parser_add_arg(&parser, "other", 0, CLI_NO_SHORT_NAME, CLIArgMode_Optional, CLIArgType_Int, CLIArgAction_Store, NULL));
    TEST_CHECK(!cli_parser_parse(&parser, ARGC(without), without));
    TEST_CHECK_EQ_INT(error_get_last(), ErrorCode_CLIMissingRequiredArg);
    cli_parser_release(&parser);
}

typedef struct ErrorCase {
    char** argv;
    ErrorCode expected;
} ErrorCase;

static void test_errors(void)
{
    const ErrorCase cases[] = {
        { ARGV("in"), ErrorCode_CLIMissingPositionalArgs },
        { ARGV("in", "out", "extra"), ErrorCode_CLITooManyPositionalArgs },
        { ARGV("in", "out", "--unknown"), ErrorCode_CLIUnknownArgument },
        { ARGV("in", "out", "-x"), ErrorCode_CLIUnknownArgument },
        { ARGV("in", "out", "--"), ErrorCode_CLIMalformedArgument },
        { ARGV("in", "out", "--threshold"), ErrorCode_CLIMalformedArgument },
        { ARGV("in", "out", "--threshold="), ErrorCode_CLIMalformedArgument },
        { ARGV("in", "out", "-n", "abc"), ErrorCode_CLIInvalidArgumentType },
        { ARGV("in", "out", "--threshold=x1"), ErrorCode_CLIInvalidArgumentType },
        { ARGV("in", "out", "--enable-feature=maybe"), ErrorCode_CLIInvalidArgumentType },
    };
    size_t i;

    for(i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        CLIParser parser;

        setup_parser(&parser);
        error_get_last();

        TEST_CHECK_MSG(!cli_parser_parse(&parser, ARGC(cases[i].argv), cases[i].argv), "case %zu should fail", i);
        TEST_CHECK_MSG(error_get_last() == cases[i].expected, "case %zu: wrong error", i);

        cli_parser_release(&parser);
    }
}

static void test_help_and_empty(void)
{
    char** help = ARGV("--help");
    char** empty = (char*[]){ "program", NULL };
    CLIParser parser;

    setup_parser(&parser);
    TEST_CHECK(!cli_parser_parse(&parser, ARGC(help), help));
    cli_parser_release(&parser);

    TEST_ASSERT(cli_parser_init(&parser));
    TEST_CHECK(cli_parser_parse(&parser, 1, empty));
    cli_parser_print_help(&parser);
    cli_parser_release(&parser);
}

static const char* const g_tokens[] = {
    "in", "out", "extra", "--verbose", "-v", "-n=5", "-n", "12", "--threshold", "0.25", "-t:1e3",
    "--enable-feature=true", "--enable-feature=0", "-l", "--level", "-q", "--name", "\"x\"", "--name=abc",
    "--", "-", "--unknown", "=", "--num-iterations=-3", "--threshold=.5", "", "--name=\"", "-h",
};

static bool property_random_command_lines(FuzzSource* source, void* user_data)
{
    char* argv[16];
    const int argc = (int)fuzz_range(source, 1, 15);
    CLIParser parser;
    int i;

    ROMANO_UNUSED(user_data);

    argv[0] = "program";

    for(i = 1; i < argc; i++)
        argv[i] = (char*)g_tokens[fuzz_index(source, sizeof(g_tokens) / sizeof(g_tokens[0]))];

    argv[argc] = NULL;

    setup_parser(&parser);

    if(cli_parser_parse(&parser, argc, argv) && argc > 1)
    {
        TEST_FUZZ_CHECK(cli_parser_arg_get_str(&parser, "input", 0, NULL) != NULL);
        TEST_FUZZ_CHECK(cli_parser_arg_get_str(&parser, "output", 0, NULL) != NULL);
    }

    cli_parser_release(&parser);

    return true;
}

static void test_fuzz_command_lines(void)
{
    logger_set_level(LogLevel_Fatal);
    test_fuzz_property("cli_command_lines", 3000, property_random_command_lines, NULL);
    logger_set_level(LogLevel_Info);
}

TEST_MAIN(
    TEST(test_parse_all_kinds),
    TEST(test_bool_values),
    TEST(test_no_positional_arguments),
    TEST(test_repeated_string_argument),
    TEST(test_required_named_argument),
    TEST(test_errors),
    TEST(test_help_and_empty),
    TEST(test_fuzz_command_lines),
)
