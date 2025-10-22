/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/logger.h"
#include "libromano/regex.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

int main(void)
{
    logger_init();
    logger_set_level(LogLevel_Debug);

    const char* regex1 = "a+b+";
    const char* match_regex1 = "aaaaabbbbb";
    const char* no_match_regex1 = "ccdd";

    Regex* regex_pattern1;

    PROFILE_US(regex_pattern1 = regex_compile(regex1, RegexFlags_DebugCompilation));
    ROMANO_ASSERT(regex_pattern1 != NULL, "Failed to compile regular expression");

    PROFILE_US(ROMANO_ASSERT(regex_match(regex_pattern1, match_regex1), "Failed to match matching regex"));
    PROFILE_US(ROMANO_ASSERT(!regex_match(regex_pattern1, no_match_regex1), "Matched non-matching regex"));

    regex_free(regex_pattern1);

    logger_release();

    return 0;
}
