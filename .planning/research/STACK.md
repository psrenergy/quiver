# Stack Research

**Domain:** C++ CLI argument parsing for quiver_lua executable
**Researched:** 2026-02-27
**Confidence:** HIGH

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| argparse (p-ranav/argparse) | v3.2 | CLI argument parsing | Header-only, single header, C++17+ (project already uses C++20), Python-like API that matches Quiver's clean-code philosophy, minimal footprint, active maintenance (last release Jan 2025). CMake target `argparse::argparse` integrates via FetchContent identically to existing dependencies. |

### Existing Stack (No Changes Needed)

These are already in the project and fully sufficient for the CLI executable. Listed for completeness -- the CLI links against `libquiver` which provides all of these.

| Technology | Version | Purpose | Status |
|------------|---------|---------|--------|
| libquiver (C++ core) | 0.5.0 | Database operations, LuaRunner | Already built; CLI links against it |
| SQLite3 | v3.50.2 | Database engine | Transitive via libquiver |
| Lua | v5.4.8 | Script execution | Transitive via libquiver |
| sol2 | v3.5.0 | Lua/C++ binding | Transitive via libquiver |
| spdlog | v1.17.0 | Logging | Transitive via libquiver |
| CMake | >= 3.21 | Build system | Already configured |

### What the CLI Executable Needs

The `quiver_lua` executable is thin: parse arguments, open a Database, run a Lua script via LuaRunner. The only NEW dependency is an argument parser. Everything else comes from linking `libquiver`.

```
quiver_lua.exe
  -> argparse (compile-time only, header-only)
  -> libquiver.dll (runtime)
       -> SQLite3, Lua, sol2, spdlog, etc.
```

## Why argparse Over Alternatives

### The Decision

**Use p-ranav/argparse v3.2** because it is the right size tool for a simple CLI with 3-5 positional/optional arguments. CLI11 is over-engineered for this use case. cxxopts would also work but argparse has a cleaner API for the patterns needed here.

### Detailed Rationale

The `quiver_lua` CLI has a narrow scope:
- A required database path argument
- A mode flag (schema vs migrations)
- A schema or migrations path argument
- A required script path argument
- Optional flags (log level, version, help)

This is not a complex CLI with subcommands, config files, or dozens of options. The argument parser needs to:
1. Parse a handful of positional and optional arguments
2. Generate automatic help text
3. Produce clear error messages on bad input

**argparse wins on simplicity-to-capability ratio for this exact use case.**

## Alternatives Considered

| Recommended | Alternative | Why Not |
|-------------|-------------|---------|
| argparse v3.2 | CLI11 v2.6.2 | CLI11 is the most feature-rich option (subcommands, config files, validators, option groups). Overkill for a CLI with ~5 arguments. Adds complexity without benefit. Would be the right choice if quiver_lua grew into a multi-subcommand tool, but PROJECT.md explicitly scopes out REPL mode and script arguments. |
| argparse v3.2 | cxxopts v3.3.1 | cxxopts is lightweight and solid. Close second. However, its API is more verbose for positional arguments (cxxopts was designed around `--flag` style options, not positional args). argparse handles positional arguments more naturally, which matters since `quiver_lua` has positional paths. |
| argparse v3.2 | Hand-rolled parsing | Violates "avoid hand-rolling CLI parsing" decision already noted in PROJECT.md. Error-prone for edge cases (missing args, wrong order, help text). |
| argparse v3.2 | Boost.ProgramOptions | Massive dependency for a trivial need. Project has zero Boost dependencies and should keep it that way. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| CLI11 | Over-engineered for ~5 arguments. Adds subcommand/config infrastructure that will never be used. | argparse |
| Boost.ProgramOptions | Pulls in Boost as a dependency. Disproportionate to the need. | argparse |
| getopt / getopt_long | C API, not type-safe, no automatic help generation, manual everything. | argparse |
| Hand-rolled argc/argv parsing | Error-prone, no help generation, maintenance burden. PROJECT.md already decided against this. | argparse |

## CMake Integration

argparse integrates via FetchContent, identical to all existing Quiver dependencies.

### Addition to `cmake/Dependencies.cmake`

```cmake
# argparse for CLI argument parsing (header-only)
FetchContent_Declare(argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG v3.2
)
FetchContent_MakeAvailable(argparse)
```

### New executable target (in `src/CMakeLists.txt` or a new `src/cli/CMakeLists.txt`)

```cmake
add_executable(quiver_lua cli/main.cpp)

target_link_libraries(quiver_lua PRIVATE
    quiver
    quiver_compiler_options
    argparse::argparse
)

set_target_properties(quiver_lua PROPERTIES
    OUTPUT_NAME quiver_lua
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
```

### Key Integration Notes

1. **argparse is header-only** -- it adds zero link-time cost and no new .dll/.so dependency. Only `#include <argparse/argparse.hpp>` in the CLI main.cpp.
2. **FetchContent pattern matches existing dependencies** -- sqlite3, spdlog, tomlplusplus, lua, sol2, rapidcsv, and googletest all use the exact same `FetchContent_Declare` + `FetchContent_MakeAvailable` pattern.
3. **C++17 requirement is a non-issue** -- the project is already C++20 (`CMAKE_CXX_STANDARD 20`), which is a superset of C++17.
4. **No new build option needed** -- unlike the C API (`QUIVER_BUILD_C_API`), the CLI executable should always be built when tests are built, or controlled by a `QUIVER_BUILD_CLI` option if preferred.

## Usage Pattern

The argparse API for the quiver_lua use case:

```cpp
#include <argparse/argparse.hpp>

int main(int argc, char* argv[]) {
    argparse::ArgumentParser program("quiver_lua", "0.5.0");

    program.add_argument("database")
        .help("Path to the database file");

    program.add_argument("script")
        .help("Path to the Lua script to execute");

    program.add_argument("--schema")
        .help("Path to SQL schema file (creates database via from_schema)");

    program.add_argument("--migrations")
        .nargs(argparse::nargs_pattern::at_least_one)
        .help("Paths to SQL migration files (creates database via from_migrations)");

    program.add_argument("--log-level")
        .default_value(std::string("info"))
        .help("Log level: debug, info, warn, error");

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    // Access parsed values
    auto db_path = program.get<std::string>("database");
    auto script_path = program.get<std::string>("script");
    // ...
}
```

This API style aligns with Quiver's "clean code" and "simple solutions over complex abstractions" principles.

## Version Compatibility

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| argparse v3.2 | C++17, C++20, C++23 | Project uses C++20; fully compatible |
| argparse v3.2 | CMake >= 3.12 | Project requires >= 3.21; fully compatible |
| argparse v3.2 | MSVC 2019+, GCC 7+, Clang 5+ | Standard modern compiler support |
| argparse v3.2 | libquiver 0.5.0 | No interaction; argparse is only used in main.cpp |

## Sources

- [p-ranav/argparse GitHub](https://github.com/p-ranav/argparse) -- v3.2 release confirmed Jan 25, 2025
- [CLIUtils/CLI11 GitHub](https://github.com/CLIUtils/CLI11) -- v2.6.2 release confirmed Feb 26, 2025
- [jarro2783/cxxopts GitHub](https://github.com/jarro2783/cxxopts) -- v3.3.1 release confirmed May 26, 2024
- [p-ranav/argparse CMakeLists.txt](https://github.com/p-ranav/argparse/blob/master/CMakeLists.txt) -- verified FetchContent compatibility and `argparse::argparse` target
- Existing project `cmake/Dependencies.cmake` -- verified FetchContent pattern used for all dependencies

---
*Stack research for: quiver_lua CLI executable*
*Researched: 2026-02-27*
