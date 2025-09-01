/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_STACK_NO_ALLOC)
#define __LIBROMANO_STACK_NO_ALLOC

#include "libromano/common.h"

#define stacknoa_init(type, name, size) \
    type name[size];                 \
    type* name##_ptr = name;

#define stacknoa_is_empty(name) (name##_ptr == name)

#define stacknoa_is_full(name) ((name##_ptr - name) >= (sizeof(name)/sizeof(name[0])))

#define stacknoa_push(name, value) (*name##_ptr++ = (value))

#define stacknoa_pop(name) (*--name##_ptr)

#define stacknoa_top(name) (stacknoa_is_empty(name) ? NULL : (name##_ptr - 1))

#endif /* !defined(__LIBROMANO_STACK_NO_ALLOC) */