/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/strview.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    const char* test = "This is a test string which can be quite long because "
                       "we need to make sure the function is robust enough";
    size_t count;

    strview* views = strview_split(test, " ", &count);

    size_t i;

    printf("String view splits : \n");

    for(i = 0; i < count; i++)
    {
        printf("- \""STRVIEW_FMT"\"\n", STRVIEW_ARG(views[i]));
    }

    free(views);


    const char* test_trim = "    trim this string  ";

    strview trim = strview_trim(test_trim);

    printf("Trimmed string : \""STRVIEW_FMT"\"\n", STRVIEW_ARG(trim));
}
