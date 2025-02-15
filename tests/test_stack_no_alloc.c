/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/stack_no_alloc.h"

int main() 
{
    stack_init(int, mystack, 5);
    
    for(int i = 0; !stack_is_full(mystack); i++) 
    {
        stack_push(mystack, i * 10);
        printf("Pushed: %d\n", *stack_top(mystack));
    }

    printf("\nPopping:\n");

    while(!stack_is_empty(mystack)) 
    {
        printf("Popped: %d\n", stack_pop(mystack));
    }

    return 0;
}