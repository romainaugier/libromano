/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/vector.h"
#include "libromano/logger.h"

#if ROMANO_DEBUG
#define NUM_LOOPS 100000
#else
#define NUM_LOOPS 1000000
#endif /* ROMANO_DEBUG */

#include <stdio.h>

void simple_dtor(void* element)
{
    void* to_free = *((void**)element);
    free(to_free);
}

int float_cmp(const void* x, const void* y)
{
    return (int)(*(const float*)x - *(const float*)y);
}

int main(void)
{
    logger_init();

    logger_log(LogLevel_Info, "Creating a new vector");
    Vector* float_vec = vector_new(0, sizeof(float));
    
    logger_log(LogLevel_Info, "Pushing new elements in the vector");

    size_t i;

    for(i = 0; i < NUM_LOOPS; i++)
    {
        float f = (float)i;
        
        if(i < (float)(NUM_LOOPS / 2))
        {
            vector_push_back(float_vec, &f);
        }
        else
        {
            vector_emplace_back(float_vec, float) = f;
        }
    }

    logger_log(LogLevel_Info, "Vector size : %d", vector_size(float_vec));
    logger_log(LogLevel_Info, "Vector capacity : %d", vector_capacity(float_vec));
    logger_log(LogLevel_Info, "Vector element size : %d", vector_element_size(float_vec));

    logger_log(LogLevel_Info, "Vector element 65438 : %f", *(float*)vector_at(float_vec, 65438));

    logger_log(LogLevel_Info, "Inserting at position 3500");
    int f = 12;
    vector_insert(float_vec, &f, 3500);

    if(vector_find(float_vec, &f) != 3500)
    {
        logger_log_error("Cannot find %d in vector", f);
        return 1;
    }
    
    logger_log(LogLevel_Info, "Vector size : %d", vector_size(float_vec));
    logger_log(LogLevel_Info, "Vector capacity : %d", vector_capacity(float_vec));
    logger_log(LogLevel_Info, "Vector element size : %d", vector_element_size(float_vec));

    logger_log(LogLevel_Info, "Removing 1000 elements at position 78392 : %f", *(float*)vector_at(float_vec, 78392));
    
    for(i = 0; i < 1000; i++)
    {
        vector_remove(float_vec, 78392);
    }

    logger_log(LogLevel_Info, "Element at position %d : %f", 78392, *(float*)vector_at(float_vec, 78392));
    logger_log(LogLevel_Info, "Element at last position (%d) : %f", vector_size(float_vec) - 1, 
                                                                    *(float*)vector_at(float_vec, vector_size(float_vec) - 1));
    
    logger_log(LogLevel_Info, "Vector size : %d", vector_size(float_vec));
    logger_log(LogLevel_Info, "Vector capacity : %d", vector_capacity(float_vec));
    logger_log(LogLevel_Info, "Vector element size : %d", vector_element_size(float_vec));

    for(i = 0; i < 10; i++)
    {
        logger_log(LogLevel_Info, "Vector at %d: %f", i, *(float*)vector_at(float_vec, i));
    }

    logger_log(LogLevel_Info, "Shuffling vector");

    vector_shuffle(float_vec, 18);

    for(i = 0; i < 10; i++)
    {
        logger_log(LogLevel_Info, "Vector at %d: %f", i, *(float*)vector_at(float_vec, i));
    }

    logger_log(LogLevel_Info, "Sorting vector");

    vector_sort(float_vec, &float_cmp);

    for(i = 0; i < 10; i++)
    {
        logger_log(LogLevel_Info, "Vector at %d: %f", i, *(float*)vector_at(float_vec, i));
    }

    logger_log(LogLevel_Info, "Vector front: %f", *(float*)vector_at(float_vec, 0));
    vector_pop_front(float_vec);
    logger_log(LogLevel_Info, "Vector front: %f", *(float*)vector_at(float_vec, 0));

    logger_log(LogLevel_Info, "Vector back: %f", *(float*)vector_back(float_vec));

    logger_log(LogLevel_Info, "Freeing vector");
    vector_free(float_vec);

    Vector* alloc_vec = vector_new(64, sizeof(void*));

    for(i = 0; i < 64; i++)
    {
        int* addr = (int*)malloc(sizeof(int));
        *addr = (int)i;

        vector_push_back(alloc_vec, &addr);
    }

    vector_free_with_dtor(alloc_vec, simple_dtor);

    logger_release();

    return 0;
}
