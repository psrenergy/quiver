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
# NOTE: lua-cmake v5.4.8.0 has no switch to skip the lua/luac binaries. The
# LUA_BUILD_INTERPRETER and LUA_BUILD_COMPILER options previously set here do not exist
# upstream and were silently doing nothing. Its lua_bin/luac_bin are unconditional
# add_executable()s in the `all` target, so a plain `cmake --build build` DOES build two
# binaries this project never uses (~550 KB) -- only the Dart hook escapes that, by asking for
# the `quiver`/`quiver_c` targets explicitly. Do not "fix" that with
# `set_target_properties(lua_bin luac_bin PROPERTIES EXCLUDE_FROM_ALL YES)`: lua-cmake installs
# both unconditionally, so `cmake --install` then dies with "file INSTALL cannot find .../lua"
# and takes the scikit-build-core wheel build with it (which is also why pyproject.toml's
# `wheel.exclude = ["bin", "lib", "include", "share"]` is load-bearing -- those are the four
# directories lua-cmake writes into). Excluding them properly needs
# `FetchContent_Declare(... EXCLUDE_FROM_ALL)`, which is CMake >= 3.28; the project floor is 3.26.
set(LUA_TESTS "None" CACHE STRING "" FORCE)
# Skips the find_package(Readline)/find_package(Editline) probes, which exist only to give
# lua_bin a line editor. Unlike the two options above, this one does exist upstream.
set(LUA_LINE_EDITOR "None" CACHE STRING "" FORCE)
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

# rapidcsv for CSV reading/writing (header-only)
FetchContent_Declare(rapidcsv
    GIT_REPOSITORY https://github.com/d99kris/rapidcsv.git
    GIT_TAG v8.92
)
FetchContent_MakeAvailable(rapidcsv)

# argparse for CLI argument parsing (header-only)
FetchContent_Declare(argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG v3.2
)
FetchContent_MakeAvailable(argparse)

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
