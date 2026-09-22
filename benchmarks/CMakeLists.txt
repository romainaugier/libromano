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

    # Windows has no rpath: put the library dll next to the executable
    if(WIN32)
        add_custom_command(
            TARGET ${BENCHMARK_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                $<TARGET_RUNTIME_DLLS:${BENCHMARK_NAME}>
                $<TARGET_FILE_DIR:${BENCHMARK_NAME}>
            COMMAND_EXPAND_LISTS
        )
    endif()
endforeach()
