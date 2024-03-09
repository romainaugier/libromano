/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/libromano.h"
#include "libromano/simd.h"

/* 
   In this source file we execute all functions that need to be executed at runtime to check and
   set some global variables (for simd vectorization, cpu frequency for profiling...) 
*/

#if defined(ROMANO_WIN)
#include <Windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) 
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            /* Code to run when the DLL is loaded */
            check_vectorization();
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            /* Code to run when the DLL is unloaded */
            break;
    }

    return TRUE;
}
#endif /* defined(ROMANO_WIN) */