/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/base64.h"

#include <string.h>

ROMANO_FORCE_INLINE size_t base64_get_encode_size(size_t data_sz)
{
    return ((data_sz + 2) / 3) * 4;
}

static const char* encode_table = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
};

bool base64_encode_scalar(const void* ROMANO_RESTRICT data,
                          size_t data_sz,
                          char* ROMANO_RESTRICT out_buffer,
                          size_t* out_sz)
{
    const uint8_t* in;
    size_t rem;
    size_t i;
    size_t g;
    size_t full_groups;
    uint32_t chunk;

    in = (const uint8_t*)data;

    *out_sz = 0;

    full_groups = data_sz / 3;

    for(g = 0; g < full_groups; g++)
    {
        i = g * 3;

        chunk = ((uint32_t)in[i + 0] << 16) |
                ((uint32_t)in[i + 1] <<  8) |
                ((uint32_t)in[i + 2]      );

        out_buffer[(*out_sz)++] = encode_table[(chunk >> 18) & 0x3F];
        out_buffer[(*out_sz)++] = encode_table[(chunk >> 12) & 0x3F];
        out_buffer[(*out_sz)++] = encode_table[(chunk >>  6) & 0x3F];
        out_buffer[(*out_sz)++] = encode_table[(chunk >>  0) & 0x3F];
    }

    rem = data_sz % 3;

    if(rem != 0)
    {
        chunk = 0;
        i = full_groups * 3;

        if(rem >= 1)
            chunk |= ((uint32_t)in[i + 0] << 16);

        if(rem >= 2)
            chunk |= ((uint32_t)in[i + 1] <<  8);

        out_buffer[(*out_sz)++] = encode_table[(chunk >> 18) & 0x3F];
        out_buffer[(*out_sz)++] = encode_table[(chunk >> 12) & 0x3F];

        if(rem == 1)
        {
            out_buffer[(*out_sz)++] = '=';
            out_buffer[(*out_sz)++] = '=';
        }
        else
        {
            out_buffer[(*out_sz)++] = encode_table[(chunk >> 6) & 0x3F];
            out_buffer[(*out_sz)++] = '=';
        }
    }

    return true;
}

char* base64_encode(const void* ROMANO_RESTRICT data, size_t data_sz, size_t* out_sz)
{
    char* buffer;
    size_t buffer_sz;

    buffer_sz = base64_get_encode_size(data_sz);
    *out_sz = 0;

    buffer = (char*)calloc(buffer_sz, sizeof(char));

    if(buffer == NULL)
    {
        return NULL;
    }

    if(!base64_encode_scalar(data, data_sz, buffer, out_sz))
    {
        return NULL;
    }

    return buffer;
}

static const uint8_t decode_table[80] = {
    62, -1, -1, -1, 63, 52, 53, 54, 55, 56,
    57, 58, 59, 60, 61, -1, -1, -1, -2, -1,
    -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,
     8,  9, 10, 11, 12, 13, 14, 15, 16, 17,
    18, 19, 20, 21, 22, 23, 24, 25, -1, -1,
    -1, -1, -1, -1, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

ROMANO_FORCE_INLINE size_t base64_get_decode_size(const char* ROMANO_RESTRICT data, size_t data_sz)
{
    size_t padding;

    if(data_sz == 0)
        return 0;

    if(data_sz % 4 != 0)
        return 0;

    padding = 0;

    if(data[data_sz - 1] == '=')
        padding++;

    if(data[data_sz - 2] == '=')
        padding++;

    return (data_sz / 4) * 3 - padding;
}

bool base64_decode_scalar(const char* ROMANO_RESTRICT data,
                          size_t data_sz,
                          uint8_t* ROMANO_RESTRICT out_buffer,
                          size_t* out_buffer_sz)
{
    size_t i;
    size_t j;
    uint32_t chunk;
    uint32_t padding;

    for(i = 0; i < (data_sz - 4); i += 4)
    {
        chunk = 0;

        chunk |= ((uint32_t)decode_table[data[i + 0] - '+']) << 18;
        chunk |= ((uint32_t)decode_table[data[i + 1] - '+']) << 12;
        chunk |= ((uint32_t)decode_table[data[i + 2] - '+']) << 6;
        chunk |= ((uint32_t)decode_table[data[i + 3] - '+']) << 0;

        out_buffer[(*out_buffer_sz)++] = (chunk >> 16);
        out_buffer[(*out_buffer_sz)++] = (chunk >> 8) & 0xFF;
        out_buffer[(*out_buffer_sz)++] = (chunk >> 0) & 0xFF;
    }

    padding = (uint32_t)(data[data_sz - 1] == '=') + (uint32_t)(data[data_sz - 2] == '=');

    chunk = 0;

    for(j = 0; j < (4 - padding); j++)
        chunk |= ((uint32_t)decode_table[data[i + j] - '+']) << ((3 - j) * 6);

    out_buffer[(*out_buffer_sz)++] = (chunk >> 16) & 0xFF;

    if(padding < 2)
        out_buffer[(*out_buffer_sz)++] = (chunk >> 8) & 0xFF;

    if(padding < 1)
        out_buffer[(*out_buffer_sz)++] = (chunk >> 0) & 0xFF;

    return true;
}

void* base64_decode(const char* ROMANO_RESTRICT data, size_t data_sz, size_t* out_sz)
{
    uint8_t* buffer;
    size_t buffer_sz;

    if(data_sz % 4 != 0)
        return NULL;

    buffer_sz = base64_get_decode_size(data, data_sz);
    *out_sz = 0;

    buffer = (uint8_t*)calloc(buffer_sz, sizeof(uint8_t));

    if(buffer == NULL)
    {
        return NULL;
    }

    if(!base64_decode_scalar(data, data_sz, buffer, out_sz))
    {
        return NULL;
    }

    return (void*)buffer;
}
