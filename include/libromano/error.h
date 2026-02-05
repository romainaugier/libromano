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
    ErrorCode_JsonExpectedKey,
    ErrorCode_JsonExpectedColon,

    /* Regex errors */
    ErrorCode_RegexUnexpectedCharacter,
    ErrorCode_RegexInvalidCharacterRange,
    ErrorCode_RegexUnexpectedEndOfExpression,
    ErrorCode_RegexMismatchedParentheses,
    ErrorCode_RegexInvalidOperator,
    ErrorCode_RegexInvalidToken,
    ErrorCode_RegexUnexpectedTokens,
    ErrorCode_RegexUnknownOpCode,

    /* Socket/HTTP/DNS errors */
    ErrorCode_HTTPInvalidRequest,
    ErrorCode_DNSCantFindHost,
    ErrorCode_HTTPContextNotAlive,

    /* CLI Parser errors */
    ErrorCode_CLIMalformedArgument,
    ErrorCode_CLIUnknownArgument,
    ErrorCode_CLIInvalidArgumentType,
    ErrorCode_CLIInvalidArgumentAction,
    ErrorCode_CLITooManyPositionalArgs,
    ErrorCode_CLIMissingPositionalArgs,
    ErrorCode_CLIMissingRequiredArg,
} ErrorCode;

/* Reserved for internal use only */
#if defined(__ROMANO_DEFINE_ERROR_EXTERNS)
extern ErrorCode g_current_error;
extern void error_set_context(const char* context, size_t context_sz);
#endif /* defined(__ROMANO_DEFINE_ERROR_EXTERNS) */

/*
 * Returns the last error from library calls
 */
ROMANO_API ErrorCode error_get_last(void);

/*
 * Returns the error as a string
 */
ROMANO_API const char* error_str(ErrorCode code);

/*
 * Returns the context error string (to get more information)
 * If not set, returns NULL
 */
ROMANO_API const char* error_context_str(void);

/*
 * Returns the last error from library calls as string
 */
ROMANO_API const char* error_str_get_last(void);

/*
 * Returns the last error from system calls (GetLastError, errno...)
 */
ROMANO_API int error_get_last_from_system(void);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_ERROR) */
