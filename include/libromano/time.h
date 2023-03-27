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

struct timeval {
    time_t tv_sec;
    long tv_usec;
};

ROMANO_API void gettimeofday(struct timeval* tv, struct timezone *tz);
#endif /* defined(ROMANO_WIN) */

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_TIME) */

