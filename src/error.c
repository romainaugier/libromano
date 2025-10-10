/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/error.h"

ErrorCode g_current_error = ErrorCode_NoError;

ErrorCode error_get_last()
{
    ErrorCode current_error = g_current_error;
    g_current_error = ErrorCode_NoError;
    return current_error;
}

int error_get_last_from_system(void)
{
#if defined(ROMANO_WIN)
    return (int)GetLastError();
#elif defined(ROMANO_LINUX)
    return errno;
#endif /* defined(ROMANO_WIN) */
}