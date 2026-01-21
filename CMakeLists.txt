cmake_minimum_required(VERSION 3.21)

project(margaux
    VERSION 1.0.0
    DESCRIPTION "Cross-platform C++ SQLite wrapper library"
    LANGUAGES CXX C
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(DECK_DATABASE_BUILD_SHARED "Build shared library" ON)
option(DECK_DATABASE_BUILD_TESTS "Build test suite" ON)
option(DECK_DATABASE_BUILD_C_API "Build C API wrapper" OFF)

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
if(DECK_DATABASE_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

# Package config for find_package() support
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/margauxConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/margauxConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/margauxConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/margaux
)

install(EXPORT margauxTargets
    FILE margauxTargets.cmake
    NAMESPACE margaux::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/margaux
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/margauxConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/margauxConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/margaux
)

# Format target
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/include/margaux/*.h
        ${CMAKE_SOURCE_DIR}/include/margaux/c/*.h
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.h
        ${CMAKE_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_SOURCE_DIR}/tests/*.h
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting code with clang-format"
    )
endif()
