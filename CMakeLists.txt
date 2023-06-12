# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 - Present Romain Augier
# All rights reserved.

include(CMakePrintHelpers)

cmake_minimum_required(VERSION 3.15)

project(libromano)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

message(STATUS ${CMAKE_MODULE_PATH})

if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(ROMANO_CLANG 1)
    set(CLANG_FLAGS "-Wall -pedantic-errors")

    if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        set(CMAKE_C_FLAGS "${CLANG_FLAGS} -O0 -fsanitize=leak -fsanitize=address")
    else()    
        set(CMAKE_C_FLAGS "${CLANG_FLAGS} -O3 -march=native")
    endif()
elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(ROMANO_GCC 1)
    set(GCC_FLAGS "-D_FORTIFY_SOURCES=2 -pipe -Wall -pedantic-errors")

    if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        set(CMAKE_C_FLAGS "${GCC_FLAGS} -O0 -fsanitize=leak -fsanitize=address")
    else()
        set(CMAKE_C_FLAGS "${GCC_FLAGS} -O3 -ffast-math -march=native -ftree-vectorizer-verbose=2")
    endif()
elseif (CMAKE_C_COMPILER_ID STREQUAL "Intel")
    set(ROMANO_INTEL 1)
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    set(ROMANO_MSVC 1)
    set(MSVC_FLAGS "/Wall /wd4710")  # 4710 is function not inlined, we don't care it pollutes more than tells useful information about the code

    if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        set(CMAKE_C_FLAGS "${MSVC_FLAGS} /O0 /fsanitize=address /fsanitize=leak")
    else()
        include(find_avx)
        set(CMAKE_C_FLAGS "${MSVC_FLAGS} /O2 /GF /Ot /Oy /GT /GL /Oi ${AVX_FLAGS} /Zi /Gm- /Zc:inline /Qpar")
    endif()
endif()

set(CMAKE_VERBOSE_MAKEFILE ON)

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
