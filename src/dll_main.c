/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/libromano.h"
#include "libromano/simd.h"
#include "libromano/memory.h"

#if defined(ROMANO_WIN)
#include <Windows.h>
#endif /* defined(ROMANO_WIN) */

/* 
   In this source file we execute all functions that need to be executed at runtime to check and
   set some global variables (for simd vectorization, cpu frequency for profiling...) 
*/

#if defined(ROMANO_WIN)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) 
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            /* Code to run when the DLL is loaded */
#if ROMANO_DEBUG
            printf("libromano dll entry\n");
#endif /* ROMANO_DEBUG */
            simd_check_vectorization();
            mem_check_endianness();
#if ROMANO_DEBUG
            printf("libromano vectorization mode: %u\n", simd_get_vectorization_mode());
            printf("libromano detected endianness: %u\n", mem_get_endianness());
#endif /* ROMANO_DEBUG */
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            /* Code to run when the DLL is unloaded */
#if ROMANO_DEBUG
            printf("libromano dll exit\n");
#endif /* ROMANO_DEBUG */
            break;
    }

    return TRUE;
}
#endif /* defined(ROMANO_WIN) */