# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 - Present Romain Augier
# All rights reserved.

include(target_options)

file(GLOB BENCHMARK_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bench_*.c)

foreach(benchmark_file ${BENCHMARK_FILES})
    get_filename_component(BENCHMARK_NAME ${benchmark_file} NAME_WLE)

    add_executable(${BENCHMARK_NAME} ${benchmark_file})
    set_target_options(${BENCHMARK_NAME})
    target_link_libraries(${BENCHMARK_NAME} ${PROJECT_NAME})
endforeach()
