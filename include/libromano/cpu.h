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

/* Prins the detected cpu features */
ROMANO_API void cpu_print_features(void);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_CPU) */
