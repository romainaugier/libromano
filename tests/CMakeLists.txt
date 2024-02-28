# SPDX-License-Identifier: BSD-3-Clause 
# Copyright (c) 2023 - Present Romain Augier 
# All rights reserved. 

include(target_options)

file(GLOB_RECURSE TEST_FILES *.c)

foreach(test_file ${TEST_FILES})
    get_filename_component(TESTNAME ${test_file} NAME_WLE)
    message(STATUS "Adding libromano test : ${TESTNAME}")

    add_executable(${TESTNAME} ${test_file})
    set_target_options(${TESTNAME})
    target_link_libraries(${TESTNAME} ${PROJECT_NAME})
    
    add_test(${TESTNAME} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TESTNAME})
endforeach()

# Copy clang asan dll to the tests directory when building in debug mode
# along pdb files

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    get_filename_component(CL_DIR ${CMAKE_C_COMPILER} DIRECTORY)

    set(ASAN_COPY_COMMAND
        ${CMAKE_COMMAND} -E copy_if_different ${CL_DIR}/clang_rt.asan_dynamic-x86_64.dll $<TARGET_FILE_DIR:${TESTNAME}>
    )

    add_custom_command(
        TARGET ${TESTNAME} POST_BUILD
        COMMAND "$<$<CONFIG:Debug>:${ASAN_COPY_COMMAND}>"
        COMMAND_EXPAND_LISTS
    )

    set(PDB_COPY_COMMAND
        ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.pdb $<TARGET_FILE_DIR:${TESTNAME}>)

    add_custom_command(
        TARGET ${TESTNAME} POST_BUILD
        COMMAND "$<$<CONFIG:Debug,RelWithDebInfo>:${PDB_COPY_COMMAND}>"
        COMMAND_EXPAND_LISTS
    )
endif()

# Copy libromano shared lib to the tests bin directory

add_custom_command(
    TARGET ${TESTNAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${TESTNAME}>
)