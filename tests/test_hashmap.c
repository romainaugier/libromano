/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/hashmap.h"
#include "libromano/random.h"

#include <stdarg.h>
#include <time.h>

/* Configuration */

#if !defined(HASHMAP_TEST_SCALE)
#if defined(ROMANO_DEBUG) && ROMANO_DEBUG
#define HASHMAP_TEST_SCALE 1
#else
#define HASHMAP_TEST_SCALE 8
#endif
#endif

#if !defined(HASHMAP_TEST_INSERT_OVERWRITES)
#define HASHMAP_TEST_INSERT_OVERWRITES -1
#endif

#if !defined(HASHMAP_TEST_UPDATE_INSERTS_MISSING)
#define HASHMAP_TEST_UPDATE_INSERTS_MISSING -1
#endif

#if !defined(HASHMAP_TEST_ZERO_SIZES)
#define HASHMAP_TEST_ZERO_SIZES 0
#endif

#if !defined(HASHMAP_TEST_ALIASING)
#define HASHMAP_TEST_ALIASING 1
#endif

#define MAX_KEY_SIZE 300
#define MAX_VALUE_SIZE 4096

static const size_t g_scale = HASHMAP_TEST_SCALE;
static const char* g_phase = "init";

/* Observed semantics, used to make sure the map behaves consistently */
static size_t g_dup_insert_overwrote = 0;
static size_t g_dup_insert_kept = 0;
static size_t g_update_missing_inserted = 0;
static size_t g_update_missing_ignored = 0;

static void test_fail(const char* file, int line, const char* expr, const char* fmt, ...)
{
    char message[512];
    va_list args;

    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    test_checkf(false, file, line, expr, "phase '%s': %s", g_phase, message);
    test_abort();
}

#define CHECK(cond, ...)                                                    \
    do                                                                      \
    {                                                                       \
        if(!(cond))                                                         \
            test_fail(__FILE__, __LINE__, #cond, __VA_ARGS__);              \
        else                                                                \
            test_check(true, __FILE__, __LINE__, #cond);                    \
    } while(0)

/* Randomness: PRNG or fuzzer-provided bytes behind the same interface */

static uint64_t splitmix64(uint64_t* state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

typedef FuzzSource Source;

#define source_u32(s) fuzz_u32(s)
#define source_range(s, n) ((uint32_t)fuzz_index((s), (n)))

/* Small helpers */

static void write_le32(uint8_t* out, uint32_t v)
{
    out[0] = (uint8_t)(v);
    out[1] = (uint8_t)(v >> 8);
    out[2] = (uint8_t)(v >> 16);
    out[3] = (uint8_t)(v >> 24);
}

static uint32_t read_le32(const void* p)
{
    const uint8_t* b = (const uint8_t*)p;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static void fill_pattern(uint8_t* out, size_t size, uint64_t seed)
{
    size_t i;
    uint64_t r = 0;

    for(i = 0; i < size; i++)
    {
        if((i & 7) == 0)
        {
            r = splitmix64(&seed);
        }

        out[i] = (uint8_t)(r >> ((i & 7) * 8));
    }
}

static double now_seconds(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

/* Custom hash functions, mostly adversarial */

static uint32_t test_hash_fnv1a(const void* data, const size_t size, const uint32_t seed)
{
    const uint8_t* p = (const uint8_t*)data;
    uint32_t h = 2166136261u ^ seed;
    size_t i;

    for(i = 0; i < size; i++)
    {
        h ^= p[i];
        h *= 16777619u;
    }

    return h;
}

/* Every key collides */
static uint32_t hash_constant(const void* data, const size_t size, const uint32_t seed)
{
    (void)data; (void)size; (void)seed;
    return 0x5BD1E995u;
}

/* Every key collides, on a value some maps use as an "empty" marker */
static uint32_t hash_zero(const void* data, const size_t size, const uint32_t seed)
{
    (void)data; (void)size; (void)seed;
    return 0u;
}

/* Every key collides, on a value some maps use as a "tombstone" marker */
static uint32_t hash_all_ones(const void* data, const size_t size, const uint32_t seed)
{
    (void)data; (void)size; (void)seed;
    return 0xFFFFFFFFu;
}

/* Only 8 distinct hashes: long collision chains */
static uint32_t hash_3bits(const void* data, const size_t size, const uint32_t seed)
{
    return test_hash_fnv1a(data, size, seed) & 7u;
}

/* Only the high byte varies: breaks maps that index with the low bits */
static uint32_t hash_high_byte(const void* data, const size_t size, const uint32_t seed)
{
    return test_hash_fnv1a(data, size, seed) & 0xFF000000u;
}

/* Generic expectation helpers */

static HashMap* new_map(size_t capacity)
{
    HashMap* map = hashmap_new(capacity);
    CHECK(map != NULL, "hashmap_new(%zu) returned NULL", capacity);
    CHECK(hashmap_size(map) == 0, "new map has size %zu", hashmap_size(map));
    return map;
}

static void expect_value(HashMap* map,
                         const void* key,
                         uint32_t key_size,
                         const void* value,
                         uint32_t value_size,
                         const char* ctx)
{
    uint32_t got_size = 0xFFFFFFFFu;
    void* got = hashmap_get(map, key, key_size, &got_size);

    CHECK(got != NULL, "%s: key (size %u) not found", ctx, key_size);
    CHECK(got_size == value_size, "%s: value size is %u, expected %u", ctx, got_size, value_size);
    CHECK(value_size == 0 || memcmp(got, value, value_size) == 0, "%s: value content mismatch", ctx);
}

static void expect_absent(HashMap* map, const void* key, uint32_t key_size, const char* ctx)
{
    uint32_t got_size = 0;
    void* got = hashmap_get(map, key, key_size, &got_size);

    CHECK(got == NULL, "%s: key (size %u) unexpectedly present", ctx, key_size);
}

static void expect_size(HashMap* map, size_t expected, const char* ctx)
{
    size_t size = hashmap_size(map);
    size_t capacity = hashmap_capacity(map);

    CHECK(size == expected, "%s: hashmap_size() = %zu, expected %zu", ctx, size, expected);
    CHECK(capacity >= size, "%s: capacity %zu < size %zu", ctx, capacity, size);
}

/*
 * Guard against unbounded growth. A map that grows whenever a probe sequence
 * gets long can grow forever under heavy collisions; catch that here instead
 * of dying from an OOM a few inserts later.
 */
static void expect_sane_capacity(HashMap* map, const char* ctx)
{
    size_t size = hashmap_size(map);
    size_t capacity = hashmap_capacity(map);
    size_t bound = 65536 + size * 64;

    CHECK(capacity <= bound,
          "%s: capacity exploded to %zu for only %zu entries (bound %zu); "
          "growth is probably triggered by collisions it can never resolve",
          ctx, capacity, size, bound);
}

/* Counts entries via iteration, guarding against infinite iteration */
static size_t count_entries(HashMap* map)
{
    size_t limit = hashmap_size(map);
    size_t n = 0;
    HashMapIterator it = 0;
    void* key = NULL;
    uint32_t key_size = 0;
    void* value = NULL;
    uint32_t value_size = 0;

    while(hashmap_iterate(map, &it, &key, &key_size, &value, &value_size))
    {
        CHECK(n < limit, "iteration yields more entries than hashmap_size() (%zu)", limit);
        n++;
    }

    /* Iterator must stay exhausted */
    CHECK(!hashmap_iterate(map, &it, &key, &key_size, &value, &value_size),
          "hashmap_iterate returned true again after reporting the end");

    return n;
}

/* Reference model for the fuzzer */

/*
 * Keys are identified by an id in [0, num_keys). The key bytes are derived
 * deterministically from (id, salt): 4 bytes of little-endian id followed by
 * 0..296 bytes of filler (random, all-zero, ...). The id prefix makes every
 * key unique and lets us map an iterated key back to its model entry.
 */
static uint32_t make_key(uint32_t id, uint32_t salt, uint8_t* out)
{
    uint64_t st = ((uint64_t)id << 32) ^ (uint64_t)salt ^ 0xC2B2AE3D27D4EB4FULL;
    uint64_t r = splitmix64(&st);
    uint32_t len;

    switch(r & 15)
    {
        case 0:
            len = 4; /* just the id */
            break;
        case 1:
            len = 64 + (uint32_t)((r >> 8) % (MAX_KEY_SIZE - 64 + 1)); /* long */
            break;
        case 2:
            len = 4 + (uint32_t)((r >> 8) % 29);
            write_le32(out, id);
            memset(out + 4, 0, len - 4); /* lots of embedded NULs */
            return len;
        default:
            len = 4 + (uint32_t)((r >> 8) % 29); /* around the typical short-key/SSO threshold */
            break;
    }

    write_le32(out, id);
    fill_pattern(out + 4, len - 4, st);

    return len;
}

typedef struct
{
    uint8_t* value;
    uint32_t value_size;
    int present;
} ModelEntry;

typedef struct
{
    ModelEntry* entries;
    uint32_t num_keys;
    uint32_t key_salt;
    size_t count;
} Model;

static void model_init(Model* m, uint32_t num_keys, uint32_t salt)
{
    m->entries = (ModelEntry*)calloc(num_keys, sizeof(ModelEntry));
    CHECK(m->entries != NULL, "out of memory allocating model");
    m->num_keys = num_keys;
    m->key_salt = salt;
    m->count = 0;
}

static void model_clear(Model* m, uint32_t id)
{
    ModelEntry* e = &m->entries[id];

    if(e->present)
    {
        free(e->value);
        e->value = NULL;
        e->value_size = 0;
        e->present = 0;
        m->count--;
    }
}

static void model_set(Model* m, uint32_t id, const uint8_t* value, uint32_t value_size)
{
    ModelEntry* e = &m->entries[id];
    uint8_t* copy = (uint8_t*)malloc(value_size > 0 ? value_size : 1);

    CHECK(copy != NULL, "out of memory in model");

    if(value_size > 0)
    {
        memcpy(copy, value, value_size);
    }

    if(e->present)
    {
        free(e->value);
    }
    else
    {
        m->count++;
    }

    e->value = copy;
    e->value_size = value_size;
    e->present = 1;
}

static void model_release(Model* m)
{
    uint32_t i;

    for(i = 0; i < m->num_keys; i++)
    {
        free(m->entries[i].value);
    }

    free(m->entries);
    m->entries = NULL;
}

static uint32_t gen_value(Source* s, uint8_t* out)
{
    uint32_t r = source_range(s, 100);
    uint32_t size;

    if(r < 60)
    {
        size = 1 + source_range(s, 16); /* straddles inline/small-value thresholds */
    }
    else if(r < 95)
    {
        size = 17 + source_range(s, 240);
    }
    else
    {
        size = 257 + source_range(s, MAX_VALUE_SIZE - 256);
    }

#if HASHMAP_TEST_ZERO_SIZES
    if(source_range(s, 64) == 0)
    {
        size = 0;
    }
#endif

    fill_pattern(out, size, ((uint64_t)source_u32(s) << 32) | size);

    return size;
}

static void verify_key(HashMap* map, const Model* m, uint32_t id)
{
    uint8_t key[MAX_KEY_SIZE];
    uint32_t key_size = make_key(id, m->key_salt, key);
    const ModelEntry* e = &m->entries[id];
    uint32_t got_size = 0xFFFFFFFFu;
    void* got = hashmap_get(map, key, key_size, &got_size);

    if(e->present)
    {
        CHECK(got != NULL, "key id %u (key size %u) is missing from the map", id, key_size);
        CHECK(got_size == e->value_size,
              "key id %u: value size %u, expected %u", id, got_size, e->value_size);
        CHECK(e->value_size == 0 || memcmp(got, e->value, e->value_size) == 0,
              "key id %u: value content corrupted (size %u)", id, e->value_size);
    }
    else
    {
        CHECK(got == NULL, "key id %u (key size %u) should be absent", id, key_size);
    }
}

static void verify_iteration(HashMap* map, const Model* m)
{
    uint8_t expected[MAX_KEY_SIZE];
    uint8_t* seen = (uint8_t*)calloc(m->num_keys, 1);
    HashMapIterator it = 0;
    void* key = NULL;
    uint32_t key_size = 0;
    void* value = NULL;
    uint32_t value_size = 0;
    size_t visited = 0;

    CHECK(seen != NULL, "out of memory");

    while(hashmap_iterate(map, &it, &key, &key_size, &value, &value_size))
    {
        uint32_t id;
        uint32_t expected_size;
        uint32_t got_size = 0;
        void* got;
        const ModelEntry* e;

        CHECK(visited < m->count, "iteration yielded more than the %zu live entries", m->count);
        CHECK(key != NULL, "iteration yielded a NULL key");
        CHECK(key_size >= 4 && key_size <= MAX_KEY_SIZE, "iteration yielded a key of impossible size %u", key_size);

        id = read_le32(key);
        CHECK(id < m->num_keys, "iteration yielded an unknown key (id %u, size %u)", id, key_size);

        expected_size = make_key(id, m->key_salt, expected);
        CHECK(key_size == expected_size && memcmp(key, expected, key_size) == 0,
              "iterated key bytes for id %u are corrupted (size %u, expected %u)", id, key_size, expected_size);

        CHECK(!seen[id], "key id %u visited twice in one iteration", id);
        seen[id] = 1;

        e = &m->entries[id];
        CHECK(e->present, "iteration yielded removed key id %u", id);
        CHECK(value_size == e->value_size,
              "iterated value size for key id %u is %u, expected %u", id, value_size, e->value_size);
        CHECK(value_size == 0 || (value != NULL && memcmp(value, e->value, value_size) == 0),
              "iterated value for key id %u is corrupted", id);

        /* Looking up with the map's own key storage must work */
        got = hashmap_get(map, key, key_size, &got_size);
        CHECK(got != NULL && got_size == value_size && (value_size == 0 || memcmp(got, e->value, value_size) == 0),
              "hashmap_get with the iterated key pointer failed for id %u", id);

        visited++;
    }

    CHECK(visited == m->count, "iteration visited %zu entries, expected %zu", visited, m->count);
    CHECK(!hashmap_iterate(map, &it, &key, &key_size, &value, &value_size),
          "hashmap_iterate returned true again after reporting the end");

    free(seen);
}

static void verify_full(HashMap* map, const Model* m)
{
    uint32_t id;

    expect_size(map, m->count, "full verify");

    for(id = 0; id < m->num_keys; id++)
    {
        verify_key(map, m, id);
    }

    verify_iteration(map, m);
}

/*
 * For operations whose semantics are not documented (insert on an existing key,
 * update on a missing key) the map may legally end up in the "old" or the "new"
 * state. Accept either (unless pinned by a knob), require consistency across
 * the whole run, and resync the model with what the map actually did.
 */
static void resolve_ambiguous(HashMap* map,
                              Model* m,
                              uint32_t id,
                              const uint8_t* new_value,
                              uint32_t new_size,
                              int mode,
                              size_t* count_new,
                              size_t* count_old,
                              const char* what)
{
    uint8_t key[MAX_KEY_SIZE];
    uint32_t key_size = make_key(id, m->key_salt, key);
    const ModelEntry* e = &m->entries[id];
    uint32_t got_size = 0;
    void* got = hashmap_get(map, key, key_size, &got_size);
    int is_new;
    int is_old;

    is_new = got != NULL && got_size == new_size && (new_size == 0 || memcmp(got, new_value, new_size) == 0);

    if(e->present)
    {
        is_old = got != NULL && got_size == e->value_size &&
                 (got_size == 0 || memcmp(got, e->value, got_size) == 0);
    }
    else
    {
        is_old = got == NULL;
    }

    CHECK(is_new || is_old, "%s on key id %u left the key in neither the old nor the new state", what, id);
    CHECK(mode != 1 || is_new, "%s on key id %u did not store the new value", what, id);
    CHECK(mode != 0 || is_old, "%s on key id %u unexpectedly changed the map", what, id);

    if(is_new && !is_old)
    {
        (*count_new)++;
    }
    else if(is_old && !is_new)
    {
        (*count_old)++;
    }

    CHECK(*count_new == 0 || *count_old == 0,
          "%s behaves inconsistently: stored the new value %zu times, kept the old state %zu times",
          what, *count_new, *count_old);

    if(is_new)
    {
        model_set(m, id, new_value, new_size);
    }
}

/* Fuzz driver */

typedef struct
{
    const char* name;
    hashmap_hash_func hash_func;
    uint32_t num_keys;
    size_t ops;
} FuzzConfig;

static const FuzzConfig k_fuzz_configs[] = {
    { "default_hash/dense",  NULL,           16,    100000 },
    { "default_hash/medium", NULL,           1024,  200000 },
    { "default_hash/sparse", NULL,           16384, 200000 },
    { "fnv1a",               test_hash_fnv1a,     2048,  100000 },
    { "high_byte_only_hash", hash_high_byte, 2048,  50000  },
    { "3bit_hash",           hash_3bits,     512,   30000  },
    { "constant_hash",       hash_constant,  128,   20000  },
    { "zero_hash",           hash_zero,      64,    10000  },
    { "all_ones_hash",       hash_all_ones,  64,    10000  },
};

#define NUM_FUZZ_CONFIGS (sizeof(k_fuzz_configs) / sizeof(k_fuzz_configs[0]))

static void fuzz_run(Source* src,
                     const char* name,
                     hashmap_hash_func hash_func,
                     uint32_t num_keys,
                     size_t max_ops,
                     size_t initial_capacity,
                     int verbose)
{
    static uint8_t value[MAX_VALUE_SIZE];
    uint8_t key[MAX_KEY_SIZE];
    uint32_t* order;
    Model m;
    HashMap* map;
    size_t op;
    size_t verify_every = num_keys * 4 > 8192 ? num_keys * 4 : 8192;
    uint32_t insert_bias = 50;
    uint32_t i;
    double start = now_seconds();

    g_phase = name;

    model_init(&m, num_keys, source_u32(src));

    map = new_map(initial_capacity);

    if(hash_func != NULL)
    {
        hashmap_set_hash_func(map, hash_func);
    }

    for(op = 0; op < max_ops && !fuzz_source_exhausted(src); op++)
    {
        uint32_t roll;
        uint32_t id;
        uint32_t key_size;
        ModelEntry* e;

        /* Alternate between growth-heavy and shrink-heavy phases */
        if(op % 2048 == 0)
        {
            insert_bias = 10 + source_range(src, 81);
        }

        if(op > 0 && op % verify_every == 0)
        {
            verify_full(map, &m);
        }

        roll = source_range(src, 1000);
        id = source_range(src, num_keys);
        e = &m.entries[id];

        if(roll < 200)
        {
            verify_key(map, &m, id);
            continue;
        }

        if(roll < 202)
        {
            verify_iteration(map, &m);
            continue;
        }

        key_size = make_key(id, m.key_salt, key);

        if(source_range(src, 100) < insert_bias)
        {
            uint32_t value_size = gen_value(src, value);
            int use_update = (int)source_range(src, 2);

            if(!use_update)
            {
                hashmap_insert(map, key, key_size, value, value_size);

                if(!e->present)
                {
                    model_set(&m, id, value, value_size);
                }
                else
                {
                    resolve_ambiguous(map, &m, id, value, value_size,
                                      HASHMAP_TEST_INSERT_OVERWRITES,
                                      &g_dup_insert_overwrote, &g_dup_insert_kept,
                                      "hashmap_insert on an existing key");
                }
            }
            else
            {
                hashmap_update(map, key, key_size, value, value_size);

                if(e->present)
                {
                    model_set(&m, id, value, value_size);
                }
                else
                {
                    resolve_ambiguous(map, &m, id, value, value_size,
                                      HASHMAP_TEST_UPDATE_INSERTS_MISSING,
                                      &g_update_missing_inserted, &g_update_missing_ignored,
                                      "hashmap_update on a missing key");
                }
            }

            /* The map must have copied both buffers: scribble over them */
            memset(value, 0xCC, value_size);
        }
        else
        {
            hashmap_remove(map, key, key_size);
            model_clear(&m, id);
        }

        memset(key, 0xDD, key_size);

        verify_key(map, &m, id);
        expect_size(map, m.count, "after mutation");
        expect_sane_capacity(map, "after mutation");
    }

    verify_full(map, &m);

    if(verbose)
    {
        logger_log_debug("  %-22s %8zu ops  size %6zu  capacity %8zu  %.2fs",
               name, op, hashmap_size(map), hashmap_capacity(map), now_seconds() - start);
    }

    /* Teardown: remove everything in a random order */
    order = (uint32_t*)malloc(num_keys * sizeof(uint32_t));
    CHECK(order != NULL, "out of memory");

    for(i = 0; i < num_keys; i++)
    {
        order[i] = i;
    }

    for(i = num_keys - 1; i > 0; i--)
    {
        uint32_t j = source_range(src, i + 1);
        uint32_t tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }

    for(i = 0; i < num_keys; i++)
    {
        uint32_t id = order[i];
        uint32_t key_size = make_key(id, m.key_salt, key);

        hashmap_remove(map, key, key_size);
        model_clear(&m, id);

        if((i & 63) == 0)
        {
            expect_size(map, m.count, "teardown");
        }
    }

    verify_full(map, &m);
    CHECK(count_entries(map) == 0, "map not empty after removing every key");

    free(order);
    hashmap_free(map);
    model_release(&m);
}

/* Deterministic tests */

static void test_empty_maps(void)
{
    static const size_t capacities[] = { 0, 1, 2, 3, 7, 8, 16, 1000, 65536 };
    size_t i;

    g_phase = "empty_maps";

    for(i = 0; i < sizeof(capacities) / sizeof(capacities[0]); i++)
    {
        HashMap* map = new_map(capacities[i]);

        expect_size(map, 0, "fresh map");
        expect_absent(map, "nothing", 7, "get on empty map");
        hashmap_remove(map, "nothing", 7);
        expect_size(map, 0, "remove on empty map");
        CHECK(count_entries(map) == 0, "iteration over empty map yielded entries");

        hashmap_free(map);
    }
}

static void test_basic(void)
{
    HashMap* map;
    int value = 1234;
    HashMapIterator it = 0;
    void* key = NULL;
    uint32_t key_size = 0;
    void* got = NULL;
    uint32_t got_size = 0;

    g_phase = "basic";

    map = new_map(0);

    hashmap_insert(map, "hello", 5, &value, sizeof(value));
    expect_size(map, 1, "single insert");
    expect_value(map, "hello", 5, &value, sizeof(value), "single insert");

    expect_absent(map, "hell", 4, "prefix of key");
    expect_absent(map, "hello!", 6, "extension of key");
    expect_absent(map, "Hello", 5, "same length, different bytes");
    expect_absent(map, "hello\0", 6, "key + NUL");

    CHECK(hashmap_iterate(map, &it, &key, &key_size, &got, &got_size), "iteration found nothing");
    CHECK(key_size == 5 && memcmp(key, "hello", 5) == 0, "iterated key is wrong");
    CHECK(got_size == sizeof(value) && memcmp(got, &value, sizeof(value)) == 0, "iterated value is wrong");
    CHECK(!hashmap_iterate(map, &it, &key, &key_size, &got, &got_size), "iteration found a second entry");

    /* NULL value_size out-parameter should be tolerated by get */
    CHECK(hashmap_get(map, "hello", 5, NULL) != NULL, "hashmap_get with NULL value_size failed");

    hashmap_free(map);
}

static void test_ownership(void)
{
    HashMap* map;
    uint8_t key_buf[32];
    uint8_t value_buf[64];
    uint8_t expected[64];
    uint8_t* got;
    uint32_t got_size = 0;
    uint32_t small = 0xA5A5A5A5u;
    uint32_t small_expected = small;

    g_phase = "ownership";

    map = new_map(0);

    memcpy(key_buf, "ownership-key-longer-than-sso", 29);
    fill_pattern(value_buf, sizeof(value_buf), 42);
    memcpy(expected, value_buf, sizeof(value_buf));

    hashmap_insert(map, key_buf, 29, value_buf, sizeof(value_buf));
    hashmap_insert(map, "small", 5, &small, sizeof(small));

    got = (uint8_t*)hashmap_get(map, key_buf, 29, &got_size);
    CHECK(got != NULL, "inserted key not found");
    CHECK(got != value_buf, "the map stores the caller's value pointer instead of a copy");

    /* Clobber the caller's buffers: the map must be unaffected */
    memset(key_buf, 'X', sizeof(key_buf));
    memset(value_buf, 0, sizeof(value_buf));
    small = 0;

    expect_value(map, "ownership-key-longer-than-sso", 29, expected, sizeof(expected), "after clobbering caller buffers");
    expect_value(map, "small", 5, &small_expected, sizeof(small_expected), "after clobbering small value");
    expect_absent(map, key_buf, 29, "clobbered key");

    /* The returned value pointer is writable and writes persist */
    got = (uint8_t*)hashmap_get(map, "ownership-key-longer-than-sso", 29, &got_size);
    got[0] ^= 0xFF;
    expected[0] ^= 0xFF;
    expect_value(map, "ownership-key-longer-than-sso", 29, expected, sizeof(expected), "in-place write");

    hashmap_free(map);
}

static void test_binary_keys(void)
{
    static const struct
    {
        const char* bytes;
        uint32_t size;
    } keys[] = {
        { "a", 1 },           { "a\0", 2 },         { "a\0b", 3 },
        { "a\0c", 3 },        { "\0a", 2 },         { "\0", 1 },
        { "\0\0", 2 },        { "\0\0\0", 3 },      { "ab", 2 },
        { "ba", 2 },          { "abc", 3 },         { "abcd", 4 },
        { "abcdefg", 7 },     { "abcdefgh", 8 },    { "abcdefghi", 9 },
        { "abcdefghij", 10 }, { "abcdefghijk", 11 }, { "abcdefghijkl", 12 },
        { "abcdefghijklm", 13 }, { "abcdefghijklmnop", 16 },
        { "abcdefghijklmnoq", 16 }, { "\xff\xff\xff\xff", 4 },
        { "\x80", 1 },        { "\xff", 1 },
        { "0123456789abcdef0123456789abcdef", 32 },
        { "0123456789abcdef0123456789abcdeg", 32 },
    };
    const uint32_t count = (uint32_t)(sizeof(keys) / sizeof(keys[0]));
    HashMap* map;
    uint32_t i;
    uint32_t pass;

    g_phase = "binary_keys";

    for(pass = 0; pass < 2; pass++)
    {
        map = new_map(pass == 0 ? 0 : 1);

        for(i = 0; i < count; i++)
        {
            uint32_t v = i * 7919u;
            hashmap_insert(map, keys[i].bytes, keys[i].size, &v, sizeof(v));
            expect_size(map, i + 1, "binary key insert (keys must all be distinct)");
        }

        for(i = 0; i < count; i++)
        {
            uint32_t v = i * 7919u;
            expect_value(map, keys[i].bytes, keys[i].size, &v, sizeof(v), "binary key lookup");
        }

        CHECK(count_entries(map) == count, "iteration count mismatch with binary keys");

        /* Remove every other one, the rest must survive */
        for(i = 0; i < count; i += 2)
        {
            hashmap_remove(map, keys[i].bytes, keys[i].size);
        }

        for(i = 0; i < count; i++)
        {
            uint32_t v = i * 7919u;

            if(i % 2 == 0)
            {
                expect_absent(map, keys[i].bytes, keys[i].size, "removed binary key");
            }
            else
            {
                expect_value(map, keys[i].bytes, keys[i].size, &v, sizeof(v), "surviving binary key");
            }
        }

        expect_size(map, count / 2, "after removing half of the binary keys");

        hashmap_free(map);
    }
}

static void test_update(void)
{
    HashMap* map;
    uint8_t big[1000];
    uint8_t big2[1000];
    uint8_t medium[24];
    uint32_t small = 7;
    uint8_t tiny = 3;
    uint64_t eight = 0x0102030405060708ULL;
    uint32_t other = 99;

    g_phase = "update";

    map = new_map(0);

    fill_pattern(big, sizeof(big), 1);
    fill_pattern(big2, sizeof(big2), 2);
    fill_pattern(medium, sizeof(medium), 3);

    hashmap_insert(map, "other", 5, &other, sizeof(other));
    hashmap_insert(map, "k", 1, &small, sizeof(small));

    /* Walk through every size transition: small <-> 8 bytes <-> medium <-> big */
    hashmap_update(map, "k", 1, big, sizeof(big));
    expect_value(map, "k", 1, big, sizeof(big), "update small -> big");

    hashmap_update(map, "k", 1, big2, sizeof(big2));
    expect_value(map, "k", 1, big2, sizeof(big2), "update big -> big (same size)");

    hashmap_update(map, "k", 1, medium, sizeof(medium));
    expect_value(map, "k", 1, medium, sizeof(medium), "update big -> medium");

    hashmap_update(map, "k", 1, &eight, sizeof(eight));
    expect_value(map, "k", 1, &eight, sizeof(eight), "update medium -> 8 bytes");

    hashmap_update(map, "k", 1, &tiny, sizeof(tiny));
    expect_value(map, "k", 1, &tiny, sizeof(tiny), "update 8 bytes -> 1 byte");

    hashmap_update(map, "k", 1, &small, sizeof(small));
    expect_value(map, "k", 1, &small, sizeof(small), "update 1 byte -> 4 bytes");

    hashmap_update(map, "k", 1, big, sizeof(big));
    expect_value(map, "k", 1, big, sizeof(big), "update 4 bytes -> big");

    hashmap_update(map, "k", 1, &small, sizeof(small));
    expect_value(map, "k", 1, &small, sizeof(small), "update big -> small");

    expect_size(map, 2, "updates must not change the size");
    expect_value(map, "other", 5, &other, sizeof(other), "unrelated key after updates");
    CHECK(count_entries(map) == 2, "iteration count after updates");

    hashmap_free(map);
}

static void test_remove_reinsert(void)
{
    HashMap* map;
    uint32_t a = 1, b = 2, c = 3, b2 = 22;

    g_phase = "remove_reinsert";

    map = new_map(0);

    hashmap_insert(map, "a", 1, &a, sizeof(a));
    hashmap_insert(map, "b", 1, &b, sizeof(b));
    hashmap_insert(map, "c", 1, &c, sizeof(c));
    expect_size(map, 3, "three inserts");

    hashmap_remove(map, "b", 1);
    expect_absent(map, "b", 1, "removed key");
    expect_value(map, "a", 1, &a, sizeof(a), "neighbour after remove");
    expect_value(map, "c", 1, &c, sizeof(c), "neighbour after remove");
    expect_size(map, 2, "after remove");

    hashmap_remove(map, "b", 1);
    expect_size(map, 2, "double remove must be a no-op");

    hashmap_remove(map, "zzz", 3);
    expect_size(map, 2, "removing a never-inserted key must be a no-op");

    hashmap_insert(map, "b", 1, &b2, sizeof(b2));
    expect_value(map, "b", 1, &b2, sizeof(b2), "reinserted key gets the new value");
    expect_size(map, 3, "after reinsert");

    hashmap_remove(map, "a", 1);
    hashmap_remove(map, "b", 1);
    hashmap_remove(map, "c", 1);
    expect_size(map, 0, "after removing all");
    CHECK(count_entries(map) == 0, "iteration after removing all");

    hashmap_insert(map, "a", 1, &a, sizeof(a));
    expect_value(map, "a", 1, &a, sizeof(a), "insert into emptied map");
    expect_size(map, 1, "insert into emptied map");

    hashmap_free(map);
}

static void test_collisions(void)
{
    static const struct
    {
        const char* name;
        hashmap_hash_func func;
    } funcs[] = {
        { "collisions/constant", hash_constant },
        { "collisions/zero",     hash_zero },
        { "collisions/all_ones", hash_all_ones },
        { "collisions/3bits",    hash_3bits },
    };
    const uint32_t count = 48;
    char key[32];
    size_t f;
    uint32_t i;

    for(f = 0; f < sizeof(funcs) / sizeof(funcs[0]); f++)
    {
        HashMap* map;
        size_t expected;

        g_phase = funcs[f].name;

        map = new_map(4);
        hashmap_set_hash_func(map, funcs[f].func);

        for(i = 0; i < count; i++)
        {
            int len = snprintf(key, sizeof(key), "collide-%u", i);
            hashmap_insert(map, key, (uint32_t)len, &i, sizeof(i));
            expect_size(map, i + 1, "colliding insert");
            expect_sane_capacity(map, "colliding insert");
        }

        for(i = 0; i < count; i++)
        {
            int len = snprintf(key, sizeof(key), "collide-%u", i);
            expect_value(map, key, (uint32_t)len, &i, sizeof(i), "colliding lookup");
        }

        /* Remove head, middle, tail of the chain and every third key */
        expected = count;

        for(i = 0; i < count; i++)
        {
            if(i == 0 || i == count / 2 || i == count - 1 || i % 3 == 1)
            {
                int len = snprintf(key, sizeof(key), "collide-%u", i);
                hashmap_remove(map, key, (uint32_t)len);
                expected--;
            }
        }

        expect_size(map, expected, "after removing from collision chain");
        CHECK(count_entries(map) == expected, "iteration after removing from collision chain");

        for(i = 0; i < count; i++)
        {
            int len = snprintf(key, sizeof(key), "collide-%u", i);

            if(i == 0 || i == count / 2 || i == count - 1 || i % 3 == 1)
            {
                expect_absent(map, key, (uint32_t)len, "removed colliding key");
            }
            else
            {
                expect_value(map, key, (uint32_t)len, &i, sizeof(i), "surviving colliding key");
            }
        }

        /* Reinsert with new values */
        for(i = 0; i < count; i++)
        {
            if(i == 0 || i == count / 2 || i == count - 1 || i % 3 == 1)
            {
                int len = snprintf(key, sizeof(key), "collide-%u", i);
                uint32_t v = i + 1000;
                hashmap_insert(map, key, (uint32_t)len, &v, sizeof(v));
                expect_sane_capacity(map, "colliding reinsert");
            }
        }

        expect_size(map, count, "after reinserting into collision chain");

        for(i = 0; i < count; i++)
        {
            int len = snprintf(key, sizeof(key), "collide-%u", i);
            uint32_t v = (i == 0 || i == count / 2 || i == count - 1 || i % 3 == 1) ? i + 1000 : i;
            expect_value(map, key, (uint32_t)len, &v, sizeof(v), "colliding key after reinsert");
        }

        for(i = 0; i < count; i++)
        {
            int len = snprintf(key, sizeof(key), "collide-%u", i);
            hashmap_remove(map, key, (uint32_t)len);
        }

        expect_size(map, 0, "after removing all colliding keys");
        CHECK(count_entries(map) == 0, "iteration after removing all colliding keys");

        hashmap_free(map);
    }
}

static void test_growth(void)
{
    const uint32_t count = (uint32_t)(50000 * g_scale);
    HashMap* map;
    uint32_t i;
    uint32_t next_check = 1;

    g_phase = "growth";

    map = new_map(1);

    for(i = 0; i < count; i++)
    {
        uint64_t v = (uint64_t)i * 0x9E3779B97F4A7C15ULL;

        hashmap_insert(map, &i, sizeof(i), &v, sizeof(v));
        expect_size(map, (size_t)i + 1, "growth insert");

        /* Verify everything inserted so far at every power of two */
        if(i + 1 == next_check)
        {
            uint32_t j;

            for(j = 0; j <= i; j++)
            {
                uint64_t w = (uint64_t)j * 0x9E3779B97F4A7C15ULL;
                expect_value(map, &j, sizeof(j), &w, sizeof(w), "growth lookup after resize");
            }

            expect_sane_capacity(map, "growth");
            next_check *= 2;
        }
    }

    CHECK(count_entries(map) == count, "growth: iteration count mismatch");

    for(i = 0; i < count; i += 2)
    {
        hashmap_remove(map, &i, sizeof(i));
    }

    expect_size(map, count / 2, "growth: after removing evens");

    for(i = 0; i < count; i++)
    {
        uint64_t v = (uint64_t)i * 0x9E3779B97F4A7C15ULL;

        if(i % 2 == 0)
        {
            expect_absent(map, &i, sizeof(i), "growth: removed even key");
        }
        else
        {
            expect_value(map, &i, sizeof(i), &v, sizeof(v), "growth: surviving odd key");
        }
    }

    for(i = 0; i < count; i += 2)
    {
        uint64_t v = (uint64_t)i * 0x9E3779B97F4A7C15ULL;
        hashmap_insert(map, &i, sizeof(i), &v, sizeof(v));
    }

    expect_size(map, count, "growth: after reinserting evens");

    for(i = 0; i < count; i++)
    {
        hashmap_remove(map, &i, sizeof(i));
    }

    expect_size(map, 0, "growth: after removing all");
    CHECK(count_entries(map) == 0, "growth: iteration after removing all");

    hashmap_free(map);
}

static void test_churn(void)
{
    const uint32_t window = 8;
    const uint32_t total = (uint32_t)(200000 * g_scale);
    HashMap* map;
    uint32_t i;

    g_phase = "churn";

    map = new_map(8);

    for(i = 0; i < total; i++)
    {
        uint32_t v = ~i;

        hashmap_insert(map, &i, sizeof(i), &v, sizeof(v));

        if(i >= window)
        {
            uint32_t old = i - window;
            hashmap_remove(map, &old, sizeof(old));
        }

        CHECK(hashmap_size(map) == (i + 1 < window ? i + 1 : window),
              "churn: size %zu at step %u", hashmap_size(map), i);
    }

    for(i = total - window; i < total; i++)
    {
        uint32_t v = ~i;
        expect_value(map, &i, sizeof(i), &v, sizeof(v), "churn: live key");
    }

    for(i = 0; i < total - window; i += 997)
    {
        expect_absent(map, &i, sizeof(i), "churn: dead key");
    }

    CHECK(count_entries(map) == window, "churn: iteration count");
    CHECK(hashmap_capacity(map) <= (1u << 16),
          "churn: capacity grew to %zu with only %u live entries (removed slots never reclaimed?)",
          hashmap_capacity(map), window);

    hashmap_free(map);
}

static void test_large_entries(void)
{
    const uint32_t key_size = 64 * 1024;
    const uint32_t value_size = 1024 * 1024;
    uint8_t* key = (uint8_t*)malloc(key_size);
    uint8_t* key2 = (uint8_t*)malloc(key_size);
    uint8_t* value = (uint8_t*)malloc(value_size);
    uint8_t* expected = (uint8_t*)malloc(value_size);
    uint32_t small = 5;
    HashMap* map;

    g_phase = "large_entries";

    CHECK(key && key2 && value && expected, "out of memory");

    fill_pattern(key, key_size, 10);
    memcpy(key2, key, key_size);
    key2[key_size - 1] ^= 1; /* differs only in the very last byte */
    fill_pattern(value, value_size, 11);
    memcpy(expected, value, value_size);

    map = new_map(0);

    hashmap_insert(map, key, key_size, value, value_size);
    hashmap_insert(map, key2, key_size, &small, sizeof(small));
    memset(value, 0, value_size);

    expect_size(map, 2, "large entries");
    expect_value(map, key, key_size, expected, value_size, "1MB value under 64KB key");
    expect_value(map, key2, key_size, &small, sizeof(small), "key differing only in last byte");
    expect_absent(map, key, key_size - 1, "truncated large key");

    hashmap_remove(map, key, key_size);
    expect_absent(map, key, key_size, "removed large key");
    expect_value(map, key2, key_size, &small, sizeof(small), "sibling of removed large key");

    hashmap_free(map);
    free(key);
    free(key2);
    free(value);
    free(expected);
}

static void test_many_maps(void)
{
    enum { NUM_MAPS = 32 };
    const uint32_t count = 5000;
    HashMap* maps[NUM_MAPS];
    uint32_t i;
    uint32_t m;

    g_phase = "many_maps";

    for(m = 0; m < NUM_MAPS; m++)
    {
        maps[m] = new_map(m); /* also exercises many initial capacities */
    }

    for(i = 0; i < count; i++)
    {
        m = i % NUM_MAPS;
        hashmap_insert(maps[m], &i, sizeof(i), &m, sizeof(m));
    }

    for(m = 0; m < NUM_MAPS; m++)
    {
        uint32_t expected = count / NUM_MAPS + (m < count % NUM_MAPS ? 1 : 0);
        expect_size(maps[m], expected, "many maps: per-map size");
    }

    for(i = 0; i < count; i++)
    {
        uint32_t owner = i % NUM_MAPS;
        uint32_t other = (owner + 1) % NUM_MAPS;

        expect_value(maps[owner], &i, sizeof(i), &owner, sizeof(owner), "many maps: owner lookup");
        expect_absent(maps[other], &i, sizeof(i), "many maps: key leaked into another map");
    }

    for(m = 0; m < NUM_MAPS; m++)
    {
        hashmap_free(maps[m]);
    }
}

#if HASHMAP_TEST_ALIASING
/*
 * Passing pointers that the map itself returned back into insert/update.
 * This is a classic source of use-after-free: if the map resizes (or
 * reallocates the value) before copying, the source pointer dangles.
 * Run with AddressSanitizer to get a precise report.
 */
static void test_aliasing(void)
{
    HashMap* map;
    uint8_t expected[256];
    uint32_t sizes[] = { 4, 8, 9, 64, 256 };
    size_t s;

    for(s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
    {
        const uint32_t value_size = sizes[s];
        char key[32];
        char prev_key[32];
        uint32_t i;
        int len;
        int prev_len;
        void* got;
        uint32_t got_size;

        g_phase = "aliasing";

        map = new_map(1); /* small, so inserts keep triggering resizes */

        fill_pattern(expected, value_size, 77 + value_size);
        prev_len = snprintf(prev_key, sizeof(prev_key), "alias-0");
        hashmap_insert(map, prev_key, (uint32_t)prev_len, expected, value_size);

        /* insert(new_key, get(prev_key)) */
        for(i = 1; i < 2000; i++)
        {
            len = snprintf(key, sizeof(key), "alias-%u", i);
            got = hashmap_get(map, prev_key, (uint32_t)prev_len, &got_size);
            CHECK(got != NULL, "aliasing: previous key missing");

            hashmap_insert(map, key, (uint32_t)len, got, got_size);
            expect_value(map, key, (uint32_t)len, expected, value_size,
                         "insert with a value pointer owned by the map");

            memcpy(prev_key, key, sizeof(key));
            prev_len = len;
        }

        /* update(k, get(k)) : same pointer */
        got = hashmap_get(map, "alias-5", 7, &got_size);
        hashmap_update(map, "alias-5", 7, got, got_size);
        expect_value(map, "alias-5", 7, expected, value_size, "update with its own value pointer");

        /* update(k, get(k) + 1) : overlapping sub-slice */
        if(value_size > 1)
        {
            got = hashmap_get(map, "alias-6", 7, &got_size);
            hashmap_update(map, "alias-6", 7, (uint8_t*)got + 1, got_size - 1);
            expect_value(map, "alias-6", 7, expected + 1, value_size - 1,
                         "update with an overlapping slice of its own value");
        }

        hashmap_free(map);
    }
}
#endif /* HASHMAP_TEST_ALIASING */

#if HASHMAP_TEST_ZERO_SIZES
static void test_zero_sizes(void)
{
    HashMap* map;
    uint32_t v = 11;
    uint32_t got_size = 0xFFFFFFFFu;

    g_phase = "zero_sizes";

    map = new_map(0);

    hashmap_insert(map, "", 0, &v, sizeof(v));
    expect_size(map, 1, "empty key insert");
    expect_value(map, "", 0, &v, sizeof(v), "empty key lookup");
    expect_absent(map, "\0", 1, "empty key vs single NUL key");

    hashmap_insert(map, "zv", 2, &v, 0);
    expect_size(map, 2, "zero-size value insert");
    (void)hashmap_get(map, "zv", 2, &got_size);
    CHECK(got_size == 0, "zero-size value reports size %u", got_size);
    CHECK(count_entries(map) == 2, "zero sizes: iteration count");

    hashmap_remove(map, "", 0);
    hashmap_remove(map, "zv", 2);
    expect_size(map, 0, "zero sizes: after remove");

    hashmap_free(map);
}
#endif /* HASHMAP_TEST_ZERO_SIZES */

static void test_semantics_probe(void)
{
    HashMap* map;
    int a = 1;
    int b = 2;
    int stored = 0;
    void* got;
    uint32_t got_size = 0;

    g_phase = "semantics_probe";

    map = new_map(0);

    hashmap_insert(map, "dup", 3, &a, sizeof(a));
    hashmap_insert(map, "dup", 3, &b, sizeof(b));

    expect_size(map, 1, "inserting the same key twice must not create a duplicate");
    CHECK(count_entries(map) == 1, "duplicate key visible in iteration");

    got = hashmap_get(map, "dup", 3, &got_size);
    CHECK(got != NULL && got_size == sizeof(int), "duplicate-inserted key lost");
    memcpy(&stored, got, sizeof(stored));
    CHECK(stored == a || stored == b, "duplicate-inserted key has a garbage value %d", stored);

    logger_log_debug("insert on existing key: %s", stored == b ? "overwrites" : "keeps the old value");

    if(stored == b)
    {
        g_dup_insert_overwrote++;
    }
    else
    {
        g_dup_insert_kept++;
    }

    CHECK(HASHMAP_TEST_INSERT_OVERWRITES != 1 || stored == b, "insert did not overwrite");
    CHECK(HASHMAP_TEST_INSERT_OVERWRITES != 0 || stored == a, "insert overwrote");

    hashmap_update(map, "ghost", 5, &a, sizeof(a));
    got = hashmap_get(map, "ghost", 5, &got_size);

    logger_log_debug("update on missing key: %s", got != NULL ? "inserts" : "is a no-op");

    if(got != NULL)
    {
        g_update_missing_inserted++;
        expect_value(map, "ghost", 5, &a, sizeof(a), "update-inserted key");
        expect_size(map, 2, "update-inserted key counts towards size");
    }
    else
    {
        g_update_missing_ignored++;
        expect_size(map, 1, "no-op update must not change size");
    }

    CHECK(HASHMAP_TEST_UPDATE_INSERTS_MISSING != 1 || got != NULL, "update did not insert");
    CHECK(HASHMAP_TEST_UPDATE_INSERTS_MISSING != 0 || got == NULL, "update inserted");

    hashmap_free(map);
}

/* Same workload as the original benchmark, kept as a correctness stress test */
static void test_string_keys_stress(void)
{
    const size_t count = 0xFFFF * g_scale;
    HashMap* map;
    char key[64];
    size_t i;
    double t0, t1, t2, t3;

    g_phase = "string_keys_stress";

    map = new_map(count);

    t0 = now_seconds();

    for(i = 0; i < count; i++)
    {
        int num = (int)i;
        int len = snprintf(key, sizeof(key), "long_key%zu", i);
        hashmap_insert(map, key, (uint32_t)len, &num, sizeof(num));
    }

    t1 = now_seconds();
    expect_size(map, count, "string stress insert");

    for(i = 0; i < count; i++)
    {
        int num = (int)i;
        int len = snprintf(key, sizeof(key), "long_key%zu", i);
        expect_value(map, key, (uint32_t)len, &num, sizeof(num), "string stress get");
    }

    t2 = now_seconds();
    CHECK(count_entries(map) == count, "string stress iteration count");

    for(i = 0; i < count; i++)
    {
        int len = snprintf(key, sizeof(key), "long_key%zu", i);
        hashmap_remove(map, key, (uint32_t)len);
    }

    t3 = now_seconds();
    expect_size(map, 0, "string stress remove");

    logger_log_debug("%zu string keys: insert %.1f ns/op, get %.1f ns/op, remove %.1f ns/op",
           count,
           (t1 - t0) * 1e9 / (double)count,
           (t2 - t1) * 1e9 / (double)count,
           (t3 - t2) * 1e9 / (double)count);

    hashmap_free(map);
}

/* Distinct keys of various shapes, like the former per-type stress tests */

typedef enum KeyShape {
    KeyShape_U32,
    KeyShape_U64,
    KeyShape_U64Incremental,
    KeyShape_U64IdentityHash,
    KeyShape_String10,
    KeyShape_String64,
    KeyShape_String256,
    KeyShape_StringRandomLength,
    KeyShape_Count,
} KeyShape;

static const char* const g_key_shape_names[KeyShape_Count] = {
    "u32", "u64", "u64_incremental", "u64_identity_hash", "string_10", "string_64", "string_256", "string_random_length",
};

static uint32_t hash_identity(const void* key, const size_t key_size, const uint32_t seed)
{
    uint64_t v = 0;

    ROMANO_UNUSED(seed);

    memcpy(&v, key, key_size < sizeof(v) ? key_size : sizeof(v));

    return (uint32_t)(v ^ (v >> 32));
}

/* Bijective in index, so every key is distinct */
static uint32_t make_shaped_key(KeyShape shape, uint64_t index, uint64_t seed, uint8_t* out)
{
    uint64_t state = seed ^ index;
    uint32_t size;
    uint32_t i;

    switch(shape)
    {
        case KeyShape_U32:
        {
            uint32_t v = (uint32_t)index * 0x9E3779B1u;
            v ^= v >> 16;
            memcpy(out, &v, 4);
            return 4;
        }
        case KeyShape_U64:
        {
            uint64_t v = murmur_64(index + seed);
            memcpy(out, &v, 8);
            return 8;
        }
        case KeyShape_U64Incremental:
        case KeyShape_U64IdentityHash:
            memcpy(out, &index, 8);
            return 8;
        case KeyShape_String10:
            size = 10;
            break;
        case KeyShape_String64:
            size = 64;
            break;
        case KeyShape_String256:
            size = 256;
            break;
        default:
            size = 9 + (uint32_t)(splitmix64(&state) % 60);
            break;
    }

    for(i = 0; i < size; i++)
        out[i] = (uint8_t)(32 + splitmix64(&state) % 95);

    for(i = 0; i < 8; i++)
        out[i] = (uint8_t)"0123456789abcdef"[(index >> (4 * i)) & 0xF];

    out[8] = ':';

    return size;
}

static void test_key_types(void)
{
    const uint64_t count = test_scaled(0xFFFF);
    uint8_t key[256];
    int shape;

    for(shape = 0; shape < KeyShape_Count; shape++)
    {
        const uint64_t seed = 0x1234567ULL * (uint64_t)(shape + 1);
        HashMap* map;
        uint64_t i;

        g_phase = g_key_shape_names[shape];

        map = new_map(shape % 2 == 0 ? 57381 : 0);

        if(shape == KeyShape_U64IdentityHash)
            hashmap_set_hash_func(map, hash_identity);

        for(i = 0; i < count; i++)
        {
            uint32_t key_size = make_shaped_key((KeyShape)shape, i, seed, key);
            hashmap_insert(map, key, key_size, &i, sizeof(i));
        }

        expect_size(map, count, "insert");

        for(i = 0; i < count; i++)
        {
            uint32_t key_size = make_shaped_key((KeyShape)shape, i, seed, key);
            expect_value(map, key, key_size, &i, sizeof(i), "get");
        }

        CHECK(count_entries(map) == count, "iteration count");

        for(i = 0; i < count; i++)
        {
            uint32_t key_size = make_shaped_key((KeyShape)shape, i, seed, key);
            hashmap_remove(map, key, key_size);
        }

        expect_size(map, 0, "remove");
        hashmap_free(map);
    }
}

/* Entry points */

#if defined(HASHMAP_TEST_LIBFUZZER)

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    Source src;
    const FuzzConfig* config;
    uint32_t num_keys;
    size_t initial_capacity;

    if(size < 4)
        return 0;

    fuzz_source_init_data(&src, data, size);

    config = &k_fuzz_configs[source_range(&src, (uint32_t)NUM_FUZZ_CONFIGS)];
    num_keys = 1 + source_range(&src, config->num_keys < 512 ? config->num_keys : 512);
    initial_capacity = source_range(&src, 64);

    fuzz_run(&src, config->name, config->hash_func, num_keys, 4096, initial_capacity, 0);

    return 0;
}

#else

static size_t g_config_index = 0;

static bool property_differential(FuzzSource* src, void* user_data)
{
    static const size_t capacities[] = { 0, 1, 2, 7, 64, 1000 };
    const FuzzConfig* config = &k_fuzz_configs[g_config_index];

    ROMANO_UNUSED(user_data);

    fuzz_run(src,
             config->name,
             config->hash_func,
             config->num_keys,
             config->ops * g_scale / 4,
             capacities[source_range(src, sizeof(capacities) / sizeof(capacities[0]))],
             1);

    return true;
}

static void test_differential_fuzzing(void)
{
    for(g_config_index = 0; g_config_index < NUM_FUZZ_CONFIGS; g_config_index++)
        test_fuzz_property(k_fuzz_configs[g_config_index].name, 4, property_differential, NULL);
}

#define DETERMINISTIC_TESTS                 \
    TEST(test_semantics_probe),             \
    TEST(test_empty_maps),                  \
    TEST(test_basic),                       \
    TEST(test_ownership),                   \
    TEST(test_binary_keys),                 \
    TEST(test_update),                      \
    TEST(test_remove_reinsert),             \
    TEST(test_many_maps),                   \
    TEST(test_large_entries),               \
    TEST(test_collisions),                  \
    TEST(test_churn),                       \
    TEST(test_growth),                      \
    TEST(test_string_keys_stress),          \
    TEST(test_key_types),

#if HASHMAP_TEST_ZERO_SIZES
#define ZERO_SIZES_TESTS TEST(test_zero_sizes),
#else
#define ZERO_SIZES_TESTS
#endif /* HASHMAP_TEST_ZERO_SIZES */

#if HASHMAP_TEST_ALIASING
#define ALIASING_TESTS TEST(test_aliasing),
#else
#define ALIASING_TESTS
#endif /* HASHMAP_TEST_ALIASING */

TEST_MAIN(
    DETERMINISTIC_TESTS
    ZERO_SIZES_TESTS
    ALIASING_TESTS
    TEST(test_differential_fuzzing),
)

#endif /* defined(HASHMAP_TEST_LIBFUZZER) */
