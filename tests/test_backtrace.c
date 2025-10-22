/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/backtrace.h"
#include "libromano/common.h"
#include "libromano/logger.h"

#define MAX_SYMBOLS 16

ROMANO_NO_INLINE int func1()
{
    uint32_t i;
    uint32_t num_symbols;
    char* symbols[MAX_SYMBOLS];

    num_symbols = backtrace_call_stack_symbols(0, MAX_SYMBOLS, symbols);

    for(i = 0; i < num_symbols; i++)
    {
        logger_log_debug("Stack Frame %u: %s", i, symbols[i]);
        free(symbols[i]);
    }

    return (int)num_symbols;
}

ROMANO_NO_INLINE int func2()
{
    return func1();
}

int main(void)
{
    backtrace_set_signal_handler();

    logger_init();
    logger_set_level(LogLevel_Debug);

    int num_symbols = func2();

    logger_log_debug("Found %d stack frames", num_symbols);

    logger_release();

    return 0;
}
