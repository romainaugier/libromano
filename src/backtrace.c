/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/backtrace.h"
#include "libromano/memory.h"
#include "libromano/error.h"
#include "libromano/logger.h"

#if defined(ROMANO_WIN)
#include <winnt.h>
#include <DbgHelp.h>
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
#include <execinfo.h>
#include <unistd.h>
#include <signal.h>
#endif /* defined(ROMANO_WIN) */

#include <string.h>

extern ErrorCode g_current_error;

#define BACKTRACE_MAX_FRAMES 256

uint32_t backtrace_call_stack(uint32_t skip, uint32_t max, void** out_stack)
{
#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    /* Unwind tables work without frame pointers, unlike walking the frame chain */
    void* frames[BACKTRACE_MAX_FRAMES];
    uint32_t available;
    uint32_t num;

    /* Skips this function too */
    skip++;

    available = (uint32_t)backtrace(frames, BACKTRACE_MAX_FRAMES);

    if(available <= skip)
        return 0;

    num = available - skip < max ? available - skip : max;
    memcpy(out_stack, frames + skip, num * sizeof(void*));

    return num;
#elif defined(ROMANO_WIN)
    return RtlCaptureStackBackTrace(skip + 1, max, out_stack, NULL);
#else
    ROMANO_UNUSED(skip);
    ROMANO_UNUSED(max);
    ROMANO_UNUSED(out_stack);
    return 0;
#endif /* defined(ROMANO_LINUX) || defined(ROMANO_APPLE) */
}

uint32_t backtrace_call_stack_symbols(uint32_t skip,
                                      uint32_t max,
                                      char** out_symbols,
                                      void** out_addresses)
{
    char* sym_name;
    uint32_t num;
    uint32_t i;
    uint32_t j;

#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    char** symbols;

#elif defined(ROMANO_WIN)
    HANDLE process;
    PSYMBOL_INFO p_symbol;

    char buffer[sizeof(SYMBOL_INFO) + (MAX_SYM_NAME + 1) * sizeof(char)];

    char module_path[MAX_PATH];
    DWORD module_path_sz;
#endif /* defined(ROMANO_LINUX) */

    num = backtrace_call_stack(skip, max, out_addresses);

#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    symbols = backtrace_symbols((void* const*)out_addresses, (int)num);

    for(i = 0; i < num; i++)
    {
        sym_name = calloc(strlen(symbols[i]) + 1, sizeof(char));

        if(sym_name == NULL)
        {
            for(j = 0; j < i; j++)
            {
                free(out_symbols[j]);
            }

            g_current_error = ErrorCode_MemAllocError;
            return 0;
        }

        strcpy(sym_name, symbols[i]);

        out_symbols[i] = sym_name;
    }

    free(symbols);

#elif defined(ROMANO_WIN)
    process = GetCurrentProcess();

    p_symbol = (PSYMBOL_INFO)buffer;
    p_symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    p_symbol->MaxNameLen = MAX_SYM_NAME;

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

    memset(module_path, 0, MAX_PATH * sizeof(char));
    module_path_sz = GetModuleFileNameA(NULL, module_path, MAX_PATH);

    if(module_path_sz == 0)
    {
        g_current_error = error_get_last_from_system();

        logger_log_error("Error when calling GetModuleFileNameA (%d)", g_current_error);

        return 0;
    }

    while(module_path[module_path_sz - 1] != '\\')
    {
        module_path[module_path_sz - 1] = '\0';
        module_path_sz--;
    }

    module_path[module_path_sz - 1] = '\0';

    if(!SymInitialize(process, module_path, TRUE))
    {
        g_current_error = error_get_last_from_system();

        logger_log_error("Error when calling SymInitialize (%d)", g_current_error);

        return 0;
    }

    for(i = 0; i < num; i++)
    {
        if(SymFromAddr(process, (DWORD64)out_addresses[i], 0, p_symbol))
        {
            sym_name = calloc(p_symbol->NameLen + 1, sizeof(char));

            if(sym_name == NULL)
            {
                for(j = 0; j < i; j++)
                {
                    free(out_symbols[j]);
                }

                g_current_error = ErrorCode_MemAllocError;

                SymCleanup(process);

                return 0;
            }

            strncpy(sym_name, p_symbol->Name, p_symbol->NameLen);
            sym_name[p_symbol->NameLen] = '\0';

            out_symbols[i] = sym_name;
        }
        else
        {
            g_current_error = error_get_last_from_system();

            logger_log_error("Error when calling SymFromAddr (%d)", g_current_error);

            sym_name = calloc(8, sizeof(char));

            if(sym_name == NULL)
            {
                for(j = 0; j < i; j++)
                {
                    free(out_symbols[j]);
                }

                g_current_error = ErrorCode_MemAllocError;

                SymCleanup(process);

                return 0;
            }

            strcpy(sym_name, "Unknown");

            out_symbols[i] = sym_name;
        }
    }

    SymCleanup(process);
#endif /* defined(ROMANO_LINUX) */

    return num;
}

#define SIG_MAX_SYMBOLS 32

#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
/* Only async-signal-safe calls in here: no stdio, no malloc */
void backtrace_signal_handler(int sig)
{
    static const char header[] = "Exception caught: signal ";
    void* addresses[SIG_MAX_SYMBOLS];
    char number[16];
    size_t number_sz = 0;
    int value = sig;
    int count;

    do
    {
        number[sizeof(number) - 2 - number_sz++] = (char)('0' + value % 10);
        value /= 10;
    } while(value > 0 && number_sz < sizeof(number) - 2);

    number[sizeof(number) - 1] = '\n';

    if(write(STDERR_FILENO, header, sizeof(header) - 1) < 0 ||
       write(STDERR_FILENO, number + sizeof(number) - 1 - number_sz, number_sz + 1) < 0)
        _exit(1);

    count = backtrace(addresses, SIG_MAX_SYMBOLS);
    backtrace_symbols_fd(addresses, count, STDERR_FILENO);

    _exit(1);
}
#elif defined(ROMANO_WIN)
LONG backtrace_signal_handler(EXCEPTION_POINTERS* exception_info)
{
    void* addresses[SIG_MAX_SYMBOLS];
    char* symbols[SIG_MAX_SYMBOLS];
    uint32_t i;
    uint32_t num_symbols;

    fprintf(stderr, "Exception caught: 0x%llx\n", exception_info->ExceptionRecord->ExceptionCode);

    num_symbols = backtrace_call_stack_symbols(0, SIG_MAX_SYMBOLS, symbols, addresses);

    for(i = 0; i < num_symbols; i++)
    {
        fprintf(stderr,
                "#%u 0x%px in %s\n",
                i,
                ((uintptr_t**)addresses)[i],
                symbols[i]);

        free(symbols[i]);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif /* defined(ROMANO_LINUX) */

void backtrace_install_signal_handler(void)
{
#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
    void* preload[1];

    /* The first call can allocate while loading the unwinder, better here than in the handler */
    backtrace(preload, 1);

    signal(SIGSEGV, backtrace_signal_handler);
    signal(SIGFPE, backtrace_signal_handler);
    signal(SIGABRT, backtrace_signal_handler);
    signal(SIGILL, backtrace_signal_handler);
#elif defined(ROMANO_WIN)
    SetUnhandledExceptionFilter(backtrace_signal_handler);
#endif /* defined(ROMANO_LINUX) */
}
