/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_CPU)
#define __LIBROMANO_CPU

#include "libromano/common.h"

#include <stdlib.h>

ROMANO_CPP_ENTER

ROMANO_API void cpu_check(void);

/* Features */

typedef enum {
    /* x86_64 Features */
    CPUFeature_MMX,
    CPUFeature_SSE,
    CPUFeature_SSE2,
    CPUFeature_SSE3,
    CPUFeature_SSSE3,
    CPUFeature_SSE4_1,
    CPUFeature_SSE4_2,
    CPUFeature_AVX,
    CPUFeature_AVX2,
    CPUFeature_FMA3,
    CPUFeature_F16C,
    CPUFeature_BMI1,
    CPUFeature_BMI2,
    CPUFeature_LZCNT,
    CPUFeature_POPCNT,
    CPUFeature_AES,
    CPUFeature_PCLMULQDQ,
    CPUFeature_SHA,
    CPUFeature_RDRAND,
    CPUFeature_RDSEED,
    CPUFeature_ADX,
    CPUFeature_AVX512F,
    CPUFeature_AVX512DQ,
    CPUFeature_AVX512IFMA,
    CPUFeature_AVX512PF,
    CPUFeature_AVX512ER,
    CPUFeature_AVX512CD,
    CPUFeature_AVX512BW,
    CPUFeature_AVX512VL,
    CPUFeature_AVX512VBMI,
    CPUFeature_AMX_TILE,
    CPUFeature_AMX_INT8,
    CPUFeature_AMX_BF16,

    /* AArch64 Features */
    CPUFeature_NEON,
    CPUFeature_FP,
    CPUFeature_AdvSIMD,
    CPUFeature_AES_ARM,
    CPUFeature_PMULL,
    CPUFeature_SHA1,
    CPUFeature_SHA2,
    CPUFeature_SHA3,
    CPUFeature_CRC32,
    CPUFeature_LSE,
    CPUFeature_RDM,
    CPUFeature_DotProd,
    CPUFeature_FP16,
    CPUFeature_FHM,
    CPUFeature_FCMA,
    CPUFeature_JSCVT,
    CPUFeature_FRINTTS,
    CPUFeature_I8MM,
    CPUFeature_BF16,
    CPUFeature_SVE,
    CPUFeature_SVE2,
    CPUFeature_SM3,
    CPUFeature_SM4,
    CPUFeature_SHA512,
    CPUFeature_DIT,

    CPUFeature_COUNT,
} CPUFeature;

ROMANO_API bool cpu_has_feature(CPUFeature feature);

#define ROMANO_CPU_NAME_SZ 49

/* The string must be allocated before, and must be 49 bytes */
ROMANO_API void cpu_get_name(char* name);

/* Returns the cpu frequency in MHz, found during library initialization (via cpuid or system calls) */
ROMANO_API uint32_t cpu_get_frequency(void);

/* Returns the current cpu frequency in MHz (via system calls) */
/* Value is cached and refreshed every 10k calls to avoid overhead */
ROMANO_API uint32_t cpu_get_current_frequency(void);

/* Returns the current timestamp counter () */
ROMANO_API uint64_t cpu_rdtsc(void);

/* Prints the detected cpu features and caches */
ROMANO_API void cpu_print_features(void);

/* Caches */

/*
 * Compile-time cache line size, for struct padding and alignment
 * (avoiding false sharing, like std::hardware_destructive_interference in C++)
 * Apple Silicon uses 128 bytes lines, the other supported targets 64 bytes.
 * Use cpu_get_cache_line_size() for the value detected at runtime.
 */
#if defined(ROMANO_AARCH64) && defined(ROMANO_APPLE)
#define ROMANO_CACHE_LINE_SIZE 128
#else
#define ROMANO_CACHE_LINE_SIZE 64
#endif /* defined(ROMANO_AARCH64) && defined(ROMANO_APPLE) */

typedef enum {
    CPUCacheLevel_L1 = 0,
    CPUCacheLevel_L2 = 1,
    CPUCacheLevel_L3 = 2,
    CPUCacheLevel_COUNT = 3,
} CPUCacheLevel;

typedef struct CPUCacheInfo {
    /* Size in bytes of one instance of the cache, 0 if absent or unknown */
    size_t size;

    /* Line size in bytes, 0 if unknown */
    uint32_t line_size;

    /*
     * Number of logical cpus sharing one instance of the cache, 0 if unknown.
     * When it comes from CPUID (no OS information), this is the maximum the topology allows and
     * can be above the real count.
     */
    uint32_t shared_by;
} CPUCacheInfo;

/*
 * Fills info with the given data/unified cache level, detected when the library is loaded.
 * Returns false (and zeroes info) if the level does not exist or could not be detected.
 *
 * Sources: sysfs on Linux, sysctl on macOS, GetLogicalProcessorInformation on Windows, completed
 * by CPUID on x86 (the only source on the BSDs). On other aarch64 systems only the line size is
 * known (CTR_EL0).
 *
 * On hybrid cpus the caches of the cores differ. The values are the ones of the performance
 * cores on Apple Silicon, and of logical cpu 0 elsewhere: a performance core on current Intel
 * hybrid cpus, often a little core on ARM big.LITTLE.
 */
ROMANO_API bool cpu_get_cache_info(CPUCacheLevel level, CPUCacheInfo* info);

/* Size in bytes of one instance of the given cache level, 0 if absent or unknown */
ROMANO_API size_t cpu_get_cache_size(CPUCacheLevel level);

/* L1 data cache line size in bytes, never 0 (falls back to ROMANO_CACHE_LINE_SIZE) */
ROMANO_API uint32_t cpu_get_cache_line_size(void);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_CPU) */
