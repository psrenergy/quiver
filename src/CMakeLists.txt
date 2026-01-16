# Core library sources
set(PSR_SOURCES
    database.cpp
    element.cpp
    lua_runner.cpp
    migration.cpp
    migrations.cpp
    result.cpp
    row.cpp
    schema.cpp
    schema_validator.cpp
    type_validator.cpp
)

# Build type
if(PSR_BUILD_SHARED)
    add_library(psr_database SHARED ${PSR_SOURCES})
    target_compile_definitions(psr_database PRIVATE PSR_DATABASE_EXPORTS)
else()
    add_library(psr_database STATIC ${PSR_SOURCES})
    target_compile_definitions(psr_database PUBLIC PSR_DATABASE_STATIC)
endif()

# Include directories
target_include_directories(psr_database
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# sol2 safety settings
target_compile_definitions(psr_database PRIVATE
    SOL_SAFE_NUMERICS=1
    SOL_SAFE_FUNCTION=1
)

# Link dependencies
target_link_libraries(psr_database
    PUBLIC
        SQLite::SQLite3
    PRIVATE
        psr_compiler_options
        tomlplusplus::tomlplusplus
        spdlog::spdlog
        lua_library
        sol2
)

# Set library properties
set_target_properties(psr_database PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME psr_database
)

# Alias for use in subdirectories
add_library(psr::database ALIAS psr_database)

# Installation rules
include(GNUInstallDirs)
install(TARGETS psr_database
    EXPORT psr_databaseTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/psr
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# C API wrapper (optional)
if(PSR_BUILD_C_API)
    add_library(psr_database_c SHARED
        c_api_common.cpp
        c_api_database.cpp
        c_api_element.cpp
        c_api_lua_runner.cpp
    )

    target_compile_definitions(psr_database_c PRIVATE
        PSR_DATABASE_C_EXPORTS
        PSR_VERSION="${PROJECT_VERSION}"
    )

    target_include_directories(psr_database_c PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(psr_database_c PRIVATE
        psr_database
        psr_compiler_options
    )

    set_target_properties(psr_database_c PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )

    add_library(psr::database_c ALIAS psr_database_c)

    install(TARGETS psr_database_c
        EXPORT psr_databaseTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
