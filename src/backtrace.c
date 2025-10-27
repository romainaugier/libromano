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
#elif defined(ROMANO_LINUX)
#include <execinfo.h>
#include <unistd.h>
#include <signal.h>
#endif /* defined(ROMANO_WIN) */

#include <string.h>

extern ErrorCode g_current_error;

#if defined(ROMANO_LINUX)
#if defined(ROMANO_GCC) || defined(ROMANO_CLANG)
static ROMANO_FORCE_INLINE uintptr_t* next_stack_frame(uintptr_t* stack_frame)
{
    uintptr_t* new_stack_frame;

    new_stack_frame = (uintptr_t*)(*stack_frame);

    if(new_stack_frame <= stack_frame)
        return NULL;

    if((uintptr_t)new_stack_frame & (sizeof(uintptr_t) - 1))
        return NULL;

    return new_stack_frame;
}
#endif /* defined(ROMANO_GCC) || defined(ROMANO_CLANG) */
#endif /* defined(ROMANO_LINUX) */

uint32_t backtrace_call_stack(uint32_t skip, uint32_t max, void** out_stack)
{
#if defined(ROMANO_LINUX)
#if defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    void* stack_frame;
    uint32_t num;

    stack_frame = (void*)__builtin_frame_address(0);
    num = 0;

    while(stack_frame != NULL && num < max)
    {
        if(((uintptr_t**)stack_frame)[1] == NULL)
            break;


        if(skip > 0)
            skip--;
        else
            out_stack[num++] = (void*)((uintptr_t**)stack_frame)[1];

        stack_frame = next_stack_frame((uintptr_t*)stack_frame);
    }

    return num;
#endif /* defined(ROMANO_GCC) || defined(ROMANO_CLANG) */
#elif defined(ROMANO_WIN)
    return RtlCaptureStackBackTrace(skip, max, out_stack, NULL);
#endif /* defined(ROMANO_LINUX) */

    return 0;
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

#if defined(ROMANO_LINUX)
    char** symbols;

#elif defined(ROMANO_WIN)
    HANDLE process;
    PSYMBOL_INFO p_symbol;

    char buffer[sizeof(SYMBOL_INFO) + (MAX_SYM_NAME + 1) * sizeof(char)];

    char module_path[MAX_PATH];
    DWORD module_path_sz;
#endif /* defined(ROMANO_LINUX) */

    num = backtrace_call_stack(skip, max, out_addresses);

#if defined(ROMANO_LINUX)
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

#if defined(ROMANO_LINUX)
void backtrace_signal_handler(int sig)
{
    void* addresses[SIG_MAX_SYMBOLS];
    char* symbols[SIG_MAX_SYMBOLS];
    uint32_t i;
    uint32_t num_symbols;

    fprintf(stderr, "Exception caught: %s\n", strsignal(sig));

    num_symbols = backtrace_call_stack_symbols(0, SIG_MAX_SYMBOLS, symbols, addresses);

    for(i = 0; i < num_symbols; i++)
    {
        fprintf(stderr,
                "#%u 0x%px in %s\n",
                i,
                (void*)((uintptr_t**)addresses)[i],
                symbols[i]);

        free(symbols[i]);
    }

    exit(1);
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

void backtrace_install_signal_handler()
{
#if defined(ROMANO_LINUX)
    signal(SIGSEGV, backtrace_signal_handler);
    signal(SIGFPE, backtrace_signal_handler);
    signal(SIGABRT, backtrace_signal_handler);
    signal(SIGILL, backtrace_signal_handler);
#elif defined(ROMANO_WIN)
    SetUnhandledExceptionFilter(backtrace_signal_handler);
#endif /* defined(ROMANO_LINUX) */
}
