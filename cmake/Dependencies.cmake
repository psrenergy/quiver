include(FetchContent)

# SQLite via FetchContent
FetchContent_Declare(sqlite3
    GIT_REPOSITORY https://github.com/sjinks/sqlite3-cmake.git
    GIT_TAG v3.50.2
)
FetchContent_MakeAvailable(sqlite3)

# toml++ for TOML parsing
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# spdlog for logging
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(spdlog)

# Lua 5.4.8 via lua-cmake wrapper
set(LUA_BUILD_INTERPRETER OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_COMPILER OFF CACHE BOOL "" FORCE)
set(LUA_TESTS "None" CACHE STRING "" FORCE)
FetchContent_Declare(lua
    GIT_REPOSITORY https://gitlab.com/codelibre/lua/lua-cmake.git
    GIT_TAG lua-cmake/v5.4.8.0
)
FetchContent_MakeAvailable(lua)

# sol2 for Lua C++ bindings
FetchContent_Declare(sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(sol2)

# rapidcsv for CSV I/O
FetchContent_Declare(rapidcsv
    GIT_REPOSITORY https://github.com/d99kris/rapidcsv.git
    GIT_TAG v8.90
)
FetchContent_MakeAvailable(rapidcsv)

# GoogleTest for testing
if(QUIVER_BUILD_TESTS)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
    )
    # Prevent overriding parent project's compiler/linker settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()
