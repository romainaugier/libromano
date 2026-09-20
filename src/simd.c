/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include <libromano/simd.h>
#include <libromano/cpu.h>

#include <string.h>

static int g_vectorization_mode = 0;
static int g_max_vectorization_mode = 0;

#if defined(ROMANO_X86_64)

void simd_check_vectorization(void)
{
    if(cpu_has_feature(CPUFeature_AVX512F))
        g_max_vectorization_mode = VectorizationMode_AVX512;
    else if(cpu_has_feature(CPUFeature_AVX2))
        g_max_vectorization_mode = VectorizationMode_AVX256;
    else if(cpu_has_feature(CPUFeature_AVX))
        g_max_vectorization_mode = VectorizationMode_AVX;
    else if(cpu_has_feature(CPUFeature_SSE4_2))
        g_max_vectorization_mode = VectorizationMode_SSE;
    else
        g_max_vectorization_mode = VectorizationMode_Scalar;

    if(getenv("LIBROMANO_VECTORIZATION") != NULL)
    {
        char* env_val = getenv("LIBROMANO_VECTORIZATION");

        if(strcmp(env_val, "0") == 0)
            g_vectorization_mode = VectorizationMode_Scalar;
        else if(strcmp(env_val, "1") == 0)
            g_vectorization_mode = VectorizationMode_SSE;
        else if(strcmp(env_val, "2") == 0)
            g_vectorization_mode = VectorizationMode_AVX;
        else if(strcmp(env_val, "3") == 0)
            g_vectorization_mode = VectorizationMode_AVX256;
        else if(strcmp(env_val, "4") == 0)
            g_vectorization_mode = VectorizationMode_AVX512;
        else
            g_vectorization_mode = VectorizationMode_Scalar;
    }
    else
    {
        g_vectorization_mode = g_max_vectorization_mode;
    }

    if(g_vectorization_mode > g_max_vectorization_mode)
        g_vectorization_mode = g_max_vectorization_mode;
}

int simd_has_sse(void)
{
    return g_max_vectorization_mode >= 1;
}

int simd_has_avx(void)
{
    return g_max_vectorization_mode >= 2;
}

int simd_has_avx256(void)
{
    return g_max_vectorization_mode >= 3;
}

int simd_has_avx512(void)
{
    return g_max_vectorization_mode >= 4;
}

VectorizationMode simd_get_vectorization_mode(void)
{
    return g_vectorization_mode;
}

void simd_force_vectorization_mode(const VectorizationMode mode)
{
    if(mode > g_max_vectorization_mode)
        return;

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
    g_max_vectorization_mode = cpu_has_feature(CPUFeature_NEON) ? VectorizationMode_NEON :
                                                                  VectorizationMode_Scalar;

    if(getenv("LIBROMANO_VECTORIZATION") != NULL)
    {
        char* env_val = getenv("LIBROMANO_VECTORIZATION");

        if(strcmp(env_val, "0") == 0)
            g_vectorization_mode = VectorizationMode_Scalar;
        else if(strcmp(env_val, "1") == 0)
            g_vectorization_mode = VectorizationMode_NEON;
        else
            g_vectorization_mode = VectorizationMode_Scalar;
    }
    else
    {
        g_vectorization_mode = g_max_vectorization_mode;
    }

    if(g_vectorization_mode > g_max_vectorization_mode)
        g_vectorization_mode = g_max_vectorization_mode;
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
    if(mode > g_max_vectorization_mode)
        return;

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
