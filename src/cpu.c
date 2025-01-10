/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/cpu.h"

#include <string.h>

#if defined(ROMANO_WIN)
#include <windows.h>
#include <powerbase.h>

typedef struct _PROCESSOR_POWER_INFORMATION {
   ULONG Number;
   ULONG MaxMhz;
   ULONG CurrentMhz;
   ULONG MhzLimit;
   ULONG MaxIdleState;
   ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;

#endif /* defined(ROMANO_WIN) */

static uint64_t _get_frequency_counter = 0;
static uint32_t _frequency = 0;

uint32_t _get_cpu_frequency(void)
{
#if defined(ROMANO_WIN)
    if(_get_frequency_counter % 100 == 0)
    {
        PROCESSOR_POWER_INFORMATION* ppi;
        LONG ret;
        size_t size;
        LPBYTE p_buffer = NULL;
        ULONG current;
        ULONG max;
        UINT num_cpus;
        SYSTEM_INFO system_info;
        system_info.dwNumberOfProcessors = 0;

        GetSystemInfo(&system_info);

        num_cpus = system_info.dwNumberOfProcessors == 0 ? 1 : system_info.dwNumberOfProcessors;

        size = num_cpus * sizeof(PROCESSOR_POWER_INFORMATION);
        p_buffer = (BYTE*)LocalAlloc(LPTR, size);

        if(!p_buffer)
        {
            return 0;
        }

        ret = CallNtPowerInformation(ProcessorInformation, NULL, 0, p_buffer, size);

        if(ret != 0)
        {
            return 0;
        }

        ppi = (PROCESSOR_POWER_INFORMATION*)p_buffer;
        current = ppi->CurrentMhz;

        LocalFree(p_buffer);

        _frequency = (uint32_t)current;
    }

    _get_frequency_counter++;

    return _frequency;
#endif /* defined(ROMANO_WIN) */

    return 0;
}

static uint32_t _cpu_freq_mhz = 0;

void cpu_check(void)
{
    /* CPU Frequency */
    int regs[4];
    memset(regs, 0, 4 * sizeof(int));

    cpuid(regs, 0);

    if(regs[0] >= 0x16)
    {
        cpuid(regs, 0x16);
        _cpu_freq_mhz = regs[0];
    }
    else
    {
        _cpu_freq_mhz = _get_cpu_frequency();
    }
}

void cpu_get_name(char* name)
{
    uint32_t regs[12];

    cpuid(&regs[0], 0x80000000);

    if(regs[0] < 0x80000004)
    {
        name[0] = '\0';
        return;
    }

    cpuid(&regs[0], 0x80000002);
    cpuid(&regs[4], 0x80000003);
    cpuid(&regs[8], 0x80000004);

    memcpy(name, regs, 12 * sizeof(uint32_t));

    name[12 * sizeof(uint32_t)] = '\0';
}

uint32_t cpu_get_frequency(void)
{
    return _cpu_freq_mhz;
}

uint32_t cpu_get_current_frequency(void)
{
    return _get_cpu_frequency();
}

uint64_t cpu_rdtsc(void)
{
    return __rdtsc();
}