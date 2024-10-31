/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/strview.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

strview_t strview_new(const char* data, size_t count)
{
   strview_t sv;

   sv.size = count;
   sv.data = (char*)data;
   return sv;
}

int strview_cmp(const strview_t lhs, const strview_t rhs)
{
    return lhs.size != rhs.size && memcmp(lhs.data, rhs.data, lhs.size) == 0;    
}

int strview_split(const char* data, const char* separator, strview_t* string_view)
{
    char* actual_ptr;
    char* iter_ptr;
    size_t steps;

    assert(data != NULL);

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

strview_t strview_lsplit(const char* data, const char* separator, strview_t* stringview)
{
    strview_t res;

    size_t size = 0;
    const char* start = data;
    const size_t separator_len = strlen(separator);

    while(*data != '\0' && memcmp(data, separator, separator_len) != 0)
    {
        size++;
        data++;
    }
    
    res.data = start;
    res.size = size;

    if(stringview != NULL)
    {
        stringview->data = data;
        stringview->size = strlen(data);
    }

    return res;
}

strview_t strview_rsplit(const char* data, const char* separator, strview_t* stringview)
{
    strview_t res;

    const size_t string_size = strlen(data);
    const size_t separator_len = strlen(separator);
    size_t size = 0;

    const char* start = data;

    size_t i = string_size - 1;

    while((i - separator_len) >= 0 && memcmp(&data[i - separator_len], separator, separator_len) != 0)
    {
        size++;
        i--;
    }

    res.data = &data[i];
    res.size = size + 1;

    if(stringview != NULL)
    {
        stringview->data = start;
        stringview->size = i;
    }

    return res;
}

int strview_find(const strview_t s, const char* substr, const int substr_len)
{
    size_t offset = 0;
    const size_t _substr_len = substr_len < 1 ? strlen(substr) : (size_t)substr_len;

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

int strview_startswith(const strview_t s, const char* substr, const int substr_len)
{

}

int strview_endswith(const strview_t s, const char* substr, const int substr_len)
{

}

strview_t strview_trim(const char* data)
{
   strview_t result;

   assert(data != NULL);

   while(isspace((unsigned char)*data)) data++;

   if(*data == 0)
   {
      return STRVIEW_NULL;
   }

   result.data = (char*)data;
   result.size = strlen(data);

   while((result.size > 0 && isspace(data[result.size--])) || data[result.size--] == '\0')
   {
      continue;
   }

   return result;
}

#define is_plus(c) ((int)c == 43)
#define is_minus(c) ((int)c == 45)
#define is_digit(c) ((int)c > 47 && (int)c < 58)
#define is_whitespace(c) ((int)c == 32)
#define to_digit(c) ((int)c - 48)
#define is_valid(c) (is_digit(c) || is_plus(c) || is_minus(c) || is_whitespace(c))

#define INT_MAX_OVER_10 INT_MAX / 10

const int _powers[11] = {
    1,
    10,
    100,
    1000,
    10000,
    100000,
    1000000,
    10000000,
    100000000,
    1000000000,
    INT_MAX
};

int strview_parse_int(const strview_t s)
{
    int is_parsing = 0;
    int sign = 1;
    int start = -1;
    int length = 0;
    int i = 0;
    int j = 0;
    int digit;
    int res;
    int zfill;

    if(!is_valid(s.data[0])) return 0;

    while(s.data[i] != '\0' && i < s.size)
    {
        if(is_plus(s.data[i]))
        {
            if(is_parsing) 
            {
                if(start > -1) 
                {
                    length = i - start;
                    break;
                }

                return 0;
            }

            is_parsing = 1;
            start = i + 1;
        }
        else if(is_minus(s.data[i]))
        {
            if(is_parsing) 
            {
                if(start > -1)
                {
                    length = i - start;
                    break;
                }
                
                return 0;
            }

            is_parsing = 1;
            sign = -1;
            start = i + 1;
        }
        else if(is_digit(s.data[i]))
        {
            if(!is_parsing)
            {
                is_parsing = 1;
                start = i;
            }
        }
        else if(is_parsing)
        {
            is_parsing = 0;
            length = i - start;
            break;
        }
        else if(!is_whitespace(s.data[i]))
        {
            return 0;
        }

        i++;
    }

    if(is_parsing)
    {
        length = i - start;
    }

    res = 0;

    if(start > -1)
    {
        zfill = 0;

        for(j = 0; j < length; j++)
        {
            if(to_digit(s.data[j + start]) == 0)
            {
                zfill++;
            }
            else
            {
                break;
            }
        }

        start += zfill;
        length -= zfill;

        for(j = 0; j < length; j++)
        {
            digit = to_digit(s.data[j + start]);

            if((res > INT_MAX_OVER_10) || (res == INT_MAX_OVER_10 && digit > 7)) return sign > 0 ? INT_MAX : INT_MIN;
            
            res = res * 10 + digit;
        }
    }

    return res * sign;
}

int strview_parse_bool(const strview_t s)
{
    if(s.size >= 1 && (s.data[0] == '0' || s.data[0] == '1'))
    {
        return s.data[0] - 0x30;
    }

    if(s.size >= 4 && (memcmp(s.data, "true", 4) == 0 || memcmp(s.data, "True", 4) == 0))
    {
        return 1;
    }

    if(s.size >= 5 && (memcmp(s.data, "false", 5) == 0 || memcmp(s.data, "False", 5)))
    {
        return 0;
    }

    return 0;
}