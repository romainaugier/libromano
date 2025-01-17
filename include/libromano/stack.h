/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_STACK)
#define __LIBROMANO_STACK

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

struct _Stack;

typedef struct _Stack Stack;

ROMANO_API Stack* stack_init(const size_t initial_capacity, const size_t element_size);

ROMANO_API void stack_push(Stack* stack, void* element);

ROMANO_API void* stack_top(Stack* stack);

ROMANO_API void stack_pop(Stack* stack, void* element);

ROMANO_API void stack_free(Stack* stack);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_STACK) */