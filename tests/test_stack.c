/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/stack.h"

#include <stdio.h>

void test_init() 
{
    Stack* stack = stack_init(10, sizeof(int));
    ROMANO_ASSERT(stack != NULL, "stack should not be null");
    ROMANO_ASSERT(stack_size(stack) == 0, "stack size should be 0 after initialization");
    stack_free(stack);
    printf("test_init passed\n");
}

void test_push_top() 
{
    Stack* stack = stack_init(2, sizeof(int));
    int element = 42;
    stack_push(stack, &element);
    ROMANO_ASSERT(stack_size(stack) == 1, "stack size should be 1");
    int* top = (int*)stack_top(stack);
    ROMANO_ASSERT(top != NULL && *top == 42, "stack top should not be null");
    stack_free(stack);
    printf("test_push_top passed\n");
}

void test_pop() 
{
    Stack* stack = stack_init(3, sizeof(int));
    int values[] = {10, 20, 30};
    for (int i = 0; i < 3; ++i) {
        stack_push(stack, &values[i]);
    }
    int popped;
    stack_pop(stack, &popped);
    ROMANO_ASSERT(popped == 30 && stack_size(stack) == 2, "");
    stack_pop(stack, &popped);
    ROMANO_ASSERT(popped == 20 && stack_size(stack) == 1, "");
    stack_pop(stack, &popped);
    ROMANO_ASSERT(popped == 10 && stack_size(stack) == 0, "");
    stack_free(stack);
    printf("test_pop passed\n");
}

void test_empty_top() 
{
    Stack* stack = stack_init(5, sizeof(int));
    ROMANO_ASSERT(stack_top(stack) == NULL, "");
    stack_free(stack);
    printf("test_empty_top passed\n");
}

void test_struct_element() 
{
    typedef struct { int a; char b; } TestStruct;
    TestStruct ts = {5, 'x'};
    Stack* stack = stack_init(1, sizeof(TestStruct));
    stack_push(stack, &ts);
    TestStruct* top = (TestStruct*)stack_top(stack);
    ROMANO_ASSERT(top->a == 5 && top->b == 'x', "");
    TestStruct popped;
    stack_pop(stack, &popped);
    ROMANO_ASSERT(popped.a == ts.a && popped.b == ts.b, "");
    stack_free(stack);
    printf("test_struct_element passed\n");
}

void test_zero_initial_capacity() 
{
    Stack* stack = stack_init(0, sizeof(int));
    ROMANO_ASSERT(stack != NULL, "");
    int element = 123;
    stack_push(stack, &element);
    ROMANO_ASSERT(stack_size(stack) == 1, "");
    ROMANO_ASSERT(*(int*)stack_top(stack) == 123, "");
    stack_free(stack);
    printf("test_zero_initial_capacity passed\n");
}

void test_pop_with_null() 
{
    Stack* stack = stack_init(2, sizeof(int));
    int element = 5;
    stack_push(stack, &element);
    stack_pop(stack, NULL);
    ROMANO_ASSERT(stack_size(stack) == 0, "");
    stack_free(stack);
    printf("test_pop_with_null passed\n");
}

int main() 
{
    test_init();
    test_push_top();
    test_pop();
    test_empty_top();
    test_struct_element();
    test_zero_initial_capacity();
    test_pop_with_null();

    printf("All tests passed!\n");

    return 0;
}