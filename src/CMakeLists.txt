# Core library sources
set(QUIVER_SOURCES
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
if(QUIVER_BUILD_SHARED)
    add_library(quiver_database SHARED ${QUIVER_SOURCES})
    target_compile_definitions(quiver_database PRIVATE QUIVER_DATABASE_EXPORTS)
else()
    add_library(quiver_database STATIC ${QUIVER_SOURCES})
    target_compile_definitions(quiver_database PUBLIC QUIVER_DATABASE_STATIC)
endif()

# Include directories
target_include_directories(quiver_database
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# sol2 safety settings
target_compile_definitions(quiver_database PRIVATE
    SOL_SAFE_NUMERICS=1
    SOL_SAFE_FUNCTION=1
)

# Link dependencies
target_link_libraries(quiver_database
    PUBLIC
        SQLite::SQLite3
    PRIVATE
        quiver_compiler_options
        tomlplusplus::tomlplusplus
        spdlog::spdlog
        lua_library
        sol2
        ${CMAKE_DL_LIBS}
)

# Set library properties
set_target_properties(quiver_database PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME quiver_database
)

# Alias for use in subdirectories
add_library(quiver::database ALIAS quiver_database)

# Installation rules
include(GNUInstallDirs)
install(TARGETS quiver_database
    EXPORT quiver_databaseTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/quiver
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# C API wrapper (optional)
if(QUIVER_BUILD_C_API)
    add_library(quiver_database_c SHARED
        c_api_common.cpp
        c_api_database.cpp
        c_api_element.cpp
        c_api_lua_runner.cpp
    )

    target_compile_definitions(quiver_database_c PRIVATE
        QUIVER_DATABASE_C_EXPORTS
        QUIVER_VERSION="${PROJECT_VERSION}"
    )

    target_include_directories(quiver_database_c PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(quiver_database_c PRIVATE
        quiver_database
        quiver_compiler_options
    )

    set_target_properties(quiver_database_c PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )

    add_library(quiver::database_c ALIAS quiver_database_c)

    install(TARGETS quiver_database_c
        EXPORT quiver_databaseTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
