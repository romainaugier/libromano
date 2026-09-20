/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/strview.h"

#include <ctype.h>

#define CHECK_VIEW(view, expected) TEST_CHECK_MSG((view).size == strlen(expected) && memcmp((view).data, (expected), (view).size) == 0, \
                                                  "\"" STRVIEW_FMT "\" != \"%s\"", STRVIEW_ARG(view), (expected))

static void test_new_and_cmp(void)
{
    StringView a = strview_new("hello", 5);
    StringView b = strview_new("hello world", 5);
    StringView c = strview_new("help!", 5);
    StringView d = strview_new("hello", 4);

    TEST_CHECK_EQ_UINT(a.size, 5);
    TEST_CHECK(strview_cmp(a, b));
    TEST_CHECK(!strview_cmp(a, c));
    TEST_CHECK(!strview_cmp(a, d));
}

static void test_split(void)
{
    static const char* const expected[] = { "a", "bc", "", "def" };
    const char* data = "a bc  def";
    StringView view = STRVIEW_NULL;
    size_t count = 0;

    while(strview_split(data, " ", &view))
    {
        TEST_ASSERT(count < 4);
        CHECK_VIEW(view, expected[count]);
        count++;
    }

    TEST_CHECK_EQ_UINT(count, 4);
}

static void test_lsplit_rsplit(void)
{
    StringView rest;
    StringView left = strview_lsplit("test this split", " ", &rest);
    StringView right;

    CHECK_VIEW(left, "test");
    CHECK_VIEW(rest, " this split");

    right = strview_rsplit("test this split", " ", &rest);
    CHECK_VIEW(right, "split");
    CHECK_VIEW(rest, "test this ");

    left = strview_lsplit("key::value", "::", &rest);
    CHECK_VIEW(left, "key");
    CHECK_VIEW(rest, "::value");

    right = strview_rsplit("a::b::c", "::", NULL);
    CHECK_VIEW(right, "c");

    left = strview_lsplit("nosep", ",", &rest);
    CHECK_VIEW(left, "nosep");
    TEST_CHECK_EQ_UINT(rest.size, 0);

    right = strview_rsplit("nosep", ",", &rest);
    CHECK_VIEW(right, "nosep");
    TEST_CHECK_EQ_UINT(rest.size, 0);

    right = strview_rsplit("", ",", &rest);
    TEST_CHECK_EQ_UINT(right.size, 0);
}

static void test_find_starts_ends(void)
{
    StringView view = strview_new("this string may contain some substrings", 39);

    TEST_CHECK_EQ_INT(strview_find(view, "this", 4), 0);
    TEST_CHECK_EQ_INT(strview_find(view, "string", 0), 5);
    TEST_CHECK_EQ_INT(strview_find(view, "substrings", 10), 29);
    TEST_CHECK_EQ_INT(strview_find(view, "contrain", 8), -1);
    TEST_CHECK_EQ_INT(strview_find(view, "this string may contain some substrings and more", 0), -1);

    TEST_CHECK(strview_startswith(view, "this", 4));
    TEST_CHECK(strview_startswith(view, "this st", 0));
    TEST_CHECK(!strview_startswith(view, "that", 4));
    TEST_CHECK(strview_endswith(view, "substrings", 0));
    TEST_CHECK(!strview_endswith(view, "string", 6));
    TEST_CHECK(!strview_endswith(strview_new("ab", 2), "abc", 3));
}

static void test_trim(void)
{
    CHECK_VIEW(strview_trim("    trim this string  "), "trim this string");
    CHECK_VIEW(strview_trim("no_trim"), "no_trim");
    CHECK_VIEW(strview_trim("\t\n x \r\n"), "x");
    CHECK_VIEW(strview_trim("x"), "x");
    TEST_CHECK_EQ_UINT(strview_trim("   ").size, 0);
    TEST_CHECK_EQ_UINT(strview_trim("").size, 0);
}

static void test_parse(void)
{
    char buffer[] = "123456789";

    TEST_CHECK_EQ_INT(strview_parse_int(strview_new("2147483647", 10)), 2147483647);
    TEST_CHECK_EQ_INT(strview_parse_int(strview_new("-483901", 7)), -483901);
    TEST_CHECK_EQ_INT(strview_parse_int(strview_new(" 57389 word", 11)), 57389);
    TEST_CHECK_EQ_INT(strview_parse_int(strview_new("word", 4)), 0);
    TEST_CHECK_EQ_INT(strview_parse_int(strview_new("189401995839", 12)), 0);
    TEST_CHECK_EQ_INT(strview_parse_int(strview_new(buffer, 3)), 123);

    TEST_CHECK(strview_parse_bool(strview_new("1", 1)));
    TEST_CHECK(!strview_parse_bool(strview_new("0", 1)));
    TEST_CHECK(strview_parse_bool(strview_new("true", 4)));
    TEST_CHECK(strview_parse_bool(strview_new("True", 4)));
    TEST_CHECK(!strview_parse_bool(strview_new("false", 5)));
    TEST_CHECK(!strview_parse_bool(strview_new("False", 5)));
    TEST_CHECK(!strview_parse_bool(strview_new("not a bool", 10)));
    TEST_CHECK(!strview_parse_bool(strview_new("tru", 3)));

    TEST_CHECK_NEAR(strview_parse_double(strview_new("3.25", 4)), 3.25, 0.0);
    TEST_CHECK_NEAR(strview_parse_double(strview_new("-1e3", 4)), -1000.0, 0.0);
    TEST_CHECK_NEAR(strview_parse_double(strview_new(buffer, 2)), 12.0, 0.0);
    TEST_CHECK_NEAR(strview_parse_double(strview_new("abc", 3)), 0.0, 0.0);
}

static bool property_trim(FuzzSource* source, void* user_data)
{
    char data[64];
    StringView view;
    size_t start = 0;
    size_t end;

    ROMANO_UNUSED(user_data);

    end = fuzz_string(source, data, sizeof(data) - 1, " \tab");

    while(start < end && isspace((unsigned char)data[start]))
        start++;

    while(end > start && isspace((unsigned char)data[end - 1]))
        end--;

    view = strview_trim(data);

    TEST_FUZZ_CHECK_MSG(view.size == end - start && (view.size == 0 || view.data == data + start),
                        "trim(\"%s\") = \"" STRVIEW_FMT "\"", data, STRVIEW_ARG(view));

    return true;
}

static bool property_parse_int(FuzzSource* source, void* user_data)
{
    char data[32];
    char bounded[32];
    size_t size = fuzz_string(source, data, sizeof(data) - 1, "-+ 0123456789x");
    size_t view_size = fuzz_range(source, 0, size);
    char* end;
    long long expected;

    ROMANO_UNUSED(user_data);

    if(fuzz_bool(source))
        size = (size_t)snprintf(data, sizeof(data), "%lld", (long long)fuzz_i64_special(source)), view_size = size;

    memcpy(bounded, data, view_size);
    bounded[view_size] = '\0';

    expected = strtoll(bounded, &end, 10);

    if(expected > INT32_MAX || expected < INT32_MIN)
        expected = 0;

    TEST_FUZZ_CHECK_MSG(strview_parse_int(strview_new(data, view_size)) == expected,
                        "parse_int(\"%s\") = %d, expected %lld", bounded, strview_parse_int(strview_new(data, view_size)), expected);

    return true;
}

static bool property_find(FuzzSource* source, void* user_data)
{
    char data[128];
    char needle[8];
    size_t size = fuzz_string(source, data, sizeof(data) - 1, "ab");
    size_t needle_size = 1 + fuzz_string(source, needle, sizeof(needle) - 2, "ab");
    StringView view = strview_new(data, size);
    const char* found;
    int expected;

    ROMANO_UNUSED(user_data);

    needle[needle_size - 1] = 'a';
    needle[needle_size] = '\0';

    found = strstr(data, needle);
    expected = found != NULL ? (int)(found - data) : -1;

    TEST_FUZZ_CHECK(strview_find(view, needle, (int)needle_size) == expected);
    TEST_FUZZ_CHECK(strview_startswith(view, needle, (int)needle_size) == (expected == 0));
    TEST_FUZZ_CHECK(strview_endswith(view, needle, (int)needle_size) ==
                    (size >= needle_size && memcmp(data + size - needle_size, needle, needle_size) == 0));

    return true;
}

static void test_fuzz(void)
{
    test_fuzz_property("strview_trim", 10000, property_trim, NULL);
    test_fuzz_property("strview_parse_int", 10000, property_parse_int, NULL);
    test_fuzz_property("strview_find", 10000, property_find, NULL);
}

TEST_MAIN(
    TEST(test_new_and_cmp),
    TEST(test_split),
    TEST(test_lsplit_rsplit),
    TEST(test_find_starts_ends),
    TEST(test_trim),
    TEST(test_parse),
    TEST(test_fuzz),
)
