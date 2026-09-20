/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

#include "test.h"

#if ROMANO_OPENCL

#include "libromano/opencl.h"

#define NUM_ELEMENTS 1024

static const char g_kernel[] =
    "__kernel void vector_mul(__global const int* a, __global const int* b, __global int* c)\n"
    "{\n"
    "    int i = get_global_id(0);\n"
    "    c[i] = a[i] * b[i];\n"
    "}\n";

static bool g_has_device = false;
static cl_device_id g_device;
static cl_context g_context;

static bool setup(void)
{
    if(g_has_device)
        return true;

    if(cl_create_device(&g_device) != 0)
    {
        logger_log_warning("no OpenCL device available, skipping");
        return false;
    }

    if(!TEST_CHECK_EQ_INT(cl_create_context(&g_context, &g_device), 0))
        return false;

    g_has_device = true;

    return true;
}

static void run_vector_mul(cl_program program)
{
    int a[NUM_ELEMENTS];
    int b[NUM_ELEMENTS];
    int c[NUM_ELEMENTS];
    const size_t global_size = NUM_ELEMENTS;
    cl_int error = 0;
    cl_command_queue queue;
    cl_kernel kernel;
    cl_mem buffers[3];
    int i;

    for(i = 0; i < NUM_ELEMENTS; i++)
    {
        a[i] = i;
        b[i] = 3 - i;
    }

    queue = clCreateCommandQueue(g_context, g_device, 0, &error);
    TEST_ASSERT_EQ_INT(error, CL_SUCCESS);

    kernel = clCreateKernel(program, "vector_mul", &error);
    TEST_ASSERT_EQ_INT(error, CL_SUCCESS);

    buffers[0] = clCreateBuffer(g_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(a), a, &error);
    buffers[1] = clCreateBuffer(g_context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(b), b, &error);
    buffers[2] = clCreateBuffer(g_context, CL_MEM_WRITE_ONLY, sizeof(c), NULL, &error);
    TEST_ASSERT_EQ_INT(error, CL_SUCCESS);

    for(i = 0; i < 3; i++)
        TEST_CHECK_EQ_INT(clSetKernelArg(kernel, (cl_uint)i, sizeof(cl_mem), &buffers[i]), CL_SUCCESS);

    TEST_CHECK_EQ_INT(clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, NULL, 0, NULL, NULL), CL_SUCCESS);
    TEST_CHECK_EQ_INT(clEnqueueReadBuffer(queue, buffers[2], CL_TRUE, 0, sizeof(c), c, 0, NULL, NULL), CL_SUCCESS);

    for(i = 0; i < NUM_ELEMENTS; i++)
        TEST_ASSERT_EQ_INT(c[i], i * (3 - i));

    for(i = 0; i < 3; i++)
        clReleaseMemObject(buffers[i]);

    clReleaseKernel(kernel);
    clReleaseCommandQueue(queue);
}

static void test_build_from_text(void)
{
    cl_program program;

    if(!setup())
        return;

    TEST_ASSERT_EQ_INT(cl_build_program_from_text(g_context, g_device, g_kernel, strlen(g_kernel), &program), 0);
    run_vector_mul(program);
    clReleaseProgram(program);
}

static void test_build_from_file(void)
{
    const char* path = test_tmp_path("test_opencl_kernel.cl");
    FILE* file;
    cl_program program;

    if(!setup())
        return;

    file = fopen(path, "wb");
    TEST_ASSERT(file != NULL);
    fwrite(g_kernel, 1, sizeof(g_kernel) - 1, file);
    fclose(file);

    TEST_ASSERT_EQ_INT(cl_build_program_from_file(g_context, g_device, path, &program), 0);
    run_vector_mul(program);
    clReleaseProgram(program);

    TEST_CHECK(cl_build_program_from_file(g_context, g_device, test_tmp_path("missing.cl"), &program) != 0);
}

static void test_build_errors(void)
{
    const char* invalid = "__kernel void broken(__global int* a) { a[0] = undefined_symbol; }";
    cl_program program;

    if(!setup())
        return;

    logger_log_info("expecting an OpenCL build error below");
    TEST_CHECK(cl_build_program_from_text(g_context, g_device, invalid, strlen(invalid), &program) != 0);
    clReleaseProgram(program);
}

static void test_release(void)
{
    if(g_has_device)
        clReleaseContext(g_context);

    g_has_device = false;
    TEST_CHECK(true);
}

TEST_MAIN(
    TEST(test_build_from_text),
    TEST(test_build_from_file),
    TEST(test_build_errors),
    TEST(test_release),
)

#else

static void test_opencl_disabled(void)
{
    logger_log_info("libromano was built without OpenCL support, nothing to test");
}

TEST_MAIN(
    TEST(test_opencl_disabled),
)

#endif /* ROMANO_OPENCL */
