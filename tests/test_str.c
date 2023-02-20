// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#include "libromano/str.h"
#include <stdio.h>
#include <assert.h>

int main(int argc, char** argv)
{
    const char t[] = "test string";
    
    str s = str_new(t);

    const size_t len_t = strlen(t);
    const size_t len_s = str_length(s);
    printf("Len t : %lu, len s : %lu\n", len_t, len_s);

    str_free(s);
    
    return 0;
}