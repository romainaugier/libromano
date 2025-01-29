/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_OPENCL)
#define __LIBROMANO_OPENCL

#include "libromano/libromano.h"

#if ROMANO_OPENCL

#if ROMANO_APPLE
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif /* ROMANO_APPLE */

ROMANO_CPP_ENTER

/* Returns 0 on success */
ROMANO_API int32_t cl_create_device(cl_device_id* device);

/* Returns 0 on success */
ROMANO_API int32_t cl_create_context(cl_context* context, cl_device_id* device);

/* Returns 0 on success */
ROMANO_API int32_t cl_build_program_from_text(cl_context context,
                                              cl_device_id device,
                                              const char* text,
                                              const size_t text_length,
                                              cl_program* program);

/* Returns 0 on success */
ROMANO_API int32_t cl_build_program_from_file(cl_context context,
                                              cl_device_id device,
                                              const char* file_path,
                                              cl_program* program);

ROMANO_CPP_END

#endif /* ROMANO_OPENCL */

#endif /* !defined(__LIBROMANO_OPENCL) */