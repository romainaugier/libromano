/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include <libromano/simd.h>
#include <libromano/cpu.h>

#include <string.h>

static int _vectorization_mode = 0;

#if defined(ROMANO_X86_64)

void simd_check_vectorization(void)
{
    int regs[4];

    cpuid(regs, 1);

    if(getenv("LIBROMANO_VECTORIZATION") != NULL)
    {
        char* env_val = getenv("LIBROMANO_VECTORIZATION");

        if(strcmp(env_val, "0") == 0)
        {
            return;
        }
        else if(strcmp(env_val, "1") == 0)
        {
            _vectorization_mode = VectorizationMode_SSE;
        }
        else if(strcmp(env_val, "2") == 0)
        {
            _vectorization_mode = VectorizationMode_AVX;
        }
    }
    else
    {
        if(regs[2] & (1 << 28))
        {
            _vectorization_mode = VectorizationMode_AVX;
        }
        else if(regs[3] & (1 << 25))
        {
            _vectorization_mode = VectorizationMode_SSE;
        }
        else
        {
            return;
        }
    }
}

int simd_has_sse(void)
{
    return _vectorization_mode >= 1;
}

int simd_has_avx(void)
{
    return _vectorization_mode >= 2;
}

VectorizationMode simd_get_vectorization_mode(void)
{
    return _vectorization_mode;
}

void simd_force_vectorization_mode(const VectorizationMode mode)
{
    _vectorization_mode = mode;
}

const char* simd_get_vectorization_mode_as_string(VectorizationMode mode)
{
    switch(mode)
    {
        case VectorizationMode_Scalar: return "Scalar";
        case VectorizationMode_SSE: return "SSE";
        case VectorizationMode_AVX: return "AVX";
        case VectorizationMode_AVX2: return "AVX2";
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
        _vectorization_mode = VectorizationMode_NEON;
#elif defined(ROMANO_LINUX)
    unsigned long hwcap = getauxval(AT_HWCAP);
    _vectorization_mode = (hwcap & HWCAP_ASIMD) != 0;
#elif defined(ROMANO_WIN)
    _vectorization_mode = IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE);
#endif /* defined(ROMANO_APPLE) */
}

int simd_has_neon(void)
{
    return _vectorization_mode >= VectorizationMode_NEON;
}

VectorizationMode simd_get_vectorization_mode(void)
{
    return _vectorization_mode;
}

void simd_force_vectorization_mode(const VectorizationMode mode)
{
    _vectorization_mode = mode;
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
