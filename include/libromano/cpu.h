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

typedef enum {
    /* x86_64 Features */
    CPUFeature_SSE = 1,
    CPUFeature_MMX = 0,
    CPUFeature_SSE2 = 2,
    CPUFeature_SSE3 = 3,
    CPUFeature_SSSE3 = 4,
    CPUFeature_SSE4_1 = 5,
    CPUFeature_SSE4_2 = 6,
    CPUFeature_AVX = 7,
    CPUFeature_AVX2 = 8,
    CPUFeature_FMA3 = 9,
    CPUFeature_F16C = 10,
    CPUFeature_BMI1 = 11,
    CPUFeature_BMI2 = 12,
    CPUFeature_LZCNT = 13,
    CPUFeature_POPCNT = 14,
    CPUFeature_AES = 15,
    CPUFeature_PCLMULQDQ = 16,
    CPUFeature_SHA = 17,
    CPUFeature_RDRAND = 18,
    CPUFeature_RDSEED = 19,
    CPUFeature_ADX = 20,
    CPUFeature_AVX512F = 21,
    CPUFeature_AVX512DQ = 22,
    CPUFeature_AVX512IFMA = 23,
    CPUFeature_AVX512PF = 24,
    CPUFeature_AVX512ER = 25,
    CPUFeature_AVX512CD = 26,
    CPUFeature_AVX512BW = 27,
    CPUFeature_AVX512VL = 28,
    CPUFeature_AVX512VBMI = 29,
    CPUFeature_AMX_TILE = 30,
    CPUFeature_AMX_INT8 = 31,
    CPUFeature_AMX_BF16 = 32,

    /* AArch64 Features */
    CPUFeature_FP = 33,
    CPUFeature_AdvSIMD = 34,
    CPUFeature_AES_ARM = 35,
    CPUFeature_PMULL = 36,
    CPUFeature_SHA1 = 37,
    CPUFeature_SHA2 = 38,
    CPUFeature_SHA3 = 39,
    CPUFeature_CRC32 = 40,
    CPUFeature_LSE = 41,
    CPUFeature_RDM = 42,
    CPUFeature_DotProd = 43,
    CPUFeature_FP16 = 44,
    CPUFeature_FHM = 45,
    CPUFeature_FCMA = 46,
    CPUFeature_JSCVT = 47,
    CPUFeature_FRINTTS = 48,
    CPUFeature_I8MM = 49,
    CPUFeature_BF16 = 50,
    CPUFeature_SVE = 51,
    CPUFeature_SVE2 = 52,
    CPUFeature_SM3 = 53,
    CPUFeature_SM4 = 54,
    CPUFeature_SHA512 = 55,
    CPUFeature_DIT = 56,

    CPUFeature_COUNT = 57
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

/* Prins the detected cpu features */
ROMANO_API void cpu_print_features(void);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_CPU) */
