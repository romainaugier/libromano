# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 - Present Romain Augier
# All rights reserved.

include(GNUInstallDirs)
include(target_options)

file(GLOB_RECURSE src_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

add_library(${PROJECT_NAME} SHARED ${src_files})

set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 99)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")

set_target_options(${PROJECT_NAME})

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC ROMANO_BUILD_SHARED)
target_compile_definitions(${PROJECT_NAME} PUBLIC ROMANO_HTTP=${ROMANO_HAS_HTTP})

target_include_directories(${PROJECT_NAME}
                           PUBLIC
                           $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_include_directories(${PROJECT_NAME} PRIVATE ${concurrentqueue_c_api_INCLUDE_DIR})

if(UNIX)
    target_compile_options(${PROJECT_NAME} PUBLIC "-pthread")
    target_link_libraries(${PROJECT_NAME} PUBLIC ${MATH_LIBRARY})
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC wsock32 ws2_32 Shlwapi Pathcch PowrProf dbghelp)
endif()

if(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ACCELERATE_NEW_LAPACK)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Accelerate")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE concurrentqueue::concurrentqueue_c_api)

if(ROMANO_MIMALLOC)
    set(MIMALLOC_DIR ${PROJECT_SOURCE_DIR}/ext/mimalloc)

    if(NOT EXISTS ${MIMALLOC_DIR}/src/static.c)
        message(FATAL_ERROR "ext/mimalloc is missing, run git submodule update --init (or configure with -DROMANO_MIMALLOC=OFF)")
    endif()

    add_library(romano_mimalloc OBJECT ${MIMALLOC_DIR}/src/static.c)
    set_target_properties(romano_mimalloc PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)
    target_include_directories(romano_mimalloc PRIVATE ${MIMALLOC_DIR}/include)
    target_compile_options(romano_mimalloc PRIVATE $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-w>)

    # Short-lived processes: no transparent huge pages (the first 2 MiB page fault costs ~1 ms at startup)
    # and 64 MiB arenas, MIMALLOC_ALLOW_THP=1 / MIMALLOC_ARENA_RESERVE override them at runtime
    target_compile_definitions(romano_mimalloc PRIVATE MI_DEFAULT_ALLOW_THP=0 MI_DEFAULT_ARENA_RESERVE=65536L)

    target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:romano_mimalloc>)
    target_include_directories(${PROJECT_NAME} PRIVATE ${MIMALLOC_DIR}/include)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ROMANO_MIMALLOC)

    if(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE psapi shell32 user32 advapi32 bcrypt)
    endif()
endif()

install(
    TARGETS ${PROJECT_NAME}
    EXPORT "${PROJECT_NAME}Targets"
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(
    EXPORT "${PROJECT_NAME}Targets"
    FILE "${PROJECT_NAME}Targets.cmake"
    NAMESPACE "${PROJECT_NAME}::"
    DESTINATION cmake
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    PATH_VARS CMAKE_INSTALL_LIBDIR CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_BINDIR
    INSTALL_DESTINATION ${CMAKE_INSTALL_PREFIX}/cmake
)

write_basic_package_version_file(
    "${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION cmake
)
