/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/regex.h"
#include "libromano/error.h"


#include <string.h>
#include <stdlib.h>

#if defined(ROMANO_LINUX)
#include <regex.h>
#define HAS_POSIX_REGEX 1
#else
#define HAS_POSIX_REGEX 0
#endif /* defined(ROMANO_LINUX) */

#define MATCH(pattern, flags, string, expected)                                              \
    do {                                                                                     \
        Regex* _regex = regex_compile((pattern), (flags));                                   \
        if(TEST_CHECK_MSG(_regex != NULL, "cannot compile \"%s\"", (pattern)))               \
        {                                                                                    \
            TEST_CHECK_MSG(regex_match(_regex, (string), strlen(string)) == (expected),      \
                           "\"%s\" on \"%s\"", (pattern), (string));                         \
            regex_free(_regex);                                                              \
        }                                                                                    \
    } while(0)

#define SEARCH(pattern, flags, string, expected_position, expected_sz)                           \
    do {                                                                                         \
        RegexMatch _match;                                                                       \
        Regex* _regex = regex_compile((pattern), (flags));                                       \
        if(TEST_CHECK_MSG(_regex != NULL, "cannot compile \"%s\"", (pattern)))                   \
        {                                                                                        \
            if(TEST_CHECK_MSG(regex_search(_regex, (string), strlen(string), &_match),           \
                              "\"%s\" not found in \"%s\"", (pattern), (string)))                \
                TEST_CHECK_MSG(_match.position == (expected_position) &&                         \
                               _match.data_sz == (expected_sz),                                  \
                               "\"%s\" in \"%s\": (%zu, %zu)", (pattern), (string),              \
                               (size_t)_match.position, (size_t)_match.data_sz);                 \
            regex_free(_regex);                                                                  \
        }                                                                                        \
    } while(0)

#define NO_SEARCH(pattern, flags, string)                                                    \
    do {                                                                                     \
        Regex* _regex = regex_compile((pattern), (flags));                                   \
        if(TEST_CHECK_MSG(_regex != NULL, "cannot compile \"%s\"", (pattern)))               \
        {                                                                                    \
            TEST_CHECK_MSG(!regex_search(_regex, (string), strlen(string), NULL),            \
                           "\"%s\" unexpectedly found in \"%s\"", (pattern), (string));      \
            regex_free(_regex);                                                              \
        }                                                                                    \
    } while(0)

#define COMPILE_FAILS(pattern)                                                               \
    do {                                                                                     \
        Regex* _regex = regex_compile((pattern), RegexFlags_None);                           \
        TEST_CHECK_MSG(_regex == NULL, "invalid pattern \"%s\" compiled", (pattern));        \
        if(_regex != NULL)                                                                   \
            regex_free(_regex);                                                              \
    } while(0)

typedef struct Words {
    char buffer[256];
    size_t buffer_sz;
    size_t count;
} Words;

static bool collect_words(const RegexMatch* match, void* user_data)
{
    Words* words = (Words*)user_data;

    memcpy(words->buffer + words->buffer_sz, match->data, match->data_sz);
    words->buffer_sz += match->data_sz;
    words->buffer[words->buffer_sz++] = ',';
    words->buffer[words->buffer_sz] = '\0';
    words->count++;

    return true;
}

static bool count_matches(const RegexMatch* match, void* user_data)
{
    ROMANO_UNUSED(match);
    (*(size_t*)user_data)++;
    return true;
}

static bool stop_after_two(const RegexMatch* match, void* user_data)
{
    ROMANO_UNUSED(match);
    return ++(*(size_t*)user_data) < 2;
}

static void test_debug_output(void)
{
    Regex* regex;

    regex = regex_compile("^(ab|c)+[[:digit:]]{2}$", RegexFlags_DebugCompilation);
    TEST_ASSERT(regex != NULL);
    TEST_CHECK(regex_match(regex, "abcab42", 7));
    TEST_CHECK(!regex_match(regex, "abcab4", 6));
    regex_free(regex);
}

static void test_operators(void)
{
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
}

static void test_bounds(void)
{
    MATCH("a{3}", RegexFlags_None, "aaa", true);
    MATCH("a{3}", RegexFlags_None, "aa", false);
    MATCH("a{2,}", RegexFlags_None, "aaaaaaa", true);
    MATCH("a{2,3}", RegexFlags_None, "aaaa", false);
    MATCH("(ab){0,2}", RegexFlags_None, "abab", true);
    MATCH("x{0}y", RegexFlags_None, "y", true);
}

static void test_brackets(void)
{
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
}

static void test_escapes(void)
{
    MATCH("\\d{3}-\\d{4}", RegexFlags_None, "555-1234", true);
    MATCH("\\w+\\s\\w+", RegexFlags_None, "hello world", true);
    MATCH("\\S+", RegexFlags_None, "a b", false);
    MATCH("a\\.b\\*", RegexFlags_None, "a.b*", true);
    MATCH("a\\.b", RegexFlags_None, "axb", false);
    MATCH("\\(\\)\\[\\]\\{\\}\\|\\\\", RegexFlags_None, "()[]{}|\\", true);
}

static void test_case_insensitive(void)
{
    MATCH("hello", RegexFlags_IgnoreCase, "HeLLo", true);
    MATCH("[a-c]+", RegexFlags_IgnoreCase, "AbC", true);
    MATCH("[^a]", RegexFlags_IgnoreCase, "A", false);
}

static void test_dot_and_newlines(void)
{
    MATCH("a.b", RegexFlags_None, "a\nb", true);
    MATCH("a.b", RegexFlags_Newline, "a\nb", false);
    MATCH("a[^x]b", RegexFlags_Newline, "a\nb", false);
}

static void test_anchors(void)
{
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
}

static void test_leftmost_longest(void)
{
    SEARCH("a|ab|abc", RegexFlags_None, "xabcd", 1, 3);
    SEARCH("(a|ab)(c|bcd)", RegexFlags_None, "abcd", 0, 4);
    SEARCH("abcd|c", RegexFlags_None, "abcd", 0, 4);
    SEARCH("b*", RegexFlags_None, "abbb", 0, 0);
}

static void test_errors(void)
{
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

    COMPILE_FAILS("(a|b)*a(a|b){20}");
    TEST_CHECK(error_get_last() == ErrorCode_SizeOverflow);
}

static void test_iterate(void)
{
    Regex* regex;
    Words words;
    size_t count;

    regex = regex_compile("[[:alpha:]]+", RegexFlags_None);
    TEST_ASSERT(regex != NULL);

    memset(&words, 0, sizeof(Words));
    TEST_CHECK(regex_iterate(regex, "  the quick, brown fox!", 23, collect_words, &words) == 4);
    TEST_CHECK(strcmp(words.buffer, "the,quick,brown,fox,") == 0);

    count = 0;
    TEST_CHECK(regex_iterate(regex, "a b c d", 7, stop_after_two, &count) == 2);

    regex_free(regex);

    regex = regex_compile("a*", RegexFlags_None);
    TEST_ASSERT(regex != NULL);

    memset(&words, 0, sizeof(Words));
    TEST_CHECK(regex_iterate(regex, "baaa", 4, collect_words, &words) == 3);
    TEST_CHECK(strcmp(words.buffer, ",aaa,,") == 0);

    regex_free(regex);
}

static void test_big_inputs(void)
{
    Regex* regex;
    char* big_string;
    size_t count;
    size_t big_string_sz;
    size_t i;

    big_string_sz = 1 << 20;
    big_string = (char*)malloc(big_string_sz);
    TEST_ASSERT(big_string != NULL);

    for(i = 0; i < big_string_sz; i++)
        big_string[i] = (i % 1000) == 999 ? 'c' : 'a';

    regex = regex_compile("(a|aa)*c", RegexFlags_None);
    TEST_ASSERT(regex != NULL);

    count = 0;
    TEST_CHECK_EQ_UINT(regex_iterate(regex, big_string, big_string_sz, count_matches, &count), big_string_sz / 1000);
    TEST_CHECK_EQ_UINT(count, big_string_sz / 1000);

    TEST_CHECK(!regex_match(regex, big_string, big_string_sz));

    regex_free(regex);

    regex = regex_compile("(x+x+)+y", RegexFlags_None);
    TEST_ASSERT(regex != NULL);

    memset(big_string, 'x', big_string_sz);

    TEST_CHECK(!regex_search(regex, big_string, big_string_sz, NULL));

    regex_free(regex);
    free(big_string);
}

static const char* const g_pattern_tokens[] = {
    "a", "b", "c", "ab", ".", "|", "*", "+", "?", "(", ")", "()", "[ab]", "[^a]", "[a-c]", "{2}", "{1,3}", "{0,}",
    "^", "$", "\\d", "\\w", "\\.", "[[:alpha:]]", "[", "]", "{", "}", "\\",
};

static size_t random_pattern(FuzzSource* source, char* pattern, size_t capacity)
{
    const size_t count = fuzz_range(source, 1, 12);
    size_t size = 0;
    size_t i;

    for(i = 0; i < count; i++)
    {
        const char* token = g_pattern_tokens[fuzz_index(source, sizeof(g_pattern_tokens) / sizeof(g_pattern_tokens[0]))];
        const size_t token_size = strlen(token);

        if(size + token_size >= capacity)
            break;

        memcpy(pattern + size, token, token_size);
        size += token_size;
    }

    pattern[size] = '\0';

    return size;
}

static bool count_iterations(const RegexMatch* match, void* user_data)
{
    ROMANO_UNUSED(match);
    (*(size_t*)user_data)++;
    return true;
}

static bool property_consistency(FuzzSource* source, void* user_data)
{
    char pattern[64];
    char subject[33];
    const RegexFlags flags = (RegexFlags)(fuzz_bool(source) ? RegexFlags_IgnoreCase : RegexFlags_None);
    Regex* regex;
    RegexMatch match;
    size_t subject_size;
    size_t iterations = 0;
    bool full;
    bool found;

    ROMANO_UNUSED(user_data);

    random_pattern(source, pattern, sizeof(pattern));
    subject_size = fuzz_string(source, subject, 32, "abcABC1. _");

    regex = regex_compile(pattern, flags);

    if(regex == NULL)
        return true;

    full = regex_match(regex, subject, subject_size);
    found = regex_search(regex, subject, subject_size, &match);
    regex_iterate(regex, subject, subject_size, count_iterations, &iterations);

    regex_free(regex);

    TEST_FUZZ_CHECK_MSG(!full || (found && match.position == 0 && match.data_sz == subject_size),
                        "\"%s\" fully matches \"%s\" but search gives (%zu, %zu)",
                        pattern, subject, found ? (size_t)match.position : 0, found ? (size_t)match.data_sz : 0);
    TEST_FUZZ_CHECK_MSG(!found || (match.position + match.data_sz <= subject_size && iterations > 0),
                        "\"%s\" on \"%s\": inconsistent search/iterate", pattern, subject);
    TEST_FUZZ_CHECK_MSG(found || iterations == 0, "\"%s\" on \"%s\": iterate found matches but search did not", pattern, subject);

    return true;
}

static void test_fuzz_consistency(void)
{
    logger_set_level(LogLevel_Fatal);
    test_fuzz_property("regex_consistency", 20000, property_consistency, NULL);
    logger_set_level(LogLevel_Info);
}

static bool target_compile(const uint8_t* data, size_t size, void* user_data)
{
    char pattern[257];
    Regex* regex;

    ROMANO_UNUSED(user_data);

    if(size > 256)
        size = 256;

    memcpy(pattern, data, size);
    pattern[size] = '\0';

    regex = regex_compile(pattern, RegexFlags_None);

    if(regex != NULL)
    {
        regex_search(regex, "aab.c\n1x_", 9, NULL);
        regex_free(regex);
    }

    return true;
}

static void test_fuzz_compile(void)
{
    static const FuzzCorpusEntry corpus[] = {
        { "^(ab|c)+[[:digit:]]{2}$", 23 },
        { "[]a]+|[^]a]|[--/]+", 18 },
        { "\\d{3}-\\d{4}|(a*)*b", 18 },
        { "[[.-.]a]+[[=e=]]x{0,2}", 22 },
    };
    FuzzDictionary dictionary = { g_pattern_tokens, sizeof(g_pattern_tokens) / sizeof(g_pattern_tokens[0]) };
    FuzzOptions options;

    fuzz_options_init(&options, "regex_compile");
    options.iterations = test_scaled(20000);
    options.max_input_size = 256;
    options.corpus = corpus;
    options.corpus_count = sizeof(corpus) / sizeof(corpus[0]);
    options.dictionary = &dictionary;

    logger_set_level(LogLevel_Fatal);
    test_fuzz_input(&options, target_compile, NULL);
    logger_set_level(LogLevel_Info);
}

#if HAS_POSIX_REGEX
static const char* const g_posix_tokens[] = { "a", "b", "ab", ".", "|", "*", "+", "?", "(a|b)", "(ab)*", "[ab]", "[^a]", "b{2}", "a{1,2}" };

static bool property_matches_posix(FuzzSource* source, void* user_data)
{
    char pattern[64];
    char subject[25];
    const size_t count = fuzz_range(source, 1, 6);
    size_t pattern_size = 0;
    size_t subject_size;
    regex_t posix;
    regmatch_t posix_match;
    Regex* regex;
    RegexMatch match;
    bool found;
    bool posix_found;
    size_t i;

    ROMANO_UNUSED(user_data);

    for(i = 0; i < count; i++)
    {
        const char* token = g_posix_tokens[fuzz_index(source, sizeof(g_posix_tokens) / sizeof(g_posix_tokens[0]))];

        /* Keep patterns valid ERE: no leading/doubled operators, no empty alternatives */
        if(strchr("|*+?", token[0]) != NULL && (pattern_size == 0 || strchr("|*+?", pattern[pattern_size - 1]) != NULL))
            continue;

        memcpy(pattern + pattern_size, token, strlen(token));
        pattern_size += strlen(token);
    }

    while(pattern_size > 0 && pattern[pattern_size - 1] == '|')
        pattern_size--;

    if(pattern_size == 0)
        return true;

    pattern[pattern_size] = '\0';
    subject_size = fuzz_string(source, subject, 24, "abc");

    if(regcomp(&posix, pattern, REG_EXTENDED) != 0)
        return true;

    regex = regex_compile(pattern, RegexFlags_None);

    if(regex == NULL)
    {
        regfree(&posix);
        TEST_FUZZ_CHECK_MSG(false, "\"%s\" compiles with regcomp but not with regex_compile", pattern);
    }

    posix_found = regexec(&posix, subject, 1, &posix_match, 0) == 0;
    found = regex_search(regex, subject, subject_size, &match);

    regfree(&posix);
    regex_free(regex);

    TEST_FUZZ_CHECK_MSG(found == posix_found, "\"%s\" on \"%s\": found %d, posix %d", pattern, subject, found, posix_found);

    if(found)
        TEST_FUZZ_CHECK_MSG(match.position == (size_t)posix_match.rm_so && match.data_sz == (size_t)(posix_match.rm_eo - posix_match.rm_so),
                            "\"%s\" on \"%s\": (%zu, %zu), posix (%d, %d)", pattern, subject,
                            (size_t)match.position, (size_t)match.data_sz, (int)posix_match.rm_so, (int)(posix_match.rm_eo - posix_match.rm_so));

    return true;
}

static void test_fuzz_matches_posix(void)
{
    test_fuzz_property("regex_vs_posix", 20000, property_matches_posix, NULL);
}

#define POSIX_TESTS TEST(test_fuzz_matches_posix),
#else
#define POSIX_TESTS
#endif /* HAS_POSIX_REGEX */

TEST_MAIN(
    TEST(test_debug_output),
    TEST(test_operators),
    TEST(test_bounds),
    TEST(test_brackets),
    TEST(test_escapes),
    TEST(test_case_insensitive),
    TEST(test_dot_and_newlines),
    TEST(test_anchors),
    TEST(test_leftmost_longest),
    TEST(test_errors),
    TEST(test_iterate),
    TEST(test_big_inputs),
    TEST(test_fuzz_consistency),
    TEST(test_fuzz_compile),
    POSIX_TESTS
)
