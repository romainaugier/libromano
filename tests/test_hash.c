/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/hash.h"

static void test_known_vectors(void)
{
    TEST_CHECK_EQ_UINT(hash_fnv1a("", 0), 0x811C9DC5u);
    TEST_CHECK_EQ_UINT(hash_fnv1a("a", 1), 0xE40C292Cu);
    TEST_CHECK_EQ_UINT(hash_fnv1a("foobar", 6), 0xBF9CF968u);

    TEST_CHECK_EQ_UINT(hash_murmur3("", 0, 0), 0);
    TEST_CHECK_EQ_UINT(hash_murmur3("", 0, 1), 0x514E28B7u);
    TEST_CHECK_EQ_UINT(hash_murmur3("hello", 5, 0), 0x248BFA47u);
    TEST_CHECK_EQ_UINT(hash_murmur3("The quick brown fox jumps over the lazy dog", 43, 0), 0x2E4FF723u);

    TEST_CHECK_EQ_UINT(hash_city64((const uint8_t*)"", 0), 0x9AE16A3B2F90404FULL);
    TEST_CHECK_EQ_UINT(hash_city32((const uint8_t*)"", 0), 0xDC56D17Au);
}

static uint64_t splitmix64(uint64_t* state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

#define CITY_REFERENCE_MAX_SIZE 1024
#define CITY_REFERENCE_OUTPUTS 16

/*
 * FNV-1a folds of the outputs of Google's reference CityHash v1.1 (city.cc) over inputs of
 * sizes 0 to 1024 bytes generated with splitmix64 (seed 12345): bytes first, then seed0 and seed1
 */
static const uint64_t g_city_reference_folds[CITY_REFERENCE_OUTPUTS] = {
    0x56AEBB52F16DD7F0ULL,
    0xB0F650D03264D8CBULL,
    0xAA0C3908493B26D3ULL,
    0x75C9E132B0C04CD1ULL,
    0x65DAB70D4CD0618EULL,
    0x46153E7CD4B62596ULL,
    0xB3E1E5299170BFB0ULL,
    0x0D61181761440250ULL,
    0x30521207FFEED095ULL,
    0x7B6F9609DEB9528AULL,
    0xDFDB49E68830B7B3ULL,
    0xD7C1B7E11D52C5E1ULL,
    0xB9CDA36D079E5248ULL,
    0xABC76083CF5CDF19ULL,
    0x1B9F607EECFEFF2DULL,
    0x27852D78B3F344AEULL,
};

static const char* const g_city_reference_names[CITY_REFERENCE_OUTPUTS] = {
    "city32", "city64", "city64_with_seed", "city64_with_seeds", "city128.lo", "city128.hi",
    "city128_with_seed.lo", "city128_with_seed.hi", "city_crc128.lo", "city_crc128.hi",
    "city_crc128_with_seed.lo", "city_crc128_with_seed.hi", "city_crc256[0]", "city_crc256[1]",
    "city_crc256[2]", "city_crc256[3]",
};

static void test_city_reference(void)
{
    uint8_t data[CITY_REFERENCE_MAX_SIZE];
    uint64_t folds[CITY_REFERENCE_OUTPUTS];
    uint64_t state = 12345;
    size_t size;
    size_t i;

    for(i = 0; i < CITY_REFERENCE_OUTPUTS; i++)
        folds[i] = 0xCBF29CE484222325ULL;

    for(size = 0; size <= CITY_REFERENCE_MAX_SIZE; size++)
    {
        uint64_t outputs[CITY_REFERENCE_OUTPUTS];
        uint64_t seed0;
        uint64_t seed1;
        hash_uint128_t h;

        for(i = 0; i < size; i++)
            data[i] = (uint8_t)(splitmix64(&state) & 0xFF);

        seed0 = splitmix64(&state);
        seed1 = splitmix64(&state);

        outputs[0] = hash_city32(data, size);
        outputs[1] = hash_city64(data, size);
        outputs[2] = hash_city64_with_seed(data, size, seed0);
        outputs[3] = hash_city64_with_seeds(data, size, seed0, seed1);
        h = hash_city128(data, size);
        outputs[4] = h.lo;
        outputs[5] = h.hi;
        h = hash_city128_with_seed(data, size, hash_uint128_make(seed0, seed1));
        outputs[6] = h.lo;
        outputs[7] = h.hi;
        h = hash_city_crc128(data, size);
        outputs[8] = h.lo;
        outputs[9] = h.hi;
        h = hash_city_crc128_with_seed(data, size, hash_uint128_make(seed0, seed1));
        outputs[10] = h.lo;
        outputs[11] = h.hi;
        hash_city_crc256(data, size, outputs + 12);

        for(i = 0; i < CITY_REFERENCE_OUTPUTS; i++)
            folds[i] = (folds[i] ^ outputs[i]) * 0x100000001B3ULL;

        if(size == 3)
        {
            TEST_CHECK_EQ_UINT(outputs[0], 0x08EB7B33u);
            TEST_CHECK_EQ_UINT(outputs[1], 0x2E92B440AF5E7E65ULL);
            TEST_CHECK_EQ_UINT(outputs[12], 0x65F08D5F9FE2DC88ULL);
        }
        else if(size == 100)
        {
            TEST_CHECK_EQ_UINT(outputs[0], 0xC665DFA0u);
            TEST_CHECK_EQ_UINT(outputs[1], 0x4A90EFD70F4888C3ULL);
            TEST_CHECK_EQ_UINT(outputs[2], 0xDA8233CF0FE1B079ULL);
        }
    }

    for(i = 0; i < CITY_REFERENCE_OUTPUTS; i++)
        TEST_CHECK_MSG(folds[i] == g_city_reference_folds[i], "%s differs from the reference implementation",
                       g_city_reference_names[i]);
}

typedef struct HashResults {
    uint64_t values[16];
} HashResults;

static void compute_all(const uint8_t* data, size_t size, uint64_t seed, HashResults* results)
{
    const hash_uint128_t seed128 = hash_uint128_make(seed, ~seed);
    hash_uint128_t h128;
    uint64_t h256[4];

    results->values[0] = hash_fnv1a((const char*)data, size);
    results->values[1] = hash_fnv1a_pippip((const char*)data, size);
    results->values[2] = hash_murmur3(data, size, (uint32_t)seed);
    results->values[3] = hash_wyhash64(data, size, seed);
    results->values[4] = hash_wyhash32(data, size, (uint32_t)seed);
    results->values[5] = hash_city64(data, size);
    results->values[6] = hash_city64_with_seed(data, size, seed);
    results->values[7] = hash_city64_with_seeds(data, size, seed, seed * 31);
    results->values[8] = hash_city32(data, size);

    h128 = hash_city128(data, size);
    results->values[9] = hash_128_to_64(h128);

    h128 = hash_city128_with_seed(data, size, seed128);
    results->values[10] = h128.lo ^ h128.hi;

    h128 = hash_city_crc128(data, size);
    results->values[11] = h128.lo ^ h128.hi;

    h128 = hash_city_crc128_with_seed(data, size, seed128);
    results->values[12] = h128.lo ^ h128.hi;

    hash_city_crc256(data, size, h256);
    results->values[13] = h256[0] ^ h256[1] ^ h256[2] ^ h256[3];
}

#define NUM_HASHES 14
#define PIPPIP_INDEX 1

static const char* const g_hash_names[NUM_HASHES] = {
    "fnv1a", "fnv1a_pippip", "murmur3", "wyhash64", "wyhash32", "city64", "city64_with_seed",
    "city64_with_seeds", "city32", "city128", "city128_with_seed", "city_crc128",
    "city_crc128_with_seed", "city_crc256"
};

static bool property_hashes(FuzzSource* source, void* user_data)
{
    uint8_t data[320];
    uint8_t unaligned[321];
    const size_t size = fuzz_range(source, 0, 300);
    const uint64_t seed = fuzz_u64(source);
    HashResults a;
    HashResults b;
    HashResults flipped;
    size_t i;

    ROMANO_UNUSED(user_data);

    fuzz_bytes(source, data, size);
    memcpy(unaligned + 1, data, size);

    compute_all(data, size, seed, &a);
    compute_all(unaligned + 1, size, seed, &b);

    for(i = 0; i < NUM_HASHES; i++)
        TEST_FUZZ_CHECK_MSG(a.values[i] == b.values[i], "%s depends on alignment or is not deterministic (size %zu)",
                            g_hash_names[i], size);

    if(size == 0)
        return true;

    data[fuzz_index(source, size)] ^= (uint8_t)(1u << fuzz_range(source, 0, 7));
    compute_all(data, size, seed, &flipped);

    for(i = 0; i < NUM_HASHES; i++)
    {
        /* pippip mixes overlapping words with xor and multiply, high bits flips can cancel out */
        if(i == PIPPIP_INDEX)
            continue;

        TEST_FUZZ_CHECK_MSG(a.values[i] != flipped.values[i], "%s ignores a bit flip (size %zu)", g_hash_names[i], size);
    }

    return true;
}

static void test_fuzz_hashes(void)
{
    test_fuzz_property("hashes", 20000, property_hashes, NULL);
}

static void test_seeds(void)
{
    const uint8_t data[] = "seed sensitivity";
    const size_t size = sizeof(data) - 1;
    hash_uint128_t a;
    hash_uint128_t b;

    TEST_CHECK(hash_murmur3(data, size, 1) != hash_murmur3(data, size, 2));
    TEST_CHECK(hash_wyhash64(data, size, 1) != hash_wyhash64(data, size, 2));
    TEST_CHECK(hash_wyhash32(data, size, 1) != hash_wyhash32(data, size, 2));
    TEST_CHECK(hash_city64_with_seed(data, size, 1) != hash_city64_with_seed(data, size, 2));
    TEST_CHECK(hash_city64_with_seeds(data, size, 1, 2) != hash_city64_with_seeds(data, size, 2, 1));

    a = hash_city128_with_seed(data, size, hash_uint128_make(1, 2));
    b = hash_city128_with_seed(data, size, hash_uint128_make(2, 1));
    TEST_CHECK(a.lo != b.lo || a.hi != b.hi);

    TEST_CHECK(hash_128_to_64(hash_uint128_make(1, 0)) != hash_128_to_64(hash_uint128_make(0, 1)));
}

TEST_MAIN(
    TEST(test_known_vectors),
    TEST(test_city_reference),
    TEST(test_seeds),
    TEST(test_fuzz_hashes),
)
