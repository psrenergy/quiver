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
    database_csv.cpp
    database_describe.cpp
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

# sol2 safety settings
target_compile_definitions(quiver PRIVATE
    SOL_SAFE_NUMERICS=1
    SOL_SAFE_FUNCTION=1
)

# MSVC needs /bigobj for sol2 heavy template instantiations
# Other Windows compilers need -Wa,-mbig-obj for the same reason
if(MSVC)
    set_source_files_properties(lua_runner.cpp PROPERTIES COMPILE_OPTIONS "/bigobj")
elseif(WIN32)
    set_source_files_properties(lua_runner.cpp PROPERTIES COMPILE_OPTIONS "-Wa,-mbig-obj")
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
        rapidcsv
        ${CMAKE_DL_LIBS}
)

# Set library properties
set_target_properties(quiver PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME quiver
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
        c/database.cpp
        c/database_csv.cpp
        c/database_create.cpp
        c/database_read.cpp
        c/database_update.cpp
        c/database_metadata.cpp
        c/database_query.cpp
        c/database_time_series.cpp
        c/database_transaction.cpp
        c/element.cpp
        c/lua_runner.cpp
    )

    target_compile_definitions(quiver_c PRIVATE
        QUIVER_C_EXPORTS
        QUIVER_VERSION="${PROJECT_VERSION}"
    )

    target_include_directories(quiver_c PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(quiver_c PRIVATE
        quiver
        quiver_compiler_options
    )

    set_target_properties(quiver_c PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
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
