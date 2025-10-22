/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_BACKTRACE)
#define __LIBROMANO_BACKTRACE

#include "libromano/common.h"

ROMANO_CPP_ENTER

/*
 * Captures the current callstack
 * Returns the number of frames captured
 */
ROMANO_API uint32_t backtrace_call_stack(uint32_t skip, uint32_t max, uintptr_t* out_stack);

/*
 * Captures the current callstack as symbol names
 * out_symbols will be filled with malloced null-terminated strings containing
 * the function name for each frame
 * Returns the number of frames captured
 */
ROMANO_API uint32_t backtrace_call_stack_symbols(uint32_t skip, uint32_t max, char** out_symbols);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_BACKTRACE) */
