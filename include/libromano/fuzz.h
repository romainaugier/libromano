/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_FUZZ)
#define __LIBROMANO_FUZZ

#include "libromano/common.h"

ROMANO_CPP_ENTER

/*
 * Lightweight fuzzing and property-based testing.
 *
 * A FuzzSource yields random values either from a seeded PRNG or from a byte buffer (e.g. from
 * libFuzzer), so the same property function works with the built-in runner, with a single-seed
 * replay, and with an external coverage-guided fuzzer (see FUZZ_LIBFUZZER_PROPERTY).
 *
 * Environment overrides, applied by the runners:
 *   ROMANO_FUZZ_SEED=<n|random>  base seed
 *   ROMANO_FUZZ_ITERATIONS=<n>   iterations per run
 *   ROMANO_FUZZ_SCALE=<n>        multiplies iterations
 *   ROMANO_FUZZ_SECONDS=<n>      time budget per run
 *   ROMANO_FUZZ_REPLAY=<n>       runs a single iteration from its iteration seed (as logged on failure)
 */

#define FUZZ_DEFAULT_SEED 0x5EEDF00DCAFEBABEULL
#define FUZZ_DEFAULT_ITERATIONS 1000
#define FUZZ_DEFAULT_MAX_INPUT_SIZE 4096

#define FUZZ_ALPHABET_DIGITS "0123456789"
#define FUZZ_ALPHABET_HEX "0123456789abcdefABCDEF"
#define FUZZ_ALPHABET_ALPHA "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define FUZZ_ALPHABET_ALNUM FUZZ_ALPHABET_DIGITS FUZZ_ALPHABET_ALPHA
#define FUZZ_ALPHABET_WHITESPACE " \t\r\n\v\f"
#define FUZZ_ALPHABET_PRINTABLE " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~" FUZZ_ALPHABET_ALNUM

typedef struct FuzzSource {
    uint64_t state[4];
    const uint8_t* data;
    size_t size;
    size_t position;
    bool from_data;
    bool exhausted;
} FuzzSource;

ROMANO_API void fuzz_source_init_seed(FuzzSource* source, uint64_t seed);

ROMANO_API void fuzz_source_init_data(FuzzSource* source, const uint8_t* data, size_t size);

/* Only meaningful for data-backed sources: true once reads went past the end of the data */
ROMANO_API bool fuzz_source_exhausted(const FuzzSource* source);

ROMANO_API uint8_t fuzz_u8(FuzzSource* source);

ROMANO_API uint32_t fuzz_u32(FuzzSource* source);

ROMANO_API uint64_t fuzz_u64(FuzzSource* source);

ROMANO_API bool fuzz_bool(FuzzSource* source);

/* Uniform in [low, high], both inclusive */
ROMANO_API uint64_t fuzz_range(FuzzSource* source, uint64_t low, uint64_t high);

ROMANO_API int64_t fuzz_range_i64(FuzzSource* source, int64_t low, int64_t high);

/* Uniform in [0, count), returns 0 if count is 0 */
ROMANO_API size_t fuzz_index(FuzzSource* source, size_t count);

/* True with a probability of 1 / n */
ROMANO_API bool fuzz_one_in(FuzzSource* source, uint32_t n);

/* Uniform in [0, 1) */
ROMANO_API double fuzz_f64_unit(FuzzSource* source);

ROMANO_API void fuzz_bytes(FuzzSource* source, void* out, size_t size);

/* Log-distributed size in [0, max], biased toward small values and the boundaries */
ROMANO_API size_t fuzz_size(FuzzSource* source, size_t max);

/*
 * Values biased toward edge cases (0, 1, min, max, powers of two and their neighbours, ...)
 * mixed with uniform values
 */
ROMANO_API uint32_t fuzz_u32_special(FuzzSource* source);

ROMANO_API uint64_t fuzz_u64_special(FuzzSource* source);

ROMANO_API int32_t fuzz_i32_special(FuzzSource* source);

ROMANO_API int64_t fuzz_i64_special(FuzzSource* source);

/* Can return nan, infinities, -0.0, subnormals and values across the whole exponent range */
ROMANO_API double fuzz_f64_special(FuzzSource* source);

/* Finite values only */
ROMANO_API double fuzz_f64_finite(FuzzSource* source);

/*
 * Fills out with a random string of at most max_size characters drawn from alphabet, and adds
 * a null terminator (out must hold max_size + 1 chars). A NULL alphabet means any byte in [1, 255].
 * Returns the length of the string
 */
ROMANO_API size_t fuzz_string(FuzzSource* source, char* out, size_t max_size, const char* alphabet);

typedef struct FuzzDictionary {
    const char* const* tokens;
    size_t count;
} FuzzDictionary;

/*
 * Applies a random mutation (bit flip, byte change, insertion, deletion, duplication, dictionary
 * token...) to data in place. capacity is the size of the data buffer.
 * Returns the new size
 */
ROMANO_API size_t fuzz_mutate(FuzzSource* source,
                              uint8_t* data,
                              size_t size,
                              size_t capacity,
                              const FuzzDictionary* dictionary);

/* Returns false when the property does not hold */
typedef bool (*FuzzPropertyFunc)(FuzzSource* source, void* user_data);

/* Returns false when the input is mishandled */
typedef bool (*FuzzInputFunc)(const uint8_t* data, size_t size, void* user_data);

typedef struct FuzzCorpusEntry {
    const void* data;
    size_t size;
} FuzzCorpusEntry;

typedef struct FuzzOptions {
    const char* name;
    uint64_t seed;
    uint64_t iterations;
    double max_seconds;
    /* Input fuzzing only */
    size_t max_input_size;
    const FuzzCorpusEntry* corpus;
    size_t corpus_count;
    const FuzzDictionary* dictionary;
    bool minimize;
    /* Logs the replay information when the process crashes during a run */
    bool catch_crashes;
} FuzzOptions;

typedef struct FuzzReport {
    uint64_t seed;
    uint64_t iterations;
    double elapsed_seconds;
    bool failed;
    uint64_t failing_iteration;
    uint64_t failing_seed;
    /* Input fuzzing only, minimized when FuzzOptions.minimize is set. Owned by the report */
    uint8_t* failing_input;
    size_t failing_input_size;
} FuzzReport;

typedef struct FuzzRunInfo {
    const char* name;
    uint64_t iteration;
    uint64_t iteration_seed;
    bool minimizing;
} FuzzRunInfo;

ROMANO_API void fuzz_options_init(FuzzOptions* options, const char* name);

/* Returns true if the property held for every iteration. report can be NULL */
ROMANO_API bool fuzz_run_property(const FuzzOptions* options,
                                  FuzzPropertyFunc property,
                                  void* user_data,
                                  FuzzReport* report);

/* Returns true if the target accepted every input. report can be NULL */
ROMANO_API bool fuzz_run_input(const FuzzOptions* options,
                               FuzzInputFunc target,
                               void* user_data,
                               FuzzReport* report);

ROMANO_API void fuzz_report_release(FuzzReport* report);

/* Information about the iteration being run, or NULL outside of a run */
ROMANO_API const FuzzRunInfo* fuzz_current_run(void);

/* Restores the state after a run was left abruptly (longjmp out of a target) */
ROMANO_API void fuzz_reset(void);

/* Hex dump of an input to the logger, truncated to max_bytes */
ROMANO_API void fuzz_log_input(const uint8_t* data, size_t size, size_t max_bytes);

#define FUZZ_LIBFUZZER_PROPERTY(property)                                       \
    int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)                \
    {                                                                           \
        FuzzSource source;                                                      \
        fuzz_source_init_data(&source, data, size);                             \
        if(!property(&source, NULL))                                            \
            abort();                                                            \
        return 0;                                                               \
    }

#define FUZZ_LIBFUZZER_INPUT(target)                                            \
    int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)                \
    {                                                                           \
        if(!target(data, size, NULL))                                           \
            abort();                                                            \
        return 0;                                                               \
    }

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_FUZZ) */
