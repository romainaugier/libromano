/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/cpu.h"

#include <string.h>

#if defined(ROMANO_WIN)
#include <windows.h>
#include <winreg.h>
#elif defined(ROMANO_APPLE)
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(ROMANO_LINUX)
#include <stdio.h>
#include <sys/auxv.h>
#elif defined(ROMANO_BSD)
#include <sys/sysctl.h>
#include <sys/types.h>
#include <stdio.h>
#endif /* defined(ROMANO_WIN) */

#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
#if defined(ROMANO_MSVC)
#include <intrin.h>
#endif /* defined(ROMANO_MSVC) */
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */

#if defined(ROMANO_AARCH64) && defined(ROMANO_LINUX)
#if !defined(AT_HWCAP2)
#define AT_HWCAP2 26
#endif /* !defined(AT_HWCAP2) */
#endif /* defined(ROMANO_AARCH64) && defined(ROMANO_LINUX) */

static bool g_cpu_features[CPUFeature_COUNT] = {0};
static uint32_t g_cpu_freq_mhz = 0;
static uint32_t g_cpu_cur_freq_mhz = 0;
static uint64_t g_cpu_freq_ctr = 0;

bool cpu_has_feature(CPUFeature feature) 
{
    uint32_t f = (uint32_t)feature;
    return f < (uint32_t)CPUFeature_COUNT && g_cpu_features[f];
}

uint32_t cpu_get_frequency(void) 
{ 
    return g_cpu_freq_mhz;
}

uint64_t cpu_rdtsc(void)
{
#if defined(ROMANO_X86_64)
    return __rdtsc();
#elif defined(ROMANO_AARCH64)
#if defined(ROMANO_WIN)
#if defined(ROMANO_MSVC)
#if !defined(ARM64_CNTVCT)
#define ARM64_CNTVCT 0x5F02 /* ARM64_SYSREG(3,3,14,0,2) */
#endif /* !defined(ARM64_CNTVCT) */
    /* MSVC */
    return (uint64_t)_ReadStatusReg(ARM64_CNTVCT);
#else
    /* MinGW or clang on Windows ARM64 */
    uint64_t value = 0;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(value));
    return value;
#endif /* defined(ROMANO_MSVC) */
#elif defined(ROMANO_APPLE) || defined(ROMANO_LINUX) || defined(ROMANO_BSD)
    uint64_t value = 0;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(value));
    return value;
#endif /* defined(ROMANO_WIN) */
#endif /* defined(ROMANO_X86_64) */
}

#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
#if defined(ROMANO_MSVC)
static inline void cpuid(int regs[4], int leaf, int subleaf) 
{
    __cpuidex(regs, leaf, subleaf);
#else
    __asm__ __volatile__(
        "cpuid"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(leaf), "c"(subleaf));
}
#endif /* defined(ROMANO_MSVC) */
#if defined(ROMANO_MSVC)
#include <intrin.h>
#define xgetbv(x) _xgetbv(x)
#else
static inline uint64_t xgetbv(uint32_t index)
{
    uint32_t eax, edx;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return ((uint64_t)edx << 32) | eax;
}
#endif /* defined(ROMANO_MSVC) */
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */

/* Feature detection */

static void cpu_detect_features(void)
{
#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
    int r[4] = {0};
    cpuid(r, 0, 0);
    int max_leaf = r[0];

    if(max_leaf >= 1) 
    {
        cpuid(r, 1, 0);
        uint32_t ecx = (uint32_t)r[2], edx = (uint32_t)r[3];

        g_cpu_features[CPUFeature_MMX] = (edx >> 23) & 1;
        g_cpu_features[CPUFeature_SSE] = (edx >> 25) & 1;
        g_cpu_features[CPUFeature_SSE2] = (edx >> 26) & 1;
        g_cpu_features[CPUFeature_SSE3] = (ecx >>  0) & 1;
        g_cpu_features[CPUFeature_SSSE3] = (ecx >>  9) & 1;
        g_cpu_features[CPUFeature_SSE4_1] = (ecx >> 19) & 1;
        g_cpu_features[CPUFeature_SSE4_2] = (ecx >> 20) & 1;
        g_cpu_features[CPUFeature_POPCNT] = (ecx >> 23) & 1;
        g_cpu_features[CPUFeature_AES] = (ecx >> 25) & 1;
        g_cpu_features[CPUFeature_PCLMULQDQ] = (ecx >>  1) & 1;
        g_cpu_features[CPUFeature_FMA3] = (ecx >> 12) & 1;
        g_cpu_features[CPUFeature_AVX] = (ecx >> 28) & 1;
        g_cpu_features[CPUFeature_F16C] = (ecx >> 29) & 1;
        g_cpu_features[CPUFeature_RDRAND] = (ecx >> 30) & 1;
    }

    if(max_leaf >= 7)
    {
        cpuid(r, 7, 0);
        uint32_t ebx = (uint32_t)r[1];
        uint32_t ecx = (uint32_t)r[2];
        uint32_t edx = (uint32_t)r[3];

        g_cpu_features[CPUFeature_BMI1] = (ebx >>  3) & 1;
        g_cpu_features[CPUFeature_AVX2] = (ebx >>  5) & 1;
        g_cpu_features[CPUFeature_BMI2] = (ebx >>  8) & 1;
        g_cpu_features[CPUFeature_AVX512F] = (ebx >> 16) & 1;
        g_cpu_features[CPUFeature_AVX512DQ] = (ebx >> 17) & 1;
        g_cpu_features[CPUFeature_RDSEED] = (ebx >> 18) & 1;
        g_cpu_features[CPUFeature_ADX] = (ebx >> 19) & 1;
        g_cpu_features[CPUFeature_AVX512IFMA] = (ebx >> 21) & 1;
        g_cpu_features[CPUFeature_AVX512PF] = (ebx >> 26) & 1;
        g_cpu_features[CPUFeature_AVX512ER] = (ebx >> 27) & 1;
        g_cpu_features[CPUFeature_AVX512CD] = (ebx >> 28) & 1;
        g_cpu_features[CPUFeature_SHA] = (ebx >> 29) & 1;
        g_cpu_features[CPUFeature_AVX512BW] = (ebx >> 30) & 1;
        g_cpu_features[CPUFeature_AVX512VL] = (ebx >> 31) & 1;
        g_cpu_features[CPUFeature_AVX512VBMI] = (ecx >>  1) & 1;
        g_cpu_features[CPUFeature_AMX_BF16] = (edx >> 22) & 1;
        g_cpu_features[CPUFeature_AMX_TILE] = (edx >> 24) & 1;
        g_cpu_features[CPUFeature_AMX_INT8] = (edx >> 25) & 1;
    }

    cpuid(r, 0x80000000, 0);

    if((uint32_t)r[0] >= 0x80000001U)
    {
        cpuid(r, 0x80000001, 0);
        g_cpu_features[CPUFeature_LZCNT] = ((uint32_t)r[2] >> 5) & 1;
    }
#elif defined(ROMANO_AARCH64)
#if defined(ROMANO_LINUX)
    unsigned long hwcap  = getauxval(AT_HWCAP);
    unsigned long hwcap2 = getauxval(AT_HWCAP2);

    g_cpu_features[CPUFeature_FP]      = (hwcap  & (1UL <<  0)) != 0;
    g_cpu_features[CPUFeature_AdvSIMD] = (hwcap  & (1UL <<  1)) != 0;
    g_cpu_features[CPUFeature_AES_ARM] = (hwcap  & (1UL <<  3)) != 0;
    g_cpu_features[CPUFeature_PMULL]   = (hwcap  & (1UL <<  4)) != 0;
    g_cpu_features[CPUFeature_SHA1]    = (hwcap  & (1UL <<  5)) != 0;
    g_cpu_features[CPUFeature_SHA2]    = (hwcap  & (1UL <<  6)) != 0;
    g_cpu_features[CPUFeature_CRC32]   = (hwcap  & (1UL <<  7)) != 0;
    g_cpu_features[CPUFeature_LSE]     = (hwcap  & (1UL <<  8)) != 0;
    g_cpu_features[CPUFeature_FP16]    = (hwcap  & (1UL << 10)) != 0;
    g_cpu_features[CPUFeature_RDM]     = (hwcap  & (1UL << 12)) != 0;
    g_cpu_features[CPUFeature_JSCVT]   = (hwcap  & (1UL << 13)) != 0;
    g_cpu_features[CPUFeature_FCMA]    = (hwcap  & (1UL << 14)) != 0;
    g_cpu_features[CPUFeature_SHA3]    = (hwcap  & (1UL << 17)) != 0;
    g_cpu_features[CPUFeature_SM3]     = (hwcap  & (1UL << 18)) != 0;
    g_cpu_features[CPUFeature_SM4]     = (hwcap  & (1UL << 19)) != 0;
    g_cpu_features[CPUFeature_DotProd] = (hwcap  & (1UL << 20)) != 0;
    g_cpu_features[CPUFeature_SHA512]  = (hwcap  & (1UL << 21)) != 0;
    g_cpu_features[CPUFeature_SVE]     = (hwcap  & (1UL << 22)) != 0;
    g_cpu_features[CPUFeature_FHM]     = (hwcap  & (1UL << 23)) != 0;
    g_cpu_features[CPUFeature_DIT]     = (hwcap  & (1UL << 24)) != 0;

    g_cpu_features[CPUFeature_SVE2]    = (hwcap2 & (1UL <<  1)) != 0;
    g_cpu_features[CPUFeature_FRINTTS] = (hwcap2 & (1UL <<  8)) != 0;
    g_cpu_features[CPUFeature_I8MM]    = (hwcap2 & (1UL << 13)) != 0;
    g_cpu_features[CPUFeature_BF16]    = (hwcap2 & (1UL << 14)) != 0;

#elif defined(ROMANO_APPLE)
    #define ROMANO_SYSCTL_BOOL(name) \
        ({ int32_t _v = 0; size_t _s = sizeof(_v); \
            sysctlbyname(name, &_v, &_s, NULL, 0) == 0 && _v != 0; })

    g_cpu_features[CPUFeature_AdvSIMD] = ROMANO_SYSCTL_BOOL("hw.optional.AdvSIMD");
    g_cpu_features[CPUFeature_FP] = ROMANO_SYSCTL_BOOL("hw.optional.floatingpoint");
    g_cpu_features[CPUFeature_AES_ARM] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_AES");
    g_cpu_features[CPUFeature_PMULL] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_PMULL");
    g_cpu_features[CPUFeature_SHA1] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_SHA1");
    g_cpu_features[CPUFeature_SHA2] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_SHA256");
    g_cpu_features[CPUFeature_SHA3] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_SHA3");
    g_cpu_features[CPUFeature_SHA512] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_SHA512");
    g_cpu_features[CPUFeature_CRC32] = ROMANO_SYSCTL_BOOL("hw.optional.armv8_crc32");
    g_cpu_features[CPUFeature_LSE] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_LSE");
    g_cpu_features[CPUFeature_RDM] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_RDM");
    g_cpu_features[CPUFeature_DotProd] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_DotProd");
    g_cpu_features[CPUFeature_FP16] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_FP16");
    g_cpu_features[CPUFeature_FHM] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_FHM");
    g_cpu_features[CPUFeature_FCMA] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_FCMA");
    g_cpu_features[CPUFeature_JSCVT] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_JSCVT");
    g_cpu_features[CPUFeature_FRINTTS] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_FRINTTS");
    g_cpu_features[CPUFeature_I8MM] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_I8MM");
    g_cpu_features[CPUFeature_BF16] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_BF16");
    g_cpu_features[CPUFeature_SVE] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_SVE");
    g_cpu_features[CPUFeature_SVE2] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_SVE2");
    g_cpu_features[CPUFeature_SM3] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_SM3");
    g_cpu_features[CPUFeature_SM4] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_SM4");
    g_cpu_features[CPUFeature_DIT] = ROMANO_SYSCTL_BOOL("hw.optional.arm.FEAT_DIT");

    #undef ROMANO_SYSCTL_BOOL
#elif defined(ROMANO_WIN)
    g_cpu_features[CPUFeature_FP] = true;
    g_cpu_features[CPUFeature_AdvSIMD] = true;
    g_cpu_features[CPUFeature_AES_ARM] = IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
    g_cpu_features[CPUFeature_PMULL] = g_cpu_features[CPUFeature_AES_ARM];
    g_cpu_features[CPUFeature_SHA1] = g_cpu_features[CPUFeature_AES_ARM];
    g_cpu_features[CPUFeature_SHA2] = g_cpu_features[CPUFeature_AES_ARM];
    g_cpu_features[CPUFeature_CRC32] = IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE);
    g_cpu_features[CPUFeature_LSE] = IsProcessorFeaturePresent(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE);
#elif defined(ROMANO_BSD)
    int32_t v = 0; size_t s = sizeof(v);

    if(sysctlbyname("hw.floatingpoint", &v, &s, NULL, 0) == 0)
        g_cpu_features[CPUFeature_FP] = v != 0;

    s = sizeof(v);
    if(sysctlbyname("hw.AdvSIMD", &v, &s, NULL, 0) == 0)
        g_cpu_features[CPUFeature_AdvSIMD] = v != 0;

    s = sizeof(v);
    if(sysctlbyname("hw.aes", &v, &s, NULL, 0) == 0)
        g_cpu_features[CPUFeature_AES_ARM] = v != 0;

    s = sizeof(v);
    if(sysctlbyname("hw.crc32", &v, &s, NULL, 0) == 0)
        g_cpu_features[CPUFeature_CRC32] = v != 0;
#endif /* defined(ROMANO_LINUX) */
#endif /*  defined(ROMANO_X86_64) || defined(ROMANO_X86) */
}

/* CPU Name */
void cpu_get_name(char* name)
{
#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
    uint32_t regs[12];

    cpuid(&regs[0], 0x80000000);

    if(regs[0] < 0x80000004)
    {
        name[0] = '\0';
        return;
    }

    cpuid(&regs[0], 0x80000002);
    cpuid(&regs[4], 0x80000003);
    cpuid(&regs[8], 0x80000004);

    memcpy(name, regs, 12 * sizeof(uint32_t));

    name[12 * sizeof(uint32_t)] = '\0';
#elif defined(ROMANO_AARCH64)
    size_t name_sz = ROMANO_CPU_NAME_SZ;
    memset(name, '\0', name_sz * sizeof(char));
#if defined(ROMANO_APPLE)
    sysctlbyname("machdep.cpu.brand_string", name, &name_sz, NULL, ROMANO_CPU_NAME_SZ);
#endif /* defined(ROMANO_APPLE) */
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */
}

#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
#if !defined(__USE_POSIX199309)
#define __USE_POSIX199309
#endif /* !defined(__USE_POSIX199309) */
#include <time.h>
#endif /* defined(ROMANO_LINUX) || defined(ROMANO_APPLE) */

/* Frequency detection */
uint32_t cpu_get_current_frequency(void) 
{
    if(g_cpu_freq_ctr % 10000 != 0)
        return g_cpu_cur_freq_mhz;

    g_cpu_freq_ctr++;
        
#if defined(ROMANO_X86_64) || defined(ROMANO_X86)
    /* CPUID leaf 0x16: base frequency in MHz in EAX */
    int r[4] = {0};

    cpuid(r, 0, 0);

    if((uint32_t)r[0] >= 0x16U) 
    {
        cpuid(r, 0x16, 0);

        if(r[0] > 0)
            return (uint32_t)r[0];
    }
#endif /* defined(ROMANO_X86_64) || defined(ROMANO_X86) */

#if defined(ROMANO_WIN)
    HKEY  key;
    DWORD mhz  = 0;
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
            return mhz;
    }
#elif defined(ROMANO_APPLE) && !defined(ROMANO_AARCH64)
    /* not exposed on Apple Silicon */
    uint64_t hz = 0;
    size_t sz = sizeof(hz);

    if(sysctlbyname("hw.cpufrequency_max", &hz, &sz, NULL, 0) == 0 && hz > 0)
        return (uint32_t)(hz / 1000000ULL);

    sz = sizeof(hz);

    if(sysctlbyname("hw.cpufrequency", &hz, &sz, NULL, 0) == 0 && hz > 0)
        return (uint32_t)(hz / 1000000ULL);
#elif defined(ROMANO_LINUX)
    /* cpufreq sysfs reports kHz */
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");

    if(f != NULL) 
    {
        unsigned long khz = 0;

        if(fscanf(f, "%lu", &khz) == 1 && khz > 0) 
        {
            fclose(f);
            return (uint32_t)(khz / 1000UL);
        }

        fclose(f);
    }

    /* Fallback: /proc/cpuinfo "cpu MHz". */
    f = fopen("/proc/cpuinfo", "r");

    if(f != NULL) 
    {
        char line[256];
        while(fgets(line, sizeof(line), f)) 
        {
            double mhz = 0.0;

            if(sscanf(line, "cpu MHz : %lf", &mhz) == 1 && mhz > 0.0) 
            {
                fclose(f);
                return (uint32_t)mhz;
            }
        }

        fclose(f);
    }
#elif defined(ROMANO_BSD)
    int mhz  = 0;
    size_t size = sizeof(mhz);

    if(sysctlbyname("hw.clockrate", &mhz, &size, NULL, 0) == 0 && mhz > 0)
        return (uint32_t)(mhz / 1000); /* kHz -> MHz */

    size = sizeof(mhz);

    if(sysctlbyname("dev.cpu.0.freq", &mhz, &size, NULL, 0) == 0 && mhz > 0)
        return (uint32_t)mhz;
#endif
    const uint64_t start = cpu_rdtsc();

    struct timespec wait_duration;
    wait_duration.tv_sec = 0;
    wait_duration.tv_nsec = 1000000;

    nanosleep(&wait_duration, NULL);

    const uint64_t end = cpu_rdtsc();

    const double frequency = (double)(end - start) * 1000;

    return (uint32_t)frequency;
}

/* cpu_check (ran on dll/dylib/so load, see dll_main.c) */

void cpu_check(void) 
{
    memset(g_cpu_features, 0, sizeof(g_cpu_features));
    g_cpu_freq_mhz = 0;

    cpu_detect_features();
    g_cpu_freq_mhz = cpu_get_current_frequency();
    g_cpu_cur_freq_mhz = cpu_get_current_frequency();
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

    printf("CPU        : %s\n", ROMANO_PLATFORM_STR);

    uint32_t mhz = cpu_get_current_frequency();

    if(mhz > 0)
        printf("Frequency  : %u MHz\n", mhz);
    else
        printf("Frequency  : unknown\n");

    printf("Features   :");

    const char *cur_group = NULL;
    int  col  = 0;
    bool any  = false;

    for(size_t i = 0; i < n; ++i)
    {
        const CPUFeatureName *e = &g_feature_names[i];

        if(!cpu_has_feature(e->feature))
            continue;

        any = true;

        if(e->group && e->group != cur_group) 
        {
            if(cur_group)
                printf("\n");

            printf("\n  %-10s:", e->group);
            col = 14;                 /* "  " + 10 + ":" */
            cur_group = e->group;
        }

        int len = (int)strlen(e->name);

        if(col + len + 1 > 78) 
        {
            printf("\n              ");
            col = 14;
        }

        printf(" %s", e->name);
        col += len + 1;
    }

    if(!any)
        printf(" (none detected)\n");
    else if(any)
        printf("\n");
}