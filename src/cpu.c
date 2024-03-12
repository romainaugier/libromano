/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/cpu.h"

#include <string.h>


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
        _cpu_freq_mhz = 3000;
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