// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#include "libromano/vector.h"
#include "libromano/logger.h"

int main(int argc, char** argv)
{
    logger_init();

    logger_log(LogLevel_Info, "Creating a new vector");
    vector float_vec = vector_new(0, sizeof(float));
    
    logger_log(LogLevel_Info, "Pushing new elements in the vector");

    for(uint32_t i = 0; i < 100000; i++)
    {
        float f = (float)i;
        vector_push_back(&float_vec, &f);
    }

    logger_log(LogLevel_Info, "Vector size : %d", vector_size(float_vec));
    logger_log(LogLevel_Info, "Vector capacity : %d", vector_capacity(float_vec));
    logger_log(LogLevel_Info, "Vector element size : %d", vector_element_size(float_vec));

    logger_log(LogLevel_Info, "Vector element 65438 : %f", *(float*)vector_at(float_vec, 65438));

    logger_log(LogLevel_Info, "Inserting at position 3500");
    float f = 12.0f;
    vector_insert(&float_vec, &f, 3500);
    
    logger_log(LogLevel_Info, "Vector size : %d", vector_size(float_vec));
    logger_log(LogLevel_Info, "Vector capacity : %d", vector_capacity(float_vec));
    logger_log(LogLevel_Info, "Vector element size : %d", vector_element_size(float_vec));

    logger_log(LogLevel_Info, "Removing 1000 elements at position 78392 : %f", *(float*)vector_at(float_vec, 78392));
    
    for(uint32_t i = 0; i < 1000; i++)
    {
        vector_remove(&float_vec, 78392);
    }

    logger_log(LogLevel_Info, "Element at position %d : %f", 78392, *(float*)vector_at(float_vec, 78392));
    logger_log(LogLevel_Info, "Element at last position (%d) : %f", vector_size(float_vec) - 1, 
                                                                    *(float*)vector_at(float_vec, vector_size(float_vec) - 1));
    
    logger_log(LogLevel_Info, "Vector size : %d", vector_size(float_vec));
    logger_log(LogLevel_Info, "Vector capacity : %d", vector_capacity(float_vec));
    logger_log(LogLevel_Info, "Vector element size : %d", vector_element_size(float_vec));

    logger_log(LogLevel_Info, "Freeing vector");
    vector_free(float_vec);

    logger_release();

    return 0;
}