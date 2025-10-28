/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/string_algo.h"
#include "libromano/logger.h"

#include <string.h>

int main(void)
{
    logger_init();
    logger_set_level(LogLevel_Debug);

    const char *haystack1 = "Hello, World!";
    const char *needle1 = "World";
    const char *result1 = strstrn(haystack1, strlen(haystack1),
                                    needle1, strlen(needle1));
    printf("Test 1: %s\n", result1 ? result1 : "NULL");

    const char *needle2 = "xyz";
    const char *result2 = strstrn(haystack1, strlen(haystack1),
                                    needle2, strlen(needle2));
    printf("Test 2: %s\n", result2 ? result2 : "NULL");

    const char *result3 = strstrn(haystack1, 5, needle1, strlen(needle1));
    printf("Test 3: %s\n", result3 ? result3 : "NULL");

    const char *result4 = strstrn(haystack1, strlen(haystack1), "", 0);
    printf("Test 4: %s\n", result4 ? result4 : "NULL");

    const char *needle5 = "Hello";
    const char *result5 = strstrn(haystack1, strlen(haystack1),
                                    needle5, strlen(needle5));
    printf("Test 5: %s\n", result5 ? result5 : "NULL");

    const char haystack6[] = {'a', 'b', '\0', 'c', 'd', 'e', 'f'};
    const char needle6[] = {'c', 'd'};
    const char *result6 = strstrn(haystack6, 7, needle6, 2);
    printf("Test 6: Found at offset %ld\n", result6 ? result6 - haystack6 : -1);

    const char *result8 = strstrn(NULL, 10, "test", 4);
    printf("Test 8: %s\n", result8 ? result8 : "NULL (safe)");

    logger_release();

    return 0;
}
