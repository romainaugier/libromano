/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/stack_no_alloc.h"

int main() 
{
    stacknoa_init(int, mystack, 5);
    
    for(int i = 0; !stacknoa_is_full(mystack); i++) 
    {
        stacknoa_push(mystack, i * 10);
        printf("Pushed: %d\n", *stacknoa_top(mystack));
    }

    printf("\nPopping:\n");

    while(!stacknoa_is_empty(mystack)) 
    {
        printf("Popped: %d\n", stacknoa_pop(mystack));
    }

    return 0;
}