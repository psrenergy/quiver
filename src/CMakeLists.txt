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
    add_library(margaux SHARED ${PSR_SOURCES})
    target_compile_definitions(margaux PRIVATE PSR_DATABASE_EXPORTS)
else()
    add_library(margaux STATIC ${PSR_SOURCES})
    target_compile_definitions(margaux PUBLIC PSR_DATABASE_STATIC)
endif()

# Include directories
target_include_directories(margaux
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# sol2 safety settings
target_compile_definitions(margaux PRIVATE
    SOL_SAFE_NUMERICS=1
    SOL_SAFE_FUNCTION=1
)

# Link dependencies
target_link_libraries(margaux
    PUBLIC
        SQLite::SQLite3
    PRIVATE
        psr_compiler_options
        tomlplusplus::tomlplusplus
        spdlog::spdlog
        lua_library
        sol2
        ${CMAKE_DL_LIBS}
)

# Set library properties
set_target_properties(margaux PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME margaux
)

# Alias for use in subdirectories
add_library(psr::database ALIAS margaux)

# Installation rules
include(GNUInstallDirs)
install(TARGETS margaux
    EXPORT margauxTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/psr
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# C API wrapper (optional)
if(PSR_BUILD_C_API)
    add_library(margaux_c SHARED
        c_api_common.cpp
        c_api_database.cpp
        c_api_element.cpp
        c_api_lua_runner.cpp
    )

    target_compile_definitions(margaux_c PRIVATE
        PSR_DATABASE_C_EXPORTS
        PSR_VERSION="${PROJECT_VERSION}"
    )

    target_include_directories(margaux_c PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(margaux_c PRIVATE
        margaux
        psr_compiler_options
    )

    set_target_properties(margaux_c PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )

    add_library(psr::database_c ALIAS margaux_c)

    install(TARGETS margaux_c
        EXPORT margauxTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
