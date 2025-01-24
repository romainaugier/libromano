/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_PROFILING)
#define __LIBROMANO_PROFILING

#include "libromano/cpu.h"

#include <stdio.h>
#if !defined(__USE_POSIX199309)
#define __USE_POSIX199309
#endif
#include <time.h>

ROMANO_CPP_ENTER

/*
    This profiling code should be able to measure near nanosecond precision, using cycles and cpu frequency
    The cpu frequency (see in cpu.c) is refreshed every 10000 calls, to ensure that the profiler is following 
    the cpu frequency regime properly.
*/

ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE uint64_t get_timestamp(void)
{
    return cpu_rdtsc();
}

ROMANO_STATIC_FUNCTION ROMANO_FORCE_INLINE double get_elapsed_time(const uint64_t start, const double unit_multiplier)
{
    const uint64_t end = cpu_rdtsc();
    const uint32_t freq = cpu_get_current_frequency();

    return ((double)(end - start) / ((double)freq * 1000000.0) * unit_multiplier);
}

#if defined(ROMANO_ENABLE_PROFILING)
/* Profiling measured in cpu cycles */
#define PROFILE_CYCLES(func) { uint64_t s = cpu_rdtsc();                                                                      \
                        do { func; } while (0);                                                                               \
                        printf("%s at %s:%d -> %zu cpu cycles\n", #func, __FILE__, __LINE__, (uint64_t)(cpu_rdtsc() - s)); }

#define SCOPED_PROFILE_CYCLES_START(name) const char* ___scp_##name = #name; uint64_t ___scp_##name##_start = cpu_rdtsc(); 
#define SCOPED_PROFILE_CYCLES_END(name) printf("Scoped profile \"%s\" -> %zu cpu cycles\n", ___scp_##name, (uint64_t)(cpu_rdtsc() - ___scp_##name##_start)); 

/* Profiling measured in nanoseconds */
#define PROFILE_NS(func) { uint64_t s = get_timestamp();                                                                      \
                                    do { func; } while (0);                                                                   \
                                    printf("%s at %s:%d -> %3f ns\n", #func, __FILE__, __LINE__, get_elapsed_time(s, 1e9)); } \

#define SCOPED_PROFILE_NS_START(name) const char* ___scp_##name = #name; uint64_t ___scp_##name##_start = get_timestamp(); 
#define SCOPED_PROFILE_NS_END(name) printf("Scoped profile \"%s\" -> %3f ns\n", ___scp_##name, get_elapsed_time(___scp_##name##_start, 1e9)); 

/* Profiling measured in microseconds */
#define PROFILE_US(func) { uint64_t s = get_timestamp();                                                                       \
                                     do { func; } while (0);                                                                   \
                                     printf("%s at %s:%d -> %3f µs\n", #func, __FILE__, __LINE__, get_elapsed_time(s, 1e6)); } \

#define SCOPED_PROFILE_US_START(name) const char* ___scp_##name = #name; uint64_t ___scp_##name##_start = get_timestamp(); 
#define SCOPED_PROFILE_US_END(name) printf("Scoped profile \"%s\" -> %3f µs\n", ___scp_##name, get_elapsed_time(___scp_##name##_start, 1e6)); 

/* Profiling measured in milliseconds */
#define PROFILE_MS(func) { uint64_t s = get_timestamp();                                                                       \
                                     do { func; } while (0);                                                                   \
                                     printf("%s at %s:%d -> %3f ms\n", #func, __FILE__, __LINE__, get_elapsed_time(s, 1e3)); } \

#define SCOPED_PROFILE_MS_START(name) const char* ___scp_##name = #name; uint64_t ___scp_##name##_start = get_timestamp(); 
#define SCOPED_PROFILE_MS_END(name) printf("Scoped profile \"%s\" -> %3f ms\n", ___scp_##name, get_elapsed_time(___scp_##name##_start, 1e3)); 

/* Profiling measured in seconds */
#define PROFILE_S(func) { uint64_t s = get_timestamp();                                                                  \
                                do { func; } while (0);                                                                  \
                                printf("%s at %s:%d -> %3f s\n", #func, __FILE__, __LINE__, get_elapsed_time(s, 1.0)); } \

#define SCOPED_PROFILE_S_START(name) const char* ___scp_##name = #name; uint64_t ___scp_##name##_start = get_timestamp(); 
#define SCOPED_PROFILE_S_END(name) printf("Scoped profile \"%s\" -> %3f s\n", ___scp_##name, get_elapsed_time(___scp_##name##_start, 1.0)); 

/* Profiling mean execution time. We use an incremental mean using linear interpolation to calculate the mean execution time */
#define ___LERP_MEAN(X, Y, t) ((double)(X) + ((double)(Y) - (double)(X)) * (double)(t))

/* Profiling mean execution time measured in cpu cycles */
#define MEAN_PROFILE_CYCLES_INIT(name) const char* ___mean_##name = #name; double ___mean_accum_##name = 0; uint64_t ___mean_counter_##name = 0;
#define MEAN_PROFILE_CYCLES_START(name) uint64_t ___mean_start_##name = cpu_rdtsc();
#define MEAN_PROFILE_CYCLES_STOP(name) const double ___time_##name = (double)(cpu_rdtsc() - ___mean_start_##name);            \
                                       ___mean_counter_##name++;                                                              \
                                       const double ___t_##name = 1.0 / (double)___mean_counter_##name;                       \
                                       ___mean_accum_##name = ___LERP_MEAN(___mean_accum_##name, ___time_##name, ___t_##name); 
#define MEAN_PROFILE_CYCLES_RELEASE(name) printf("Mean profile \"%s\" -> %f cycles\n", ___mean_##name, ___mean_accum_##name);

/* Profiling mean execution time measured in nanoseconds */
#define MEAN_PROFILE_CYCLES_INIT(name) const char* ___mean_##name = #name; double ___mean_accum_##name = 0; uint64_t ___mean_counter_##name = 0;
#define MEAN_PROFILE_NS_INIT(name) const char* ___mean_##name = #name; double ___mean_accum_##name = 0; uint64_t ___mean_counter_##name = 0;
#define MEAN_PROFILE_NS_START(name) uint64_t ___mean_start_##name = get_timestamp();
#define MEAN_PROFILE_NS_STOP(name) const double ___time_##name = get_elapsed_time(___mean_start_##name, 1e9);           \
                                ___mean_counter_##name++;                                                               \
                                const double ___t_##name = 1.0 / (double)___mean_counter_##name;                        \
                                ___mean_accum_##name = ___LERP_MEAN(___mean_accum_##name, ___time_##name, ___t_##name); 
#define MEAN_PROFILE_NS_RELEASE(name) printf("Mean profile \"%s\" -> %f ns\n", ___mean_##name, ___mean_accum_##name);

/* Profiling mean execution time measured in microseconds */
#define MEAN_PROFILE_US_INIT(name) const char* ___mean_##name = #name; double ___mean_accum_##name = 0; uint64_t ___mean_counter_##name = 0;
#define MEAN_PROFILE_US_START(name) uint64_t ___mean_start_##name = get_timestamp();
#define MEAN_PROFILE_US_STOP(name) const double ___time_##name = get_elapsed_time(___mean_start_##name, 1e6);           \
                                ___mean_counter_##name++;                                                               \
                                const double ___t_##name = 1.0 / (double)___mean_counter_##name;                        \
                                ___mean_accum_##name = ___LERP_MEAN(___mean_accum_##name, ___time_##name, ___t_##name); 
#define MEAN_PROFILE_US_RELEASE(name) printf("Mean profile \"%s\" -> %f µs\n", ___mean_##name, ___mean_accum_##name);

/* Profiling mean execution time measured in milliseconds */
#define MEAN_PROFILE_MS_INIT(name) const char* ___mean_##name = #name; double ___mean_accum_##name = 0; uint64_t ___mean_counter_##name = 0;
#define MEAN_PROFILE_MS_START(name) uint64_t ___mean_start_##name = get_timestamp();
#define MEAN_PROFILE_MS_STOP(name) const double ___time_##name = get_elapsed_time(___mean_start_##name, 1e3);           \
                                ___mean_counter_##name++;                                                               \
                                const double ___t_##name = 1.0 / (double)___mean_counter_##name;                        \
                                ___mean_accum_##name = ___LERP_MEAN(___mean_accum_##name, ___time_##name, ___t_##name); 
#define MEAN_PROFILE_MS_RELEASE(name) printf("Mean profile \"%s\" -> %f ms\n", ___mean_##name, ___mean_accum_##name);

/* Profiling mean execution time measured in seconds */
#define MEAN_PROFILE_S_INIT(name) const char* ___mean_##name = #name; double ___mean_accum_##name = 0; uint64_t ___mean_counter_##name = 0;
#define MEAN_PROFILE_S_START(name) uint64_t ___mean_start_##name = get_timestamp();
#define MEAN_PROFILE_S_STOP(name) const double ___time_##name = get_elapsed_time(___mean_start_##name, 1);              \
                                ___mean_counter_##name++;                                                               \
                                const double ___t_##name = 1.0 / (double)___mean_counter_##name;                        \
                                ___mean_accum_##name = ___LERP_MEAN(___mean_accum_##name, ___time_##name, ___t_##name); 
#define MEAN_PROFILE_S_RELEASE(name) printf("Mean profile \"%s\" -> %f s\n", ___mean_##name, ___mean_accum_##name);
#else
#define PROFILE_CYCLES(func) func
#define SCOPED_PROFILE_CYCLES_START(name)
#define SCOPED_PROFILE_CYCLES_END(name)

#define PROFILE_NS(func) func 
#define SCOPED_PROFILE_NS_START(name)
#define SCOPED_PROFILE_NS_END(name)

#define PROFILE_US(func) func
#define SCOPED_PROFILE_US_START(name)
#define SCOPED_PROFILE_US_END(name)

#define PROFILE_MS(func) func
#define SCOPED_PROFILE_MS_START(name)
#define SCOPED_PROFILE_MS_END(name)

#define PROFILE_S(func) func
#define SCOPED_PROFILE_S_START(name)
#define SCOPED_PROFILE_S_END(name)

#define MEAN_PROFILE_CYCLES_INIT(name)
#define MEAN_PROFILE_CYCLES_START(name)
#define MEAN_PROFILE_CYCLES_STOP(name)
#define MEAN_PROFILE_CYCLES_RELEASE(name)

#define MEAN_PROFILE_NS_INIT(name)
#define MEAN_PROFILE_NS_START(name)
#define MEAN_PROFILE_NS_STOP(name)
#define MEAN_PROFILE_NS_RELEASE(name)

#define MEAN_PROFILE_US_INIT(name)
#define MEAN_PROFILE_US_START(name)
#define MEAN_PROFILE_US_STOP(name)
#define MEAN_PROFILE_US_RELEASE(name)

#define MEAN_PROFILE_MS_INIT(name)
#define MEAN_PROFILE_MS_START(name)
#define MEAN_PROFILE_MS_STOP(name)
#define MEAN_PROFILE_MS_RELEASE(name)

#define MEAN_PROFILE_S_INIT(name)
#define MEAN_PROFILE_S_START(name)
#define MEAN_PROFILE_S_STOP(name)
#define MEAN_PROFILE_S_RELEASE(name)
#endif /* defined(ROMANO_ENABLE_PROFILING) */

ROMANO_CPP_END

#endif /* __LIBROMANO_PROFILING */
