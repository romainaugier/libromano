file(GLOB_RECURSE src_files *.c)

add_definitions(-DROMANO_BUILD_DLL)

add_library(${PROJECT_NAME} SHARED ${src_files})
set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 90)

# Copy to the bin directory
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_SOURCE_DIR}/bin
)