/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/cli.h"
#include "libromano/logger.h"
#include "libromano/error.h"

int main(int argc, char** argv)
{
    logger_init();
    logger_set_level(LogLevel_Debug);

    logger_log_info("Starting cli test");

    CLIParser parser;

    if(!cli_parser_init(&parser))
    {
        logger_log_error("Could not initialize cli parser");
        return 1;
    }

    if(!cli_parser_add_arg(&parser, "int_arg", 0, CLIArgType_Int, CLIArgAction_Store))
    {
        logger_log_error("Could not add argument to cli parser");
        return 1;
    }

    if(!cli_parser_add_arg(&parser, "double_arg", 0, CLIArgType_Float, CLIArgAction_Store))
    {
        logger_log_error("Could not add argument to cli parser");
        return 1;
    }

    if(!cli_parser_add_arg(&parser, "str_arg", 0, CLIArgType_Str, CLIArgAction_Store))
    {
        logger_log_error("Could not add argument to cli parser");
        return 1;
    }

    if(!cli_parser_add_arg(&parser, "bool_arg", 0, CLIArgType_Bool, CLIArgAction_Store))
    {
        logger_log_error("Could not add argument to cli parser");
        return 1;
    }

    if(!cli_parser_parse(&parser, argc, argv))
    {
        logger_log_error("Could not parse arguments (%d)", error_get_last());
        return 1;
    }

    int64_t* i64_arg = cli_parser_arg_get_i64(&parser, "int_arg", 0);
    ROMANO_ASSERT(i64_arg != NULL, "i64_arg is NULL");
    logger_log_debug("int_arg: %ll", *i64_arg);

    double* f64_arg = cli_parser_arg_get_f64(&parser, "double_arg", 0);
    ROMANO_ASSERT(f64_arg != NULL, "f64_arg is NULL");
    logger_log_debug("double_arg: %f", *f64_arg);

    size_t str_arg_sz;
    char* str_arg = cli_parser_arg_get_str(&parser, "str_arg", 0, &str_arg_sz);
    ROMANO_ASSERT(str_arg != NULL, "str_arg is NULL");
    logger_log_debug("str_arg: %s", str_arg);

    bool* bool_arg = cli_parser_arg_get_bool(&parser, "bool_arg", 0);
    ROMANO_ASSERT(bool_arg != NULL, "bool_arg is NULL");
    logger_log_debug("bool_arg: %s", *bool_arg ? "true" : "false");

    cli_parser_release(&parser);

    logger_log_info("Finished cli test");

    logger_release();

    return 0;
}
