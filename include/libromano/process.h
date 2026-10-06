/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_PROCESS)
#define __LIBROMANO_PROCESS

#include "libromano/common.h"

ROMANO_CPP_ENTER

typedef enum ProcessFlag {
    ProcessFlag_None = 0x0,
    ProcessFlag_CaptureStdout = 0x1,
    ProcessFlag_CaptureStderr = 0x2,
    ProcessFlag_MergeStderr = 0x4,
} ProcessFlag;

typedef struct ProcessOptions {
    /* NULL-terminated, argv[0] is searched in PATH when it has no separator */
    const char* const* argv;

    /* NULL to inherit the current working directory */
    const char* cwd;

    /* NULL-terminated array of "KEY=VALUE", NULL to inherit the current environment */
    const char* const* env;

    uint32_t flags;

    /*
     * Kills the process (and on POSIX its process group) after this many milliseconds, 0 to wait
     * forever. A process with a timeout runs in its own process group on POSIX
     */
    uint32_t timeout_ms;
} ProcessOptions;

typedef struct ProcessResult {
    int exit_code;

    /* Signal that terminated the process (POSIX only), 0 otherwise */
    int signal;

    /* Captured outputs, NULL-terminated, NULL when not captured */
    char* out;
    size_t out_sz;
    char* err;
    size_t err_sz;

    /* The process was killed because it exceeded timeout_ms */
    bool timed_out;
} ProcessResult;

/*
 * Runs a process and waits for its completion. Captured outputs are drained concurrently so the
 * child never blocks on a full pipe.
 * Returns false if the process could not be started (the error is set), true otherwise, whatever
 * its exit code.
 */
ROMANO_API bool process_run(const ProcessOptions* options, ProcessResult* result);

ROMANO_API void process_result_release(ProcessResult* result);

/* Returns the name of a POSIX signal ("SIGSEGV"), or "UNKNOWN" */
ROMANO_API const char* process_signal_name(int signal);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_PROCESS) */
