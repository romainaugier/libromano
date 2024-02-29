/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#if !defined(__LIBROMANO_PROFILING)
#define __LIBROMANO_PROFILING

#include "libromano/libromano.h"

#if defined(ROMANO_MSVC)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif /* defined(ROMANO_MSVC) */

ROMANO_CPP_ENTER

#if defined(ROMANO_ENABLE_PROFILING)

/* It's not really cpu cycles but well.. */
#define PROFILE(func) { uint64_t s = __rdtsc();                                                                           \
                      do { func; } while (0);                                                                             \
                      printf("%s at %s:%d -> %lld cpu cycles\n", #func, __FILE__, __LINE__, (uint64_t)(__rdtsc() - s)); } \

#else

#define PROFILE(func) func

#endif /* defined(ROMANO_ENABLE_PROFILING) */

ROMANO_CPP_END

#endif /* __LIBROMANO_PROFILING */
