/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

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

typedef struct timezone timezone_t;

#if !defined(ROMANO_HAS_WINSOCK)

struct timeval {
    int32_t tv_sec;
    int32_t tv_usec;
};

typedef struct timeval timeval_t;

#else

typedef TIMEVAL timeval_t;

#endif /* defined(TIMEVAL) */

ROMANO_API void gettimeofday(timeval_t* tv, timezone_t* tz);

#elif defined(ROMANO_LINUX)
#include <sys/time.h>

typedef struct timezone timezone_t;
typedef struct timeval timeval_t;

#endif /* defined(ROMANO_WIN) */

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_TIME) */
