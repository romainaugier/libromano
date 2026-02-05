/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/error.h"

#include <string.h>

ErrorCode g_current_error = ErrorCode_NoError;

size_t g_context_cap = 255;
size_t g_context_sz = 0;
char g_context[256];

void error_set_context(const char* context, size_t context_sz)
{
    if(context_sz == 0)
        context_sz = strlen(context);

    if(context_sz > g_context_cap)
        context_sz = g_context_cap;

    memset(g_context, 0, (g_context_cap + 1) * sizeof(char));

    memcpy(g_context, context, context_sz * sizeof(char));

    g_context_sz = context_sz;
}

ErrorCode error_get_last(void)
{
    ErrorCode current_error = g_current_error;
    g_current_error = ErrorCode_NoError;
    return current_error;
}

const char *error_str(ErrorCode code)
{
    switch(code)
    {
        case ErrorCode_NoError:
            return "No error";
        case ErrorCode_MemAllocError:
            return "Memory allocation error";
        case ErrorCode_FormattingError:
            return "Formatting error";
        case ErrorCode_SizeOverflow:
            return "Size overflow";
        case ErrorCode_WrongIPAddressFormat:
            return "Wrong IP address format";
        case ErrorCode_JsonUnexpectedCharacter:
            return "Json: unexpected character";
        case ErrorCode_JsonExpectedKey:
            return "Json: expected a key";
        case ErrorCode_JsonExpectedColon:
            return "Json: expected colon";
        case ErrorCode_RegexUnexpectedCharacter:
            return "Regex: unexpected character";
        case ErrorCode_RegexInvalidCharacterRange:
            return "Regex: invalid character range";
        case ErrorCode_RegexUnexpectedEndOfExpression:
            return "Regex: unexpected end of expression";
        case ErrorCode_RegexMismatchedParentheses:
            return "Regex: mismatched parentheses";
        case ErrorCode_RegexInvalidOperator:
            return "Regex: invalid operator";
        case ErrorCode_RegexInvalidToken:
            return "Regex: invalid token";
        case ErrorCode_RegexUnexpectedTokens:
            return "Regex: unexpected tokens";
        case ErrorCode_RegexUnknownOpCode:
            return "Regex: unknown op code";
        case ErrorCode_HTTPInvalidRequest:
            return "HTTP: invalid request";
        case ErrorCode_DNSCantFindHost:
            return "DNS: cant find host";
        case ErrorCode_HTTPContextNotAlive:
            return "HTTP: context is not alive";
        case ErrorCode_CLIMalformedArgument:
            return "CLI: malformed argument";
        case ErrorCode_CLIUnknownArgument:
            return "CLI: unknown argument";
        case ErrorCode_CLIInvalidArgumentType:
            return "CLI: invalid argument type";
        case ErrorCode_CLIInvalidArgumentAction:
            return "CLI: invalid argument action";
        case ErrorCode_CLITooManyPositionalArgs:
            return "CLI: too many positional args";
        case ErrorCode_CLIMissingPositionalArgs:
            return "CLI: missing positional args";
        case ErrorCode_CLIMissingRequiredArg:
            return "CLI: missing required arg";
        default:
            return "Unknown error";
    }
}

const char* error_str_get_last(void)
{
    return error_str(error_get_last());
}

const char* error_context_str(void)
{
    if(g_context_sz == 0)
        return NULL;

    return (const char*)g_context;
}

int error_get_last_from_system(void)
{
#if defined(ROMANO_WIN)
    return (int)GetLastError();
#elif defined(ROMANO_LINUX)
    return errno;
#endif /* defined(ROMANO_WIN) */
}
