/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#if ROMANO_OPENCL

#include "libromano/opencl.h"
#include "libromano/logger.h"

#include <string.h>

const char* kernel = "__kernel void vector_mul(__global int* a, __global int* b, __global int* c)\n"
                     "{\n    int i = get_global_id(0);\n    c[i] = a[i] * b[i];\n    }";

int main(void)
{
    logger_init();

    logger_log_info("Starting OpenCL test");

    cl_device_id device;
    int32_t error;

    error = cl_create_device(&device);

    logger_log_info("%d = cl_create_device", error);

    if(error != 0)
    {
        return 1;
    }

    cl_context context;

    error = cl_create_context(&context, &device);

    logger_log_info("%d = cl_create_context", error);

    if(error != 0)
    {
        return 1;
    }

    cl_program program;

    error = cl_build_program_from_text(context, device, kernel, strlen(kernel), &program);

    logger_log_info("%d = cl_build_program_from_text", error);

    logger_log_info("Finished OpenCL test");

    logger_release();

    return 0;
}

#else

int main(void)
{
    return 0;
}

#endif /* ROMANO_OPENCL */