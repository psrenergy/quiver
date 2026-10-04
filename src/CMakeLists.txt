# Core library sources
set(QUIVER_SOURCES
    database.cpp
    database_create.cpp
    database_read.cpp
    database_update.cpp
    database_delete.cpp
    database_metadata.cpp
    database_time_series.cpp
    database_query.cpp
    database_csv_export.cpp
    database_csv_import.cpp
    csv/csv_read.cpp
    csv/csv_write.cpp
    database_describe.cpp
    element.cpp
    lua_runner/binary.cpp
    lua_runner/csv.cpp
    lua_runner/database.cpp
    lua_runner/database_create.cpp
    lua_runner/database_csv_export.cpp
    lua_runner/database_csv_import.cpp
    lua_runner/database_delete.cpp
    lua_runner/database_describe.cpp
    lua_runner/database_metadata.cpp
    lua_runner/database_query.cpp
    lua_runner/database_read.cpp
    lua_runner/database_time_series.cpp
    lua_runner/database_update.cpp
    lua_runner/internal.h
    lua_runner/lua_runner.cpp
    lua_runner/path_policy.cpp
    lua_runner/path_policy.h
    lua_runner/return_json.cpp
    migration.cpp
    migrations.cpp
    result.cpp
    row.cpp
    schema.cpp
    schema_validator.cpp
    type_validator.cpp
    ui_metadata.cpp
    binary/binary_file.cpp
    binary/iteration.cpp
    binary/csv_converter.cpp
    binary/binary_metadata.cpp
    binary/time_properties.cpp
    expression/expression.cpp
    expression/expression_file.cpp
    expression/expression_scalar.cpp
    expression/expression_binary.cpp
    expression/expression_unary.cpp
    expression/expression_ternary.cpp
    expression/expression_aggregate.cpp
    expression/expression_aggregate_agents.cpp
    expression/expression_select_agents.cpp
    expression/expression_rename_agents.cpp
)

# Build type
if(QUIVER_BUILD_SHARED)
    add_library(quiver SHARED ${QUIVER_SOURCES})
    target_compile_definitions(quiver PRIVATE QUIVER_EXPORTS)
else()
    add_library(quiver STATIC ${QUIVER_SOURCES})
    target_compile_definitions(quiver PUBLIC QUIVER_STATIC)
endif()

# Include directories
target_include_directories(quiver
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# sol2 safety settings: sol2's safeties are on in every build, as the backstop behind the explicit
# Pattern 1 checks in src/lua_runner/. sol2 never prints a caught error to the host's stderr.
# SOL_SAFE_NUMERICS is implied by SOL_ALL_SAFETIES_ON but stays explicit because the code depends on it.
# The checked getter and the stack check are off in every build: with them on, a Release bulk read
# cost 16% more. lua_cell_as and the key checks cover the getter. Never turn off
# SOL_SAFE_FUNCTION_CALLS or SOL_SAFE_USERTYPE (the argument and `self` checks).
target_compile_definitions(quiver PRIVATE
    SOL_ALL_SAFETIES_ON=1
    SOL_PRINT_ERRORS=0
    SOL_SAFE_NUMERICS=1
    SOL_NO_NIL=1
    SOL_SAFE_GETTER=0
    SOL_SAFE_STACK_CHECK=0
)

# sol2's template depth needs /bigobj (MSVC) or -Wa,-mbig-obj (other Windows compilers) in every
# src/lua_runner/ TU; set for the whole target so a new file cannot miss it.
if(MSVC)
    target_compile_options(quiver PRIVATE /bigobj)
elseif(WIN32)
    target_compile_options(quiver PRIVATE -Wa,-mbig-obj)
endif()

# Link dependencies
target_link_libraries(quiver
    PUBLIC
        SQLite::SQLite3
    PRIVATE
        quiver_compiler_options
        tomlplusplus::tomlplusplus
        spdlog::spdlog
        lua_library
        sol2
        csv
)

# Set library properties
# QUIVER_LIB_VERSION is VERSION/SOVERSION, or empty when QUIVER_UNVERSIONED_SHARED is on
# (set next to the option in the root CMakeLists.txt).
set_target_properties(quiver PROPERTIES
    ${QUIVER_LIB_VERSION}
    PREFIX "lib"
)

# Alias for use in subdirectories
add_library(quiver::database ALIAS quiver)

# Installation rules
include(GNUInstallDirs)
if(NOT DEFINED SKBUILD)
    install(TARGETS quiver
        EXPORT quiverTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/quiver
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

# C API wrapper (optional)
if(QUIVER_BUILD_C_API)
    add_library(quiver_c SHARED
        c/common.cpp
        c/options.cpp
        c/database.cpp
        c/database_csv_export.cpp
        c/database_csv_import.cpp
        c/database_create.cpp
        c/database_delete.cpp
        c/database_read.cpp
        c/database_update.cpp
        c/database_metadata.cpp
        c/database_query.cpp
        c/database_time_series.cpp
        c/database_transaction.cpp
        c/element.cpp
        c/lua_runner.cpp
        c/binary/binary_file.cpp
        c/binary/csv_converter.cpp
        c/binary/binary_metadata.cpp
        c/expression/expression.cpp
    )

    target_compile_definitions(quiver_c PRIVATE
        QUIVER_C_EXPORTS
        QUIVER_VERSION="${PROJECT_VERSION}"
    )

    target_include_directories(quiver_c
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(quiver_c PRIVATE
        quiver
        quiver_compiler_options
    )

    set_target_properties(quiver_c PROPERTIES
        ${QUIVER_LIB_VERSION}
        PREFIX "lib"
    )

    add_library(quiver::database_c ALIAS quiver_c)

    if(NOT DEFINED SKBUILD)
        install(TARGETS quiver_c
            EXPORT quiverTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()
endif()

# CLI executable
add_executable(quiver_cli cli/main.cpp)

target_compile_definitions(quiver_cli PRIVATE
    QUIVER_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(quiver_cli PRIVATE
    quiver
    quiver_compiler_options
    argparse
)
