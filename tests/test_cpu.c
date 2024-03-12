/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/cpu.h"
#include "libromano/logger.h"


int main(void)
{
    logger_init();

    char cpu_name[49];

    cpu_get_name(cpu_name);

    logger_log(LogLevel_Info, "CPU Name: %s", cpu_name);

    logger_release();

    return 0;
}