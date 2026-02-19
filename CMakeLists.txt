cmake_minimum_required(VERSION 3.21)

project(quiver
    VERSION 0.3.0
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
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

# Package config for find_package() support
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/quiverConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/quiverConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/quiverConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quiver
)

install(EXPORT quiverTargets
    FILE quiverTargets.cmake
    NAMESPACE quiver::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quiver
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/quiverConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/quiverConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quiver
)

# Source files for format and tidy targets
file(GLOB_RECURSE ALL_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/include/quiver/*.h
    ${CMAKE_SOURCE_DIR}/include/quiver/c/*.h
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/src/blob/*.cpp
    ${CMAKE_SOURCE_DIR}/src/blob/*.h
    ${CMAKE_SOURCE_DIR}/src/c/*.cpp
    ${CMAKE_SOURCE_DIR}/src/c/*.h
    ${CMAKE_SOURCE_DIR}/tests/*.cpp
    ${CMAKE_SOURCE_DIR}/tests/*.h
)

# Format target
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting code with clang-format"
    )
endif()

# Tidy target — runs clang-tidy via script to avoid command-line length limits on Windows
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    add_custom_target(tidy
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/tidy.bat
        COMMENT "Running clang-tidy"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
