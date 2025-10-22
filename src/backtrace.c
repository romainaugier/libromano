/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/backtrace.h"
#include "libromano/memory.h"
#include "libromano/error.h"

#if defined(ROMANO_WIN)
#include <winnt.h>
#include <DbgHelp.h>
#endif /* defined(ROMANO_WIN) */

extern ErrorCode g_current_error;

#if defined(ROMANO_LINUX)
#if defined(ROMANO_GCC) || defined(ROMANO_CLANG)
static ROMANO_FORCE_INLINE uintptr_t* next_stack_frame(uintptr_t* stack_frame)
{
    uintptr_t* new_stack_frame;

    new_stack_frame = (uintptr_t*)(*stack_frame);

    if(new_stack_frame <= stack_frame)
        return NULL;

    if(new_stack_frame & (sizeof(uintptr_t) - 1))
        return NULL;

    return new_stack_frame;
}
#endif /* defined(ROMANO_GCC) || defined(ROMANO_CLANG) */
#endif /* defined(ROMANO_LINUX) */

uint32_t backtrace_call_stack(uint32_t skip, uint32_t max, uintptr_t* out_stack)
{
#if defined(ROMANO_LINUX)
#if defined(ROMANO_GCC) || defined(ROMANO_CLANG)
    uintptr_t* stack_frame;
    uint32_t num;

    stack_frame = (uintptr_t*)__builtin_frame_address(0);
    num = 0;

    while(stack_frame != NULL && num < max)
    {
        if(stackframe[1] == NULL)
            break;


        if(skip > 0)
            skip--;
        else
            out_stack[num++] = stack_frame[1];

        stack_frame = next_stack_frame(stack_frame);
    }

    return num;
#endif /* defined(ROMANO_GCC) || defined(ROMANO_CLANG) */
#elif defined(ROMANO_WIN)
    return RtlCaptureStackBackTrace(skip, max, (PVOID*)&out_stack, NULL);
#endif /* defined(ROMANO_LINUX) */

    return 0;
}

uint32_t backtrace_call_stack_symbols(uint32_t skip, uint32_t max, char** out_symbols)
{
    uintptr_t* out_stack;
    char* sym_name;
    uint32_t num;
    uint32_t i;

#if defined(ROMANO_WIN)
    HANDLE process;
    SYMBOL_INFO* symbol;

    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
#endif /* defined(ROMANO_WIN) */

    out_stack = mem_alloca(max * sizeof(uintptr_t));

    num = backtrace_call_stack(skip, max, out_stack);

#if defined(ROMANO_WIN)
    process = GetCurrentProcess();

    symbol = (SYMBOL_INFO*)buffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    SymInitialize(process, NULL, TRUE);

    for(i = 0; i < num; i++)
    {
        if(SymFromAddr(process, (DWORD64)out_stack[i], 0, symbol))
        {
            sym_name = calloc(strlen(symbol->Name) + 1, sizeof(char));

            if(sym_name == NULL)
            {
                g_current_error = ErrorCode_MemAllocError;
                return 0;
            }

            strcpy(sym_name, symbol->Name);

            out_symbols[i] = sym_name;
        }
        else
        {
            sym_name = calloc(8, sizeof(char));

            if(sym_name == NULL)
            {
                g_current_error = ErrorCode_MemAllocError;
                return 0;
            }

            strcpy(sym_name, "Unknown");

            out_symbols[i] = sym_name;
        }
    }

    SymCleanup(process);
#endif /* defined(ROMANO_WIN) */

    return num;
}
