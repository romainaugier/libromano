/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/str.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define GET_STR_PTR(ptr) ((char*)ptr + sizeof(size_t))
#define GET_RAW_PTR(ptr) ((char*)ptr - sizeof(size_t))

str str_new(const char* string)
{
    size_t length;
    char* str_ptr;
    
    length = strlen(string);

    str_ptr = (char*)malloc(length + 1 + sizeof(size_t));

    if(str_ptr == NULL) return NULL;

    *(size_t*)str_ptr = length;

    memcpy(GET_STR_PTR(str_ptr), string, length + 1);

    return GET_STR_PTR(str_ptr);
}

str str_new_fmt(const char* format, ...)
{
    return NULL;
}

void str_free(str string)
{
    if(string != NULL)
    {
        free(GET_RAW_PTR(string));
        string = NULL;
    }
}

size_t str_length(str string)
{
    return (size_t)*GET_RAW_PTR(string);
}

str* str_split(char* string, const char* separator, uint32_t* count)
{
    size_t i;
    str* result;
    char* token;
    
    *count = 0;

    for(i = 0; i < strlen(string); i++)
    {
        if(string[i] == (char)separator[0]) (*count)++;
    }

    (*count)++;

    result = malloc(*count * sizeof(str));
    token = strtok(string, separator);

    i = 0;

    while(token != NULL)
    {
        result[i] = str_new(token);
        token = strtok(NULL, separator);
        i++;
    }
    
    return result;
}