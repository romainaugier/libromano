/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void test_string_new() 
{
    String s = string_new("Hello, World!");
    ROMANO_ASSERT(s != NULL, "string_new should not return NULL");
    ROMANO_ASSERT(string_length(s) == 13, "Length should be 13");
    ROMANO_ASSERT(strcmp(s, "Hello, World!") == 0, "Content should match");
    ROMANO_ASSERT(string_capacity(s) >= string_length(s), "Capacity >= Length");
    string_free(s);

    s = string_new("");
    ROMANO_ASSERT(s != NULL, "Empty string creation");
    ROMANO_ASSERT(string_length(s) == 0, "Empty string length should be 0");
    string_free(s);
}

void test_string_newz() 
{
    const size_t length = 10;
    String s = string_newz(length);
    ROMANO_ASSERT(s != NULL, "string_newz should not return NULL");
    ROMANO_ASSERT(string_length(s) == length, "Length should be 10");
    for (size_t i = 0; i < length; i++) {
        ROMANO_ASSERT(s[i] == '\0', "All characters should be zero");
    }
    ROMANO_ASSERT(string_capacity(s) >= length, "Capacity >= Length");
    string_free(s);
}

void test_string_newf() 
{
    String s = string_newf("Formatted: %d %s", 42, "test");
    ROMANO_ASSERT(s != NULL, "string_newf should not return NULL");
    ROMANO_ASSERT(strcmp(s, "Formatted: 42 test") == 0, "Formatted content mismatch");
    string_free(s);

    s = string_newf("");
    ROMANO_ASSERT(string_length(s) == 0, "Empty formatted string");
    string_free(s);
}

void test_string_capacity() 
{
    String s = string_new("Test");
    size_t cap = string_capacity(s);
    ROMANO_ASSERT(cap >= string_length(s), "Initial capacity >= length");
    string_free(s);
}

void test_string_length() 
{
    String s = string_new("12345");
    ROMANO_ASSERT(string_length(s) == 5, "Length should be 5");
    string_free(s);
}

void test_string_resize() 
{
    String s = string_new("Hello");
    size_t original_size = string_length(s);
    size_t original_cap = string_capacity(s);

    string_resize(&s, 10);
    ROMANO_ASSERT(string_length(s) == original_size, "Size changed after resize");
    ROMANO_ASSERT(string_capacity(s) >= 10, "Capacity after resize up");
    ROMANO_ASSERT(s[original_size] == '\0', "Null terminator present after resize");
    string_free(s);
}

void test_string_copy() 
{
    String s1 = string_new("Original");
    String s2 = string_copy(s1);
    ROMANO_ASSERT(string_eq(s1, s2), "Copied string should be equal");
    ROMANO_ASSERT(string_capacity(s2) == string_capacity(s1), "Copy capacity match");

    string_appendc(&s1, " modified");
    ROMANO_ASSERT(!string_eq(s1, s2), "Modification affects only original");
    string_free(s1);
    string_free(s2);
}

void test_string_setc() 
{
    String s = string_new("Initial");
    string_setc(&s, "New");
    ROMANO_ASSERT(strcmp(s, "New") == 0, "Content after string_setc");
    ROMANO_ASSERT(string_length(s) == 3, "Length after setc");
    string_free(s);
}

void test_string_sets() 
{
    String s1 = string_new("Source");
    String s2 = string_new("Destination");
    string_sets(&s2, s1);
    ROMANO_ASSERT(string_eq(s1, s2), "Sets copies content");
    string_free(s1);
    string_free(s2);
}

void test_string_setf() 
{
    String s = string_new("Before");
    string_setf(&s, "Formatted %d", 123);
    ROMANO_ASSERT(strcmp(s, "Formatted 123") == 0, "Formatted set");
    string_free(s);
}

void test_string_appendc() 
{
    String s = string_new("Hello");
    string_appendc(&s, ", World!");
    ROMANO_ASSERT(strcmp(s, "Hello, World!") == 0, "Append C string");
    ROMANO_ASSERT(string_length(s) == 13, "Length after append");
    string_free(s);

    s = string_new("");
    string_appendc(&s, "Append to empty");
    ROMANO_ASSERT(strcmp(s, "Append to empty") == 0, "Append to empty string");
    string_free(s);
}

void test_string_appends() 
{
    String s1 = string_new("Hello");
    String s2 = string_new(", World!");
    string_appends(&s1, s2);
    ROMANO_ASSERT(strcmp(s1, "Hello, World!") == 0, "Append String");
    string_free(s1);
    string_free(s2);
}

void test_string_appendf() 
{
    String s = string_new("Count: ");
    string_appendf(&s, "%d", 42);
    ROMANO_ASSERT(strcmp(s, "Count: 42") == 0, "Append formatted");
    string_free(s);
}

void test_string_prependc() 
{
    String s = string_new("World");
    string_prependc(&s, "Hello ");
    ROMANO_ASSERT(strcmp(s, "Hello World") == 0, "Prepend C string");
    string_free(s);
}

void test_string_prepends() 
{
    String s1 = string_new("World");
    String s2 = string_new("Hello ");
    string_prepends(&s1, s2);
    ROMANO_ASSERT(strcmp(s1, "Hello World") == 0, "Prepend String");
    string_free(s1);
    string_free(s2);
}

void test_string_prependf() 
{
    String s = string_new("World");
    string_prependf(&s, "Hello %s ", "there");
    ROMANO_ASSERT(strcmp(s, "Hello there World") == 0, "Prepend formatted");
    string_free(s);
}

void test_string_clear()
{
    String s = string_new("Content");
    size_t cap = string_capacity(s);
    string_clear(s);
    ROMANO_ASSERT(string_length(s) == 0, "Length after clear");
    ROMANO_ASSERT(string_capacity(s) == cap, "Capacity remains after clear");
    ROMANO_ASSERT(s[0] == '\0', "Data is cleared");
    string_free(s);
}

void test_string_split() 
{
    char* data = strdup("a,b,c");
    uint32_t count;
    String* arr = string_splitc(data, ",", &count);
    ROMANO_ASSERT(count == 3, "Split into 3 parts");
    ROMANO_ASSERT(strcmp(arr[0], "a") == 0, "First part 'a'");
    ROMANO_ASSERT(strcmp(arr[1], "b") == 0, "Second part 'b'");
    ROMANO_ASSERT(strcmp(arr[2], "c") == 0, "Third part 'c'");
    for (uint32_t i = 0; i < count; i++) string_free(arr[i]);
    free(arr);
    free(data);

    data = strdup("");
    arr = string_splitc(data, ",", &count);
    ROMANO_ASSERT(count == 0, "Empty data split");
    free(arr);
    free(data);

    data = strdup("single");
    arr = string_splitc(data, ":", &count);
    ROMANO_ASSERT(count == 1, "No separator found");
    ROMANO_ASSERT(strcmp(arr[0], "single") == 0, "Single part");
    for (uint32_t i = 0; i < count; i++) string_free(arr[i]);
    free(arr);
    free(data);
}

void test_string_eq() 
{
    String s1 = string_new("Same");
    String s2 = string_new("Same");
    String s3 = string_new("Different");
    ROMANO_ASSERT(string_eq(s1, s2), "Equal strings");
    ROMANO_ASSERT(!string_eq(s1, s3), "Different strings");
    string_free(s1);
    string_free(s2);
    string_free(s3);

    s1 = string_new("");
    s2 = string_new("");
    ROMANO_ASSERT(string_eq(s1, s2), "Empty strings are equal");
    string_free(s1);
    string_free(s2);
}

int main() 
{
    test_string_new();
    test_string_newz();
    test_string_newf();
    test_string_capacity();
    test_string_length();
    test_string_resize();
    test_string_copy();
    test_string_setc();
    test_string_sets();
    test_string_setf();
    test_string_appendc();
    test_string_appends();
    test_string_appendf();
    test_string_prependc();
    test_string_prepends();
    test_string_prependf();
    test_string_clear();
    test_string_split();
    test_string_eq();

    printf("All tests passed\n");

    return 0;
}