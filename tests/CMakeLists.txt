file(GLOB_RECURSE TEST_FILES *.c)


foreach(test_file ${TEST_FILES})
    get_filename_component(TESTNAME ${test_file} NAME_WLE)
    message(STATUS "Adding romanolib test : ${TESTNAME}")

    add_executable(${TESTNAME} ${test_file})
    target_link_libraries(${TESTNAME} ${PROJECT_NAME})
    
    add_test(${TESTNAME} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TESTNAME})
endforeach()

# Copy all binaries from the bin in the runtime directory to link dynamic libraries to test executables
if(WIN32)
    set(DLL_DIR ${CMAKE_SOURCE_DIR}/bin)

    file(GLOB_RECURSE DLLS ${DLL_DIR}/*.dll)

    foreach(dll_file ${DLLS})
        add_custom_command(
            TARGET ${TESTNAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            ${dll_file} $<TARGET_FILE_DIR:${TESTNAME}>
        )
    endforeach(dll_file)
endif()