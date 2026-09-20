/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/string.h"

static void test_constructors(void)
{
    String a = string_new("hello");
    String b = string_newz(10);
    String c = string_newf("%s %d %.2f", "value", 42, 1.5);
    String d = string_copy(a);
    String e = string_new("");

    TEST_ASSERT(a != NULL && b != NULL && c != NULL && d != NULL && e != NULL);

    TEST_CHECK_EQ_STR(a, "hello");
    TEST_CHECK_EQ_UINT(string_length(a), 5);
    TEST_CHECK(string_capacity(a) >= 5);

    TEST_CHECK_EQ_UINT(string_length(b), 10);
    TEST_CHECK_EQ_UINT(b[0], '\0');

    TEST_CHECK_EQ_STR(c, "value 42 1.50");
    TEST_CHECK_EQ_UINT(string_length(c), 13);

    TEST_CHECK_EQ_STR(d, "hello");
    TEST_CHECK(d != a);
    TEST_CHECK(string_eq(a, d));
    TEST_CHECK(!string_eq(a, c));

    TEST_CHECK_EQ_UINT(string_length(e), 0);
    TEST_CHECK(string_appendc(&e, "grown from empty"));
    TEST_CHECK_EQ_STR(e, "grown from empty");

    string_free(a);
    string_free(b);
    string_free(c);
    string_free(d);
    string_free(e);
    string_free(NULL);
}

static void test_set_append_prepend(void)
{
    String s = string_new("middle");
    String other = string_new("<>");

    TEST_ASSERT(string_prependc(&s, "start-"));
    TEST_ASSERT(string_appendc(&s, "-end"));
    TEST_CHECK_EQ_STR(s, "start-middle-end");

    TEST_ASSERT(string_appends(&s, other));
    TEST_ASSERT(string_prepends(&s, other));
    TEST_CHECK_EQ_STR(s, "<>start-middle-end<>");

    TEST_ASSERT(string_appendf(&s, "%03d", 7));
    TEST_ASSERT(string_prependf(&s, "%c|", 'x'));
    TEST_CHECK_EQ_STR(s, "x|<>start-middle-end<>007");
    TEST_CHECK_EQ_UINT(string_length(s), strlen(s));

    TEST_ASSERT(string_setc(&s, "reset"));
    TEST_CHECK_EQ_STR(s, "reset");
    TEST_ASSERT(string_sets(&s, other));
    TEST_CHECK_EQ_STR(s, "<>");
    TEST_ASSERT(string_setf(&s, "%s-%s", "a", "b"));
    TEST_CHECK_EQ_STR(s, "a-b");

    string_clear(s);
    TEST_CHECK_EQ_UINT(string_length(s), 0);
    TEST_CHECK_EQ_STR(s, "");

    TEST_ASSERT(string_resize(&s, 1000));
    TEST_CHECK(string_capacity(s) >= 1000);
    TEST_CHECK_EQ_STR(s, "");

    string_free(s);
    string_free(other);
}

static void test_split(void)
{
    char data[] = "a,bb,,ccc";
    char empty[] = "";
    uint32_t count;
    String* parts = string_splitc(data, ",", &count);
    uint32_t i;

    TEST_ASSERT(parts != NULL);
    TEST_ASSERT_EQ_UINT(count, 3);
    TEST_CHECK_EQ_STR(parts[0], "a");
    TEST_CHECK_EQ_STR(parts[1], "bb");
    TEST_CHECK_EQ_STR(parts[2], "ccc");

    for(i = 0; i < count; i++)
        string_free(parts[i]);

    free(parts);

    TEST_CHECK(string_splitc(empty, ",", &count) == NULL);
    TEST_CHECK_EQ_UINT(count, 0);
}

static bool property_string_model(FuzzSource* source, void* user_data)
{
    char reference[8192];
    char chunk[128];
    size_t reference_size;
    size_t operations = fuzz_range(source, 1, 48);
    String string;
    String other;
    bool ok = true;
    size_t i;

    ROMANO_UNUSED(user_data);

    reference_size = fuzz_string(source, reference, 64, FUZZ_ALPHABET_PRINTABLE);
    string = string_new(reference);

    TEST_FUZZ_CHECK(string != NULL);

    for(i = 0; i < operations && ok; i++)
    {
        size_t chunk_size = fuzz_string(source, chunk, sizeof(chunk) - 1, FUZZ_ALPHABET_ALNUM);

        if(reference_size + chunk_size + 16 >= sizeof(reference))
            break;

        other = NULL;

        switch(fuzz_range(source, 0, 10))
        {
            case 0:
                ok = string_appendc(&string, chunk);
                memcpy(reference + reference_size, chunk, chunk_size + 1);
                reference_size += chunk_size;
                break;
            case 1:
                other = string_new(chunk);
                ok = string_appends(&string, other);
                memcpy(reference + reference_size, chunk, chunk_size + 1);
                reference_size += chunk_size;
                break;
            case 2:
                ok = string_appendf(&string, "%s#", chunk);
                memcpy(reference + reference_size, chunk, chunk_size);
                reference[reference_size + chunk_size] = '#';
                reference_size += chunk_size + 1;
                reference[reference_size] = '\0';
                break;
            case 3:
                ok = string_prependc(&string, chunk);
                memmove(reference + chunk_size, reference, reference_size + 1);
                memcpy(reference, chunk, chunk_size);
                reference_size += chunk_size;
                break;
            case 4:
                other = string_new(chunk);
                ok = string_prepends(&string, other);
                memmove(reference + chunk_size, reference, reference_size + 1);
                memcpy(reference, chunk, chunk_size);
                reference_size += chunk_size;
                break;
            case 5:
                ok = string_prependf(&string, "#%s", chunk);
                memmove(reference + chunk_size + 1, reference, reference_size + 1);
                reference[0] = '#';
                memcpy(reference + 1, chunk, chunk_size);
                reference_size += chunk_size + 1;
                break;
            case 6:
                ok = string_setc(&string, chunk);
                memcpy(reference, chunk, chunk_size + 1);
                reference_size = chunk_size;
                break;
            case 7:
                other = string_new(chunk);
                ok = string_sets(&string, other);
                memcpy(reference, chunk, chunk_size + 1);
                reference_size = chunk_size;
                break;
            case 8:
                ok = string_setf(&string, "%s", chunk);
                memcpy(reference, chunk, chunk_size + 1);
                reference_size = chunk_size;
                break;
            case 9:
                string_clear(string);
                reference[0] = '\0';
                reference_size = 0;
                break;
            default:
            {
                String copy = string_copy(string);
                string_free(string);
                string = copy;
                break;
            }
        }

        string_free(other);

        ok &= string != NULL && string_length(string) == reference_size && strcmp(string, reference) == 0;
        ok &= string_capacity(string) >= reference_size;
    }

    TEST_FUZZ_CHECK_MSG(ok, "string \"%s\" diverged from \"%s\" at operation %zu", string, reference, i);

    string_free(string);

    return true;
}

static void test_fuzz_model(void)
{
    test_fuzz_property("string_model", 3000, property_string_model, NULL);
}

TEST_MAIN(
    TEST(test_constructors),
    TEST(test_set_append_prepend),
    TEST(test_split),
    TEST(test_fuzz_model),
)
