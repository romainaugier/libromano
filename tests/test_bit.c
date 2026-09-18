/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/bit.h"
#include "libromano/logger.h"

int main(void)
{
    logger_init();
    logger_set_level(LogLevel_Debug);

    logger_log_info("Starting bit test");

    ROMANO_ASSERT(round_u32_to_next_pow2(1ul) == 2, "");
    ROMANO_ASSERT(round_u32_to_next_pow2(7ul) == 8, "");
    ROMANO_ASSERT(round_u32_to_next_pow2(1234ul) == 2048, "");
    ROMANO_ASSERT(round_u32_to_next_pow2(30000ul) == 32768, "");
    ROMANO_ASSERT(round_u32_to_next_pow2(42000ul) == 65536, "");

    ROMANO_ASSERT(round_u64_to_next_pow2(1ull) == 2, "");
    ROMANO_ASSERT(round_u64_to_next_pow2(7ull) == 8, "");
    ROMANO_ASSERT(round_u64_to_next_pow2(1234ull) == 2048, "");
    ROMANO_ASSERT(round_u64_to_next_pow2(30000ull) == 32768, "");
    ROMANO_ASSERT(round_u64_to_next_pow2(42000ull) == 65536, "");

    logger_log_info("Finished bit test");

    logger_release();

    return 0;
}