# SPDX-License-Identifier: BSD-3-Clause 
# Copyright (c) 2023 - Present Romain Augier 
# All rights reserved. 

file(GLOB_RECURSE TEST_FILES *.c)

foreach(test_file ${TEST_FILES})
    get_filename_component(TESTNAME ${test_file} NAME_WLE)
    message(STATUS "Adding libromano test : ${TESTNAME}")

    add_executable(${TESTNAME} ${test_file})
    target_link_libraries(${TESTNAME} ${PROJECT_NAME})
    
    add_test(${TESTNAME} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TESTNAME})
endforeach()

# Copy all binaries from the bin in the runtime directory to link dynamic libraries to test executables
add_custom_command(
    TARGET ${TESTNAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${TESTNAME}>
)
