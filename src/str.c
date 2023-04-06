/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/str.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define GET_STR_PTR(ptr) ((char*)ptr + sizeof(size_t))
#define GET_RAW_PTR(ptr) ((char*)ptr - sizeof(size_t))

str str_new(const char* data)
{
    size_t length;
    char* str_ptr;
    
    length = strlen(data);

    str_ptr = (char*)malloc(length + 1 + sizeof(size_t));

    if(str_ptr == NULL) return NULL;

    *(size_t*)str_ptr = length;

    memcpy(GET_STR_PTR(str_ptr), data, length + 1);

    return GET_STR_PTR(str_ptr);
}

str str_new_fmt(const char* format, ...)
{
    return NULL;
}

void str_free(str data)
{
    if(data != NULL)
    {
        free(GET_RAW_PTR(data));
        data = NULL;
    }
}

size_t str_length(str data)
{
    return (size_t)*GET_RAW_PTR(data);
}

str* str_split(char* data, const char* separator, uint32_t* count)
{
    size_t i;
    str* result;
    char* token;
    
    *count = 0;

    for(i = 0; i < strlen(data); i++)
    {
        if(data[i] == (char)separator[0]) (*count)++;
    }

    (*count)++;

    result = malloc(*count * sizeof(str));
    token = strtok(data, separator);

    i = 0;

    while(token != NULL)
    {
        result[i] = str_new(token);
        token = strtok(NULL, separator);
        i++;
    }
    
    return result;
}

