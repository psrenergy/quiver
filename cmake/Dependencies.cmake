include(FetchContent)

# SQLite via FetchContent
FetchContent_Declare(sqlite3
    GIT_REPOSITORY https://github.com/psrenergy/sqlite3-cmake.git
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

# csv-parser for streaming, quoting-correct CSV reads (Lua db:read_csv / db:read_csv_stream).
# Upstream defaults are ON/OFF/ON/ON respectively; all four are FORCEd the other way, before
# FetchContent_MakeAvailable:
#   CSV_ENABLE_THREADS=OFF -- with threads off, the read window is a single unmultiplied chunk
#     (src/csv_read.cpp never calls format.chunk_size(...)), which is what keeps the memory
#     window from scaling with the host's CPU count. If this is ever turned back on,
#     format.threading(false) must be added in src/csv_read.cpp to preserve that guarantee.
#   CSV_NO_SIMD=ON -- with SIMD on, csv-parser adds a PUBLIC /arch:AVX2 (or -mavx2) compile
#     option that would propagate into `quiver` itself and SIGILL on pre-AVX2 x86 for every
#     shipped PyPI wheel, npm native, Julia artifact and S3 binary. Do not turn this back on.
#   CSV_BUILD_PROGRAMS=OFF / CSV_BUILD_TESTS=OFF -- this project only needs the library target.
# GIT_SHALLOW (used by no other dependency here) because this checkout is by far the largest:
# 230 MB, of which 69 MB is history nothing reads. GIT_TAG is a tag, so the shallow fetch resolves.
FetchContent_Declare(csv_parser
    GIT_REPOSITORY https://github.com/vincentlaucsb/csv-parser.git
    GIT_TAG 5.3.0
    GIT_SHALLOW TRUE
)
set(CSV_ENABLE_THREADS OFF CACHE BOOL "" FORCE)
set(CSV_NO_SIMD ON CACHE BOOL "" FORCE)
set(CSV_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(CSV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(csv_parser)
# `csv_no_simd` duplicates all nine of `csv`'s sources; nothing links it, and csv-parser declares
# no install()/export() rules for it, so excluding it from `all` is safe here. This is NOT the
# same situation as the lua-cmake EXCLUDE_FROM_ALL warning above -- that one is about lua-cmake's
# own install() rules -- so do not delete this line by analogy with that comment.
set_target_properties(csv_no_simd PROPERTIES EXCLUDE_FROM_ALL YES)

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
