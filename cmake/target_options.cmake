
function(set_target_options target_name)
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
        set(ROMANO_CLANG 1)
        set(CMAKE_C_FLAGS "-Wall -pedantic-errors")

        target_compile_options(${target_name} PRIVATE $<$<CONFIG:Debug>:-O0 -fsanitize=leak -fsanitize=address>)
        target_compile_options(${target_name} PRIVATE $<$<CONFIG:Release,RelWithDebInfo>:-O3 -march=native>)
    elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(ROMANO_GCC 1)
        set(CMAKE_C_FLAGS "-D_FORTIFY_SOURCES=2 -pipe -Wall -pedantic-errors")

        target_compile_options(${target_name} PRIVATE $<$<CONFIG:Debug>:-O0 -fsanitize=leak -fsanitize=address>)
        target_compile_options(${target_name} PRIVATE $<$<CONFIG:Release,RelWithDebInfo>:-O3 -ffast-math -march=native -ftree-vectorizer-verbose=2>)
    elseif (CMAKE_C_COMPILER_ID STREQUAL "Intel")
        set(ROMANO_INTEL 1)
    elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        set(ROMANO_MSVC 1)
        include(find_avx)

        # 4710 is "Function not inlined", we don't care it pollutes more than tells useful information about the code
        # 5045 is "Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified", again we don't care
        set(CMAKE_C_FLAGS "/Wall /wd4710 /wd5045") 

        target_compile_options(${target_name} PRIVATE $<$<CONFIG:Debug>:/fsanitize=address>)
        target_compile_options(${target_name} PRIVATE $<$<CONFIG:Release,RelWithDebInfo>:/O2 /GF /Ot /Oy /GT /GL /Oi ${AVX_FLAGS} /Zi /Gm- /Zc:inline /Qpar>)
    endif()
endfunction()