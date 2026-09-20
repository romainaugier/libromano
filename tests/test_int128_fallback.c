/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/* Runs the int128 tests against the fallback implementation used where __int128 is not available */

#include "libromano/common.h"

#if defined(ROMANO_X86_64) && !defined(ROMANO_MSVC)

#define ROMANO_INT128_NO_NATIVE
#include "test_int128.c"

#else

#include "test.h"

static void test_fallback_unavailable(void)
{
    logger_log_info("the int128 fallback implementation is only testable on x86_64 with gcc/clang");
}

TEST_MAIN(
    TEST(test_fallback_unavailable),
)

#endif /* defined(ROMANO_X86_64) && !defined(ROMANO_MSVC) */
