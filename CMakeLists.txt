# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 - Present Romain Augier
# All rights reserved.

include(CMakePrintHelpers)

cmake_minimum_required(VERSION 3.15)

project(libromano VERSION ${VERSION})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_VERBOSE_MAKEFILE ON)

if(RUN_TESTS EQUAL 1)
    message(STATUS "RUN_TESTS enabled, building and running tests")
    add_definitions(-DTESTS_DATA_DIR="${CMAKE_SOURCE_DIR}/tests_data")
    enable_testing()
endif()

# Maths library
find_library(MATH_LIBRARY m)

# OpenCL
# Usually OpenCL comes with CUDA on windows (this is how I install it) so we ask for Cuda toolkit and 
# then use the OpenCL provided by Nvidia
if(WIN32)
    find_package(CUDAToolkit)

    set(OpenCL_ROOT "${CUDA_TOOLKIT_ROOT_DIR}")
endif()

find_package(OpenCL)

if(OpenCL_FOUND)
    message(STATUS "Found OpenCL (version: ${OpenCL_VERSION_STRING}), enabling OpenCL support in libromano")
    set(ROMANO_HAS_OPENCL 1)
else()
    message(STATUS "Can't find OpenCL, disabling OpenCL support in libromano")
    set(ROMANO_HAS_OPENCL 0)
endif()

# Concurrent queue c api
set(concurrentqueue_c_api_ROOT "${CMAKE_SOURCE_DIR}/concurrentqueue/cqinstall/cmake")
find_package(concurrentqueue_c_api REQUIRED)

add_subdirectory(src)

if(RUN_TESTS EQUAL 1)
    add_subdirectory(tests)
endif()
