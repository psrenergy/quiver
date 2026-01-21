# Compiler warnings and options
add_library(margaux_compiler_options INTERFACE)

if(MSVC)
    target_compile_options(margaux_compiler_options INTERFACE
        /W4
        /permissive-
        /Zc:__cplusplus
        /utf-8
    )
    # Disable specific warnings
    target_compile_options(margaux_compiler_options INTERFACE
        /wd4251  # DLL interface warning
    )
else()
    target_compile_options(margaux_compiler_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
    # Static link runtime libraries on MinGW for portable binaries
    if(MINGW)
        target_link_options(margaux_compiler_options INTERFACE
            -static-libgcc
            -static-libstdc++
            -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic
        )
    endif()
endif()

# Position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
