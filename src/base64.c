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

    buffer = (char*)calloc(buffer_sz > 0 ? buffer_sz : 1, sizeof(char));

    if(buffer == NULL)
    {
        return NULL;
    }

    if(!base64_encode_scalar(data, data_sz, buffer, out_sz))
    {
        free(buffer);
        return NULL;
    }

    return buffer;
}

#define BASE64_INVALID 0xFF

static const uint8_t decode_table[80] = {
    62, 0xFF, 0xFF, 0xFF, 63, 52, 53, 54, 55, 56,
    57, 58, 59, 60, 61, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF,  0,  1,  2,  3,  4,  5,  6,  7,
     8,  9, 10, 11, 12, 13, 14, 15, 16, 17,
    18, 19, 20, 21, 22, 23, 24, 25, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

ROMANO_FORCE_INLINE uint8_t base64_decode_char(const char c)
{
    const uint8_t index = (uint8_t)c - (uint8_t)'+';

    return index < sizeof(decode_table) ? decode_table[index] : BASE64_INVALID;
}

static size_t base64_get_padding(const char* ROMANO_RESTRICT data, size_t data_sz)
{
    if(data_sz == 0)
        return 0;

    if(data[data_sz - 1] != '=')
        return 0;

    return data[data_sz - 2] == '=' ? 2 : 1;
}

bool base64_decode_scalar(const char* ROMANO_RESTRICT data,
                          size_t data_sz,
                          uint8_t* ROMANO_RESTRICT out_buffer,
                          size_t* out_buffer_sz)
{
    const size_t padding = base64_get_padding(data, data_sz);
    size_t i;
    size_t j;

    for(i = 0; i < data_sz; i += 4)
    {
        const size_t chars = i + 4 == data_sz ? 4 - padding : 4;
        uint32_t chunk = 0;

        for(j = 0; j < chars; j++)
        {
            const uint8_t value = base64_decode_char(data[i + j]);

            if(value == BASE64_INVALID)
                return false;

            chunk |= (uint32_t)value << ((3 - j) * 6);
        }

        /* Non-canonical encodings (non-zero bits in the padded part) are rejected */
        if((padding == 1 && chars == 3 && (chunk & 0xFF) != 0) ||
           (padding == 2 && chars == 2 && (chunk & 0xFFFF) != 0))
            return false;

        out_buffer[(*out_buffer_sz)++] = (uint8_t)(chunk >> 16);

        if(chars > 2)
            out_buffer[(*out_buffer_sz)++] = (uint8_t)(chunk >> 8);

        if(chars > 3)
            out_buffer[(*out_buffer_sz)++] = (uint8_t)chunk;
    }

    return true;
}

void* base64_decode(const char* ROMANO_RESTRICT data, size_t data_sz, size_t* out_sz)
{
    uint8_t* buffer;
    size_t buffer_sz;

    *out_sz = 0;

    if(data_sz % 4 != 0)
        return NULL;

    buffer_sz = (data_sz / 4) * 3 - base64_get_padding(data, data_sz);

    buffer = (uint8_t*)calloc(buffer_sz > 0 ? buffer_sz : 1, sizeof(uint8_t));

    if(buffer == NULL)
        return NULL;

    if(!base64_decode_scalar(data, data_sz, buffer, out_sz))
    {
        free(buffer);
        *out_sz = 0;
        return NULL;
    }

    return (void*)buffer;
}
