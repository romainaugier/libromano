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
#define cpuid(regs, mode) __cpuid(regs, mode)
#else
#define cpuid(regs, mode) asm volatile ("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3]) : "a" (mode), "c" (0))
#endif /* defined(ROMANO_MSVC) */

void cpu_check(void);

/* The string must be allocated before, and should be sizeof(int) * 12 + 1 (49 bytes) */
ROMANO_API void cpu_get_name(char* name);

/* Returns the frequency in MHz */
ROMANO_API uint32_t cpu_get_frequency(void);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_CPU) */