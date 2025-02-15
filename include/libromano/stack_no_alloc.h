/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_STACK_NO_ALLOC)
#define __LIBROMANO_STACK_NO_ALLOC

#include "libromano/libromano.h"

#define stack_init(type, name, size) \
    type name[size];                 \
    type* name##_ptr = name;

#define stack_is_empty(name) (name##_ptr == name)

#define stack_is_full(name) ((name##_ptr - name) >= (sizeof(name)/sizeof(name[0])))

#define stack_push(name, value) (*name##_ptr++ = (value))

#define stack_pop(name) (*--name##_ptr)

#define stack_top(name) (stack_is_empty(name) ? NULL : (name##_ptr - 1))

#endif /* !defined(__LIBROMANO_STACK_NO_ALLOC) */