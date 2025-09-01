/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once 

#if !defined(__LIBROMANO_ERROR)
#define __LIBROMANO_ERROR

#include "libromano/common.h"

#if defined(ROMANO_WIN)
#include <Windows.h>
#elif defined(ROMANO_LINUX)
#include <errno.h>
#endif /* defined(ROMANO_WIN) */

ROMANO_CPP_ENTER

static ROMANO_FORCE_INLINE int get_last_error(void)
{
#if defined(ROMANO_WIN)
    return GetLastError();
#elif defined(ROMANO_LINUX)
    return errno;
#endif /* defined(ROMANO_WIN) */
}

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_ERROR) */
