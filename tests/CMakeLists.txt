# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 - Present Romain Augier
# All rights reserved.

include(target_options)

set(TESTS_TMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/tmp")

add_library(romano_test STATIC test.c)
set_target_options(romano_test)
target_link_libraries(romano_test PUBLIC ${PROJECT_NAME})
target_include_directories(romano_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(romano_test PUBLIC TESTS_TMP_DIR="${TESTS_TMP_DIR}")

file(GLOB TEST_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.c)

set(NETWORK_TESTS test_socket test_socket_server test_http)
set(SLOW_TESTS test_hashmap test_atomic test_numeric test_thread test_threadpool)

foreach(test_file ${TEST_FILES})
    get_filename_component(TESTNAME ${test_file} NAME_WLE)

    if(TESTNAME STREQUAL "test_http" AND NOT ROMANO_HAS_HTTP EQUAL 1)
        continue()
    endif()

    message(STATUS "Adding libromano test : ${TESTNAME}")

    add_executable(${TESTNAME} ${test_file})
    set_target_options(${TESTNAME})
    target_link_libraries(${TESTNAME} romano_test)

    # The sgemm driver and kernels are internal (not exported from the shared library), the
    # test compiles them directly to drive the kernels with custom block sizes
    if(TESTNAME STREQUAL "test_math_sgemm")
        file(GLOB SGEMM_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/math_linalg32_sgemm*.c)
        target_sources(${TESTNAME} PRIVATE ${SGEMM_SOURCES} ${PROJECT_SOURCE_DIR}/src/math_linalg_parallel.c)
        target_include_directories(${TESTNAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    endif()

    add_test(NAME ${TESTNAME}
             COMMAND ${TESTNAME}
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    set(TEST_LABELS unit)
    set(TEST_TIMEOUT 120)

    if(TESTNAME IN_LIST NETWORK_TESTS)
        list(APPEND TEST_LABELS network)
    endif()

    if(TESTNAME IN_LIST SLOW_TESTS)
        list(APPEND TEST_LABELS slow)
        set(TEST_TIMEOUT 600)
    endif()

    set_tests_properties(${TESTNAME} PROPERTIES LABELS "${TEST_LABELS}" TIMEOUT ${TEST_TIMEOUT})

    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        get_filename_component(CL_DIR ${CMAKE_C_COMPILER} DIRECTORY)

        set(ASAN_COPY_COMMAND
            ${CMAKE_COMMAND} -E copy_if_different ${CL_DIR}/clang_rt.asan_dynamic-x86_64.dll $<TARGET_FILE_DIR:${TESTNAME}>)

        set(PDB_COPY_COMMAND
            ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.pdb $<TARGET_FILE_DIR:${TESTNAME}>)

        add_custom_command(
            TARGET ${TESTNAME} POST_BUILD
            COMMAND "$<$<CONFIG:Debug,RelWithDebInfo>:${ASAN_COPY_COMMAND}>"
            COMMAND "$<$<CONFIG:Debug,RelWithDebInfo>:${PDB_COPY_COMMAND}>"
            COMMAND_EXPAND_LISTS
        )
    endif()

    if(WIN32)
        add_custom_command(
            TARGET ${TESTNAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                $<TARGET_RUNTIME_DLLS:${TESTNAME}>
                $<TARGET_FILE_DIR:${TESTNAME}>
            COMMAND_EXPAND_LISTS
        )
    endif()
endforeach()

foreach(mode 0 1 4 invalid)
    add_test(NAME test_simd_env_${mode}
             COMMAND test_simd test_env test_features
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(test_simd_env_${mode} PROPERTIES
                         LABELS unit
                         ENVIRONMENT "LIBROMANO_VECTORIZATION=${mode}")
endforeach()
