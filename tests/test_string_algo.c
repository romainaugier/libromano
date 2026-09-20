/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/string_algo.h"

static const char* reference_strstrn(const char* haystack, size_t haystack_size, const char* needle, size_t needle_size)
{
    size_t i;

    if(needle_size == 0)
        return haystack;

    for(i = 0; i + needle_size <= haystack_size; i++)
        if(memcmp(haystack + i, needle, needle_size) == 0)
            return haystack + i;

    return NULL;
}

static void test_known_cases(void)
{
    const char* haystack = "Hello, World!";
    const char binary[] = { 'a', 'b', '\0', 'c', 'd', 'e', 'f' };

    TEST_CHECK(strstrn(haystack, strlen(haystack), "World", 5) == haystack + 7);
    TEST_CHECK(strstrn(haystack, strlen(haystack), "xyz", 3) == NULL);
    TEST_CHECK(strstrn(haystack, 5, "World", 5) == NULL);
    TEST_CHECK(strstrn(haystack, strlen(haystack), "", 0) == haystack);
    TEST_CHECK(strstrn(haystack, strlen(haystack), "Hello", 5) == haystack);
    TEST_CHECK(strstrn(haystack, 3, "Hello", 5) == NULL);
    TEST_CHECK(strstrn(binary, sizeof(binary), "cd", 2) == binary + 3);
    TEST_CHECK(strstrn(NULL, 10, "test", 4) == NULL);
    TEST_CHECK(strstrn(haystack, 10, NULL, 4) == NULL);
}

static bool property_matches_reference(FuzzSource* source, void* user_data)
{
    char haystack[256];
    char needle[16];
    size_t haystack_size = fuzz_string(source, haystack, sizeof(haystack) - 1, "ab");
    size_t needle_size = fuzz_string(source, needle, sizeof(needle) - 1, "ab");

    ROMANO_UNUSED(user_data);

    if(fuzz_one_in(source, 4) && haystack_size > 0)
    {
        size_t start = fuzz_index(source, haystack_size);
        needle_size = fuzz_range(source, 0, haystack_size - start < 15 ? haystack_size - start : 15);
        memcpy(needle, haystack + start, needle_size);
    }

    TEST_FUZZ_CHECK_MSG(strstrn(haystack, haystack_size, needle, needle_size) ==
                        reference_strstrn(haystack, haystack_size, needle, needle_size),
                        "haystack \"%s\", needle \"%.*s\"", haystack, (int)needle_size, needle);

    return true;
}

static void test_fuzz_reference(void)
{
    test_fuzz_property("strstrn", 20000, property_matches_reference, NULL);
}

TEST_MAIN(
    TEST(test_known_cases),
    TEST(test_fuzz_reference),
)
