/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif /* defined(__linux__) && !defined(_GNU_SOURCE) */

#include "libromano/cpu.h"

#include <stdio.h>
#include <string.h>

#if defined(ROMANO_WIN)
#include <windows.h>
#include <winreg.h>
#elif defined(ROMANO_APPLE)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <pthread.h>
#include <time.h>
#elif defined(ROMANO_LINUX)
#include <sys/auxv.h>
#include <time.h>
#elif defined(ROMANO_BSD)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <time.h>
#if defined(__FreeBSD__)
#include <sys/auxv.h>
#endif /* defined(__FreeBSD__) */
#if defined(__OpenBSD__) && defined(ROMANO_AARCH64)
#include <machine/cpu.h>
#endif /* defined(__OpenBSD__) && defined(ROMANO_AARCH64) */
#endif /* defined(ROMANO_WIN) */

#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
#if defined(ROMANO_MSVC)
#include <intrin.h> /* __cpuidex, _xgetbv, __rdtsc */
#else
#include <x86intrin.h> /* __rdtsc */
#endif /* defined(ROMANO_MSVC) */
#elif defined(ROMANO_AARCH64) && defined(ROMANO_MSVC)
#include <intrin.h> /* _ReadStatusReg */
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */

#if defined(ROMANO_AARCH64) && defined(ROMANO_LINUX)
#if !defined(AT_HWCAP2)
#define AT_HWCAP2 26
#endif /* !defined(AT_HWCAP2) */
#endif /* defined(ROMANO_AARCH64) && defined(ROMANO_LINUX) */

#if defined(__GNUC__) || defined(__clang__)
#define ROMANO_CPU_UNUSED __attribute__((unused))
#else
#define ROMANO_CPU_UNUSED
#endif /* defined(__GNUC__) || defined(__clang__) */

/* How many calls to cpu_get_current_frequency() share the same cached value */
#define ROMANO_CPU_FREQ_REFRESH_INTERVAL 10000

static bool g_cpu_features[CPUFeature_COUNT] = {0};
static bool g_cpu_freq_detected = false;
static uint32_t g_cpu_freq_mhz = 0;
static uint32_t g_cpu_cur_freq_mhz = 0;
static uint64_t g_cpu_cur_freq_ctr = 0;

bool cpu_has_feature(CPUFeature feature) 
{
    uint32_t f = (uint32_t)feature;
    return f < (uint32_t)CPUFeature_COUNT && g_cpu_features[f];
}

/* Time helpers */

ROMANO_CPU_UNUSED static uint64_t cpu_now_ns(void)
{
#if defined(ROMANO_WIN)
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);

    /* Split to avoid overflowing counter * 1e9 */
    const uint64_t f = (uint64_t)freq.QuadPart;
    const uint64_t c = (uint64_t)counter.QuadPart;

    return (c / f) * 1000000000ULL + ((c % f) * 1000000000ULL) / f;
#else
    struct timespec ts;
#if defined(CLOCK_MONOTONIC_RAW)
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif /* defined(CLOCK_MONOTONIC_RAW) */
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif /* defined(ROMANO_WIN) */
}

uint64_t cpu_rdtsc(void)
{
#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
    return (uint64_t)__rdtsc();
#elif defined(ROMANO_AARCH64)
    /* Note: this is the generic timer counter (CNTFRQ_EL0 Hz), not a cycle counter like the TSC */
#if defined(ROMANO_WIN) && defined(ROMANO_MSVC)
#if !defined(ARM64_CNTVCT)
#define ARM64_CNTVCT 0x5F02 /* ARM64_SYSREG(3,3,14,0,2) */
#endif /* !defined(ARM64_CNTVCT) */
    return (uint64_t)_ReadStatusReg(ARM64_CNTVCT);
#else
    /* MinGW/clang on Windows ARM64, Apple, Linux, BSD */
    uint64_t value = 0;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(value));
    return value;
#endif /* defined(ROMANO_WIN) && defined(ROMANO_MSVC) */
#else
    return cpu_now_ns();
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */
}

/* x86 intrinsics */

#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
static inline void cpuid(uint32_t regs[4], uint32_t leaf, uint32_t subleaf) 
{
#if defined(ROMANO_MSVC)
    int r[4] = {0};
    __cpuidex(r, (int)leaf, (int)subleaf);
    regs[0] = (uint32_t)r[0];
    regs[1] = (uint32_t)r[1];
    regs[2] = (uint32_t)r[2];
    regs[3] = (uint32_t)r[3];
#else
    __asm__ __volatile__(
        "cpuid"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(leaf), "c"(subleaf));
#endif /* defined(ROMANO_MSVC) */
}

/* Only legal when CPUID.1:ECX.OSXSAVE is set, #UD otherwise */
static inline uint64_t xgetbv(uint32_t index)
{
#if defined(ROMANO_MSVC)
    return (uint64_t)_xgetbv(index);
#else
    uint32_t eax = 0, edx = 0;
    __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return ((uint64_t)edx << 32) | eax;
#endif /* defined(ROMANO_MSVC) */
}

/* XCR0 state components the OS must save for each extension to be usable */
#define ROMANO_XCR0_SSE_AVX ((1ULL << 1) | (1ULL << 2))                /* XMM | YMM */
#define ROMANO_XCR0_AVX512 ((1ULL << 5) | (1ULL << 6) | (1ULL << 7))   /* opmask | ZMM_Hi256 | Hi16_ZMM */
#define ROMANO_XCR0_AMX ((1ULL << 17) | (1ULL << 18))                  /* XTILECFG | XTILEDATA */
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */

/* OS helpers */

#if defined(ROMANO_APPLE)
static bool cpu_sysctl_bool(const char* name)
{
    int32_t value = 0;
    size_t size = sizeof(value);
    return sysctlbyname(name, &value, &size, NULL, 0) == 0 && value != 0;
}
#endif /* defined(ROMANO_APPLE) */

#if defined(ROMANO_LINUX)
/* Reads the first unsigned integer of a sysfs-like file, returns 0 on failure */
static unsigned long cpu_read_ulong_file(const char* path)
{
    FILE* f = fopen(path, "r");

    if(f == NULL)
        return 0;

    unsigned long value = 0;

    if(fscanf(f, "%lu", &value) != 1)
        value = 0;

    fclose(f);

    return value;
}

/* Reads "cpu MHz" from /proc/cpuinfo (x86 only, current frequency of cpu0), returns 0 on failure */
static uint32_t cpu_read_proc_cpuinfo_mhz(void)
{
    FILE* f = fopen("/proc/cpuinfo", "r");

    if(f == NULL)
        return 0;

    char line[256];
    uint32_t result = 0;

    while(fgets(line, sizeof(line), f)) 
    {
        double mhz = 0.0;

        if(sscanf(line, "cpu MHz : %lf", &mhz) == 1 && mhz > 0.0) 
        {
            result = (uint32_t)(mhz + 0.5);
            break;
        }
    }

    fclose(f);

    return result;
}
#endif /* defined(ROMANO_LINUX) */

/* Feature detection */

#if defined(ROMANO_AARCH64) && (defined(ROMANO_LINUX) || defined(__FreeBSD__))
/* FreeBSD uses the same HWCAP bit layout as Linux */
static void cpu_decode_hwcaps(unsigned long hwcap, unsigned long hwcap2)
{
    g_cpu_features[CPUFeature_FP] = (hwcap  & (1UL <<  0)) != 0;
    g_cpu_features[CPUFeature_AdvSIMD] = (hwcap  & (1UL <<  1)) != 0;
    g_cpu_features[CPUFeature_AES_ARM] = (hwcap  & (1UL <<  3)) != 0;
    g_cpu_features[CPUFeature_PMULL] = (hwcap  & (1UL <<  4)) != 0;
    g_cpu_features[CPUFeature_SHA1] = (hwcap  & (1UL <<  5)) != 0;
    g_cpu_features[CPUFeature_SHA2] = (hwcap  & (1UL <<  6)) != 0;
    g_cpu_features[CPUFeature_CRC32] = (hwcap  & (1UL <<  7)) != 0;
    g_cpu_features[CPUFeature_LSE] = (hwcap  & (1UL <<  8)) != 0;
    /* FPHP (scalar) and ASIMDHP (vector) */
    g_cpu_features[CPUFeature_FP16] = (hwcap  & (1UL <<  9)) != 0 && (hwcap & (1UL << 10)) != 0;
    g_cpu_features[CPUFeature_RDM] = (hwcap  & (1UL << 12)) != 0;
    g_cpu_features[CPUFeature_JSCVT] = (hwcap  & (1UL << 13)) != 0;
    g_cpu_features[CPUFeature_FCMA] = (hwcap  & (1UL << 14)) != 0;
    g_cpu_features[CPUFeature_SHA3] = (hwcap  & (1UL << 17)) != 0;
    g_cpu_features[CPUFeature_SM3] = (hwcap  & (1UL << 18)) != 0;
    g_cpu_features[CPUFeature_SM4] = (hwcap  & (1UL << 19)) != 0;
    g_cpu_features[CPUFeature_DotProd] = (hwcap  & (1UL << 20)) != 0;
    g_cpu_features[CPUFeature_SHA512] = (hwcap  & (1UL << 21)) != 0;
    g_cpu_features[CPUFeature_SVE] = (hwcap  & (1UL << 22)) != 0;
    g_cpu_features[CPUFeature_FHM] = (hwcap  & (1UL << 23)) != 0;
    g_cpu_features[CPUFeature_DIT] = (hwcap  & (1UL << 24)) != 0;

    g_cpu_features[CPUFeature_SVE2] = (hwcap2 & (1UL <<  1)) != 0;
    g_cpu_features[CPUFeature_FRINTTS] = (hwcap2 & (1UL <<  8)) != 0;
    g_cpu_features[CPUFeature_I8MM] = (hwcap2 & (1UL << 13)) != 0;
    g_cpu_features[CPUFeature_BF16] = (hwcap2 & (1UL << 14)) != 0;
}
#endif /* defined(ROMANO_AARCH64) && (defined(ROMANO_LINUX) || defined(__FreeBSD__)) */

static void cpu_detect_features(void)
{
#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
    uint32_t r[4] = {0};
    cpuid(r, 0, 0);
    const uint32_t max_leaf = r[0];

    /*
     * CPUID only reports what the silicon supports. For AVX/AVX-512/AMX the OS
     * also has to save the extended register state on context switches, which
     * is advertised through XCR0. Without it, using those registers either
     * faults or silently gets the upper bits clobbered.
     */
    bool os_avx = false;
    bool os_avx512 = false;
    bool os_amx = false;

    if(max_leaf >= 1) 
    {
        cpuid(r, 1, 0);
        const uint32_t ecx = r[2];
        const uint32_t edx = r[3];

        const bool osxsave = (ecx >> 27) & 1;

        if(osxsave)
        {
            const uint64_t xcr0 = xgetbv(0);

            os_avx = (xcr0 & ROMANO_XCR0_SSE_AVX) == ROMANO_XCR0_SSE_AVX;
            os_avx512 = os_avx && (xcr0 & ROMANO_XCR0_AVX512) == ROMANO_XCR0_AVX512;
            os_amx = (xcr0 & ROMANO_XCR0_AMX) == ROMANO_XCR0_AMX;

#if defined(ROMANO_APPLE)
            /* macOS enables the AVX-512 state lazily on first use, so XCR0 does not show it yet */
            if(os_avx && !os_avx512)
                os_avx512 = cpu_sysctl_bool("hw.optional.avx512f");
#endif /* defined(ROMANO_APPLE) */
        }

        g_cpu_features[CPUFeature_MMX] = (edx >> 23) & 1;
        g_cpu_features[CPUFeature_SSE] = (edx >> 25) & 1;
        g_cpu_features[CPUFeature_SSE2] = (edx >> 26) & 1;
        g_cpu_features[CPUFeature_SSE3] = (ecx >>  0) & 1;
        g_cpu_features[CPUFeature_PCLMULQDQ] = (ecx >>  1) & 1;
        g_cpu_features[CPUFeature_SSSE3] = (ecx >>  9) & 1;
        g_cpu_features[CPUFeature_SSE4_1] = (ecx >> 19) & 1;
        g_cpu_features[CPUFeature_SSE4_2] = (ecx >> 20) & 1;
        g_cpu_features[CPUFeature_POPCNT] = (ecx >> 23) & 1;
        g_cpu_features[CPUFeature_AES] = (ecx >> 25) & 1;
        g_cpu_features[CPUFeature_RDRAND] = (ecx >> 30) & 1;

        /* VEX encoded, need the YMM state */
        g_cpu_features[CPUFeature_FMA3] = ((ecx >> 12) & 1) && os_avx;
        g_cpu_features[CPUFeature_AVX] = ((ecx >> 28) & 1) && os_avx;
        g_cpu_features[CPUFeature_F16C] = ((ecx >> 29) & 1) && os_avx;
    }

    if(max_leaf >= 7)
    {
        cpuid(r, 7, 0);
        const uint32_t ebx = r[1];
        const uint32_t ecx = r[2];
        const uint32_t edx = r[3];

        g_cpu_features[CPUFeature_BMI1] = (ebx >>  3) & 1;
        g_cpu_features[CPUFeature_BMI2] = (ebx >>  8) & 1;
        g_cpu_features[CPUFeature_RDSEED] = (ebx >> 18) & 1;
        g_cpu_features[CPUFeature_ADX] = (ebx >> 19) & 1;
        g_cpu_features[CPUFeature_SHA] = (ebx >> 29) & 1;

        g_cpu_features[CPUFeature_AVX2] = ((ebx >>  5) & 1) && os_avx;

        g_cpu_features[CPUFeature_AVX512F] = ((ebx >> 16) & 1) && os_avx512;
        g_cpu_features[CPUFeature_AVX512DQ] = ((ebx >> 17) & 1) && os_avx512;
        g_cpu_features[CPUFeature_AVX512IFMA] = ((ebx >> 21) & 1) && os_avx512;
        g_cpu_features[CPUFeature_AVX512PF] = ((ebx >> 26) & 1) && os_avx512;
        g_cpu_features[CPUFeature_AVX512ER] = ((ebx >> 27) & 1) && os_avx512;
        g_cpu_features[CPUFeature_AVX512CD] = ((ebx >> 28) & 1) && os_avx512;
        g_cpu_features[CPUFeature_AVX512BW] = ((ebx >> 30) & 1) && os_avx512;
        g_cpu_features[CPUFeature_AVX512VL] = ((ebx >> 31) & 1) && os_avx512;
        g_cpu_features[CPUFeature_AVX512VBMI] = ((ecx >>  1) & 1) && os_avx512;

        /*
         * On Linux, AMX tile data additionally needs a per-process permission
         * (arch_prctl(ARCH_REQ_XCOMP_PERM, XFEATURE_XTILEDATA)) before first use,
         * this is left to the caller since it grows the signal frame of the whole process
         */
        g_cpu_features[CPUFeature_AMX_BF16] = ((edx >> 22) & 1) && os_amx;
        g_cpu_features[CPUFeature_AMX_TILE] = ((edx >> 24) & 1) && os_amx;
        g_cpu_features[CPUFeature_AMX_INT8] = ((edx >> 25) & 1) && os_amx;
    }

    cpuid(r, 0x80000000, 0);

    if(r[0] >= 0x80000001U)
    {
        cpuid(r, 0x80000001, 0);
        g_cpu_features[CPUFeature_LZCNT] = (r[2] >> 5) & 1;
    }
#elif defined(ROMANO_AARCH64)
#if defined(ROMANO_LINUX)
    unsigned long hwcap = getauxval(AT_HWCAP);
    cpu_decode_hwcaps(hwcap, getauxval(AT_HWCAP2));

    g_cpu_features[CPUFeature_NEON] = (hwcap & HWCAP_ASIMD) != 0;
#elif defined(ROMANO_APPLE)
    g_cpu_features[CPUFeature_NEON] = cpu_sysctl_bool("hw.optional.neon");
    g_cpu_features[CPUFeature_FP] = cpu_sysctl_bool("hw.optional.floatingpoint");
    g_cpu_features[CPUFeature_AdvSIMD] = cpu_sysctl_bool("hw.optional.AdvSIMD");
    g_cpu_features[CPUFeature_AES_ARM] = cpu_sysctl_bool("hw.optional.arm.FEAT_AES");
    g_cpu_features[CPUFeature_PMULL] = cpu_sysctl_bool("hw.optional.arm.FEAT_PMULL");
    g_cpu_features[CPUFeature_SHA1] = cpu_sysctl_bool("hw.optional.arm.FEAT_SHA1");
    g_cpu_features[CPUFeature_SHA2] = cpu_sysctl_bool("hw.optional.arm.FEAT_SHA256");
    g_cpu_features[CPUFeature_CRC32] = cpu_sysctl_bool("hw.optional.armv8_crc32");
    g_cpu_features[CPUFeature_RDM] = cpu_sysctl_bool("hw.optional.arm.FEAT_RDM");
    g_cpu_features[CPUFeature_DotProd] = cpu_sysctl_bool("hw.optional.arm.FEAT_DotProd");
    g_cpu_features[CPUFeature_FP16] = cpu_sysctl_bool("hw.optional.arm.FEAT_FP16");
    g_cpu_features[CPUFeature_FCMA] = cpu_sysctl_bool("hw.optional.arm.FEAT_FCMA");
    g_cpu_features[CPUFeature_JSCVT] = cpu_sysctl_bool("hw.optional.arm.FEAT_JSCVT");
    g_cpu_features[CPUFeature_FRINTTS] = cpu_sysctl_bool("hw.optional.arm.FEAT_FRINTTS");
    g_cpu_features[CPUFeature_I8MM] = cpu_sysctl_bool("hw.optional.arm.FEAT_I8MM");
    g_cpu_features[CPUFeature_BF16] = cpu_sysctl_bool("hw.optional.arm.FEAT_BF16");
    g_cpu_features[CPUFeature_SVE] = cpu_sysctl_bool("hw.optional.arm.FEAT_SVE");
    g_cpu_features[CPUFeature_SVE2] = cpu_sysctl_bool("hw.optional.arm.FEAT_SVE2");
    g_cpu_features[CPUFeature_SM3] = cpu_sysctl_bool("hw.optional.arm.FEAT_SM3");
    g_cpu_features[CPUFeature_SM4] = cpu_sysctl_bool("hw.optional.arm.FEAT_SM4");
    g_cpu_features[CPUFeature_DIT] = cpu_sysctl_bool("hw.optional.arm.FEAT_DIT");

    /* The FEAT_ names appeared in macOS 12, fall back to the older ones on macOS 11 */
    g_cpu_features[CPUFeature_LSE] = cpu_sysctl_bool("hw.optional.arm.FEAT_LSE") ||
                                     cpu_sysctl_bool("hw.optional.armv8_1_atomics");
    g_cpu_features[CPUFeature_SHA3] = cpu_sysctl_bool("hw.optional.arm.FEAT_SHA3") ||
                                      cpu_sysctl_bool("hw.optional.armv8_2_sha3");
    g_cpu_features[CPUFeature_SHA512] = cpu_sysctl_bool("hw.optional.arm.FEAT_SHA512") ||
                                        cpu_sysctl_bool("hw.optional.armv8_2_sha512");
    g_cpu_features[CPUFeature_FHM] = cpu_sysctl_bool("hw.optional.arm.FEAT_FHM") ||
                                     cpu_sysctl_bool("hw.optional.armv8_2_fhm");
#elif defined(ROMANO_WIN)
    /* Mandatory on Windows on ARM */

    g_cpu_features[CPUFeature_NEON] = IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE);
    g_cpu_features[CPUFeature_FP] = true;
    g_cpu_features[CPUFeature_AdvSIMD] = true;

    g_cpu_features[CPUFeature_AES_ARM] = IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
    g_cpu_features[CPUFeature_PMULL] = g_cpu_features[CPUFeature_AES_ARM];
    g_cpu_features[CPUFeature_SHA1] = g_cpu_features[CPUFeature_AES_ARM];
    g_cpu_features[CPUFeature_SHA2] = g_cpu_features[CPUFeature_AES_ARM];
    g_cpu_features[CPUFeature_CRC32] = IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE);
    g_cpu_features[CPUFeature_LSE] = IsProcessorFeaturePresent(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE);

    /* Newer SDKs only */
#if defined(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)
    g_cpu_features[CPUFeature_DotProd] = IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE);
#endif /* defined(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE) */
#if defined(PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE)
    g_cpu_features[CPUFeature_JSCVT] = IsProcessorFeaturePresent(PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE);
#endif /* defined(PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE) */
#if defined(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE)
    g_cpu_features[CPUFeature_SVE] = IsProcessorFeaturePresent(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE);
#endif /* defined(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE) */
#if defined(PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE)
    g_cpu_features[CPUFeature_SVE2] = IsProcessorFeaturePresent(PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE);
#endif /* defined(PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE) */
#if defined(PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE)
    g_cpu_features[CPUFeature_SHA3] = IsProcessorFeaturePresent(PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE);
#endif /* defined(PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE) */
#if defined(PF_ARM_SHA512_INSTRUCTIONS_AVAILABLE)
    g_cpu_features[CPUFeature_SHA512] = IsProcessorFeaturePresent(PF_ARM_SHA512_INSTRUCTIONS_AVAILABLE);
#endif /* defined(PF_ARM_SHA512_INSTRUCTIONS_AVAILABLE) */
#if defined(PF_ARM_V82_I8MM_INSTRUCTIONS_AVAILABLE)
    g_cpu_features[CPUFeature_I8MM] = IsProcessorFeaturePresent(PF_ARM_V82_I8MM_INSTRUCTIONS_AVAILABLE);
#endif /* defined(PF_ARM_V82_I8MM_INSTRUCTIONS_AVAILABLE) */
#if defined(PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE)
    g_cpu_features[CPUFeature_FP16] = IsProcessorFeaturePresent(PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE);
#endif /* defined(PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE) */
#if defined(PF_ARM_V86_BF16_INSTRUCTIONS_AVAILABLE)
    g_cpu_features[CPUFeature_BF16] = IsProcessorFeaturePresent(PF_ARM_V86_BF16_INSTRUCTIONS_AVAILABLE);
#endif /* defined(PF_ARM_V86_BF16_INSTRUCTIONS_AVAILABLE) */
#elif defined(ROMANO_BSD)
#if defined(__FreeBSD__)
    unsigned long hwcap = 0;
    unsigned long hwcap2 = 0;

    elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
    elf_aux_info(AT_HWCAP2, &hwcap2, sizeof(hwcap2));

    cpu_decode_hwcaps(hwcap, hwcap2);

    g_cpu_features[CPUFeature_NEON] = (hwcap & HWCAP_ASIMD) != 0;
#else
    /* Required by the ABI on every BSD */
    g_cpu_features[CPUFeature_FP] = true;
    g_cpu_features[CPUFeature_AdvSIMD] = true;

#if defined(__OpenBSD__) && defined(CPU_ID_AA64ISAR0)
    /* OpenBSD exposes the raw ID_AA64ISAR0_EL1 register */
    int mib[2] = { CTL_MACHDEP, CPU_ID_AA64ISAR0 };
    uint64_t isar0 = 0;
    size_t size = sizeof(isar0);

    if(sysctl(mib, 2, &isar0, &size, NULL, 0) == 0)
    {
        g_cpu_features[CPUFeature_AES_ARM] = ((isar0 >>  4) & 0xF) >= 1;
        g_cpu_features[CPUFeature_PMULL] = ((isar0 >>  4) & 0xF) >= 2;
        g_cpu_features[CPUFeature_SHA1] = ((isar0 >>  8) & 0xF) >= 1;
        g_cpu_features[CPUFeature_SHA2] = ((isar0 >> 12) & 0xF) >= 1;
        g_cpu_features[CPUFeature_SHA512] = ((isar0 >> 12) & 0xF) >= 2;
        g_cpu_features[CPUFeature_CRC32] = ((isar0 >> 16) & 0xF) >= 1;
        g_cpu_features[CPUFeature_LSE] = ((isar0 >> 20) & 0xF) >= 2;
        g_cpu_features[CPUFeature_RDM] = ((isar0 >> 28) & 0xF) >= 1;
        g_cpu_features[CPUFeature_SHA3] = ((isar0 >> 32) & 0xF) >= 1;
        g_cpu_features[CPUFeature_SM3] = ((isar0 >> 36) & 0xF) >= 1;
        g_cpu_features[CPUFeature_SM4] = ((isar0 >> 40) & 0xF) >= 1;
        g_cpu_features[CPUFeature_DotProd] = ((isar0 >> 44) & 0xF) >= 1;
        g_cpu_features[CPUFeature_FHM] = ((isar0 >> 48) & 0xF) >= 1;
    }
#endif /* defined(__OpenBSD__) && defined(CPU_ID_AA64ISAR0) */
#endif /* defined(__FreeBSD__) */
#endif /* defined(ROMANO_LINUX) */
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */
}

/* CPU Name */

/* Copies src into name (ROMANO_CPU_NAME_SZ), trimming the padding spaces some vendors put around it */
static void cpu_copy_name(char* name, const char* src)
{
    while(*src == ' ' || *src == '\t')
        src++;

    size_t len = strlen(src);

    while(len > 0 && (src[len - 1] == ' ' || src[len - 1] == '\n' || src[len - 1] == '\t'))
        len--;

    if(len > (size_t)ROMANO_CPU_NAME_SZ - 1)
        len = (size_t)ROMANO_CPU_NAME_SZ - 1;

    memcpy(name, src, len);
    name[len] = '\0';
}

#if defined(ROMANO_AARCH64) && defined(ROMANO_LINUX)
/* arm64 /proc/cpuinfo has no model name, only the MIDR fields (of cpu0, usually a little core on big.LITTLE) */
static void cpu_get_name_linux_aarch64(char* name)
{
    FILE* f = fopen("/proc/cpuinfo", "r");

    if(f == NULL)
        return;

    char line[256];
    unsigned int implementer = 0;
    unsigned int part = 0;
    bool has_implementer = false;
    bool has_part = false;

    while(fgets(line, sizeof(line), f) && !(has_implementer && has_part))
    {
        if(!has_implementer && sscanf(line, "CPU implementer : %x", &implementer) == 1)
            has_implementer = true;
        else if(!has_part && sscanf(line, "CPU part : %x", &part) == 1)
            has_part = true;
    }

    fclose(f);

    if(!has_implementer)
        return;

    const char* vendor = NULL;

    switch(implementer)
    {
        case 0x41: vendor = "ARM"; break;
        case 0x42: vendor = "Broadcom"; break;
        case 0x43: vendor = "Cavium"; break;
        case 0x46: vendor = "Fujitsu"; break;
        case 0x48: vendor = "HiSilicon"; break;
        case 0x4E: vendor = "NVIDIA"; break;
        case 0x50: vendor = "APM"; break;
        case 0x51: vendor = "Qualcomm"; break;
        case 0x53: vendor = "Samsung"; break;
        case 0x61: vendor = "Apple"; break;
        case 0x6D: vendor = "Microsoft"; break;
        case 0xC0: vendor = "Ampere"; break;
        default: break;
    }

    char buffer[64];

    if(vendor != NULL)
        snprintf(buffer, sizeof(buffer), "%s part 0x%03x", vendor, part);
    else
        snprintf(buffer, sizeof(buffer), "Implementer 0x%02x part 0x%03x", implementer, part);

    cpu_copy_name(name, buffer);
}
#endif /* defined(ROMANO_AARCH64) && defined(ROMANO_LINUX) */

void cpu_get_name(char* name)
{
    memset(name, '\0', ROMANO_CPU_NAME_SZ * sizeof(char));

#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
    uint32_t regs[12] = {0};

    cpuid(regs, 0x80000000, 0);

    if(regs[0] >= 0x80000004U)
    {
        cpuid(&regs[0], 0x80000002, 0);
        cpuid(&regs[4], 0x80000003, 0);
        cpuid(&regs[8], 0x80000004, 0);

        char brand[12 * sizeof(uint32_t) + 1];
        memcpy(brand, regs, 12 * sizeof(uint32_t));
        brand[12 * sizeof(uint32_t)] = '\0';

        cpu_copy_name(name, brand);

        if(name[0] != '\0')
            return;
    }
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */

#if defined(ROMANO_WIN)
    HKEY key;
    char buffer[256];
    DWORD size = sizeof(buffer) - 1;

    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                     "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                     0,
                     KEY_READ,
                     &key) == ERROR_SUCCESS)
    {
        LONG rc = RegQueryValueExA(key, "ProcessorNameString", NULL, NULL, (LPBYTE)buffer, &size);
        RegCloseKey(key);

        if(rc == ERROR_SUCCESS)
        {
            /* REG_SZ is not guaranteed to be null-terminated */
            buffer[size < sizeof(buffer) ? size : sizeof(buffer) - 1] = '\0';
            cpu_copy_name(name, buffer);
        }
    }
#elif defined(ROMANO_APPLE)
    char buffer[256] = {0};
    size_t size = sizeof(buffer) - 1;

    if(sysctlbyname("machdep.cpu.brand_string", buffer, &size, NULL, 0) == 0)
        cpu_copy_name(name, buffer);
#elif defined(ROMANO_LINUX) && defined(ROMANO_AARCH64)
    cpu_get_name_linux_aarch64(name);
#elif defined(ROMANO_BSD)
    char buffer[256] = {0};
    size_t size = sizeof(buffer) - 1;
#if defined(__OpenBSD__)
    int mib[2] = { CTL_HW, HW_MODEL };

    if(sysctl(mib, 2, buffer, &size, NULL, 0) == 0)
        cpu_copy_name(name, buffer);
#else
    if(sysctlbyname("hw.model", buffer, &size, NULL, 0) == 0)
        cpu_copy_name(name, buffer);
#endif /* defined(__OpenBSD__) */
#endif /* defined(ROMANO_WIN) */
}

/* Frequency measurement fallbacks */

#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
/*
 * With an invariant TSC, the TSC ticks at roughly the nominal frequency of the CPU.
 * The clock reads bracket the TSC reads, so the elapsed time is slightly overestimated,
 * which is why we keep the highest rate across a few samples.
 */
static uint32_t cpu_measure_tsc_mhz(void)
{
    double best_mhz = 0.0;

    for(int i = 0; i < 3; ++i)
    {
        const uint64_t t_start = cpu_now_ns();
        const uint64_t c_start = cpu_rdtsc();

#if defined(ROMANO_WIN)
        Sleep(10);
#else
        struct timespec wait_duration;
        wait_duration.tv_sec = 0;
        wait_duration.tv_nsec = 10000000;
        nanosleep(&wait_duration, NULL);
#endif /* defined(ROMANO_WIN) */

        const uint64_t c_end = cpu_rdtsc();
        const uint64_t t_end = cpu_now_ns();

        if(t_end <= t_start)
            continue;

        /* ticks / ns * 1e9 = Hz, / 1e6 = MHz */
        const double mhz = (double)(c_end - c_start) * 1000.0 / (double)(t_end - t_start);

        if(mhz > best_mhz)
            best_mhz = mhz;
    }

    return (uint32_t)(best_mhz + 0.5);
}
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */

#if defined(ROMANO_AARCH64) && (defined(ROMANO_APPLE) || defined(ROMANO_LINUX) || defined(ROMANO_BSD))
#define ROMANO_CPU_HAS_ADD_CHAIN 1

/*
 * On aarch64 there is no TSC equivalent (CNTVCT ticks at a fixed rate unrelated to the
 * CPU clock), and Apple Silicon does not expose its frequency at all.
 *
 * Instead we time a dependency chain: an add has a latency of exactly one clock cycle,
 * so a loop whose body is a chain of N adds on the same register cannot complete more
 * than freq / N iterations per second, no matter how wide or out-of-order the core is.
 * The loop counter lives on its own 1-cycle chain so it never is the bottleneck.
 * It's written in assembly so the compiler cannot fold the adds.
 */
#define ROMANO_CPU_ADD_CHAIN_LEN 4

#if defined(ROMANO_APPLE)
#define ROMANO_CPU_ASM_SYM "_cpu_add_chain_loop"
#define ROMANO_CPU_ASM_VISIBILITY ".private_extern " ROMANO_CPU_ASM_SYM "\n"
#else
#define ROMANO_CPU_ASM_SYM "cpu_add_chain_loop"
#define ROMANO_CPU_ASM_VISIBILITY ".hidden " ROMANO_CPU_ASM_SYM "\n" \
                                  ".type " ROMANO_CPU_ASM_SYM ", %function\n"
#endif /* defined(ROMANO_APPLE) */

__asm__(
    ".text\n"
    ".p2align 4\n"
    ".globl " ROMANO_CPU_ASM_SYM "\n"
    ROMANO_CPU_ASM_VISIBILITY
    ROMANO_CPU_ASM_SYM ":\n"
    "    hint #34\n"                /* BTI C, NOP on cores without BTI */
    "    mov x1, #0\n"
    "    mov x2, #1\n"
    "1:\n"
    "    add x1, x1, x2\n"
    "    add x1, x1, x2\n"
    "    add x1, x1, x2\n"
    "    add x1, x1, x2\n"
    "    subs x0, x0, #1\n"
    "    b.ne 1b\n"
    "    mov x0, x1\n"
    "    ret\n"
);

/* iterations must be > 0 */
__attribute__((visibility("hidden"))) uint64_t cpu_add_chain_loop(uint64_t iterations);

static uint32_t cpu_measure_add_chain_mhz(void)
{
#if defined(ROMANO_APPLE)
    /* Hint the scheduler towards a P-core, otherwise we might measure an E-core */
    const qos_class_t previous_qos = qos_class_self();
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif /* defined(ROMANO_APPLE) */

    uint64_t iterations = 1ULL << 18;
    uint64_t elapsed = 0;

    /* Grow the run until it takes ~5ms so the clock resolution becomes negligible */
    for(;;)
    {
        const uint64_t start = cpu_now_ns();
        cpu_add_chain_loop(iterations);
        elapsed = cpu_now_ns() - start;

        if(elapsed >= 5000000ULL || iterations >= (1ULL << 36))
            break;

        iterations <<= 1;
    }

    /*
     * Preemption, interrupts and DVFS ramp-up can only make a run slower,
     * so the fastest run is the closest to the real frequency
     */
    double best_mhz = 0.0;

    for(int i = 0; i < 3; ++i)
    {
        const uint64_t start = cpu_now_ns();
        cpu_add_chain_loop(iterations);
        elapsed = cpu_now_ns() - start;

        if(elapsed == 0)
            continue;

        const double cycles = (double)iterations * (double)ROMANO_CPU_ADD_CHAIN_LEN;
        const double mhz = cycles * 1000.0 / (double)elapsed;

        if(mhz > best_mhz)
            best_mhz = mhz;
    }

#if defined(ROMANO_APPLE)
    if(previous_qos != QOS_CLASS_UNSPECIFIED)
        pthread_set_qos_class_self_np(previous_qos, 0);
#endif /* defined(ROMANO_APPLE) */

    return (uint32_t)(best_mhz + 0.5);
}
#endif /* defined(ROMANO_AARCH64) && (defined(ROMANO_APPLE) || defined(ROMANO_LINUX) || defined(ROMANO_BSD)) */

/* Frequency detection */

/* Nominal (base/max) frequency in MHz, 0 if unknown */
static uint32_t cpu_detect_frequency(void)
{
#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
    /* CPUID leaf 0x16: base frequency in MHz in EAX[15:0] (Intel Skylake+, not on AMD) */
    uint32_t r[4] = {0};

    cpuid(r, 0, 0);

    if(r[0] >= 0x16U) 
    {
        cpuid(r, 0x16, 0);

        if((r[0] & 0xFFFF) > 0)
            return r[0] & 0xFFFF;
    }
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */

#if defined(ROMANO_WIN)
    HKEY key;
    DWORD mhz = 0;
    DWORD size = sizeof(mhz);
    LSTATUS res = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                                "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                                0,
                                KEY_READ,
                                &key);

    if(res == ERROR_SUCCESS) 
    {
        LONG rc = RegQueryValueExA(key, "~MHz", NULL, NULL, (LPBYTE)&mhz, &size);
        RegCloseKey(key);

        if(rc == ERROR_SUCCESS && mhz > 0)
            return (uint32_t)mhz;
    }
#elif defined(ROMANO_APPLE) && !defined(ROMANO_AARCH64)
    uint64_t hz = 0;
    size_t sz = sizeof(hz);

    if(sysctlbyname("hw.cpufrequency_max", &hz, &sz, NULL, 0) == 0 && hz > 0)
        return (uint32_t)(hz / 1000000ULL);

    sz = sizeof(hz);

    if(sysctlbyname("hw.cpufrequency", &hz, &sz, NULL, 0) == 0 && hz > 0)
        return (uint32_t)(hz / 1000000ULL);
#elif defined(ROMANO_LINUX)
    /* cpufreq sysfs reports kHz */
    const unsigned long khz = cpu_read_ulong_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");

    if(khz > 0)
        return (uint32_t)(khz / 1000UL);

    /* No cpufreq (VMs, some ARM boards) */
    const uint32_t mhz = cpu_read_proc_cpuinfo_mhz();

    if(mhz > 0)
        return mhz;
#elif defined(ROMANO_BSD)
#if defined(__OpenBSD__)
    int mib[2] = { CTL_HW, HW_CPUSPEED };
    int mhz = 0;
    size_t size = sizeof(mhz);

    if(sysctl(mib, 2, &mhz, &size, NULL, 0) == 0 && mhz > 0)
        return (uint32_t)mhz;
#else
    /* "MHz/mW MHz/mW ...", highest level first */
    char levels[256] = {0};
    size_t size = sizeof(levels) - 1;
    unsigned int mhz = 0;

    if(sysctlbyname("dev.cpu.0.freq_levels", levels, &size, NULL, 0) == 0 &&
       sscanf(levels, "%u", &mhz) == 1 && mhz > 0)
        return (uint32_t)mhz;

    int cur_mhz = 0;
    size = sizeof(cur_mhz);

    if(sysctlbyname("dev.cpu.0.freq", &cur_mhz, &size, NULL, 0) == 0 && cur_mhz > 0)
        return (uint32_t)cur_mhz;
#endif /* defined(__OpenBSD__) */
#endif /* defined(ROMANO_WIN) */

#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
    return cpu_measure_tsc_mhz();
#elif defined(ROMANO_CPU_HAS_ADD_CHAIN)
    /* Apple Silicon always ends here, as well as ARM Linux/BSD without cpufreq */
    return cpu_measure_add_chain_mhz();
#else
    return 0;
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */
}

/* Current (dynamic) frequency in MHz, falls back to the nominal one where the OS does not expose it */
static uint32_t cpu_read_current_frequency(void)
{
#if defined(ROMANO_LINUX)
    const unsigned long khz = cpu_read_ulong_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");

    if(khz > 0)
        return (uint32_t)(khz / 1000UL);

    const uint32_t mhz = cpu_read_proc_cpuinfo_mhz();

    if(mhz > 0)
        return mhz;
#elif defined(ROMANO_BSD) && !defined(__OpenBSD__)
    int mhz = 0;
    size_t size = sizeof(mhz);

    if(sysctlbyname("dev.cpu.0.freq", &mhz, &size, NULL, 0) == 0 && mhz > 0)
        return (uint32_t)mhz;
#endif /* defined(ROMANO_LINUX) */

    return cpu_get_frequency();
}

/*
 * Detection is lazy since the fallbacks sleep or spin for a few tens of ms,
 * which we don't want to pay on library load. 
 * It's not thread-safe, but racing callers compute the same value so it's ok, later use atomic maybe
 */
uint32_t cpu_get_frequency(void) 
{ 
    if(!g_cpu_freq_detected)
    {
        g_cpu_freq_mhz = cpu_detect_frequency();
        g_cpu_freq_detected = true;
    }

    return g_cpu_freq_mhz;
}

uint32_t cpu_get_current_frequency(void) 
{
    if((g_cpu_cur_freq_ctr++ % ROMANO_CPU_FREQ_REFRESH_INTERVAL) == 0)
        g_cpu_cur_freq_mhz = cpu_read_current_frequency();

    return g_cpu_cur_freq_mhz;
}

/* Caches */

static CPUCacheInfo g_cpu_caches[CPUCacheLevel_COUNT];
static uint32_t g_cpu_cache_line_size = ROMANO_CACHE_LINE_SIZE;

/*
 * Records a data or unified cache of the given level (1-based). Only fills the fields still
 * unknown: the first source to report a value wins, the next ones only complete it.
 */
ROMANO_CPU_UNUSED static void cpu_cache_set(CPUCacheInfo* caches,
                                            uint32_t level,
                                            size_t size,
                                            uint32_t line_size,
                                            uint32_t shared_by)
{
    CPUCacheInfo* cache;

    if(level < 1 || level > (uint32_t)CPUCacheLevel_COUNT)
        return;

    cache = &caches[level - 1];

    if(cache->size == 0)
        cache->size = size;

    if(cache->line_size == 0)
        cache->line_size = line_size;

    if(cache->shared_by == 0)
        cache->shared_by = shared_by;
}

#if defined(ROMANO_LINUX)

/* Reads the first line of a sysfs-like file without the line break, returns false on failure */
static bool cpu_read_line_file(const char* path, char* buffer, size_t buffer_size)
{
    FILE* f = fopen(path, "r");

    if(f == NULL)
        return false;

    if(fgets(buffer, (int)buffer_size, f) == NULL)
    {
        fclose(f);
        return false;
    }

    fclose(f);

    buffer[strcspn(buffer, "\r\n")] = '\0';

    return true;
}

/* "48K", "2048K", "8M", "32768" -> bytes */
static size_t cpu_parse_cache_size(const char* str)
{
    char* end = NULL;
    unsigned long long value = strtoull(str, &end, 10);

    if(end == str)
        return 0;

    switch(*end)
    {
        case 'K': case 'k': value *= 1024ull; break;
        case 'M': case 'm': value *= 1024ull * 1024ull; break;
        case 'G': case 'g': value *= 1024ull * 1024ull * 1024ull; break;
        default: break;
    }

    return (size_t)value;
}

/* Counts the cpus of a kernel cpu list: "0-3,8,10-11" -> 7 */
static uint32_t cpu_count_cpu_list(const char* str)
{
    uint32_t count = 0;

    while(*str != '\0')
    {
        char* end = NULL;
        unsigned long first = strtoul(str, &end, 10);
        unsigned long last = first;

        if(end == str)
            break;

        if(*end == '-')
        {
            str = end + 1;
            last = strtoul(str, &end, 10);

            if(end == str)
                break;
        }

        if(last >= first)
            count += (uint32_t)(last - first + 1);

        str = end;

        if(*str != ',')
            break;

        str++;
    }

    return count;
}

static void cpu_detect_caches_linux(CPUCacheInfo* caches)
{
    char path[128];
    char buffer[256];
    uint32_t index;

    for(index = 0; index < 32; index++)
    {
        unsigned long level;
        size_t size = 0;
        uint32_t line_size;
        uint32_t shared_by = 0;

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%u/level", index);
        level = cpu_read_ulong_file(path);

        /* Indices are contiguous, a missing one ends the list */
        if(level == 0)
            break;

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%u/type", index);

        if(!cpu_read_line_file(path, buffer, sizeof(buffer)) ||
           (strcmp(buffer, "Data") != 0 && strcmp(buffer, "Unified") != 0))
            continue;

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%u/size", index);

        if(cpu_read_line_file(path, buffer, sizeof(buffer)))
            size = cpu_parse_cache_size(buffer);

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%u/coherency_line_size", index);
        line_size = (uint32_t)cpu_read_ulong_file(path);

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index%u/shared_cpu_list", index);

        if(cpu_read_line_file(path, buffer, sizeof(buffer)))
            shared_by = cpu_count_cpu_list(buffer);

        cpu_cache_set(caches, (uint32_t)level, size, line_size, shared_by);
    }
}

#elif defined(ROMANO_APPLE)

/* Reads an integer sysctl of 32 or 64 bits, returns 0 on failure */
static uint64_t cpu_sysctl_u64(const char* name)
{
    uint64_t value64 = 0;
    size_t size = sizeof(value64);

    if(sysctlbyname(name, &value64, &size, NULL, 0) != 0)
        return 0;

    if(size == sizeof(uint32_t))
    {
        uint32_t value32;
        memcpy(&value32, &value64, sizeof(value32));
        return value32;
    }

    return size == sizeof(uint64_t) ? value64 : 0;
}

static void cpu_detect_caches_apple(CPUCacheInfo* caches)
{
    const uint32_t line_size = (uint32_t)cpu_sysctl_u64("hw.cachelinesize");
    uint64_t cache_config[8];
    size_t cache_config_size = sizeof(cache_config);

    /*
     * Apple Silicon reports its caches per performance level, 0 being the performance cores.
     * The global hw.l*cachesize keys describe the efficiency cores there, so read these first.
     * There is no core-private L3 (the SLC is not reported).
     */
    if(cpu_sysctl_u64("hw.nperflevels") > 0)
    {
        cpu_cache_set(caches, 1, (size_t)cpu_sysctl_u64("hw.perflevel0.l1dcachesize"), line_size, 1);
        cpu_cache_set(caches,
                      2,
                      (size_t)cpu_sysctl_u64("hw.perflevel0.l2cachesize"),
                      line_size,
                      (uint32_t)cpu_sysctl_u64("hw.perflevel0.cpusperl2"));
        cpu_cache_set(caches,
                      3,
                      (size_t)cpu_sysctl_u64("hw.perflevel0.l3cachesize"),
                      line_size,
                      (uint32_t)cpu_sysctl_u64("hw.perflevel0.cpusperl3"));
    }

    /* hw.cacheconfig: logical cpus sharing each level, index 0 being the memory */
    memset(cache_config, 0, sizeof(cache_config));

    if(sysctlbyname("hw.cacheconfig", cache_config, &cache_config_size, NULL, 0) != 0 ||
       cache_config_size % sizeof(uint64_t) != 0)
        memset(cache_config, 0, sizeof(cache_config));

    cpu_cache_set(caches, 1, (size_t)cpu_sysctl_u64("hw.l1dcachesize"), line_size, (uint32_t)cache_config[1]);
    cpu_cache_set(caches, 2, (size_t)cpu_sysctl_u64("hw.l2cachesize"), line_size, (uint32_t)cache_config[2]);
    cpu_cache_set(caches, 3, (size_t)cpu_sysctl_u64("hw.l3cachesize"), line_size, (uint32_t)cache_config[3]);
}

#elif defined(ROMANO_WIN)

static uint32_t cpu_popcount64(uint64_t x)
{
    uint32_t count = 0;

    while(x != 0)
    {
        x &= x - 1;
        count++;
    }

    return count;
}

static void cpu_detect_caches_windows(CPUCacheInfo* caches)
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer;
    DWORD length = 0;
    DWORD count;
    DWORD i;
    int pass;

    if(GetLogicalProcessorInformation(NULL, &length) || GetLastError() != ERROR_INSUFFICIENT_BUFFER || length == 0)
        return;

    buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(length);

    if(buffer == NULL)
        return;

    if(!GetLogicalProcessorInformation(buffer, &length))
    {
        free(buffer);
        return;
    }

    count = length / (DWORD)sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

    /*
     * First pass: the caches of logical processor 0 (a performance core on current hybrid x86
     * cpus), the caches of other cores differ on hybrid cpus. Second pass: anything else.
     */
    for(pass = 0; pass < 2; pass++)
    {
        for(i = 0; i < count; i++)
        {
            const SYSTEM_LOGICAL_PROCESSOR_INFORMATION* entry = &buffer[i];
            const CACHE_DESCRIPTOR* cache = &entry->Cache;

            if(entry->Relationship != RelationCache)
                continue;

            if(cache->Type != CacheData && cache->Type != CacheUnified)
                continue;

            if(pass == 0 && (entry->ProcessorMask & 1) == 0)
                continue;

            cpu_cache_set(caches,
                          (uint32_t)cache->Level,
                          (size_t)cache->Size,
                          (uint32_t)cache->LineSize,
                          cpu_popcount64((uint64_t)entry->ProcessorMask));
        }
    }

    free(buffer);
}

#endif /* defined(ROMANO_LINUX) */

#if defined(ROMANO_X86_64) || defined(ROMANO_X86)

/* CPUID leaf 4 (Intel) and 0x8000001D (AMD with TOPOEXT) share the same layout */
static void cpu_detect_caches_cpuid_deterministic(CPUCacheInfo* caches, uint32_t leaf)
{
    uint32_t r[4];
    uint32_t subleaf;

    for(subleaf = 0; subleaf < 16; subleaf++)
    {
        cpuid(r, leaf, subleaf);

        const uint32_t type = r[0] & 0x1F; /* 0: no more caches, 1: data, 2: instruction, 3: unified */

        if(type == 0)
            break;

        if(type != 1 && type != 3)
            continue;

        const uint32_t level = (r[0] >> 5) & 0x7;
        const uint32_t shared_by = ((r[0] >> 14) & 0xFFF) + 1;
        const uint32_t line_size = (r[1] & 0xFFF) + 1;
        const uint32_t partitions = ((r[1] >> 12) & 0x3FF) + 1;
        const uint32_t ways = ((r[1] >> 22) & 0x3FF) + 1;
        const uint32_t sets = r[2] + 1;

        cpu_cache_set(caches,
                      level,
                      (size_t)ways * partitions * line_size * sets,
                      line_size,
                      shared_by);
    }
}

/*
 * Fills what the OS did not report from CPUID, and returns the CLFLUSH line size as a last
 * resort line size (0 if unavailable)
 */
static uint32_t cpu_detect_caches_cpuid(CPUCacheInfo* caches)
{
    uint32_t r[4];
    uint32_t max_leaf;
    uint32_t max_ext_leaf;
    uint32_t clflush_line_size = 0;
    char vendor[13];
    bool amd;

    cpuid(r, 0, 0);
    max_leaf = r[0];

    memcpy(vendor + 0, &r[1], 4);
    memcpy(vendor + 4, &r[3], 4);
    memcpy(vendor + 8, &r[2], 4);
    vendor[12] = '\0';

    amd = strcmp(vendor, "AuthenticAMD") == 0 || strcmp(vendor, "HygonGenuine") == 0;

    if(max_leaf >= 1)
    {
        cpuid(r, 1, 0);
        clflush_line_size = ((r[1] >> 8) & 0xFF) * 8;
    }

    cpuid(r, 0x80000000, 0);
    max_ext_leaf = r[0];

    if(amd)
    {
        bool topoext = false;

        if(max_ext_leaf >= 0x80000001)
        {
            cpuid(r, 0x80000001, 0);
            topoext = ((r[2] >> 22) & 1) != 0;
        }

        if(topoext && max_ext_leaf >= 0x8000001D)
        {
            cpu_detect_caches_cpuid_deterministic(caches, 0x8000001D);
            return clflush_line_size;
        }

        /* Legacy AMD leaves: sizes and line sizes only */
        if(max_ext_leaf >= 0x80000005)
        {
            cpuid(r, 0x80000005, 0);
            cpu_cache_set(caches, 1, (size_t)(r[2] >> 24) * 1024, r[2] & 0xFF, 0);
        }

        if(max_ext_leaf >= 0x80000006)
        {
            cpuid(r, 0x80000006, 0);
            cpu_cache_set(caches, 2, (size_t)(r[2] >> 16) * 1024, r[2] & 0xFF, 0);
            cpu_cache_set(caches, 3, (size_t)(r[3] >> 18) * 512 * 1024, r[3] & 0xFF, 0);
        }

        return clflush_line_size;
    }

    if(max_leaf >= 4)
        cpu_detect_caches_cpuid_deterministic(caches, 4);

    return clflush_line_size;
}

#elif defined(ROMANO_AARCH64) && (defined(ROMANO_LINUX) || defined(__FreeBSD__)) && defined(__GNUC__)

/*
 * CTR_EL0.DminLine: log2 of the words in the smallest data cache line of all levels.
 * Readable from EL0 on Linux and FreeBSD (or trapped and emulated by the kernel).
 */
static uint32_t cpu_aarch64_ctr_line_size(void)
{
    uint64_t ctr;

    __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));

    return 4u << ((ctr >> 16) & 0xF);
}

#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */

static bool cpu_is_valid_line_size(uint32_t line_size)
{
    /* A power of two in a plausible range, rejects garbage from VMs and odd firmwares */
    return line_size >= 16 && line_size <= 1024 && (line_size & (line_size - 1)) == 0;
}

static void cpu_detect_caches(void)
{
    uint32_t fallback_line_size = 0;
    uint32_t i;

    memset(g_cpu_caches, 0, sizeof(g_cpu_caches));

#if defined(ROMANO_LINUX)
    cpu_detect_caches_linux(g_cpu_caches);
#elif defined(ROMANO_APPLE)
    cpu_detect_caches_apple(g_cpu_caches);
#elif defined(ROMANO_WIN)
    cpu_detect_caches_windows(g_cpu_caches);
#endif /* defined(ROMANO_LINUX) */

#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
    fallback_line_size = cpu_detect_caches_cpuid(g_cpu_caches);
#elif defined(ROMANO_AARCH64) && (defined(ROMANO_LINUX) || defined(__FreeBSD__)) && defined(__GNUC__)
    fallback_line_size = cpu_aarch64_ctr_line_size();
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */

    if(cpu_is_valid_line_size(g_cpu_caches[CPUCacheLevel_L1].line_size))
        g_cpu_cache_line_size = g_cpu_caches[CPUCacheLevel_L1].line_size;
    else if(cpu_is_valid_line_size(fallback_line_size))
        g_cpu_cache_line_size = fallback_line_size;
    else
        g_cpu_cache_line_size = ROMANO_CACHE_LINE_SIZE;

    for(i = 0; i < (uint32_t)CPUCacheLevel_COUNT; i++)
    {
        CPUCacheInfo* cache = &g_cpu_caches[i];

        if(cache->size == 0)
        {
            memset(cache, 0, sizeof(CPUCacheInfo));
            continue;
        }

        if(!cpu_is_valid_line_size(cache->line_size))
            cache->line_size = g_cpu_cache_line_size;
    }
}

bool cpu_get_cache_info(CPUCacheLevel level, CPUCacheInfo* info)
{
    ROMANO_ASSERT(info != NULL, "info is NULL");

    if((uint32_t)level >= (uint32_t)CPUCacheLevel_COUNT || g_cpu_caches[level].size == 0)
    {
        memset(info, 0, sizeof(CPUCacheInfo));
        return false;
    }

    *info = g_cpu_caches[level];

    return true;
}

size_t cpu_get_cache_size(CPUCacheLevel level)
{
    if((uint32_t)level >= (uint32_t)CPUCacheLevel_COUNT)
        return 0;

    return g_cpu_caches[level].size;
}

uint32_t cpu_get_cache_line_size(void)
{
    return g_cpu_cache_line_size;
}

/* cpu_check (ran on dll/dylib/so load, see dll_main.c) */

void cpu_check(void) 
{
    memset(g_cpu_features, 0, sizeof(g_cpu_features));

    g_cpu_freq_detected = false;
    g_cpu_freq_mhz = 0;
    g_cpu_cur_freq_mhz = 0;
    g_cpu_cur_freq_ctr = 0;

    cpu_detect_features();
    cpu_detect_caches();
}

/* Features print */

typedef struct {
    CPUFeature  feature;
    const char *name;
    const char *group; /* NULL = continue the previous group */
} CPUFeatureName;

#define F_(feat, name, group) { CPUFeature_##feat, name, group },

static const CPUFeatureName g_feature_names[] = {
    /* x86 / x86_64 */
    F_(MMX,        "MMX",         "x86 SIMD")
    F_(SSE,        "SSE",         NULL)
    F_(SSE2,       "SSE2",        NULL)
    F_(SSE3,       "SSE3",        NULL)
    F_(SSSE3,      "SSSE3",       NULL)
    F_(SSE4_1,     "SSE4.1",      NULL)
    F_(SSE4_2,     "SSE4.2",      NULL)
    F_(AVX,        "AVX",         NULL)
    F_(AVX2,       "AVX2",        NULL)
    F_(FMA3,       "FMA3",        NULL)
    F_(F16C,       "F16C",        NULL)
    F_(AVX512F,    "AVX512F",     NULL)
    F_(AVX512DQ,   "AVX512DQ",    NULL)
    F_(AVX512IFMA, "AVX512IFMA",  NULL)
    F_(AVX512PF,   "AVX512PF",    NULL)
    F_(AVX512ER,   "AVX512ER",    NULL)
    F_(AVX512CD,   "AVX512CD",    NULL)
    F_(AVX512BW,   "AVX512BW",    NULL)
    F_(AVX512VL,   "AVX512VL",    NULL)
    F_(AVX512VBMI, "AVX512VBMI",  NULL)

    F_(AES,        "AES",         "x86 Crypto")
    F_(PCLMULQDQ,  "PCLMULQDQ",   NULL)
    F_(SHA,        "SHA",         NULL)
    F_(RDRAND,     "RDRAND",      NULL)
    F_(RDSEED,     "RDSEED",      NULL)

    F_(BMI1,       "BMI1",        "x86 Bitmanip")
    F_(BMI2,       "BMI2",        NULL)
    F_(LZCNT,      "LZCNT",       NULL)
    F_(POPCNT,     "POPCNT",      NULL)
    F_(ADX,        "ADX",         NULL)

    F_(AMX_TILE,   "AMX_TILE",    "x86 AMX")
    F_(AMX_INT8,   "AMX_INT8",    NULL)
    F_(AMX_BF16,   "AMX_BF16",    NULL)

    /* aarch64 */
    F_(FP,         "FP",          "ARM SIMD")
    F_(AdvSIMD,    "AdvSIMD",     NULL)
    F_(FP16,       "FP16",        NULL)
    F_(FHM,        "FHM",         NULL)
    F_(FCMA,       "FCMA",        NULL)
    F_(RDM,        "RDM",         NULL)
    F_(DotProd,    "DotProd",     NULL)
    F_(I8MM,       "I8MM",        NULL)
    F_(BF16,       "BF16",        NULL)
    F_(FRINTTS,    "FRINTTS",     NULL)
    F_(JSCVT,      "JSCVT",       NULL)

    F_(AES_ARM,    "AES",         "ARM Crypto")
    F_(PMULL,      "PMULL",       NULL)
    F_(SHA1,       "SHA1",        NULL)
    F_(SHA2,       "SHA256",      NULL)
    F_(SHA3,       "SHA3",        NULL)
    F_(SHA512,     "SHA512",      NULL)
    F_(SM3,        "SM3",         NULL)
    F_(SM4,        "SM4",         NULL)
    F_(CRC32,      "CRC32",       NULL)

    F_(SVE,        "SVE",         "ARM Scalable")
    F_(SVE2,       "SVE2",        NULL)

    F_(LSE,        "LSE",         "ARM System")
    F_(DIT,        "DIT",         NULL)
};
#undef F_

void cpu_print_features(void)
{
    const size_t n = sizeof(g_feature_names) / sizeof(g_feature_names[0]);

    char name[ROMANO_CPU_NAME_SZ];
    cpu_get_name(name);

    printf("CPU        : %s\n", ROMANO_PLATFORM_STR);
    printf("Name       : %s\n", name[0] != '\0' ? name : "unknown");

    const uint32_t mhz = cpu_get_frequency();

    if(mhz > 0)
        printf("Frequency  : %u MHz\n", mhz);
    else
        printf("Frequency  : unknown\n");

    const uint32_t cur_mhz = cpu_get_current_frequency();

    if(cur_mhz > 0)
        printf("Current    : %u MHz\n", cur_mhz);
    else
        printf("Current    : unknown\n");

    for(uint32_t level = 0; level < (uint32_t)CPUCacheLevel_COUNT; level++)
    {
        CPUCacheInfo cache;
        static const char* const cache_names[CPUCacheLevel_COUNT] = { "L1d", "L2", "L3" };

        if(!cpu_get_cache_info((CPUCacheLevel)level, &cache))
            continue;

        printf("Cache %-5s: ", cache_names[level]);

        if(cache.size >= 1024 * 1024 && cache.size % (1024 * 1024) == 0)
            printf("%llu MiB", (unsigned long long)(cache.size / (1024 * 1024)));
        else
            printf("%llu KiB", (unsigned long long)(cache.size / 1024));

        printf(", %u B lines", cache.line_size);

        if(cache.shared_by > 0)
            printf(", shared by %u logical cpu%s", cache.shared_by, cache.shared_by > 1 ? "s" : "");

        printf("\n");
    }

    printf("Line size  : %u B\n", cpu_get_cache_line_size());

    printf("Features   :");

    const char *entry_group = NULL;   /* group the current entry belongs to */
    const char *printed_group = NULL; /* last group header printed */
    int col = 0;
    bool any = false;

    for(size_t i = 0; i < n; ++i)
    {
        const CPUFeatureName *e = &g_feature_names[i];

        /* Track the group even for skipped entries, the header is only on the first one */
        if(e->group != NULL)
            entry_group = e->group;

        if(!cpu_has_feature(e->feature))
            continue;

        any = true;

        if(entry_group != printed_group) 
        {
            if(printed_group != NULL)
                printf("\n");

            printf("\n  %-12s:", entry_group);
            col = 15;                 /* "  " + 12 + ":" */
            printed_group = entry_group;
        }

        int len = (int)strlen(e->name);

        if(col + len + 1 > 78) 
        {
            printf("\n               ");
            col = 15;
        }

        printf(" %s", e->name);
        col += len + 1;
    }

    if(!any)
        printf(" (none detected)\n");
    else
        printf("\n");
}