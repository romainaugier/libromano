file(GLOB_RECURSE src_files *.c)

add_definitions(-DROMANO_BUILD_SHARED)


add_library(${PROJECT_NAME} SHARED ${src_files})
set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 90)
set_target_properties(${PROJECT_NAME} PROPERTIES CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set_target_properties(${PROJECT_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/static)

if(UNIX)
    target_compile_options(${PROJECT_NAME} PUBLIC "-pthread")
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC wsock32 ws2_32)
endif()


# Copy to shared lib to the bin directory
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_SOURCE_DIR}/bin
)