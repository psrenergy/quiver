cmake_minimum_required(VERSION 3.26.0)

project(quiver
    VERSION 0.13.6
    DESCRIPTION "Quiver library"
    LANGUAGES CXX C
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(QUIVER_BUILD_SHARED "Build shared library" ON)
option(QUIVER_BUILD_TESTS "Build test suite" ON)
option(QUIVER_BUILD_C_API "Build C API wrapper" OFF)
# Omit VERSION/SOVERSION on shared libraries, so the built files carry their plain
# names (libquiver.dylib) instead of a version infix plus unversioned symlinks.
# Default OFF: the shipped natives are addressed by their versioned names --
# bindings/julia/src/c_api.jl hardcodes libquiver.0.dylib, and scripts/ci/native_s3.sh
# and publish-s3.yml ship libquiver.0.dylib / libquiver.so.0 by name.
# Turned ON by the Dart hook, whose asset scanner skips symlinks and matches only the
# unversioned name. CMAKE_PLATFORM_NO_VERSIONED_SONAME is NOT a substitute: under the Xcode
# generator that hook selects, it drops the symlinks but keeps the versioned file name,
# leaving libquiver.0.10.5.dylib with an @rpath/libquiver.dylib install name that the
# scanner still matches nothing against.
option(QUIVER_UNVERSIONED_SHARED "Omit VERSION/SOVERSION on shared libraries" OFF)
if(NOT QUIVER_UNVERSIONED_SHARED)
    set(QUIVER_LIB_VERSION VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
endif()

# --- Python wheel packaging (scikit-build-core) ---
# When building via scikit-build-core (pip wheel / python -m build),
# SKBUILD is defined. Force C API on and disable tests for wheel builds.
if(DEFINED SKBUILD)
    set(QUIVER_BUILD_C_API ON CACHE BOOL "Build C API wrapper" FORCE)
    set(QUIVER_BUILD_TESTS OFF CACHE BOOL "Build test suite" FORCE)
endif()

# Include CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Dependencies)
include(Platform)

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Core library (and C API if enabled)
add_subdirectory(src)

# Tests
if(QUIVER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Source files for the format target (recursive, matching CI's clang-format check)
file(GLOB_RECURSE ALL_SOURCE_FILES
    ${PROJECT_SOURCE_DIR}/include/*.h
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    ${PROJECT_SOURCE_DIR}/src/*.h
    ${PROJECT_SOURCE_DIR}/tests/*.cpp
    ${PROJECT_SOURCE_DIR}/tests/*.h
)

# Format target
find_program(CLANG_FORMAT NAMES clang-format-22 clang-format)
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting code with clang-format"
    )
endif()

# Install native libraries into the Python wheel package directory
if(DEFINED SKBUILD)
    install(TARGETS quiver quiver_c
        LIBRARY DESTINATION quiverdb/_libs
        RUNTIME DESTINATION quiverdb/_libs
    )
    # Resolve the core beside the C API in the wheel's _libs directory.
    if(APPLE)
        set_target_properties(quiver_c PROPERTIES
            INSTALL_RPATH "@loader_path"
        )
    elseif(NOT WIN32)
        set_target_properties(quiver_c PROPERTIES
            INSTALL_RPATH "$ORIGIN"
        )
    endif()
endif()
