/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/base64.h"

ROMANO_FORCE_INLINE size_t base64_get_encode_size(size_t data_sz)
{
    return (data_sz + 3) & ~0x3;
}

ROMANO_FORCE_INLINE size_t base64_get_decode_size(size_t data_sz)
{
    return (size_t)((double)data_sz * 1.25) + 1;
}

static const char* encode_table = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
};

bool base64_encode_scalar(const void* ROMANO_RESTRICT data,
                          size_t data_sz,
                          char* ROMANO_RESTRICT out_buffer,
                          size_t* out_sz)
{
    size_t loop_sz;
    size_t i;
    size_t j;
    uint32_t chunk;

    loop_sz = data_sz % 3;

    for(i = 0; i < loop_sz; i += 3)
    {
        chunk = 0;
        chunk |= ((uint8_t*)data)[i + 0] << 0;
        chunk |= ((uint8_t*)data)[i + 1] << 8;
        chunk |= ((uint8_t*)data)[i + 2] << 16;

        for(j = 0; j < 4; j++)
        {
            out_buffer[*out_sz] = encode_table[(chunk >> (j * 6)) & 0x3F];
            (*out_sz)++;
        }
    }

    chunk = 0;

    j = i;

    for(; i < data_sz; i++)
        chunk |= ((uint8_t*)data)[i] << (i * 8);

    for(; j < data_sz + 1; j++)
    {
        out_buffer[*out_sz] = encode_table[(chunk >> (j * 6)) & 0x3F];
        (*out_sz)++;
    }

    while((*out_sz % 4) != 0)
    {
        out_buffer[*out_sz] = '=';
        (*out_sz)++;
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

static const uint8_t decode_table[64] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
};

void* base64_decode(const char* ROMANO_RESTRICT data, size_t data_siz, size_t* out_sz)
{

}
