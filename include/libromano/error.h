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

typedef enum ErrorCode {
    ErrorCode_NoError = 0,

    /* Memory error */
    ErrorCode_MemAllocError = 0x0FFFFFFF, 
    ErrorCode_FormattingError, 
    ErrorCode_SizeOverflow, 
    ErrorCode_WrongIPAddressFormat, 

    /* Other error codes will be set from system error
     * For Windows see: 
     * https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499- 
     * Codes range from 0 (no error) to 15999
     * For Linux see:
     * https://en.wikipedia.org/wiki/Errno.h
     * Codes range from 0 (no error) to 143
     */

    /* Json errors */
    ErrorCode_JsonUnexpectedCharacter,
} ErrorCode;

/*
 * Returns the last error from library calls
 */
ROMANO_API ErrorCode error_get_last(void);

/*
 * Returns the last error from system calls (GetLastError, errno...)
 */
ROMANO_API int error_get_last_from_system(void);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_ERROR) */
