/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/strview.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


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

strview_t* strview_split(const char* data, const char* separator, size_t* count)
{
    size_t i, j;
    size_t data_length;
    size_t previous_length;
    strview_t* result;
    char* buffer;

    assert(data != NULL);
    
    *count = 0;
    i = 0;

    while(1)
    {
        if(data[i] == '\0')
        {
            data_length = i;
            break;
        }

        if(data[i] == (char)separator[0]) (*count)++;
        i++;
    }

    (*count)++;

    result = malloc(*count * sizeof(strview_t));
    buffer = (char*)data;
    previous_length = 0;
    j = 0;

    for(i = 0; i < (data_length + 1); i++)
    {
        if(data[i] == (char)separator[0] || 
           data[i] == '\0')
        {
            result[j] = strview_new(buffer, i - previous_length);

            buffer = (char*)&data[i + 1];
            previous_length = i + 1;
            j++;
        }
    }
    
    return result;
}

int strview_split_yield(const char* data, const char* separator, strview_t* string_view)
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
