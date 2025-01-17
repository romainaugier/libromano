/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/stack.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define GET_STACK_PTR(ptr) ((char*)(ptr) + 3 * sizeof(size_t))
#define GOLDEN_RATIO 1.61f
#define HEADER_SIZE 3

struct _Stack
{
    void* data;
};

Stack* stack_init(const size_t initial_capacity, const size_t element_size)
{
    Stack* new_stack;
    size_t capacity;

    new_stack = (Stack*)malloc(sizeof(Stack));

    capacity = initial_capacity == 0 ? 128 : initial_capacity;

    new_stack->data = calloc(HEADER_SIZE * sizeof(size_t) + capacity * element_size, sizeof(char));

    ((size_t*)new_stack->data)[0] = 0;
    ((size_t*)new_stack->data)[1] = capacity;
    ((size_t*)new_stack->data)[2] = element_size;

    return new_stack;
}

ROMANO_FORCE_INLINE size_t stack_get_size(Stack* stack)
{
    return ((size_t*)stack->data)[0];
}

ROMANO_FORCE_INLINE void stack_set_size(Stack* stack, const size_t size)
{
    ((size_t*)stack->data)[0] = size;
}

ROMANO_FORCE_INLINE void stack_increment_size(Stack* stack)
{
    ((size_t*)stack->data)[0]++;
}

ROMANO_FORCE_INLINE void stack_decrement_size(Stack* stack)
{
    ((size_t*)stack->data)[0]--;
}

ROMANO_FORCE_INLINE size_t stack_get_capacity(Stack* stack)
{
    return ((size_t*)stack->data)[1];
}

ROMANO_FORCE_INLINE void stack_set_capacity(Stack* stack, const size_t capacity)
{
    ((size_t*)stack->data)[1] = capacity;
}

ROMANO_FORCE_INLINE size_t stack_get_element_size(Stack* stack)
{
    return ((size_t*)stack->data)[2];
}

ROMANO_FORCE_INLINE void* stack_get_data_ptr(Stack* stack)
{
    return (void*)((char*)stack->data + HEADER_SIZE * sizeof(size_t));
}

void stack_grow(Stack* stack)
{
    size_t new_capacity;
    void* new_data_ptr;

    assert(stack != NULL && "stack_grow() failed: Stack is NULL");

    new_capacity = (size_t)((float)stack_get_capacity(stack) * GOLDEN_RATIO);

    new_data_ptr = realloc(stack->data, new_capacity + HEADER_SIZE * sizeof(size_t));

    stack->data = new_data_ptr;
    stack_set_capacity(stack, new_capacity);
}

void stack_push(Stack* stack, void* element)
{
    assert(stack != NULL && "stack_push() failed: Stack is NULL");

    stack_increment_size(stack);

    memcpy(stack_top(stack), element, stack_get_element_size(stack));
}

void* stack_top(Stack* stack)
{
    assert(stack != NULL && "stack_top() failed: Stack is NULL");
    assert(stack_get_size(stack) > 0 && "stack_top() failed: Stack is empty");

    return (void*)((char*)stack_get_data_ptr(stack) + (stack_get_size(stack) - 1) * stack_get_element_size(stack));
}

void stack_pop(Stack* stack, void* element)
{
    assert(stack != NULL && "stack_pop() failed: Stack is NULL");
    assert(stack_get_size(stack) > 0 && "stack_pop() failed: Stack is empty");

    memcpy(element, stack_top(stack), stack_get_element_size(stack));

    stack_decrement_size(stack);
}

void stack_free(Stack* stack)
{
    if(stack != NULL)
    {
        if(stack->data != NULL)
        {
            free(stack->data);
        }

        free(stack);
        stack = NULL;
    }
}
