cmake_minimum_required(VERSION 3.15)

project(romanolib)

if(DEFINED RUN_TESTS)
    message(STATUS "RUN_TESTS defined, building and running tests")
    enable_testing()
endif()

include_directories(include)

add_subdirectory(src)

if(DEFINED RUN_TESTS)
    add_subdirectory(tests)
endif()