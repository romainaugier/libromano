/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/base64.h"

static void test_known_vectors(void)
{
    static const char* const plain[] = { "f", "fo", "foo", "foob", "fooba", "foobar", "light work." };
    static const char* const encoded[] = { "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy", "bGlnaHQgd29yay4=" };
    size_t i;

    for(i = 0; i < sizeof(plain) / sizeof(plain[0]); i++)
    {
        size_t encoded_size;
        size_t decoded_size;
        char* e = base64_encode(plain[i], strlen(plain[i]), &encoded_size);
        char* d;

        TEST_ASSERT(e != NULL);
        TEST_CHECK_EQ_UINT(encoded_size, strlen(encoded[i]));
        TEST_CHECK_EQ_MEM(e, encoded[i], encoded_size);

        d = (char*)base64_decode(encoded[i], strlen(encoded[i]), &decoded_size);

        TEST_ASSERT(d != NULL);
        TEST_CHECK_EQ_UINT(decoded_size, strlen(plain[i]));
        TEST_CHECK_EQ_MEM(d, plain[i], decoded_size);

        free(e);
        free(d);
    }
}

static void test_empty(void)
{
    size_t size = 1;
    char* e = base64_encode("", 0, &size);

    TEST_CHECK_EQ_UINT(size, 0);
    free(e);

    size = 1;
    free(base64_decode("", 0, &size));
    TEST_CHECK_EQ_UINT(size, 0);
}

static void test_invalid_input(void)
{
    static const char* const invalid[] = { "abc", "a===", "ab!d", "Zm9v\n", "Zm9v Zg==", "Zg=a", "\x80\x81\x82\x83", "~~~~" };
    size_t i;

    for(i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
    {
        size_t size;
        void* d = base64_decode(invalid[i], strlen(invalid[i]), &size);

        TEST_CHECK_MSG(d == NULL, "\"%s\" should not decode", invalid[i]);
        free(d);
    }
}

static bool property_roundtrip(FuzzSource* source, void* user_data)
{
    uint8_t data[512];
    size_t size = fuzz_size(source, sizeof(data));
    size_t encoded_size;
    size_t decoded_size;
    char* encoded;
    uint8_t* decoded;
    size_t i;
    bool ok;

    ROMANO_UNUSED(user_data);

    fuzz_bytes(source, data, size);

    encoded = base64_encode(data, size, &encoded_size);
    TEST_FUZZ_CHECK(encoded != NULL);
    TEST_FUZZ_CHECK(encoded_size == (size + 2) / 3 * 4);

    for(i = 0; i < encoded_size; i++)
    {
        const char c = encoded[i];
        TEST_FUZZ_CHECK((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                        c == '+' || c == '/' || (c == '=' && i >= encoded_size - 2));
    }

    decoded = (uint8_t*)base64_decode(encoded, encoded_size, &decoded_size);
    ok = decoded != NULL && decoded_size == size && memcmp(decoded, data, size) == 0;

    free(encoded);
    free(decoded);

    TEST_FUZZ_CHECK_MSG(ok, "roundtrip failed for %zu bytes", size);

    return true;
}

static void test_fuzz_roundtrip(void)
{
    test_fuzz_property("base64_roundtrip", 5000, property_roundtrip, NULL);
}

static bool target_decode(const uint8_t* data, size_t size, void* user_data)
{
    size_t decoded_size = 0;
    void* decoded = base64_decode((const char*)data, size, &decoded_size);

    ROMANO_UNUSED(user_data);

    if(decoded != NULL)
    {
        size_t encoded_size;
        char* encoded = base64_encode(decoded, decoded_size, &encoded_size);
        bool same = encoded != NULL && encoded_size == size && memcmp(encoded, data, size) == 0;

        free(encoded);
        free(decoded);

        TEST_FUZZ_CHECK_MSG(same, "decoded input does not re-encode to itself");
    }

    return true;
}

static void test_fuzz_decode(void)
{
    static const char* const tokens[] = { "=", "==", "Zm9v", "+/", "AAAA" };
    static const FuzzCorpusEntry corpus[] = { { "Zm9vYmFy", 8 }, { "Zm9vYg==", 8 }, { "bGlnaHQgd29yay4=", 16 } };
    FuzzDictionary dictionary = { tokens, sizeof(tokens) / sizeof(tokens[0]) };
    FuzzOptions options;

    fuzz_options_init(&options, "base64_decode");
    options.iterations = test_scaled(20000);
    options.max_input_size = 64;
    options.corpus = corpus;
    options.corpus_count = sizeof(corpus) / sizeof(corpus[0]);
    options.dictionary = &dictionary;

    test_fuzz_input(&options, target_decode, NULL);
}

TEST_MAIN(
    TEST(test_known_vectors),
    TEST(test_empty),
    TEST(test_invalid_input),
    TEST(test_fuzz_roundtrip),
    TEST(test_fuzz_decode),
)
