/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/string.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#if !defined(LIBROMANO_STR_MAX_FMT_SIZE)
#define LIBROMANO_STR_MAX_FMT_SIZE 8192
#endif /* !defined(LIBROMANO_STR_MAX_FMT_SIZE) */

#if !defined(LIBROMANO_STRING_GROWTH_RATE)
#define LIBROMANO_STRING_GROWTH_RATE 1.61f
#endif /* !defined(LIBROMANO_STRING_GROWTH_RATE) */

#define HEADER_SIZE (2 * sizeof(size_t))
#define STRING_SIZE(length) (((length) + 1) * sizeof(char))

#define GET_STR_PTR(ptr) ((char*)(ptr) + HEADER_SIZE)
#define GET_RAW_PTR(ptr) ((char*)(ptr) - HEADER_SIZE)

#define GET_CAPACITY_FROM_RAW(ptr) (((size_t*)(ptr))[0])
#define GET_SIZE_FROM_RAW(ptr) (((size_t*)(ptr))[1])

#define GET_CAPACITY_FROM_STR(ptr) (((size_t*)(GET_RAW_PTR(ptr)))[0])
#define GET_SIZE_FROM_STR(ptr) (((size_t*)(GET_RAW_PTR(ptr)))[1])

#define SET_NULL_TERMINATOR(ptr) ((ptr)[(GET_SIZE_FROM_STR(ptr))] = '\0')

String string_new(const char* data)
{
    size_t length;
    size_t size;
    char* str_ptr;
    
    length = strlen(data);

    size = (size_t)((float)length * LIBROMANO_STRING_GROWTH_RATE);

    str_ptr = (char*)malloc(STRING_SIZE(size) + HEADER_SIZE);

    if(str_ptr == NULL) 
    {
        return NULL;
    }

    GET_CAPACITY_FROM_RAW(str_ptr) = size;
    GET_SIZE_FROM_RAW(str_ptr) = length;

    memcpy(GET_STR_PTR(str_ptr), data, length + 1);

    return GET_STR_PTR(str_ptr);
}

String string_newz(const size_t length)
{
    size_t size;
    char* str_ptr;

    size = (size_t)((float)length * LIBROMANO_STRING_GROWTH_RATE);

    str_ptr = (char*)malloc(STRING_SIZE(size) + HEADER_SIZE);

    if(str_ptr == NULL) return NULL;

    GET_CAPACITY_FROM_RAW(str_ptr) = size;
    GET_SIZE_FROM_RAW(str_ptr) = length;

    memset(GET_STR_PTR(str_ptr), 0, length + 1);

    return GET_STR_PTR(str_ptr);
}

String string_newf(const char* format, ...)
{
    va_list args;
    char buffer[LIBROMANO_STR_MAX_FMT_SIZE];

    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    return string_new(buffer);
}

size_t string_capacity(const String string)
{
    return GET_CAPACITY_FROM_STR(string);
}

size_t string_length(const String string)
{
    return GET_SIZE_FROM_STR(string);
}

void string_resize(String* string, const size_t new_size)
{
    size_t existing_capacity;
    size_t copy_size;
    char* new_str_ptr;

    existing_capacity = GET_CAPACITY_FROM_STR(*string);

    if(new_size < existing_capacity)
    {
        return;
    }

    new_str_ptr = (char*)malloc(STRING_SIZE(new_size) + HEADER_SIZE);

    if(new_str_ptr == NULL)
    {
        return;
    }

    copy_size = HEADER_SIZE + GET_SIZE_FROM_STR(*string);

    memcpy(new_str_ptr, GET_RAW_PTR(*string), copy_size);

    free(GET_RAW_PTR(*string));

    *string = GET_STR_PTR(new_str_ptr);

    GET_CAPACITY_FROM_STR(*string) = new_size;
    SET_NULL_TERMINATOR(*string);
}

String string_copy(const String other)
{
    size_t other_size;
    size_t other_capacity;
    String res;
    char* new_ptr;

    ROMANO_ASSERT(other != NULL, "Cannot copy an empty string");

    other_size = GET_SIZE_FROM_STR(other);
    other_capacity = GET_CAPACITY_FROM_STR(other);

    new_ptr = (char*)malloc(STRING_SIZE(other_capacity) + HEADER_SIZE);

    memcpy(new_ptr, GET_RAW_PTR(other), STRING_SIZE(other_capacity) + HEADER_SIZE);

    res = new_ptr;

    return GET_STR_PTR(res);
}

void string_setc(String* string, const char* data)
{
    size_t data_len;
    size_t string_capacity;

    data_len = strlen(data);

    string_capacity = GET_CAPACITY_FROM_STR(*string);

    if(data_len >= string_capacity)
    {
        string_resize(string, data_len);
    }

    memcpy(*string, data, STRING_SIZE(data_len));

    GET_SIZE_FROM_STR(*string) = data_len;
    SET_NULL_TERMINATOR(*string);
}

void string_sets(String* string, const String other)
{
    size_t data_len;
    size_t string_capacity;

    data_len = string_length(other);

    string_capacity = GET_CAPACITY_FROM_STR(*string);

    if(data_len >= string_capacity)
    {
        string_resize(string, data_len);
    }

    memcpy(*string, other, STRING_SIZE(data_len));

    GET_SIZE_FROM_STR(*string) = data_len;
    SET_NULL_TERMINATOR(*string);
}

void string_setf(String* string, const char* format, ...)
{
    size_t format_size;
    size_t string_capacity;
    va_list args;
    char buffer[LIBROMANO_STR_MAX_FMT_SIZE];

    va_start(args, format);
    format_size = (size_t)vsprintf(buffer, format, args) + 1;
    va_end(args);

    string_capacity = GET_CAPACITY_FROM_STR(*string);

    if(format_size >= string_capacity)
    {
        string_resize(string, format_size);
    }

    memcpy(*string, buffer, format_size);

    GET_SIZE_FROM_STR(*string) = format_size;
    SET_NULL_TERMINATOR(*string);
}

void string_appendc(String* string, const char* data)
{
    size_t data_len;
    size_t string_capacity;
    size_t string_size;

    data_len = strlen(data);

    string_capacity = GET_CAPACITY_FROM_STR(*string);
    string_size = GET_SIZE_FROM_STR(*string);

    if((string_size + data_len) >= string_capacity)
    {
        string_resize(string, (size_t)((float)string_capacity + (float)data_len) * LIBROMANO_STRING_GROWTH_RATE);
    }

    memcpy((*string) + string_size, data, STRING_SIZE(data_len));

    GET_SIZE_FROM_STR(*string) = string_size + data_len;
    SET_NULL_TERMINATOR(*string);
}

void string_appends(String* string, const String other)
{
    size_t other_len;
    size_t string_capacity;
    size_t string_size;

    other_len = string_length(other);

    string_capacity = GET_CAPACITY_FROM_STR(*string);
    string_size = GET_SIZE_FROM_STR(*string);

    if((string_size + other_len) >= string_capacity)
    {
        string_resize(string, (size_t)((float)string_capacity + (float)other_len) * LIBROMANO_STRING_GROWTH_RATE);
    }

    memcpy((*string) + string_size, other, STRING_SIZE(other_len));

    GET_SIZE_FROM_STR(*string) = string_size + other_len;
    SET_NULL_TERMINATOR(*string);
}

void string_appendf(String* string, const char* format, ...)
{
    size_t string_capacity;
    size_t string_size;
    size_t format_size;
    va_list args;
    char buffer[LIBROMANO_STR_MAX_FMT_SIZE];

    va_start(args, format);
    format_size = (size_t)vsprintf(buffer, format, args) + 1;
    va_end(args);

    string_capacity = GET_CAPACITY_FROM_STR(*string);
    string_size = GET_SIZE_FROM_STR(*string);

    if((string_size + format_size) >= string_capacity)
    {
        string_resize(string, (size_t)((float)string_capacity + (float)format_size) * LIBROMANO_STRING_GROWTH_RATE);
    }

    memcpy((*string) + string_size, buffer, format_size);

    GET_SIZE_FROM_STR(*string) = string_size + format_size;
    SET_NULL_TERMINATOR(*string);
}

void string_prependc(String* string, const char* data)
{
    size_t data_len;
    size_t string_capacity;
    size_t string_size;

    data_len = strlen(data);

    string_capacity = GET_CAPACITY_FROM_STR(*string);
    string_size = GET_SIZE_FROM_STR(*string);

    if((string_size + data_len) >= string_capacity)
    {
        string_resize(string, (size_t)((float)string_capacity + (float)data_len) * LIBROMANO_STRING_GROWTH_RATE);
    }

    memmove((*string) + data_len, *string, STRING_SIZE(string_size));
    memcpy(*string, data, data_len);

    GET_SIZE_FROM_STR(*string) = string_size + data_len;
    SET_NULL_TERMINATOR(*string);
}

void string_prepends(String* string, const String other)
{
    size_t other_len;
    size_t string_capacity;
    size_t string_size;

    other_len = string_length(other);

    string_capacity = GET_CAPACITY_FROM_STR(*string);
    string_size = GET_SIZE_FROM_STR(*string);

    if((string_size + other_len) >= string_capacity)
    {
        string_resize(string, (size_t)((float)string_capacity + (float)other_len) * LIBROMANO_STRING_GROWTH_RATE);
    }

    memmove((*string) + other_len, *string, STRING_SIZE(string_size));
    memcpy(*string, other, other_len);

    GET_SIZE_FROM_STR(*string) = string_size + other_len;
    SET_NULL_TERMINATOR(*string);
}

void string_prependf(String* string, const char* format, ...)
{
    size_t string_capacity;
    size_t string_size;
    size_t format_size;
    va_list args;
    char buffer[LIBROMANO_STR_MAX_FMT_SIZE];

    va_start(args, format);
    format_size = (size_t)vsprintf(buffer, format, args);
    va_end(args);

    string_capacity = GET_CAPACITY_FROM_STR(*string);
    string_size = GET_SIZE_FROM_STR(*string);

    if((string_size + (format_size + 1)) >= string_capacity)
    {
        string_resize(string, (size_t)((float)string_capacity + (float)(format_size + 1)) * LIBROMANO_STRING_GROWTH_RATE);
    }

    memmove((*string) + format_size, *string, STRING_SIZE(string_size));
    memcpy(*string, buffer, format_size);

    GET_SIZE_FROM_STR(*string) = string_size + format_size;
    SET_NULL_TERMINATOR(*string);
}

void string_clear(String string)
{
    size_t len;

    len = string_length(string);

    memset(string, 0, STRING_SIZE(len));

    GET_SIZE_FROM_STR(string) = 0;
}

String* string_splitc(char* data, const char* separator, uint32_t* count)
{
    size_t i;
    size_t data_len;
    char* data_char;
    String* result;
    char* token;

    data_len = 0;
    *count = 0;

    for(data_char = data; *data_char != (char)'\0'; data_char++)
    {
        if(*data_char == (char)separator[0]) (*count)++;
        data_len++;
    }

    if(data_len == 0)
    {
        return NULL;
    }

    (*count)++;

    result = malloc(*count * sizeof(String));
    token = strtok(data, separator);

    i = 0;

    while(token != NULL)
    {
        result[i] = string_new(token);
        token = strtok(NULL, separator);
        i++;
    }
    
    return result;
}

bool string_eq(const String a, const String b)
{
    size_t i;

    if(string_length(a) != string_length(b))
    {
        return 0;
    }

    return memcmp(a, b, string_length(a) * sizeof(char)) == 0 ? true : false;
}

void string_free(String data)
{
    if(data != NULL)
    {
        free(GET_RAW_PTR(data));
        data = NULL;
    }
}