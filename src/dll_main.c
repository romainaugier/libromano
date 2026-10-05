/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/common.h"
#include "libromano/simd.h"
#include "libromano/memory.h"
#include "libromano/cpu.h"

#include <stdio.h>

#if defined(ROMANO_WIN)
#include <Windows.h>
#endif /* defined(ROMANO_WIN) */

/*
   In this source file we execute all functions that need to be executed at runtime to check and
   set some global variables (for simd vectorization, cpu frequency for profiling...)

   lib_entry is executed on dlopen / LoadLibrary
   lib_exit is executed on dlclose / CloseLibrary
*/

void ROMANO_LIB_ENTRY lib_entry(void)
{
    cpu_check();
    simd_check_vectorization();
    mem_check_endianness();
}

void ROMANO_LIB_EXIT lib_exit(void)
{
}

#if defined(ROMANO_WIN)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    ROMANO_UNUSED(hModule);
    ROMANO_UNUSED(lpReserved);

    switch(ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            /* Code to run when the DLL is loaded */
            lib_entry();
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            /* Code to run when the DLL is unloaded */
            lib_exit();
            break;
    }

    return TRUE;
}
#endif /* defined(ROMANO_WIN) */
