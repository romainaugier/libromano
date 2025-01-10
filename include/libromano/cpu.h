/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_CPU)
#define __LIBROMANO_CPU

#include "libromano/libromano.h"

#include <stdlib.h>

ROMANO_CPP_ENTER

#if defined(ROMANO_MSVC)
#include <intrin.h>
#define cpuid(__regs, __mode) __cpuid(__regs, __mode)
#else
#define cpuid(__regs, __mode) asm volatile ("cpuid" : "=a" ((__regs)[0]), "=b" ((__regs)[1]), "=c" ((__regs)[2]), "=d" ((__regs)[3]) : "a" (__mode), "c" (0))
#endif /* defined(ROMANO_MSVC) */

/* The string must be allocated before, and should be sizeof(int) * 12 + 1 (49 bytes) */
ROMANO_API void cpu_get_name(char* name);

/* Returns the cpu frequency in MHz, found during library initialization (via cpuid or system calls) */
ROMANO_API uint32_t cpu_get_frequency(void);

/* Returns the current cpu frequency in MHz (via system calls) */
ROMANO_API uint32_t cpu_get_current_frequency(void);

ROMANO_API uint64_t cpu_rdtsc(void);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_CPU) */