// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#pragma once

#if !defined(__LIBROMANO_TIME)
#define __LIBROMANO_TIME

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

#if defined(ROMANO_WIN)

#include <Windows.h>

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};   

struct timeval {
    time_t tv_sec;
    long tv_usec;
};

ROMANO_API int gettimeofday(struct timeval* tv, struct timezone *tz);

#elif defined(ROMANO_LINUX)
#include <sys/time.h>
#endif // defined(ROMANO_WIN)

ROMANO_CPP_END

#endif // !defined(__LIBROMANO_TIME)