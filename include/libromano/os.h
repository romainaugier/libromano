/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_OS)
#define __LIBROMANO_OS

#include "libromano/common.h"

ROMANO_CPP_ENTER

typedef enum OSKind {
    OSKind_Linux,
    OSKind_Windows,
    OSKind_MacOS,
    OSKind_BSD,
} OSKind;

typedef enum OSArch {
    OSArch_X86_64,
    OSArch_X86,
    OSArch_Aarch64,
    OSArch_Arm,
} OSArch;

ROMANO_API OSKind os_kind(void);

ROMANO_API OSArch os_arch(void);

/* "linux", "windows", "macos", "bsd" */
ROMANO_API const char* os_kind_str(OSKind kind);

/* "x86_64", "x86", "aarch64", "arm" */
ROMANO_API const char* os_arch_str(OSArch arch);

/*
 * The following functions write a null-terminated string to buffer and return false on failure
 * or if buffer is too small
 */
ROMANO_API bool os_version(char* buffer, size_t buffer_sz);

ROMANO_API bool os_hostname(char* buffer, size_t buffer_sz);

ROMANO_API bool os_exe_path(char* buffer, size_t buffer_sz);

ROMANO_API bool os_home_dir(char* buffer, size_t buffer_sz);

ROMANO_API bool os_temp_dir(char* buffer, size_t buffer_sz);

/* Physical memory in bytes, 0 if unknown */
ROMANO_API uint64_t os_memory_total(void);

ROMANO_API uint64_t os_memory_available(void);

ROMANO_API bool os_is_tty(int fd);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_OS) */
