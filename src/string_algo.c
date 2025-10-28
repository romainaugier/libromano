/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/string_algo.h"

const char* strstrn(const char *haystack,
                    size_t haystack_len,
                    const char *needle,
                    size_t needle_len)
{
    char first;
    size_t search_sz;
    size_t i;
    size_t j;

    if(haystack == NULL || needle == NULL)
        return NULL;

    if(needle_len == 0)
        return haystack;

    if(needle_len > haystack_len)
        return NULL;

    first = needle[0];
    search_sz = haystack_len - needle_len + 1;

    for(i = 0; i < search_sz; i++)
    {
        if(haystack[i] != first)
            continue;

        for(j = 1; j < needle_len; j++)
            if(haystack[i + j] != needle[j])
                break;

        if(j == needle_len)
            return &haystack[i];
    }

    return NULL;
}
