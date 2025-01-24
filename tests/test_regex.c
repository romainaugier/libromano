/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/regex.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

int main(void)
{
    const char* regex1 = "a+b+";
    const char* match_regex1 = "aaaaabbbbb";
    const char* no_match_regex1 = "ccdd";

    Regex* regex_pattern1;

    PROFILE_US(regex_pattern1 = regex_compile(regex1));
    ROMANO_ASSERT(regex_pattern1 != NULL, "Failed to compile regular expression");

    PROFILE_US(ROMANO_ASSERT(regex_match(regex_pattern1, match_regex1), "Failed to match matching regex"));
    PROFILE_US(ROMANO_ASSERT(!regex_match(regex_pattern1, no_match_regex1), "Matched non-matching regex"));

    regex_destroy(regex_pattern1);

    return 0;
}