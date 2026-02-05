/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/cli.h"
#include "libromano/logger.h"
#include "libromano/error.h"

int main(int argc, char** argv)
{
    CLIParser parser;
    int64_t* num_iterations;
    double* threshold;
    bool* verbose;
    bool* enable_feature;
    char* input_file;
    char* output_file;

    logger_init();
    logger_set_level(LogLevel_Debug);

    logger_log_info("Starting cli test");

    if(!cli_parser_init(&parser))
    {
        logger_log_error("Failed to initialize parser");
        return 1;
    }

    cli_parser_set_program_info(&parser,
                                argv[0],
                                "Test application demonstrating CLI parsing");

    cli_parser_add_arg(&parser,
                      "input",
                      0,
                      CLI_NO_SHORT_NAME,
                      CLIArgMode_Positional,
                      CLIArgType_Str,
                      CLIArgAction_Store,
                      "Input file path");

    cli_parser_add_arg(&parser,
                      "output",
                      0,
                      CLI_NO_SHORT_NAME,
                      CLIArgMode_Positional,
                      CLIArgType_Str,
                      CLIArgAction_Store,
                      "Output file path");

    cli_parser_add_arg(&parser,
                      "num-iterations",
                      0,
                      'n',
                      CLIArgMode_Optional,
                      CLIArgType_Int,
                      CLIArgAction_Store,
                      "Number of iterations to perform");

    cli_parser_add_arg(&parser,
                      "threshold",
                      0,
                      't',
                      CLIArgMode_Optional,
                      CLIArgType_Float,
                      CLIArgAction_Store,
                      "Threshold value (0.0 to 1.0)");

    cli_parser_add_arg(&parser,
                      "verbose",
                      0,
                      'v',
                      CLIArgMode_Optional,
                      CLIArgType_Bool,
                      CLIArgAction_StoreTrue,
                      "Enable verbose output");

    cli_parser_add_arg(&parser,
                      "enable-feature",
                      0,
                      CLI_NO_SHORT_NAME,
                      CLIArgMode_Optional,
                      CLIArgType_Bool,
                      CLIArgAction_StoreTrue,
                      "Enable experimental feature");

    if(!cli_parser_parse(&parser, argc, argv))
    {
        logger_log_error("Failed to parse CLI arguments, check the log for more information");
        logger_log_error("Error: %s (%s)",
                         error_str_get_last(),
                         error_context_str() != NULL ? error_context_str() : "No context");

        cli_parser_release(&parser);

        return 1;
    }

    input_file = cli_parser_arg_get_str(&parser, "input", 0, NULL);
    output_file = cli_parser_arg_get_str(&parser, "output", 0, NULL);

    logger_log_debug("Input file: %s", input_file);
    logger_log_debug("Output file: %s", output_file);

    if(cli_parser_has_arg(&parser, "num-iterations", 0))
    {
        num_iterations = cli_parser_arg_get_i64(&parser, "num-iterations", 0);
        logger_log_debug("Number of iterations: %lld", (long long)*num_iterations);
    }

    if(cli_parser_has_arg(&parser, "threshold", 0))
    {
        threshold = cli_parser_arg_get_f64(&parser, "threshold", 0);
        logger_log_debug("Threshold: %f", *threshold);
    }

    verbose = cli_parser_arg_get_bool(&parser, "verbose", 0);

    if(verbose != NULL && *verbose)
        logger_log_debug("Verbose mode enabled");

    enable_feature = cli_parser_arg_get_bool(&parser, "enable-feature", 0);

    if(enable_feature != NULL && *enable_feature)
        logger_log_debug("Experimental feature enabled");

    cli_parser_release(&parser);

    logger_log_info("Finished cli test");

    logger_release();

    return 0;
}
