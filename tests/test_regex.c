/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/logger.h"
#include "libromano/regex.h"
#include "libromano/error.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#include <string.h>
#include <stdlib.h>

#define MATCH(pattern, flags, string, expected)                                     \
    do {                                                                            \
        Regex* _regex = regex_compile((pattern), (flags));                          \
        ROMANO_ASSERT(_regex != NULL, "Failed to compile regular expression");      \
        ROMANO_ASSERT(regex_match(_regex, (string), strlen(string)) == (expected),  \
                      "Unexpected full match result: " pattern);                     \
        regex_free(_regex);                                                         \
    } while(0)

#define SEARCH(pattern, flags, string, expected_position, expected_sz)                        \
    do {                                                                                      \
        RegexMatch _match;                                                                    \
        Regex* _regex = regex_compile((pattern), (flags));                                    \
        ROMANO_ASSERT(_regex != NULL, "Failed to compile regular expression");                \
        ROMANO_ASSERT(regex_search(_regex, (string), strlen(string), &_match),                \
                      "Search failed: " pattern);                                            \
        ROMANO_ASSERT(_match.position == (expected_position) && _match.data_sz == (expected_sz), \
                      "Unexpected search result: " pattern);                                 \
        regex_free(_regex);                                                                   \
    } while(0)

#define NO_SEARCH(pattern, flags, string)                                             \
    do {                                                                              \
        Regex* _regex = regex_compile((pattern), (flags));                            \
        ROMANO_ASSERT(_regex != NULL, "Failed to compile regular expression");        \
        ROMANO_ASSERT(!regex_search(_regex, (string), strlen(string), NULL),          \
                      "Unexpected search match: " pattern);                          \
        regex_free(_regex);                                                           \
    } while(0)

#define COMPILE_FAILS(pattern)                                                         \
    do {                                                                               \
        Regex* _regex = regex_compile((pattern), RegexFlags_None);                     \
        ROMANO_ASSERT(_regex == NULL, "Invalid pattern compiled: " pattern);           \
    } while(0)

typedef struct Words {
    char buffer[256];
    size_t buffer_sz;
    size_t count;
} Words;

bool collect_words(const RegexMatch* match, void* user_data)
{
    Words* words = (Words*)user_data;

    memcpy(words->buffer + words->buffer_sz, match->data, match->data_sz);
    words->buffer_sz += match->data_sz;
    words->buffer[words->buffer_sz++] = ',';
    words->buffer[words->buffer_sz] = '\0';
    words->count++;

    return true;
}

bool count_matches(const RegexMatch* match, void* user_data)
{
    ROMANO_UNUSED(match);
    (*(size_t*)user_data)++;
    return true;
}

bool stop_after_two(const RegexMatch* match, void* user_data)
{
    ROMANO_UNUSED(match);
    return ++(*(size_t*)user_data) < 2;
}

int main(void)
{
    Regex* regex;
    Words words;
    char* big_string;
    size_t count;
    size_t big_string_sz;
    size_t i;

    logger_init();
    logger_set_level(LogLevel_Debug);

    /* Debug output */
    regex = regex_compile("^(ab|c)+[[:digit:]]{2}$", RegexFlags_DebugCompilation);
    ROMANO_ASSERT(regex != NULL, "Failed to compile regular expression");
    ROMANO_ASSERT(regex_match(regex, "abcab42", 7), "");
    ROMANO_ASSERT(!regex_match(regex, "abcab4", 6), "");
    regex_free(regex);

    logger_set_level(LogLevel_Info);

    /* Operators */
    MATCH("a+b+", RegexFlags_None, "aaaaabbbbb", true);
    MATCH("a+b+", RegexFlags_None, "ccdd", false);
    MATCH("a*", RegexFlags_None, "", true);
    MATCH("a|b|", RegexFlags_None, "", true);
    MATCH("colou?r", RegexFlags_None, "color", true);
    MATCH("colou?r", RegexFlags_None, "colour", true);
    MATCH("(ab)*c", RegexFlags_None, "ababc", true);
    MATCH("(ab)*c", RegexFlags_None, "abac", false);
    MATCH("()", RegexFlags_None, "", true);
    MATCH("(a*)*b", RegexFlags_None, "aaab", true);

    /* Bounds */
    MATCH("a{3}", RegexFlags_None, "aaa", true);
    MATCH("a{3}", RegexFlags_None, "aa", false);
    MATCH("a{2,}", RegexFlags_None, "aaaaaaa", true);
    MATCH("a{2,3}", RegexFlags_None, "aaaa", false);
    MATCH("(ab){0,2}", RegexFlags_None, "abab", true);
    MATCH("x{0}y", RegexFlags_None, "y", true);

    /* Brackets */
    MATCH("[]a]+", RegexFlags_None, "]a]", true);
    MATCH("[^]a]", RegexFlags_None, "b", true);
    MATCH("[a-]+", RegexFlags_None, "a--a", true);
    MATCH("[--/]+", RegexFlags_None, "-./", true);
    MATCH("[\\n]+", RegexFlags_None, "\\n\\", true);
    MATCH("[[:alpha:]_][[:alnum:]_]*", RegexFlags_None, "_my_var1", true);
    MATCH("[[:alpha:]_][[:alnum:]_]*", RegexFlags_None, "1var", false);
    MATCH("[[:xdigit:]]+", RegexFlags_None, "DeadBeef09", true);
    MATCH("[[:space:][:punct:]]+", RegexFlags_None, " \t!?", true);
    MATCH("[[.-.]a]+", RegexFlags_None, "-a", true);
    MATCH("[[=e=]]", RegexFlags_None, "e", true);

    /* Escapes */
    MATCH("\\d{3}-\\d{4}", RegexFlags_None, "555-1234", true);
    MATCH("\\w+\\s\\w+", RegexFlags_None, "hello world", true);
    MATCH("\\S+", RegexFlags_None, "a b", false);
    MATCH("a\\.b\\*", RegexFlags_None, "a.b*", true);
    MATCH("a\\.b", RegexFlags_None, "axb", false);
    MATCH("\\(\\)\\[\\]\\{\\}\\|\\\\", RegexFlags_None, "()[]{}|\\", true);

    /* Case insensitive */
    MATCH("hello", RegexFlags_IgnoreCase, "HeLLo", true);
    MATCH("[a-c]+", RegexFlags_IgnoreCase, "AbC", true);
    MATCH("[^a]", RegexFlags_IgnoreCase, "A", false);

    /* Dot and newlines */
    MATCH("a.b", RegexFlags_None, "a\nb", true);
    MATCH("a.b", RegexFlags_Newline, "a\nb", false);
    MATCH("a[^x]b", RegexFlags_Newline, "a\nb", false);

    /* Anchors */
    SEARCH("^abc", RegexFlags_None, "abcabc", 0, 3);
    NO_SEARCH("^abc", RegexFlags_None, "xabc");
    SEARCH("abc$", RegexFlags_None, "abcabc", 3, 3);
    NO_SEARCH("a^b", RegexFlags_None, "ab");
    NO_SEARCH("a$b", RegexFlags_None, "ab");
    SEARCH("^$", RegexFlags_None, "", 0, 0);
    SEARCH("(^a|b)+", RegexFlags_None, "abab", 0, 2);
    NO_SEARCH("(^A){2}", RegexFlags_None, "AA");
    NO_SEARCH("x\n^y", RegexFlags_None, "x\ny");
    SEARCH("x\n^y", RegexFlags_Newline, "x\ny", 0, 3);
    SEARCH("x$\ny", RegexFlags_Newline, "x\ny", 0, 3);
    SEARCH("^b", RegexFlags_Newline, "a\nb", 2, 1);
    SEARCH("a$", RegexFlags_Newline, "a\nb", 0, 1);
    NO_SEARCH("a$", RegexFlags_None, "a\nb");

    /* Leftmost-longest */
    SEARCH("a|ab|abc", RegexFlags_None, "xabcd", 1, 3);
    SEARCH("(a|ab)(c|bcd)", RegexFlags_None, "abcd", 0, 4);
    SEARCH("abcd|c", RegexFlags_None, "abcd", 0, 4);
    SEARCH("b*", RegexFlags_None, "abbb", 0, 0);

    /* Errors */
    COMPILE_FAILS("(ab");
    COMPILE_FAILS("ab)");
    COMPILE_FAILS("[ab");
    COMPILE_FAILS("[z-a]");
    COMPILE_FAILS("[[:foo:]]");
    COMPILE_FAILS("*a");
    COMPILE_FAILS("a|+");
    COMPILE_FAILS("a{2");
    COMPILE_FAILS("a{3,2}");
    COMPILE_FAILS("a{256}");
    COMPILE_FAILS("a\\");
    COMPILE_FAILS("\\q");

    /* Exponential DFA blowup is reported instead of eating all the memory */
    COMPILE_FAILS("(a|b)*a(a|b){20}");
    ROMANO_ASSERT(error_get_last() == ErrorCode_SizeOverflow, "Expected a size overflow error");

    /* Iterate */
    regex = regex_compile("[[:alpha:]]+", RegexFlags_None);
    ROMANO_ASSERT(regex != NULL, "Failed to compile regular expression");

    memset(&words, 0, sizeof(Words));
    ROMANO_ASSERT(regex_iterate(regex, "  the quick, brown fox!", 23, collect_words, &words) == 4, "");
    ROMANO_ASSERT(strcmp(words.buffer, "the,quick,brown,fox,") == 0, "Unexpected iterated words");

    count = 0;
    ROMANO_ASSERT(regex_iterate(regex, "a b c d", 7, stop_after_two, &count) == 2, "Iteration did not stop");

    regex_free(regex);

    regex = regex_compile("a*", RegexFlags_None);
    ROMANO_ASSERT(regex != NULL, "Failed to compile regular expression");

    memset(&words, 0, sizeof(Words));
    ROMANO_ASSERT(regex_iterate(regex, "baaa", 4, collect_words, &words) == 3, "");
    ROMANO_ASSERT(strcmp(words.buffer, ",aaa,,") == 0, "Unexpected empty matches");

    regex_free(regex);

    /* Big inputs, linear time, heap fallback of regex_iterate */
    big_string_sz = 1 << 20;
    big_string = (char*)malloc(big_string_sz);
    ROMANO_ASSERT(big_string != NULL, "");

    for(i = 0; i < big_string_sz; i++)
        big_string[i] = (i % 1000) == 999 ? 'c' : 'a';

    regex = regex_compile("(a|aa)*c", RegexFlags_None);
    ROMANO_ASSERT(regex != NULL, "Failed to compile regular expression");

    count = 0;
    PROFILE_US(count = regex_iterate(regex, big_string, big_string_sz, count_matches, &count));
    ROMANO_ASSERT(count == big_string_sz / 1000, "Unexpected number of matches in big string");

    PROFILE_US(ROMANO_ASSERT(!regex_match(regex, big_string, big_string_sz), ""));

    regex_free(regex);

    regex = regex_compile("(x+x+)+y", RegexFlags_None);
    ROMANO_ASSERT(regex != NULL, "Failed to compile regular expression");

    memset(big_string, 'x', big_string_sz);

    PROFILE_US(ROMANO_ASSERT(!regex_search(regex, big_string, big_string_sz, NULL), "Catastrophic pattern matched"));

    regex_free(regex);
    free(big_string);

    logger_release();

    return 0;
}