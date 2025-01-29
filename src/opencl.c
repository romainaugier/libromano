/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#if ROMANO_OPENCL 

#include "libromano/opencl.h"
#include "libromano/logger.h"

int32_t cl_create_device(cl_device_id* device)
{
    cl_platform_id platform;
    int32_t error;

    error = clGetPlatformIDs(1, &platform, NULL);

    if(error < 0)
    {
        logger_log_fatal("Could not identify an OpenCL platform (%d)", error);
        return error;
    }

    error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, device, NULL);

    if(error == CL_DEVICE_NOT_FOUND)
    {
        error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, device, NULL);
    }

    if(error < 0)
    {
        logger_log_fatal("Could not access any OpenCL device (%d)", error);
        return error;
    }

    return 0;
}

int32_t cl_create_context(cl_context* context, cl_device_id* device)
{
    int32_t error;

    error = 0;

    *context = clCreateContext(NULL, 1, device, NULL, NULL, &error);

    return error;
}

int32_t cl_build_program_from_text(cl_context context,
                                   cl_device_id device,
                                   const char* text,
                                   const size_t text_length,
                                   cl_program* program)
{
    size_t log_size;
    char* log;
    int32_t error;

    *program = clCreateProgramWithSource(context, 1, &text, &text_length, &error);

    if(error < 0)
    {
        logger_log_fatal("Could not create OpenCL program");
        return error;
    }

    error = clBuildProgram(*program, 0, NULL, NULL, NULL, NULL);

    if(error < 0)
    {
        clGetProgramBuildInfo(*program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

        log = (char*)calloc(log_size + 1, sizeof(char));

        clGetProgramBuildInfo(*program, device, CL_PROGRAM_BUILD_LOG, log_size + 1, log, NULL);

        logger_log_fatal("Could not build OpenCL program");
        logger_log_fatal("OpenCL log:\n%s", log);

        free(log);

        return error;
    }

    return 0;
}

int32_t cl_build_program_from_file(cl_context context,
                                   cl_device_id device,
                                   const char* file_path,
                                   cl_program* program)
{
    char* file_buffer;
    size_t file_buffer_size;
    FILE* file_handle;
    int32_t error;

    file_handle = fopen(file_path, "r");

    if(file_handle == NULL)
    {
        logger_log_fatal("Could not find OpenCL program file: %s", file_path);
        return 1;
    }

    fseek(file_handle, 0, SEEK_END);
    file_buffer_size = ftell(file_handle);
    rewind(file_handle);
    file_buffer = (char*)calloc(file_buffer_size + 1, sizeof(char));
    fread(file_buffer, sizeof(char), file_buffer_size, file_handle);
    fclose(file_handle);

    error = cl_build_program_from_text(context, device, file_buffer, file_buffer_size, program);

    free(file_buffer);

    return error;
}

#endif /* ROMANO_OPENCL */