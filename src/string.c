/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/string.h"
#include "libromano/error.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#if !defined(LIBROMANO_STRING_MAX_FMT_SIZE)
#define LIBROMANO_STRING_MAX_FMT_SIZE 8192
#endif /* !defined(LIBROMANO_STRING_MAX_FMT_SIZE) */

#if !defined(LIBROMANO_STRING_GROWTH_RATE)
#define LIBROMANO_STRING_GROWTH_RATE ((double)1.61)
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

extern ErrorCode g_current_error;

String string_new(const char* data)
{
    size_t length;
    size_t sz;
    char* str_ptr;
    
    length = strlen(data);

    sz = (size_t)((double)length * LIBROMANO_STRING_GROWTH_RATE);

    str_ptr = (char*)calloc(STRING_SIZE(sz) + HEADER_SIZE, sizeof(char));

    if(str_ptr == NULL) 
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    GET_CAPACITY_FROM_RAW(str_ptr) = sz;
    GET_SIZE_FROM_RAW(str_ptr) = length;

    memcpy(GET_STR_PTR(str_ptr), data, length + 1);

    return GET_STR_PTR(str_ptr);
}

String string_newz(const size_t length)
{
    size_t sz;
    char* str_ptr;

    sz = (size_t)((double)length * LIBROMANO_STRING_GROWTH_RATE);

    str_ptr = (char*)calloc(STRING_SIZE(sz) + HEADER_SIZE, sizeof(char));

    if(str_ptr == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    GET_CAPACITY_FROM_RAW(str_ptr) = sz;
    GET_SIZE_FROM_RAW(str_ptr) = length;

    memset(GET_STR_PTR(str_ptr), 0, length + 1);

    return GET_STR_PTR(str_ptr);
}

String string_newf(const char* format, ...)
{
    va_list args;
    char buffer[LIBROMANO_STRING_MAX_FMT_SIZE];

    va_start(args, format);
    int ret = vsnprintf(buffer, LIBROMANO_STRING_MAX_FMT_SIZE, format, args);
    va_end(args);

    if(ret < 0)
    {
        g_current_error = ErrorCode_FormattingError;
        return NULL;
    }

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

bool string_resize(String* string, size_t new_size)
{
    size_t existing_capacity;
    size_t copy_sz;
    char* new_str_ptr;

    existing_capacity = GET_CAPACITY_FROM_STR(*string);

    if(new_size < existing_capacity)
        return true;

    new_str_ptr = (char*)malloc(STRING_SIZE(new_size) + HEADER_SIZE);

    if(new_str_ptr == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return false;
    }

    copy_sz = HEADER_SIZE + GET_SIZE_FROM_STR(*string);

    memcpy(new_str_ptr, GET_RAW_PTR(*string), copy_sz);

    free(GET_RAW_PTR(*string));

    *string = GET_STR_PTR(new_str_ptr);

    GET_CAPACITY_FROM_STR(*string) = new_size;
    SET_NULL_TERMINATOR(*string);

    return true;
}

String string_copy(const String other)
{
    size_t other_sz;
    size_t other_capacity;
    String res;
    char* new_ptr;

    ROMANO_ASSERT(other != NULL, "Cannot copy an empty string");

    other_sz = GET_SIZE_FROM_STR(other);
    other_capacity = GET_CAPACITY_FROM_STR(other);

    new_ptr = (char*)malloc(STRING_SIZE(other_capacity) + HEADER_SIZE);

    if(new_ptr == NULL)
    {
        g_current_error = ErrorCode_FormattingError;
        return NULL;
    }

    memcpy(new_ptr, GET_RAW_PTR(other), STRING_SIZE(other_capacity) + HEADER_SIZE);

    res = new_ptr;

    return GET_STR_PTR(res);
}

bool string_setc(String* string, const char* data)
{
    size_t data_sz;
    size_t string_capacity;

    ROMANO_ASSERT(string != NULL, "string is NULL");

    data_sz = strlen(data);

    string_capacity = GET_CAPACITY_FROM_STR(*string);

    if(data_sz >= string_capacity)
        if(!string_resize(string, data_sz))
            return false;

    memcpy(*string, data, STRING_SIZE(data_sz));

    GET_SIZE_FROM_STR(*string) = data_sz;
    SET_NULL_TERMINATOR(*string);

    return true;
}

bool string_sets(String* string, const String other)
{
    size_t data_sz;
    size_t string_capacity;

    ROMANO_ASSERT(string != NULL, "string is NULL");

    data_sz = string_length(other);

    string_capacity = GET_CAPACITY_FROM_STR(*string);

    if(data_sz >= string_capacity)
        if(!string_resize(string, data_sz))
            return false;

    memcpy(*string, other, STRING_SIZE(data_sz));

    GET_SIZE_FROM_STR(*string) = data_sz;
    SET_NULL_TERMINATOR(*string);

    return true;
}

bool string_setf(String* string, const char* format, ...)
{
    size_t format_sz;
    size_t string_capacity;
    va_list args;
    char buffer[LIBROMANO_STRING_MAX_FMT_SIZE];

    ROMANO_ASSERT(string != NULL, "string is NULL");

    va_start(args, format);
    format_sz = (size_t)vsnprintf(buffer, LIBROMANO_STRING_MAX_FMT_SIZE, format, args) + 1;
    va_end(args);

    if(format_sz <= 0)
    {
        g_current_error = ErrorCode_FormattingError;
        return false;
    }

    string_capacity = GET_CAPACITY_FROM_STR(*string);

    if(format_sz >= string_capacity)
        if(!string_resize(string, format_sz))
            return false;

    memcpy(*string, buffer, format_sz);

    GET_SIZE_FROM_STR(*string) = format_sz;
    SET_NULL_TERMINATOR(*string);

    return true;
}

bool string_appendc(String* string, const char* data)
{
    size_t data_sz;
    size_t string_capacity;
    size_t string_sz;

    ROMANO_ASSERT(string != NULL, "string is NULL");

    data_sz = strlen(data);

    string_capacity = GET_CAPACITY_FROM_STR(*string);
    string_sz = GET_SIZE_FROM_STR(*string);

    if((string_sz + data_sz) >= string_capacity)
    {
        size_t new_capacity = (size_t)(((double)string_capacity + (double)data_sz) * LIBROMANO_STRING_GROWTH_RATE);

        if(!string_resize(string, new_capacity))
            return false;
    }

    memcpy((*string) + string_sz, data, STRING_SIZE(data_sz));

    GET_SIZE_FROM_STR(*string) = string_sz + data_sz;
    SET_NULL_TERMINATOR(*string);

    return true;
}

bool string_appends(String* string, const String other)
{
    size_t other_sz;
    size_t string_capacity;
    size_t string_sz;

    ROMANO_ASSERT(string != NULL, "string is NULL");

    other_sz = string_length(other);

    string_capacity = GET_CAPACITY_FROM_STR(*string);
    string_sz = GET_SIZE_FROM_STR(*string);

    if((string_sz + other_sz) >= string_capacity)
    {
        size_t new_capacity = (size_t)(((double)string_capacity + (double)other_sz) * LIBROMANO_STRING_GROWTH_RATE);

        if(!string_resize(string, new_capacity))
            return false;
    }

    memcpy((*string) + string_sz, other, STRING_SIZE(other_sz));

    GET_SIZE_FROM_STR(*string) = string_sz + other_sz;
    SET_NULL_TERMINATOR(*string);

    return true;
}

bool string_appendf(String* string, const char* format, ...)
{
    size_t string_capacity;
    size_t string_sz;
    size_t format_sz;
    va_list args;
    char buffer[LIBROMANO_STRING_MAX_FMT_SIZE];

    ROMANO_ASSERT(string != NULL, "string is NULL");

    va_start(args, format);
    format_sz = (size_t)vsnprintf(buffer, LIBROMANO_STRING_MAX_FMT_SIZE, format, args) + 1;
    va_end(args);

    if(format_sz <= 0)
    {
        g_current_error = ErrorCode_FormattingError;
        return false;
    }

    string_capacity = GET_CAPACITY_FROM_STR(*string);
    string_sz = GET_SIZE_FROM_STR(*string);

    if((string_sz + format_sz) >= string_capacity)
    {
        size_t new_capacity = (size_t)(((double)string_capacity + (double)format_sz) * LIBROMANO_STRING_GROWTH_RATE);

        if(!string_resize(string, new_capacity))
            return false;
    }

    memcpy((*string) + string_sz, buffer, format_sz);

    GET_SIZE_FROM_STR(*string) = string_sz + format_sz - 1;
    SET_NULL_TERMINATOR(*string);

    return true;
}

bool string_prependc(String* string, const char* data)
{
    size_t data_sz;
    size_t string_capacity;
    size_t string_sz;

    ROMANO_ASSERT(string != NULL, "string is NULL");

    data_sz = strlen(data);

    string_capacity = GET_CAPACITY_FROM_STR(*string);
    string_sz = GET_SIZE_FROM_STR(*string);

    if((string_sz + data_sz) >= string_capacity)
    {
        size_t new_capacity = (size_t)(((double)string_capacity + (double)data_sz) * LIBROMANO_STRING_GROWTH_RATE);
        
        if(!string_resize(string, new_capacity))
            return false;
    }

    memmove((*string) + data_sz, *string, STRING_SIZE(string_sz));
    memcpy(*string, data, data_sz);

    GET_SIZE_FROM_STR(*string) = string_sz + data_sz;
    SET_NULL_TERMINATOR(*string);

    return true;
}

bool string_prepends(String* string, const String other)
{
    size_t other_sz;
    size_t string_capacity;
    size_t string_sz;

    ROMANO_ASSERT(string != NULL, "string is NULL");

    other_sz = string_length(other);

    string_capacity = GET_CAPACITY_FROM_STR(*string);
    string_sz = GET_SIZE_FROM_STR(*string);

    if((string_sz + other_sz) >= string_capacity)
    {
        size_t new_capacity = (size_t)(((double)string_capacity + (double)other_sz) * LIBROMANO_STRING_GROWTH_RATE);

        if(!string_resize(string, new_capacity))
            return false;
    }

    memmove((*string) + other_sz, *string, STRING_SIZE(string_sz));
    memcpy(*string, other, other_sz);

    GET_SIZE_FROM_STR(*string) = string_sz + other_sz;
    SET_NULL_TERMINATOR(*string);

    return true;
}

bool string_prependf(String* string, const char* format, ...)
{
    size_t string_capacity;
    size_t string_sz;
    size_t format_sz;
    va_list args;
    char buffer[LIBROMANO_STRING_MAX_FMT_SIZE];

    ROMANO_ASSERT(string != NULL, "string is NULL");

    va_start(args, format);
    format_sz = (size_t)vsnprintf(buffer, LIBROMANO_STRING_MAX_FMT_SIZE, format, args);
    va_end(args);

    if(format_sz <= 0)
    {
        g_current_error = ErrorCode_FormattingError;
        return false;
    }

    string_capacity = GET_CAPACITY_FROM_STR(*string);
    string_sz = GET_SIZE_FROM_STR(*string);

    if((string_sz + (format_sz + 1)) >= string_capacity)
    {
        size_t new_capacity = (size_t)(((double)string_capacity + (double)(format_sz + 1)) * LIBROMANO_STRING_GROWTH_RATE);

        if(!string_resize(string, new_capacity))
            return false;
    }

    memmove((*string) + format_sz, *string, STRING_SIZE(string_sz));
    memcpy(*string, buffer, format_sz);

    GET_SIZE_FROM_STR(*string) = string_sz + format_sz;
    SET_NULL_TERMINATOR(*string);

    return true;
}

void string_clear(String string)
{
    size_t sz;

    sz = string_length(string);

    memset(string, 0, STRING_SIZE(sz));

    GET_SIZE_FROM_STR(string) = 0;
}

String* string_splitc(char* data, const char* separator, uint32_t* count)
{
    size_t i;
    size_t data_sz;
    char* data_char;
    String* result;
    char* token;

    data_sz = 0;
    *count = 0;

    for(data_char = data; *data_char != (char)'\0'; data_char++)
    {
        if(*data_char == (char)separator[0]) (*count)++;
        data_sz++;
    }

    if(data_sz == 0)
    {
        return NULL;
    }

    (*count)++;

    result = malloc(*count * sizeof(String));

    if(result == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    token = strtok(data, separator);

    i = 0;

    while(token != NULL)
    {
        result[i++] = string_new(token);
        token = strtok(NULL, separator);
    }
    
    return result;
}

bool string_eq(const String a, const String b)
{
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