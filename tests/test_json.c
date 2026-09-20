/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/json.h"

static const char g_document[] =
    "{\n"
    "  \"name\": \"libromano\",\n"
    "  \"version\": 3,\n"
    "  \"offset\": -42,\n"
    "  \"ratio\": 0.125,\n"
    "  \"big\": 1.5e300,\n"
    "  \"enabled\": true,\n"
    "  \"disabled\": false,\n"
    "  \"nothing\": null,\n"
    "  \"tags\": [\"c\", \"library\", 1, [], {}],\n"
    "  \"nested\": { \"deep\": { \"value\": [1, 2, 3] } },\n"
    "  \"escaped\": \"line\\nbreak \\\"quoted\\\" \\\\ \\/ \\t\\b\\f\\r\"\n"
    "}";

static Json* parse(const char* str)
{
    return json_loads(str, strlen(str));
}

static void test_parse_document(void)
{
    Json* json = parse(g_document);
    JsonValue* root;
    JsonValue* tags;
    JsonValue* value;
    JsonArrayIterator array_it = { NULL };
    JsonDictIterator dict_it = { NULL };
    JsonKeyValue* key_value;
    size_t count = 0;

    TEST_ASSERT(json != NULL);
    root = json->root;

    TEST_ASSERT(json_is_dict(root));
    TEST_CHECK_EQ_UINT(json_dict_get_size(root), 11);
    TEST_CHECK_EQ_UINT(json_array_get_size(root), 0);

    value = json_dict_find(json, root, "name");
    TEST_ASSERT(value != NULL && json_is_str(value));
    TEST_CHECK_EQ_STR(json_str_get(value), "libromano");
    TEST_CHECK_EQ_UINT(json_str_get_size(value), 9);

    value = json_dict_find(json, root, "version");
    TEST_CHECK(json_is_u64(value) && json_u64_get(value) == 3);
    TEST_CHECK(json_i64_get(value) == JSON_INVALID_I64);

    value = json_dict_find(json, root, "offset");
    TEST_CHECK(json_is_i64(value) && json_i64_get(value) == -42);
    TEST_CHECK(json_u64_get(value) == JSON_INVALID_U64);

    TEST_CHECK_NEAR(json_f64_get(json_dict_find(json, root, "ratio")), 0.125, 0.0);
    TEST_CHECK_NEAR(json_f64_get(json_dict_find(json, root, "big")), 1.5e300, 1e285);
    TEST_CHECK(json_is_bool(json_dict_find(json, root, "enabled")) && json_bool_get(json_dict_find(json, root, "enabled")));
    TEST_CHECK(!json_bool_get(json_dict_find(json, root, "disabled")));
    TEST_CHECK(json_is_null(json_dict_find(json, root, "nothing")));
    TEST_CHECK(json_dict_find(json, root, "missing") == NULL);
    TEST_CHECK(json_str_get(json_dict_find(json, root, "version")) == NULL);

    TEST_CHECK_EQ_STR(json_str_get(json_dict_find(json, root, "escaped")), "line\nbreak \"quoted\" \\ / \t\b\f\r");

    tags = json_dict_find(json, root, "tags");
    TEST_ASSERT(json_is_array(tags));
    TEST_CHECK_EQ_UINT(json_array_get_size(tags), 5);

    while((value = json_array_get_next(json, tags, &array_it)) != NULL)
        count++;

    TEST_CHECK_EQ_UINT(count, 5);

    value = json_dict_find(json, json_dict_find(json, json_dict_find(json, root, "nested"), "deep"), "value");
    TEST_CHECK_EQ_UINT(json_array_get_size(value), 3);

    count = 0;

    while((key_value = json_dict_get_next(json, root, &dict_it)) != NULL)
    {
        TEST_CHECK(json_dict_find(json, root, key_value->key) == key_value->value);
        count++;
    }

    TEST_CHECK_EQ_UINT(count, 11);

    json_free(json);
}

static void test_numbers(void)
{
    Json* json;

    json = parse("18446744073709551615");
    TEST_CHECK(json != NULL && json_u64_get(json->root) == UINT64_MAX);
    json_free(json);

    json = parse("-9223372036854775808");
    TEST_CHECK(json != NULL && json_i64_get(json->root) == INT64_MIN);
    json_free(json);

    json = parse("18446744073709551616");
    TEST_CHECK(json != NULL && json_is_f64(json->root));
    TEST_CHECK_NEAR(json_f64_get(json->root), 18446744073709551616.0, 1.0);
    json_free(json);

    json = parse("-9223372036854775809");
    TEST_CHECK(json != NULL && json_is_f64(json->root));
    json_free(json);

    json = parse("[0, -0, 1E3, 2e-3, 0.1, -1.5E+2, 123456789012345678901234567890]");
    TEST_ASSERT(json != NULL);
    {
        static const double expected[] = { 0.0, 0.0, 1e3, 2e-3, 0.1, -150.0, 1.2345678901234568e29 };
        JsonArrayIterator it = { NULL };
        JsonValue* value;
        size_t i = 0;

        while((value = json_array_get_next(json, json->root, &it)) != NULL)
        {
            const double v = json_is_f64(value) ? json_f64_get(value) :
                             json_is_i64(value) ? (double)json_i64_get(value) : (double)json_u64_get(value);

            TEST_CHECK_NEAR(v, expected[i], fabs(expected[i]) * 1e-15);
            i++;
        }
    }
    json_free(json);
}

static void test_strings(void)
{
    Json* json;

    json = parse("\"caf\\u00e9 \\u20ac \\ud83d\\ude00 \\u0041\"");
    TEST_ASSERT(json != NULL);
    TEST_CHECK_EQ_STR(json_str_get(json->root), "caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80 A");
    TEST_CHECK_EQ_UINT(json_str_get_size(json->root), strlen("caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80 A"));
    json_free(json);

    json = parse("\"\"");
    TEST_CHECK(json != NULL && json_str_get_size(json->root) == 0);
    json_free(json);
}

static void test_invalid_documents(void)
{
    static const char* const invalid[] = {
        "", " ", "{", "}", "[", "[1,]", "[1 2]", "{\"a\" 1}", "{\"a\":}", "{\"a\":1,}", "{a:1}", "{\"a\":1 \"b\":2}",
        "\"unterminated", "\"bad \\x escape\"", "\"\\u12\"", "\"\\uZZZZ\"", "\"\\ud800\"", "\"\\udc00\"", "\"\\ud800\\u0041\"",
        "\"raw\ncontrol\"", "tru", "nul", "falsey", "01", "1.", ".5", "-", "1e", "1e+", "+1", "[1] [2]", "{} x", "NaN",
    };
    char deep[2048];
    size_t i;

    for(i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
    {
        Json* json = parse(invalid[i]);

        TEST_CHECK_MSG(json == NULL, "\"%s\" should not parse", invalid[i]);

        if(json != NULL)
            json_free(json);
    }

    TEST_CHECK(json_loads(NULL, 0) == NULL);

    memset(deep, '[', 1000);
    memset(deep + 1000, ']', 1000);
    deep[2000] = '\0';
    TEST_CHECK(parse(deep) == NULL);

    memset(deep, '[', 100);
    memset(deep + 100, ']', 100);
    deep[200] = '\0';
    {
        Json* json = parse(deep);
        TEST_CHECK(json != NULL);
        json_free(json);
    }
}

static void test_build_and_dump(void)
{
    Json* json = json_new();
    JsonValue* root;
    JsonValue* array;
    JsonValue* value;
    char* dumped;
    size_t dumped_size;

    TEST_ASSERT(json != NULL);
    TEST_CHECK(json_dumps(json, 0, &dumped_size) == NULL);

    root = json_dict_new(json, NULL);
    array = json_array_new(json);
    json_set_root(json, root);

    json_array_append(json, array, json_u64_new(json, 1), true);
    json_array_append(json, array, json_i64_new(json, -2), false);
    json_array_append(json, array, json_f64_new(json, 2.5), true);
    json_array_append(json, array, json_f64_new(json, 3.0), true);
    json_array_append(json, array, json_f64_new(json, NAN), true);

    json_dict_append(json, root, "array", array, true);
    json_dict_append(json, root, "str", json_str_new(json, "tab\there \x01 'q' \"dq\""), false);
    json_dict_append(json, root, "flag", json_bool_new(json, false), true);
    json_dict_append(json, root, "none", json_null_new(json), true);
    json_dict_append(json, root, "gone", json_u64_new(json, 7), true);

    dumped = json_dumps(json, 0, &dumped_size);
    TEST_ASSERT(dumped != NULL);
    TEST_CHECK_EQ_UINT(dumped_size, strlen(dumped));
    TEST_CHECK_EQ_STR(dumped, "{\"array\": [1,-2,2.5,3.0,null],\"str\": \"tab\\there \\u0001 'q' \\\"dq\\\"\","
                              "\"flag\": false,\"none\": null,\"gone\": 7}");
    free(dumped);

    json_array_pop(json, array, 4);
    json_array_pop(json, array, 0);
    json_array_pop(json, array, 100);
    json_array_append(json, array, json_u64_new(json, 9), true);
    TEST_CHECK_EQ_UINT(json_array_get_size(array), 4);

    json_dict_pop(json, root, "array");
    json_dict_pop(json, root, "gone");
    json_dict_pop(json, root, "missing");
    json_dict_append(json, root, "array", array, true);
    TEST_CHECK_EQ_UINT(json_dict_get_size(root), 4);

    value = json_dict_find(json, root, "flag");
    json_bool_set(json, value, true);
    value = json_dict_find(json, root, "none");
    json_str_set(json, value, "now a string");
    TEST_CHECK_EQ_UINT(json_str_get_size(value), 12);

    dumped = json_dumps(json, 0, NULL);
    TEST_ASSERT(dumped != NULL);
    TEST_CHECK_EQ_STR(dumped, "{\"str\": \"tab\\there \\u0001 'q' \\\"dq\\\"\",\"flag\": true,\"none\": \"now a string\","
                              "\"array\": [-2,2.5,3.0,9]}");
    free(dumped);

    dumped = json_dumps(json, 2, NULL);
    TEST_ASSERT(dumped != NULL);
    TEST_CHECK(strstr(dumped, "\n  \"flag\": true") != NULL);
    TEST_CHECK(strstr(dumped, "\n    9\n  ]") != NULL);
    free(dumped);

    value = json_u64_new(json, 0);
    json_null_set(json, value);
    TEST_CHECK(json_is_null(value));
    json_u64_set(json, value, 5);
    TEST_CHECK(json_u64_get(value) == 5);
    json_i64_set(json, value, -5);
    TEST_CHECK(json_i64_get(value) == -5);
    json_f64_set(json, value, 0.5);
    TEST_CHECK(json_f64_get(value) == 0.5);

    json_free(json);
}

static void test_long_strings(void)
{
    const size_t size = 300 * 1024;
    char* document = (char*)malloc(size + 3);
    Json* json;

    TEST_ASSERT(document != NULL);

    document[0] = '"';
    memset(document + 1, 'x', size);
    document[size + 1] = '"';
    document[size + 2] = '\0';

    json = json_loads(document, size + 2);
    TEST_ASSERT(json != NULL);
    TEST_CHECK_EQ_UINT(json_str_get_size(json->root), size);

    json_free(json);
    free(document);
}

static void test_files(void)
{
    const char* path = test_tmp_path("test_json.json");
    const char* empty_path = test_tmp_path("test_json_empty.json");
    Json* json = parse(g_document);
    Json* reloaded;
    char* a;
    char* b;
    FILE* empty;

    TEST_ASSERT(json != NULL);
    TEST_ASSERT(json_dumpf(json, 4, path));

    reloaded = json_loadf(path);
    TEST_ASSERT(reloaded != NULL);

    a = json_dumps(json, 0, NULL);
    b = json_dumps(reloaded, 0, NULL);
    TEST_CHECK_EQ_STR(a, b);

    free(a);
    free(b);
    json_free(reloaded);

    TEST_CHECK(!json_dumpf(json, 0, "/this/directory/does/not/exist.json"));
    json_free(json);

    TEST_CHECK(json_loadf("/this/directory/does/not/exist.json") == NULL);

    empty = fopen(empty_path, "wb");
    TEST_ASSERT(empty != NULL);
    fclose(empty);
    TEST_CHECK(json_loadf(empty_path) == NULL);
}

static bool values_equal(Json* ja, JsonValue* a, Json* jb, JsonValue* b)
{
    if(json_is_null(a) || json_is_null(b))
        return json_is_null(a) && json_is_null(b);

    if(json_is_bool(a))
        return json_is_bool(b) && json_bool_get(a) == json_bool_get(b);

    if(json_is_u64(a))
        return json_is_u64(b) && json_u64_get(a) == json_u64_get(b);

    if(json_is_i64(a))
        return json_is_i64(b) && json_i64_get(a) == json_i64_get(b);

    if(json_is_f64(a))
        return json_is_f64(b) && (json_f64_get(a) == json_f64_get(b) || (!isfinite(json_f64_get(a)) && json_is_null(b)));

    if(json_is_str(a))
        return json_is_str(b) && strcmp(json_str_get(a), json_str_get(b)) == 0;

    if(json_is_array(a))
    {
        JsonArrayIterator ita = { NULL };
        JsonArrayIterator itb = { NULL };
        JsonValue* va;
        JsonValue* vb;

        if(!json_is_array(b) || json_array_get_size(a) != json_array_get_size(b))
            return false;

        while((va = json_array_get_next(ja, a, &ita)) != NULL)
        {
            vb = json_array_get_next(jb, b, &itb);

            if(vb == NULL || !values_equal(ja, va, jb, vb))
                return false;
        }

        return true;
    }

    if(json_is_dict(a))
    {
        JsonDictIterator ita = { NULL };
        JsonDictIterator itb = { NULL };
        JsonKeyValue* ka;
        JsonKeyValue* kb;

        if(!json_is_dict(b) || json_dict_get_size(a) != json_dict_get_size(b))
            return false;

        while((ka = json_dict_get_next(ja, a, &ita)) != NULL)
        {
            kb = json_dict_get_next(jb, b, &itb);

            if(kb == NULL || strcmp(ka->key, kb->key) != 0 || !values_equal(ja, ka->value, jb, kb->value))
                return false;
        }

        return true;
    }

    return false;
}

static JsonValue* random_value(FuzzSource* source, Json* json, int depth)
{
    char buffer[33];
    size_t i;
    size_t count;
    JsonValue* container;

    switch(fuzz_range(source, 0, depth < 4 ? 8 : 6))
    {
        case 0: return json_null_new(json);
        case 1: return json_bool_new(json, fuzz_bool(source));
        case 2: return json_u64_new(json, fuzz_u64_special(source));
        case 3:
        {
            int64_t v = fuzz_i64_special(source);
            return v < 0 ? json_i64_new(json, v) : json_u64_new(json, (uint64_t)v);
        }
        case 4: return json_f64_new(json, fuzz_f64_finite(source));
        case 5:
        case 6:
            fuzz_string(source, buffer, 32, NULL);
            return json_str_new(json, buffer);
        case 7:
            container = json_array_new(json);
            count = fuzz_size(source, 6);

            for(i = 0; i < count; i++)
                json_array_append(json, container, random_value(source, json, depth + 1), true);

            return container;
        default:
            container = json_dict_new(json, NULL);
            count = fuzz_size(source, 6);

            for(i = 0; i < count; i++)
            {
                fuzz_string(source, buffer, 16, NULL);
                json_dict_append(json, container, buffer, random_value(source, json, depth + 1), true);
            }

            return container;
    }
}

static bool property_roundtrip(FuzzSource* source, void* user_data)
{
    Json* json = json_new();
    Json* parsed;
    const size_t indent = fuzz_range(source, 0, 4);
    char* dumped;
    char* dumped_again;
    size_t dumped_size;
    bool equal;

    ROMANO_UNUSED(user_data);

    json_set_root(json, random_value(source, json, 0));

    dumped = json_dumps(json, indent, &dumped_size);
    TEST_FUZZ_CHECK(dumped != NULL);

    parsed = json_loads(dumped, dumped_size);

    if(parsed == NULL)
    {
        free(dumped);
        json_free(json);
        TEST_FUZZ_CHECK_MSG(false, "dumped document does not parse back");
    }

    equal = values_equal(json, json->root, parsed, parsed->root);
    dumped_again = json_dumps(parsed, indent, NULL);

    TEST_FUZZ_CHECK_MSG(equal, "roundtrip changed the document: %.200s", dumped);
    TEST_FUZZ_CHECK_MSG(dumped_again != NULL && strcmp(dumped, dumped_again) == 0, "dumps is not stable: %.200s", dumped);

    free(dumped);
    free(dumped_again);
    json_free(parsed);
    json_free(json);

    return true;
}

static void test_fuzz_roundtrip(void)
{
    test_fuzz_property("json_roundtrip", 3000, property_roundtrip, NULL);
}

static bool target_parse(const uint8_t* data, size_t size, void* user_data)
{
    Json* json = json_loads((const char*)data, size);
    Json* reparsed;
    char* dumped;
    char* dumped_again;
    size_t dumped_size;
    bool stable;

    ROMANO_UNUSED(user_data);

    if(json == NULL)
        return true;

    dumped = json_dumps(json, 0, &dumped_size);
    json_free(json);

    TEST_FUZZ_CHECK(dumped != NULL);

    reparsed = json_loads(dumped, dumped_size);

    if(reparsed == NULL)
    {
        free(dumped);
        TEST_FUZZ_CHECK_MSG(false, "dumped document does not parse back");
    }

    dumped_again = json_dumps(reparsed, 0, NULL);
    stable = dumped_again != NULL && strcmp(dumped, dumped_again) == 0;

    free(dumped);
    free(dumped_again);
    json_free(reparsed);

    TEST_FUZZ_CHECK(stable);

    return true;
}

static void test_fuzz_parser(void)
{
    static const char* const tokens[] = {
        "{", "}", "[", "]", ",", ":", "\"", "\\", "\\u", "\\ud83d\\ude00", "\\u00e9", "true", "false", "null",
        "-", "0", "1e308", "1e-400", "18446744073709551616", ".5", "e+", " ", "\n", "\"key\":", "[[[[", "]]]]",
    };
    static const char small[] = "{\"a\": [1, -2, 3.5, true, null, \"s\\n\"], \"b\": {\"c\": {}}}";
    static const char numbers[] = "[0, -0, 1e10, -1.5e-3, 18446744073709551615, -9223372036854775808]";
    static const char strings[] = "[\"\", \"\\u0000\", \"\\ud83d\\ude00\", \"\\\"\\\\\\/\\b\\f\\n\\r\\t\"]";
    const FuzzCorpusEntry corpus[] = {
        { g_document, sizeof(g_document) - 1 },
        { small, sizeof(small) - 1 },
        { numbers, sizeof(numbers) - 1 },
        { strings, sizeof(strings) - 1 },
    };
    FuzzDictionary dictionary = { tokens, sizeof(tokens) / sizeof(tokens[0]) };
    FuzzOptions options;

    fuzz_options_init(&options, "json_parser");
    options.iterations = test_scaled(20000);
    options.max_input_size = 1024;
    options.corpus = corpus;
    options.corpus_count = sizeof(corpus) / sizeof(corpus[0]);
    options.dictionary = &dictionary;

    test_fuzz_input(&options, target_parse, NULL);
}

TEST_MAIN(
    TEST(test_parse_document),
    TEST(test_numbers),
    TEST(test_strings),
    TEST(test_invalid_documents),
    TEST(test_build_and_dump),
    TEST(test_long_strings),
    TEST(test_files),
    TEST(test_fuzz_roundtrip),
    TEST(test_fuzz_parser),
)
