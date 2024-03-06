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

#define SCOPED_PROFILE_START(name) { const char* ___scp_name = name; uint64_t ___scp_start = __rdtsc(); \

#define SCOPED_PROFILE_END printf("Scoped profile \"%s\" -> %lld cpu cycles\n", ___scp_name, (uint64_t)(__rdtsc() - ___scp_start)); } \

#else

#define PROFILE(func) func
#define SCOPED_PROFILE_START(name)
#define SCOPED_PROFILE_END

#endif /* defined(ROMANO_ENABLE_PROFILING) */

ROMANO_CPP_END

#endif /* __LIBROMANO_PROFILING */
