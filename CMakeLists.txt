# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 - Present Romain Augier
# All rights reserved.

cmake_minimum_required(VERSION 3.15)

project(libromano)

if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(ROMANO_CLANG 1)
elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(ROMANO_GCC 1)
elseif (CMAKE_C_COMPILER_ID STREQUAL "Intel")
    set(ROMANO_INTEL 1)
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    set(ROMANO_MSVC 1)
endif()

if(DEFINED RUN_TESTS)
    message(STATUS "RUN_TESTS defined, building and running tests")
    enable_testing()
endif()

# Maths library
find_library(MATH_LIBRARY m)

file(MAKE_DIRECTORY bin)

include_directories(include)

add_subdirectory(src)

if(DEFINED RUN_TESTS)
    add_subdirectory(tests)
endif()