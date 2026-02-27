# Architecture Patterns

**Domain:** Standalone CLI executable for existing C++ library
**Researched:** 2026-02-27

## Recommended Architecture

The `quiver_lua` executable is a thin CLI shell around existing library internals. It introduces **one new CMake target** (an executable) and **one new source file** (`main.cpp`). No modifications to `libquiver`, `LuaRunner`, or any existing code are required.

```
User runs: quiver_lua --schema db.sqlite schema.sql script.lua
                       |
                       v
               +------------------+
               |    main.cpp      |  NEW - CLI entry point
               |  (argparse CLI)  |
               +------------------+
                       |
          parses args, reads script file,
          constructs DatabaseOptions,
          opens Database, creates LuaRunner
                       |
                       v
               +------------------+
               |   libquiver.dll  |  EXISTING - unchanged
               |  Database class  |
               |  LuaRunner class |
               +------------------+
                       |
                       v
               +------------------+
               |  SQLite / sol2   |  EXISTING - unchanged
               +------------------+
```

### Component Boundaries

| Component | Responsibility | Communicates With | Status |
|-----------|---------------|-------------------|--------|
| `main.cpp` | Parse CLI args, read script file, open DB, run Lua, handle errors, set exit code | `Database`, `LuaRunner`, argparse | **NEW** |
| `Database` | All DB operations via Pimpl | SQLite, spdlog | EXISTING - unchanged |
| `LuaRunner` | Lua script execution with `db` userdata | `Database`, sol2/Lua | EXISTING - unchanged |
| argparse | CLI argument parsing | `main.cpp` only | **NEW dependency** |

### Data Flow

1. **CLI parsing:** `main(argc, argv)` -> argparse parses positional/optional args -> yields `db_path`, `schema_or_migrations_path`, `script_path`, `log_level`, `read_only`, `mode` (schema vs migrations)
2. **Script loading:** `std::ifstream` reads `script_path` into `std::string` (standard C++ file I/O, no new dependency needed)
3. **Database opening:** Construct `DatabaseOptions{read_only, console_level}` -> call `Database::from_schema()` or `Database::from_migrations()` based on mode flag
4. **Execution:** Construct `LuaRunner(db)` -> call `lua.run(script_contents)`
5. **Error handling:** Catch `std::runtime_error` from any step, print to stderr, return non-zero exit code

## New CMake Target

### Placement

The executable target belongs in `tests/CMakeLists.txt` alongside `quiver_benchmark` and `quiver_sandbox`, because the project already groups all executables there (test runners, benchmark, sandbox). This is consistent with the existing pattern. **Confidence: HIGH** -- directly observed in `tests/CMakeLists.txt` lines 87-107.

Alternatively, a dedicated `tools/CMakeLists.txt` could be created and added via `add_subdirectory(tools)` in the root CMakeLists.txt. This is cleaner semantically (a CLI tool is not a test), but adds a new directory to the project structure. Given this is a small project with few executables, either approach works. The `tests/CMakeLists.txt` approach is the path of least resistance.

### CMake Configuration

```cmake
# Lua CLI executable
add_executable(quiver_lua
    tools/main.cpp
)

target_link_libraries(quiver_lua
    PRIVATE
        quiver
        quiver_compiler_options
        argparse::argparse
)

# Copy DLLs for Windows
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET quiver_lua POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:quiver>
            $<TARGET_FILE_DIR:quiver_lua>
    )
endif()
```

The executable links against `quiver` (the core C++ library) directly -- not `quiver_c`. It uses C++ classes (`Database`, `LuaRunner`) directly. This is the same pattern as `quiver_tests`, `quiver_benchmark`, and `quiver_sandbox`.

### New Dependency: argparse

Add to `cmake/Dependencies.cmake`:

```cmake
# argparse for CLI argument parsing
FetchContent_Declare(argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG v3.2
)
FetchContent_MakeAvailable(argparse)
```

**Why p-ranav/argparse:** Header-only, MIT license, C++17 (project uses C++20 -- compatible), single header, well-maintained (3.6k GitHub stars), FetchContent-ready with CMake target `argparse::argparse`. Matches project's existing pattern of pulling dependencies via FetchContent (sqlite3, spdlog, sol2, rapidcsv, etc.).

**Confidence: HIGH** -- version and CMake target name verified from official GitHub README.

## main.cpp Structure

### Recommended Implementation

```cpp
#include <argparse/argparse.hpp>
#include <quiver/database.h>
#include <quiver/lua_runner.h>
#include <quiver/c/options.h>   // for quiver_log_level_t enum values

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open script: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    argparse::ArgumentParser program("quiver_lua", "0.5.0");

    program.add_argument("database")
        .help("path to SQLite database file");

    program.add_argument("schema")
        .help("path to schema (.sql) or migrations directory");

    program.add_argument("script")
        .help("path to Lua script to execute");

    program.add_argument("--migrations")
        .help("treat schema path as migrations directory")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--read-only")
        .help("open database in read-only mode")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--log-level")
        .help("log level: debug, info, warn, error, off")
        .default_value(std::string("info"));

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << program;
        return EXIT_FAILURE;
    }

    auto db_path = program.get<std::string>("database");
    auto schema_path = program.get<std::string>("schema");
    auto script_path = program.get<std::string>("script");
    bool use_migrations = program.get<bool>("--migrations");
    bool read_only = program.get<bool>("--read-only");
    auto log_level_str = program.get<std::string>("--log-level");

    // Map string to log level enum
    quiver_log_level_t log_level = QUIVER_LOG_INFO;
    if (log_level_str == "debug") log_level = QUIVER_LOG_DEBUG;
    else if (log_level_str == "warn") log_level = QUIVER_LOG_WARN;
    else if (log_level_str == "error") log_level = QUIVER_LOG_ERROR;
    else if (log_level_str == "off") log_level = QUIVER_LOG_OFF;

    quiver::DatabaseOptions options{
        .read_only = read_only ? 1 : 0,
        .console_level = log_level
    };

    try {
        // Read script file
        auto script = read_file(script_path);

        // Open database
        auto db = use_migrations
            ? quiver::Database::from_migrations(db_path, schema_path, options)
            : quiver::Database::from_schema(db_path, schema_path, options);

        // Run Lua script
        quiver::LuaRunner lua(db);
        lua.run(script);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
```

### Design Decisions in main.cpp

**Three positional arguments (database, schema, script):** This is the natural order -- you specify what database to operate on, how to create/validate it, and what to do with it. All three are required.

**`--migrations` flag:** Selects between `from_schema()` and `from_migrations()`. Default is schema mode because it is simpler and more common for single-file schemas. The flag name `--migrations` is self-documenting.

**`--read-only` flag:** Maps directly to `DatabaseOptions.read_only`. Useful for scripts that only query data.

**`--log-level` string argument:** Maps to the existing `quiver_log_level_t` enum. String-based because argparse handles string validation cleanly, and the mapping is trivial (5 values).

**Script is read as a string, not streamed:** `LuaRunner::run()` takes a `const std::string&`, so the entire file must be in memory anyway. Simple `ifstream` + `rdbuf()` is sufficient.

**Error handling via stderr + exit code:** Matches Unix CLI conventions. No custom error formatting -- the C++ layer's error messages are already well-structured (see error patterns in CLAUDE.md).

**No `--version` explicit setup:** argparse adds `--version` automatically when constructed with `ArgumentParser("quiver_lua", "0.5.0")`.

## Patterns to Follow

### Pattern 1: Link Against libquiver Directly (Not C API)

**What:** The executable uses C++ classes (`Database`, `LuaRunner`) directly, bypassing the C API layer entirely.

**Why:** The C API exists for FFI consumers (Julia, Dart, Python). A C++ executable has no reason to go through the C shim. This matches how `quiver_tests`, `quiver_benchmark`, and `quiver_sandbox` all link to `quiver` directly.

**Confidence: HIGH** -- all existing executables in `tests/CMakeLists.txt` use `target_link_libraries(... quiver ...)`.

### Pattern 2: DLL Copy Post-Build (Windows)

**What:** A `POST_BUILD` command copies `libquiver.dll` to the executable output directory.

**Why:** On Windows with shared libraries, the DLL must be adjacent to the `.exe` or on PATH. Every existing executable in the project does this. The `quiver_lua` target must do the same.

**Confidence: HIGH** -- pattern observed in `tests/CMakeLists.txt` for `quiver_tests`, `quiver_c_tests`, `quiver_benchmark`.

### Pattern 3: Error Messages Flow from C++ Layer

**What:** `main.cpp` catches `std::runtime_error` and prints `e.what()` to stderr. It does NOT craft its own error messages for library failures.

**Why:** Per project philosophy: "Error Messages: All error messages are defined in the C++/C API layer." The CLI is a thin shell that surfaces errors, not a layer that interprets them.

### Pattern 4: Source File Placement

**What:** Place `main.cpp` under `tests/tools/main.cpp` (new subdirectory) rather than loose in `tests/` or `src/`.

**Why:** Keeps the executable source separate from test files (which are all `test_*.cpp`) while staying within the existing `tests/` CMake scope. The `benchmark/` and `sandbox/` subdirectories already follow this pattern within `tests/`.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Adding sol2/Lua Dependencies to the Executable

**What:** Linking `quiver_lua` against `lua_library`, `sol2`, or including `<sol/sol.hpp>`.

**Why bad:** `LuaRunner` is part of `libquiver`, which already links sol2 and Lua privately. The executable only needs `#include <quiver/lua_runner.h>` and links to `quiver`. Double-linking Lua can cause symbol conflicts.

**Instead:** Only link `quiver` and `argparse::argparse`. Let the shared library handle Lua internally.

### Anti-Pattern 2: Creating a New Library for CLI Logic

**What:** Factoring out CLI logic into a separate static library so it can be tested independently.

**Why bad:** Over-engineering for a 60-line `main()`. The CLI is a thin glue layer. Testing it means running the executable with test scripts (integration tests), not unit-testing the argument parsing.

**Instead:** Keep everything in `main.cpp`. Test via running `quiver_lua.exe` with known scripts and schemas.

### Anti-Pattern 3: Modifying LuaRunner to Accept Script File Paths

**What:** Adding a `LuaRunner::run_file(path)` method.

**Why bad:** File I/O is trivial in the caller (4 lines). Adding it to `LuaRunner` breaks the single responsibility (it runs scripts, not reads files). It also forces `LuaRunner` to handle file-not-found errors, which belong in the CLI layer.

**Instead:** Read the file in `main.cpp`, pass the string to `LuaRunner::run()`.

### Anti-Pattern 4: Making the Executable Conditional on QUIVER_BUILD_C_API

**What:** Gating `quiver_lua` behind the C API build flag.

**Why bad:** The executable uses `libquiver` (core C++ library), not `libquiver_c`. It has no dependency on the C API whatsoever.

**Instead:** Gate it behind `QUIVER_BUILD_TESTS` (since it lives in `tests/CMakeLists.txt`) or introduce a new `QUIVER_BUILD_TOOLS` option if desired.

## Integration Points Summary

### New Components

| Component | Location | Purpose |
|-----------|----------|---------|
| `main.cpp` | `tests/tools/main.cpp` | CLI entry point |
| argparse dependency | `cmake/Dependencies.cmake` | CLI argument parsing |
| CMake target `quiver_lua` | `tests/CMakeLists.txt` | Build configuration |

### Modified Components

| Component | Change | Why |
|-----------|--------|-----|
| `cmake/Dependencies.cmake` | Add argparse FetchContent block | New dependency for CLI |
| `tests/CMakeLists.txt` | Add `quiver_lua` executable target | Build the new executable |
| `scripts/build-all.bat` | No change required | Executable is built by `cmake --build build` already; not a test to run |

### Unchanged Components

| Component | Why Unchanged |
|-----------|---------------|
| `libquiver` (all `src/*.cpp`) | Executable consumes existing public API |
| `LuaRunner` | Used as-is via `run(script_string)` |
| `Database`, `DatabaseOptions` | Used as-is via factory methods |
| `libquiver_c` (C API) | Not involved -- executable links C++ directly |
| All bindings (Julia, Dart, Python) | Completely independent |
| All test files | Completely independent |

## Build Order and Dependencies

```
1. FetchContent downloads (including new argparse)
2. libquiver.dll builds (unchanged)
3. quiver_lua.exe builds (links libquiver + argparse)
4. POST_BUILD copies libquiver.dll next to quiver_lua.exe (Windows)
```

The executable depends only on `libquiver` being built. It is completely independent of `libquiver_c`, all test targets, and all binding targets. It can be built in parallel with tests if the build system supports it.

### Build Command (Unchanged)

```bash
cmake --build build --config Debug
```

This already builds all targets. For targeted builds:

```bash
cmake --build build --config Debug --target quiver_lua
```

## Scalability Considerations

| Concern | Current (v0.5) | Future (REPL, script args) |
|---------|----------------|---------------------------|
| Argument count | 3 positional + 3 optional flags | Add `--` separator for script args, add `--interactive` flag |
| Script loading | Read entire file into string | Same -- Lua scripts are small |
| Multiple scripts | Not supported | Could accept multiple script paths (run sequentially) |
| Exit code | 0 success, 1 failure | Could propagate Lua script return values |

## Sources

- `tests/CMakeLists.txt` -- existing executable patterns (benchmark, sandbox)
- `src/CMakeLists.txt` -- library build and link patterns
- `cmake/Dependencies.cmake` -- FetchContent pattern for all dependencies
- `include/quiver/lua_runner.h` -- LuaRunner public API
- `include/quiver/options.h` -- DatabaseOptions type and defaults
- `include/quiver/c/options.h` -- quiver_log_level_t enum values
- [p-ranav/argparse](https://github.com/p-ranav/argparse) -- recommended CLI parsing library (v3.2, MIT, header-only, C++17)
