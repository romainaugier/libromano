/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/time.h"

#if defined(ROMANO_WIN)

void gettimeofday(struct timeval* tv, struct timezone *tz)
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
        tv->tv_sec = (time_t)(usec / 1000000ULL);
        tv->tv_usec = (long)(usec % 1000000ULL);
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

