/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/time.h"

#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
#include <time.h>
#endif /* defined(ROMANO_LINUX) || defined(ROMANO_APPLE) */

#if defined(ROMANO_WIN)

void gettimeofday(timeval_t* tv, timezone_t* tz)
{
    static const ULONGLONG epoch_offset_us = 11644473600000000ULL;

    if(tv != NULL)
    {
        FILETIME filetime;
        ULARGE_INTEGER x;
        ULONGLONG usec;

#if _WIN32_WINNT >= _WIN32_WINNT_WIN8
        GetSystemTimePreciseAsFileTime(&filetime);
#else
        GetSystemTimeAsFileTime(&filetime);
#endif
        x.LowPart = filetime.dwLowDateTime;
        x.HighPart = filetime.dwHighDateTime;

        usec = x.QuadPart / 10 - epoch_offset_us;
        tv->tv_sec = (int32_t)(usec / 1000000ULL);
        tv->tv_usec = (int32_t)(usec % 1000000ULL);
    }
    if(tz != NULL)
    {
        TIME_ZONE_INFORMATION timezone;

        GetTimeZoneInformation(&timezone);
        tz->tz_minuteswest = timezone.Bias;
        tz->tz_dsttime = 0;
    }
}

#endif /* defined(ROMANO_WIN) */


uint64_t time_monotonic_ns(void)
{
#if defined(ROMANO_WIN)
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    if(frequency.QuadPart == 0)
        QueryPerformanceFrequency(&frequency);

    QueryPerformanceCounter(&counter);

    return (uint64_t)(counter.QuadPart / frequency.QuadPart) * 1000000000ULL +
           (uint64_t)(counter.QuadPart % frequency.QuadPart) * 1000000000ULL / (uint64_t)frequency.QuadPart;
#else
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif /* defined(ROMANO_WIN) */
}
