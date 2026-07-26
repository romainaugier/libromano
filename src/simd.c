/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include <libromano/simd.h>
#include <libromano/cpu.h>

#include <string.h>

static int g_vectorization_mode = 0;

#if defined(ROMANO_X86_64)

#if defined(ROMANO_WIN)
#include <intrin.h>
#define xgetbv(x) _xgetbv(x)
#else
static inline uint64_t xgetbv(uint32_t index)
{
    uint32_t eax, edx;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return ((uint64_t)edx << 32) | eax;
}
#endif

void simd_check_vectorization(void)
{
    int regs[4];

    cpuid(regs, 1);

    if(getenv("LIBROMANO_VECTORIZATION") != NULL)
    {
        char* env_val = getenv("LIBROMANO_VECTORIZATION");

        if(strcmp(env_val, "0") == 0)
            return;
        else if(strcmp(env_val, "1") == 0)
            g_vectorization_mode = VectorizationMode_SSE;
        else if(strcmp(env_val, "2") == 0)
            g_vectorization_mode = VectorizationMode_AVX;
        else if(strcmp(env_val, "3") == 0)
            g_vectorization_mode = VectorizationMode_AVX256;
        else if(strcmp(env_val, "4") == 0)
            g_vectorization_mode = VectorizationMode_AVX512;
    }

    int sse = regs[3] & (1 << 25); // EDX bit 25
    int sse2 = regs[3] & (1 << 26);
    int avx_cpu = regs[2] & (1 << 28); // ECX bit 28
    int osxsave = regs[2] & (1 << 27); // ECX bit 27

    int avx_os = 0, avx512_os = 0;

    if(osxsave)
    {
        uint64_t xcr0 = xgetbv(0);
        avx_os = (xcr0 & 0x6) == 0x6; // XMM+YMM state
        avx512_os = (xcr0 & 0xE6) == 0xE6; // + opmask/ZMM state
    }

    int avx = avx_cpu && avx_os;

    int avx2 = 0, avx512 = 0;

    if(avx)
    {
        // Leaf 7 requires checking max leaf first (leaf 0, EAX result >= 7)
        cpuid(regs, 7);
        avx2 = regs[1] & (1 << 5); // EBX bit 5
        avx512 = (regs[1] & (1 << 16)) && avx512_os; // AVX512F
    }

    if(avx512)
        g_vectorization_mode = VectorizationMode_AVX512;
    else if(avx2)
        g_vectorization_mode = VectorizationMode_AVX256;
    else if(avx)
        g_vectorization_mode = VectorizationMode_AVX;
    else if(sse)
        g_vectorization_mode = VectorizationMode_SSE;
    else
        g_vectorization_mode = VectorizationMode_Scalar;
}

int simd_has_sse(void)
{
    return g_vectorization_mode >= 1;
}

int simd_has_avx(void)
{
    return g_vectorization_mode >= 2;
}

int simd_has_avx256(void)
{
    return g_vectorization_mode >= 3;
}

int simd_has_avx512(void)
{
    return g_vectorization_mode >= 4;
}

VectorizationMode simd_get_vectorization_mode(void)
{
    return g_vectorization_mode;
}

void simd_force_vectorization_mode(const VectorizationMode mode)
{
    g_vectorization_mode = mode;
}

const char* simd_get_vectorization_mode_as_string(VectorizationMode mode)
{
    switch(mode)
    {
        case VectorizationMode_Scalar: return "Scalar";
        case VectorizationMode_SSE: return "SSE";
        case VectorizationMode_AVX: return "AVX";
        case VectorizationMode_AVX256: return "AVX256";
        case VectorizationMode_AVX512: return "AVX512";
        default: return "Unknown";
    }
}

#elif defined(ROMANO_AARCH64)

#if defined(ROMANO_APPLE)
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(ROMANO_LINUX)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#elif defined(ROMANO_WIN)
#include <Windows.h>
#endif /* defined(ROMANO_APPLE) */

void simd_check_vectorization(void)
{
    /* On Apple Silicon we can assume neon is always there but still */
#if defined(ROMANO_APPLE)
    int has_neon = 0;
    size_t size = sizeof(has_neon);

    if(sysctlbyname("hw.optional.neon", &has_neon, &size, NULL, 0) == 0)
        g_vectorization_mode = VectorizationMode_NEON;
#elif defined(ROMANO_LINUX)
    unsigned long hwcap = getauxval(AT_HWCAP);
    g_vectorization_mode = (hwcap & HWCAP_ASIMD) != 0;
#elif defined(ROMANO_WIN)
    g_vectorization_mode = IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE);
#endif /* defined(ROMANO_APPLE) */
}

int simd_has_neon(void)
{
    return g_vectorization_mode >= VectorizationMode_NEON;
}

VectorizationMode simd_get_vectorization_mode(void)
{
    return g_vectorization_mode;
}

void simd_force_vectorization_mode(const VectorizationMode mode)
{
    g_vectorization_mode = mode;
}

const char* simd_get_vectorization_mode_as_string(VectorizationMode mode)
{
    switch(mode)
    {
        case VectorizationMode_Scalar: return "Scalar";
        case VectorizationMode_NEON: return "NEON";
        default: return "Unknown";
    }
}

#endif /* defined(ROMANO_X86_64) */
