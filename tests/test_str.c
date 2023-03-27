/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/str.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    const char t[] = "test string";
    
    str s = str_new(t);

    size_t len_t = strlen(t);
    size_t len_s = str_length(s);
    printf("Len t : %llu, len s : %llu\n", len_t, len_s);

    str_free(s);
    
    char t2[] = "test split string";

    uint32_t count = 0;
    str* t2_split = str_split(t2, " ", &count);
    
    printf("Splits (%d): \n", count);
    printf(" - %s\n", t2_split[0]);
    printf(" - %s\n", t2_split[1]);
    printf(" - %s\n", t2_split[2]);

    size_t i;
    for(i = 0; i < count; i++)
    {
        str_free(t2_split[i]);
    }

    free(t2_split);

    return 0;
}

