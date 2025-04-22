/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/int128.h"

int main() 
{
    const uint128_t a = make_uint128(0x0000000000000001, 0xFFFFFFFFFFFFFFFF);
    const uint128_t b = make_uint128(0x0000000000000000, 0x0000000000000002);
    
    printf("a = ");
    print_uint128(a);
    printf("\n");
    
    printf("b = ");
    print_uint128(b);
    printf("\n");
    
    const uint128_t sum = uint128_add(a, b);
    printf("a + b = ");
    print_uint128(sum);
    printf("\n");
    
    const uint128_t diff = uint128_sub(a, b);
    printf("a - b = ");
    print_uint128(diff);
    printf("\n");
    
    printf("a & b = ");
    print_uint128(uint128_and(a, b));
    printf("\n");
    
    printf("a | b = ");
    print_uint128(uint128_or(a, b));
    printf("\n");
    
    printf("a ^ b = ");
    print_uint128(uint128_xor(a, b));
    printf("\n");
    
    printf("a << 4 = ");
    print_uint128(uint128_shl(a, 4));
    printf("\n");
    
    printf("a >> 4 = ");
    print_uint128(uint128_shr(a, 4));
    printf("\n");
    
    return 0;
}