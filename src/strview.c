/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/strview.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>


strview strview_new(const char* data, size_t count)
{
   strview sv;
   sv.size = count;
   sv.data = data;
   return sv;
}

strview* strview_split(const char* data, const char* separator, size_t* count)
{
    size_t i, j;
    size_t data_length;
    size_t previous_length;
    strview* result;
    char* buffer;
    
    *count = 0;
    data_length = strlen(data);

    for(i = 0; i < data_length; i++)
    {
        if(data[i] == (char)separator[0]) (*count)++;
    }

    (*count)++;

    result = malloc(*count * sizeof(strview));
    buffer = data;
    previous_length = 0;
    j = 0;

    for(i = 0; i < (data_length + 1); i++)
    {
        if(data[i] == (char)separator[0] || 
           data[i] == '\0')
        {
            result[j] = strview_new(buffer, i - previous_length);

            buffer = &data[i + 1];
            previous_length = i + 1;
            j++;
        }
    }
    
    return result;
}

strview strview_trim(const char* data)
{
   strview result;

   while(isspace((unsigned char)*data)) data++;

   if(*data == 0)
   {
      return STRVIEW_NULL;
   }

   result.data = data;
   result.size = strlen(data);

   while((result.size > 0 && isspace(data[result.size--])) || data[result.size--] == '\0')
   {
      continue;
   }

   return result;
}
