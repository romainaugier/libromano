// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#include "libromano/hash.h"

#include <stdio.h>

int main(int argc, char** argv)
{
    const char t[] = "test_hash";
    
    uint32_t hash = hash_fnv1a(t);

    printf("Hash : %d\n", hash);
    
    return 0;
}