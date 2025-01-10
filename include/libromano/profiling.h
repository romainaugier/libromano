/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_PROFILING)
#define __LIBROMANO_PROFILING

#include "libromano/cpu.h"

#include <stdio.h>

#if defined(ROMANO_MSVC)
ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE uint64_t get_timestamp(void)
{
    return cpu_rdtsc();
}

ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE double get_elapsed_time(const uint64_t start, const double unit_multiplier) 
{
    return ((double)(cpu_rdtsc() - start) / ((double)cpu_get_frequency() * 1000000.0) * unit_multiplier);
}
#else
#if !defined(__USE_POSIX199309)
#define __USE_POSIX199309
#endif /* !defined(__USE_POSIX199309) */
#include <time.h>

ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE uint64_t get_timestamp(void)
{
    struct timespec s; 
    clock_gettime(CLOCK_MONOTONIC, &s); 
    return (uint64_t)s.tv_sec * 1000000000 + (uint64_t)s.tv_nsec;
}

ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE double get_elapsed_time(const uint64_t start, const double unit_multiplier)
{
    double elapsed = (get_timestamp() - start) * 1e-9;
    return elapsed * unit_multiplier;
}

#endif /* defined(ROMANO_MSVC) */

ROMANO_CPP_ENTER

#if defined(ROMANO_ENABLE_PROFILING)
/* Profiling measured in cpu cycles */
/* It's not 100% precise because we'd need the cpu frequency while measuring */
#define PROFILE(func) { uint64_t s = cpu_rdtsc();                                                                             \
                        do { func; } while (0);                                                                               \
                        printf("%s at %s:%d -> %zu cpu cycles\n", #func, __FILE__, __LINE__, (uint64_t)(cpu_rdtsc() - s)); }

#define SCOPED_PROFILE_START(name) const char* ___scp_##name = #name; uint64_t ___scp_##name##_start = cpu_rdtsc(); 
#define SCOPED_PROFILE_END(name) printf("Scoped profile \"%s\" -> %zu cpu cycles\n", ___scp_##name, (uint64_t)(cpu_rdtsc() - ___scp_##name##_start)); 

/* Profiling measured in nanoseconds */
#define PROFILE_NANOSECONDS(func) { uint64_t s = get_timestamp();                                                              \
                                     do { func; } while (0);                                                                   \
                                     printf("%s at %s:%d -> %3f µs\n", #func, __FILE__, __LINE__, get_elapsed_time(s, 1e9)); } \

#define SCOPED_PROFILE_START_NANOSECONDS(name) const char* ___scp_##name = #name; uint64_t ___scp_##name##_start = get_timestamp(); 
#define SCOPED_PROFILE_END_NANOSECONDS(name) printf("Scoped profile \"%s\" -> %3f ns\n", ___scp_##name, get_elapsed_time(___scp_##name##_start, 1e9)); 

/* Profiling measured in microseconds */
#define PROFILE_MICROSECONDS(func) { uint64_t s = get_timestamp();                                                             \
                                     do { func; } while (0);                                                                   \
                                     printf("%s at %s:%d -> %3f µs\n", #func, __FILE__, __LINE__, get_elapsed_time(s, 1e6)); } \

#define SCOPED_PROFILE_START_MICROSECONDS(name) const char* ___scp_##name = #name; uint64_t ___scp_##name##_start = get_timestamp(); 
#define SCOPED_PROFILE_END_MICROSECONDS(name) printf("Scoped profile \"%s\" -> %3f µs\n", ___scp_##name, get_elapsed_time(___scp_##name##_start, 1e6)); 

/* Profiling measured in milliseconds */
#define PROFILE_MILLISECONDS(func) { uint64_t s = get_timestamp();                                                             \
                                     do { func; } while (0);                                                                   \
                                     printf("%s at %s:%d -> %3f ms\n", #func, __FILE__, __LINE__, get_elapsed_time(s, 1e3)); } \

#define SCOPED_PROFILE_START_MILLISECONDS(name) const char* ___scp_##name = #name; uint64_t ___scp_##name##_start = get_timestamp(); 
#define SCOPED_PROFILE_END_MILLISECONDS(name) printf("Scoped profile \"%s\" -> %3f ms\n", ___scp_##name, get_elapsed_time(___scp_##name##_start, 1e3)); 

/* Profiling measured in seconds */
#define PROFILE_SECONDS(func) { uint64_t s = get_timestamp();                                                            \
                                do { func; } while (0);                                                                  \
                                printf("%s at %s:%d -> %3f s\n", #func, __FILE__, __LINE__, get_elapsed_time(s, 1.0)); } \

#define SCOPED_PROFILE_START_SECONDS(name) const char* ___scp_##name = #name; uint64_t ___scp_##name##_start = get_timestamp(); 
#define SCOPED_PROFILE_END_SECONDS(name) printf("Scoped profile \"%s\" -> %3f s\n", ___scp_##name, get_elapsed_time(___scp_##name##_start, 1.0)); 

/* Profiling mean time execution */
#define ___LERP_MEAN(X, Y, t) ((double)(X) + ((double)(Y) - (double)(X)) * (double)(t))

#define MEAN_PROFILE_INIT(name) const char* ___mean_##name = #name; double ___mean_accum_##name = 0; uint64_t ___mean_counter_##name = 0;
#define MEAN_PROFILE_START(name) uint64_t ___mean_start_##name = get_timestamp();
#define MEAN_PROFILE_STOP(name) const double ___time_##name = get_elapsed_time(___mean_start_##name, 1e9);             \
                                ___mean_counter_##name++;                                                              \
                                const double ___t_##name = 1.0 / (double)___mean_counter_##name;                       \
                                ___mean_accum_##name = ___LERP_MEAN(___mean_accum_##name, ___time_##name, ___t_##name); 
#define MEAN_PROFILE_RELEASE(name) printf("Mean profile \"%s\" -> %f ns\n", ___mean_##name, ___mean_accum_##name);

#define MEAN_PROFILE_CYCLES_INIT(name) const char* ___mean_##name = #name; double ___mean_accum_##name = 0; uint64_t ___mean_counter_##name = 0;
#define MEAN_PROFILE_CYCLES_START(name) uint64_t ___mean_start_##name = cpu_rdtsc();
#define MEAN_PROFILE_CYCLES_STOP(name) const double ___time_##name = (double)(cpu_rdtsc() - ___mean_start_##name);            \
                                       ___mean_counter_##name++;                                                              \
                                       const double ___t_##name = 1.0 / (double)___mean_counter_##name;                       \
                                       ___mean_accum_##name = ___LERP_MEAN(___mean_accum_##name, ___time_##name, ___t_##name); 
#define MEAN_PROFILE_CYCLES_RELEASE(name) printf("Mean profile \"%s\" -> %f cycles\n", ___mean_##name, ___mean_accum_##name);
#else
#define PROFILE(func) func
#define SCOPED_PROFILE_START(name)
#define SCOPED_PROFILE_END(name)

#define PROFILE_MICROSECONDS(func) func
#define SCOPED_PROFILE_START_MICROSECONDS(name)
#define SCOPED_PROFILE_END_MICROSECONDS(name)

#define PROFILE_MILLISECONDS(func) func
#define SCOPED_PROFILE_START_MILLISECONDS(name)
#define SCOPED_PROFILE_END_MILLISECONDS(name)

#define PROFILE_SECONDS(func) func
#define SCOPED_PROFILE_START_SECONDS(name)
#define SCOPED_PROFILE_END_SECONDS(name)

#define MEAN_PROFILE_INIT(name)
#define MEAN_PROFILE_START(name)
#define MEAN_PROFILE_STOP(name)
#define MEAN_PROFILE_RELEASE(name)
#endif /* defined(ROMANO_ENABLE_PROFILING) */

ROMANO_CPP_END

#endif /* __LIBROMANO_PROFILING */
