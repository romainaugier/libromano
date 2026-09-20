/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/strview.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

StringView strview_new(const char* data, size_t count)
{
   StringView sv;

   sv.size = count;
   sv.data = (char*)data;
   return sv;
}

int strview_cmp(const StringView lhs, const StringView rhs)
{
    return lhs.size == rhs.size && memcmp(lhs.data, rhs.data, lhs.size) == 0;
}

int strview_split(const char* data, const char* separator, StringView* string_view)
{
    char* actual_ptr;
    char* iter_ptr;
    size_t steps;

    ROMANO_ASSERT(data != NULL, "data is NULL");

    actual_ptr = string_view->data != NULL ?
                 string_view->data + string_view->size : (char*)data;

    if(*(actual_ptr) == '\0')
    {
        return 0;
    }

    actual_ptr = ((char)actual_ptr[0] == (char)separator[0]) ? actual_ptr + 1 : actual_ptr; 

    iter_ptr = actual_ptr;
    steps = 0;

    while(1)
    {   
        if(*iter_ptr == (char)separator[0] || 
           *iter_ptr == '\0')
        {
            *string_view = strview_new(actual_ptr, steps);

            return 1;
        }

        iter_ptr++;
        steps++;
    }
}

StringView strview_lsplit(const char* data, const char* separator, StringView* stringview)
{
    StringView res;

    size_t size = 0;
    const char* start = data;
    const size_t separator_len = strlen(separator);

    while(*data != '\0' && memcmp(data, separator, separator_len) != 0)
    {
        size++;
        data++;
    }
    
    res.data = (char*)start;
    res.size = size;

    if(stringview != NULL)
    {
        stringview->data = (char*)data;
        stringview->size = strlen(data);
    }

    return res;
}

StringView strview_rsplit(const char* data, const char* separator, StringView* stringview)
{
    StringView res;

    const size_t string_size = strlen(data);
    const size_t separator_len = strlen(separator);
    size_t i = string_size;

    while(i >= separator_len && separator_len > 0)
    {
        if(memcmp(&data[i - separator_len], separator, separator_len) == 0)
            break;

        i--;
    }

    if(i < separator_len || separator_len == 0)
        i = 0;

    res.data = (char*)&data[i];
    res.size = string_size - i;

    if(stringview != NULL)
    {
        stringview->data = (char*)data;
        stringview->size = i;
    }

    return res;
}

int strview_find(const StringView s, const char* substr, const int substr_len)
{
    int offset = 0;
    int _substr_len = substr_len < 1 ? (int)strlen(substr) : substr_len;

    if(_substr_len > s.size) return -1;

    while((_substr_len - 1 + offset) < s.size)
    {
        if(memcmp(s.data + offset, substr, _substr_len * sizeof(char)) == 0)
        {
            return offset;
        }

        offset++;
    }

    return -1;
}

bool strview_startswith(const StringView s, const char* substr, const int substr_len)
{
    const size_t _substr_len = substr_len < 1 ? strlen(substr) : (size_t)substr_len;

    if(_substr_len > s.size) return 0;

    return memcmp(substr, s.data, _substr_len) == 0;
}

bool strview_endswith(const StringView s, const char* substr, const int substr_len)
{
    const size_t _substr_len = substr_len < 1 ? strlen(substr) : (size_t)substr_len;

    if(_substr_len > s.size) return 0;

    return memcmp(substr, &(s.data[s.size - _substr_len]), _substr_len) == 0;
}

StringView strview_trim(const char* data)
{
    StringView result;

    ROMANO_ASSERT(data != NULL, "Data is null");

    while(isspace((unsigned char)*data))
        data++;

    result.data = (char*)data;
    result.size = strlen(data);

    while(result.size > 0 && isspace((unsigned char)data[result.size - 1]))
        result.size--;

    return result;
}

#define STRVIEW_PARSE_BUFFER_SIZE 512

static size_t strview_to_cstr(const StringView s, char* buffer)
{
    size_t size = s.size < (STRVIEW_PARSE_BUFFER_SIZE - 1) ? s.size : (STRVIEW_PARSE_BUFFER_SIZE - 1);

    if(size > 0)
        memcpy(buffer, s.data, size);

    buffer[size] = '\0';

    return size;
}

int strview_parse_int(const StringView s)
{
    char buffer[STRVIEW_PARSE_BUFFER_SIZE];
    long long value;

    strview_to_cstr(s, buffer);

    value = strtoll(buffer, NULL, 10);

    if(value > INT_MAX || value < INT_MIN)
        return 0;

    return (int)value;
}

bool strview_parse_bool(const StringView s)
{
    if(s.size >= 1 && (s.data[0] == '0' || s.data[0] == '1'))
    {
        return (bool)(s.data[0] - 0x30);
    }

    if(s.size >= 4 && (memcmp(s.data, "true", 4) == 0 || memcmp(s.data, "True", 4) == 0))
    {
        return true;
    }

    if(s.size >= 5 && (memcmp(s.data, "false", 5) == 0 || memcmp(s.data, "False", 5) == 0))
    {
        return false;
    }

    return false;
}

double strview_parse_double(const StringView s)
{
    char buffer[STRVIEW_PARSE_BUFFER_SIZE];

    strview_to_cstr(s, buffer);

    return strtod(buffer, NULL);
}