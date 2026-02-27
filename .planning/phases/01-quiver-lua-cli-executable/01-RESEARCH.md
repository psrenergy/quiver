# Phase 1: quiver_lua CLI Executable - Research

**Researched:** 2026-02-27
**Domain:** C++ CLI executable (argument parsing, file I/O, process lifecycle)
**Confidence:** HIGH

## Summary

This phase creates a single new executable target (`quiver_lua`) that wraps existing `Database` and `LuaRunner` classes with CLI argument parsing. The implementation is approximately 100-150 lines of C++ in one new source file, plus CMake target configuration. All database intelligence, Lua execution, and error generation already exist in libquiver -- the CLI is a thin pass-through layer.

The primary technical components are: (1) integrating p-ranav/argparse v3.2 via FetchContent for argument parsing, (2) reading a Lua script file with UTF-8 BOM stripping, (3) constructing a `Database` in one of three modes and a `LuaRunner`, and (4) mapping C++ exceptions to exit codes with stderr error output.

**Primary recommendation:** One new source file (`src/cli/main.cpp`), one FetchContent dependency (argparse), one new CMake executable target linked against `quiver`. No new library code needed -- only a `main()` entry point.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- `--help` shows flags only: no program description, no usage examples
- `--version` prints "quiver_lua X.Y.Z" (name + version)
- Version number sourced from CMake variable (single source of truth, injected at build time)
- No prefix on error messages (no "error: " prefix) -- exit code signals the error
- Lua runtime errors show message only, no stack trace
- Usage errors (exit 2) show just the error message, not the full help text
- File-not-found errors use the user's input path, not resolved absolute path
- Database errors pass through from C++/SQLite layer as-is
- Conflicting flags (--schema + --migrations) use simple statement: "Cannot use --schema and --migrations together"
- spdlog output goes to stderr, keeping stdout clean for script output
- `--migrations` accepts a directory path; CLI collects all files in the directory (any extension, not just .sql); file paths passed directly to `from_migrations()`; top-level files only (no recursive subdirectory discovery); empty directory: pass empty list to C++, let it handle errors
- `--schema` accepts a file path, passed directly to `from_schema()` -- no file existence check in CLI
- Three modes: positional-only (open existing), `--schema` (from schema), `--migrations` (from migrations)
- `--schema` and `--migrations` are mutually exclusive
- Lua `print()` goes to stdout
- Silent exit (code 0) on success with no output
- Default log level: warn
- Script file path is optional in argparse (errors with "no script provided" for now -- prepares for future REPL mode)

### Claude's Discretion
- argparse configuration details
- CMake target setup and linking
- UTF-8 BOM stripping implementation
- Exact error message wording (within the patterns above)
- Source file location and structure

### Deferred Ideas (OUT OF SCOPE)
- REPL mode when no script is passed (REPL-01 already in REQUIREMENTS.md as future requirement) -- script path made optional to prepare for this
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CLI-01 | `--help` shows usage information | argparse auto-generates help; constructor controls display |
| CLI-02 | `--version` shows version number | argparse constructor accepts version string; CMake `QUIVER_VERSION` define |
| CLI-03 | Usage error (exit 2) for invalid arguments | argparse throws `std::runtime_error` on parse failure; catch and return exit 2 |
| DB-01 | Open existing database via positional arg | `Database(path, options)` constructor -- direct pass-through |
| DB-02 | Create/open with `--schema PATH` | `Database::from_schema(db_path, schema_path, options)` -- direct pass-through |
| DB-03 | Create/open with `--migrations PATH` | `Database::from_migrations(db_path, migrations_path, options)` -- pass directory path directly; C++ handles directory traversal |
| DB-04 | Error when both `--schema` and `--migrations` | argparse mutually exclusive group OR manual check after parse |
| DB-05 | `--read-only` flag | Maps to `DatabaseOptions.read_only = 1` |
| SCRIPT-01 | Execute Lua script via positional arg | Read file, strip BOM, pass to `LuaRunner::run()` |
| SCRIPT-02 | Clear error when script file not found | `std::filesystem::exists()` check before reading; print user's input path |
| SCRIPT-03 | UTF-8 BOM handled on Windows | Strip 3-byte BOM (0xEF 0xBB 0xBF) prefix if present before passing to LuaRunner |
| ERR-01 | Lua runtime errors on stderr with exit 1 | `LuaRunner::run()` throws `std::runtime_error` with "Lua error: " prefix |
| ERR-02 | Database errors on stderr with exit 1 | Database methods throw `std::runtime_error`; catch and print to stderr |
| ERR-03 | `--log-level` controls verbosity | Maps string arg to `quiver_log_level_t` enum in `DatabaseOptions` |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| p-ranav/argparse | v3.2 | CLI argument parsing | Header-only, C++17, zero dependencies, native CMake FetchContent support. Already decided in STATE.md. |
| libquiver (project) | 0.5.0 | Database + LuaRunner | Existing project library -- all intelligence lives here |
| spdlog | v1.17.0 | Logging | Already a project dependency; Database creates per-instance loggers |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| std::filesystem | C++17 | File existence checks, directory listing | Script file existence (SCRIPT-02), migrations directory listing |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| argparse | CLI11, cxxopts | argparse is already decided; CLI11 is heavier, cxxopts is comparable but argparse has better mutually-exclusive group support |

**Installation (CMake):**
```cmake
FetchContent_Declare(argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG v3.2
)
FetchContent_MakeAvailable(argparse)
```

## Architecture Patterns

### Recommended Project Structure
```
src/
  cli/
    main.cpp              # Single source file for quiver_lua executable (~100-150 lines)
```

The executable lives under `src/cli/` rather than at top level. The CMake target is added in `src/CMakeLists.txt` alongside existing library targets.

### Pattern 1: Thin CLI Entry Point
**What:** `main()` does only: parse args, construct Database, construct LuaRunner, read script file, call `run()`, map exceptions to exit codes.
**When to use:** Always -- this is the single pattern for this phase.
**Example:**
```cpp
// Pseudocode structure
int main(int argc, char* argv[]) {
    // 1. Parse arguments (argparse)
    // 2. Validate mutual exclusivity / required combinations
    // 3. Configure DatabaseOptions from parsed flags
    // 4. Construct Database (one of 3 modes)
    // 5. Read script file (with BOM stripping)
    // 6. Construct LuaRunner, call run()
    // 7. Return 0
    // catch: usage errors -> stderr + exit 2
    // catch: runtime errors -> stderr + exit 1
}
```

### Pattern 2: Version Injection via CMake
**What:** CMake defines `QUIVER_VERSION` as a preprocessor macro, consumed by main.cpp.
**When to use:** For `--version` output.
**Example:**
```cmake
target_compile_definitions(quiver_lua PRIVATE
    QUIVER_VERSION="${PROJECT_VERSION}"
)
```
This pattern is already used by the `quiver_c` target (line 117 of `src/CMakeLists.txt`).

### Pattern 3: argparse Configuration
**What:** Two positional args (database, script), optional flags, mutually exclusive group.
**When to use:** Argument setup in main.
**Example:**
```cpp
argparse::ArgumentParser program("quiver_lua", QUIVER_VERSION);

program.add_argument("database")
    .help("path to the database file");

program.add_argument("script")
    .help("path to the Lua script file")
    .nargs(argparse::nargs_pattern::optional);  // optional for future REPL

program.add_argument("--schema")
    .help("create database from schema file");

program.add_argument("--migrations")
    .help("create database from migrations directory");

program.add_argument("--read-only")
    .help("open database in read-only mode")
    .flag();

program.add_argument("--log-level")
    .help("set log verbosity (debug, info, warn, error, off)")
    .default_value(std::string("warn"));
```

### Pattern 4: Exit Code Strategy
**What:** Distinct exit codes for success (0), runtime errors (1), and usage errors (2).
**When to use:** All error paths.
**Example:**
```cpp
try {
    program.parse_args(argc, argv);
} catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 2;  // usage error
}

try {
    // database + lua execution
} catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;  // runtime/database error
}
return 0;
```

### Anti-Patterns to Avoid
- **Adding validation logic in CLI:** The CLI must NOT check if schema files exist, if migration files are valid, or if the database path is writable. All of that is handled by the C++ library. The CLI is a pass-through.
- **Crafting custom error messages:** Error messages come from the C++ layer. The CLI catches exceptions and prints `e.what()` directly. No reformatting.
- **Using spdlog in the CLI itself:** The CLI should use `std::cerr` for its own error output. spdlog is used internally by the Database library.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Argument parsing | Custom getopt/manual parsing | argparse v3.2 | Handles help generation, type checking, mutual exclusivity, error messages |
| UTF-8 BOM detection | Complex encoding detection | 3-byte prefix check | BOM is exactly 0xEF 0xBB 0xBF -- a simple `if` on the first 3 bytes suffices |
| Directory listing | Manual `opendir`/`readdir` | `std::filesystem::directory_iterator` | Standard library, cross-platform, already used in project |
| Log level mapping | String-to-enum with manual parsing | Simple map/switch | Only 5 values (debug/info/warn/error/off) |

**Key insight:** This is a ~100-line `main()`. The only "engineering" is in argument configuration. Everything else is glue between argparse output and existing C++ API calls.

## Common Pitfalls

### Pitfall 1: spdlog Writing to stdout
**What goes wrong:** The Database's internal logger currently uses `stdout_color_sink_mt`, which writes to stdout. If the CLI doesn't address this, spdlog log messages will interleave with Lua `print()` output on stdout.
**Why it happens:** The logger is created inside `database.cpp` in an anonymous namespace function `create_database_logger()`. It uses `spdlog::sinks::stdout_color_sink_mt()`.
**How to avoid:** Change `stdout_color_sink_mt` to `stderr_color_sink_mt` in `src/database.cpp`. This is a library-level change, but the project is WIP with no backwards compatibility requirement. The CONTEXT.md explicitly states "spdlog output goes to stderr." The header is `<spdlog/sinks/stderr_color_sinks.h>`.
**Warning signs:** Running a Lua script with `print()` and `--log-level debug` produces garbled output on stdout.

### Pitfall 2: argparse Default Version Format
**What goes wrong:** argparse's default `--version` behavior prints just the version string (e.g., "0.5.0"), but the requirement is "quiver_lua 0.5.0".
**Why it happens:** The `ArgumentParser` constructor's second argument is the version string, and `--version` prints exactly that string.
**How to avoid:** Pass the full desired string as the version: `ArgumentParser("quiver_lua", "quiver_lua 0.5.0")`. Or alternatively, pass `default_arguments::help` only (exclude default version) and add a custom `--version` argument.
**Warning signs:** `--version` output doesn't match "quiver_lua X.Y.Z" format.

### Pitfall 3: argparse Calls exit() on --help/--version
**What goes wrong:** By default, argparse calls `std::exit()` when `--help` or `--version` is provided. This bypasses destructors and cleanup.
**Why it happens:** Constructor parameter `exit_on_default_arguments` defaults to `true`.
**How to avoid:** This is actually fine for a CLI tool -- `--help` and `--version` should exit immediately. The default behavior is correct. But if cleanup matters, set `exit_on_default_arguments = false` and handle the exceptions manually.
**Warning signs:** Resource leaks in valgrind/ASAN runs with `--help` flag.

### Pitfall 4: LuaRunner Error Prefix
**What goes wrong:** `LuaRunner::run()` throws `std::runtime_error` with message `"Lua error: " + err.what()`. The requirement says "Lua runtime errors show message only, no stack trace." The "Lua error: " prefix is part of the C++ layer's error message -- it does NOT include a stack trace.
**Why it happens:** The `run()` method at line 985 of `src/lua_runner.cpp` formats errors as `"Lua error: " + err.what()`.
**How to avoid:** No action needed. The "Lua error: " prefix is from the C++ layer and is fine per the "Database errors pass through from C++/SQLite layer as-is" decision. sol2 error messages may contain `[string "..."]:N:` line info but NOT Lua stack traces (because `sol::script_pass_on_error` is used, not `pcall` with a traceback handler).
**Warning signs:** N/A -- current behavior matches requirements.

### Pitfall 5: Migrations Directory Listing Order
**What goes wrong:** `std::filesystem::directory_iterator` does not guarantee alphabetical ordering of entries.
**Why it happens:** OS-dependent iteration order.
**How to avoid:** The CONTEXT.md explicitly states "no ordering" -- file paths are passed directly to `from_migrations()` and the CLI does not sort them. The C++ layer (via `Migrations` class) handles ordering. So this is a non-issue: just collect unsorted paths and pass them through.
**Warning signs:** N/A -- by design.

### Pitfall 6: UTF-8 BOM in File Read
**What goes wrong:** Windows editors (Notepad, VS Code with certain settings) save Lua files with a 3-byte UTF-8 BOM prefix (0xEF 0xBB 0xBF). If passed to the Lua interpreter, it causes a syntax error on the first line.
**Why it happens:** The BOM bytes are not valid Lua syntax characters.
**How to avoid:** After reading the file into a `std::string`, check if the first 3 bytes are `\xEF\xBB\xBF` and strip them with `content.substr(3)` or `content.erase(0, 3)`.
**Warning signs:** Scripts work on Linux/Mac but fail on Windows with "unexpected symbol near ''" errors.

## Code Examples

### Example 1: CMake Target Configuration
```cmake
# In src/CMakeLists.txt, after existing library targets
add_executable(quiver_lua cli/main.cpp)

target_compile_definitions(quiver_lua PRIVATE
    QUIVER_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(quiver_lua PRIVATE
    quiver
    quiver_compiler_options
    argparse
)

# MSVC needs /bigobj for sol2 if headers are transitively included
# (unlikely for CLI since it only uses public quiver headers)
```

### Example 2: UTF-8 BOM Stripping
```cpp
std::string read_script_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        // This path is unreachable if we check existence first,
        // but defensive for file permission errors
        throw std::runtime_error("Script file not found: " + path);
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // Strip UTF-8 BOM if present
    if (content.size() >= 3 &&
        content[0] == '\xEF' &&
        content[1] == '\xBB' &&
        content[2] == '\xBF') {
        content.erase(0, 3);
    }
    return content;
}
```

### Example 3: Log Level Mapping
```cpp
quiver_log_level_t parse_log_level(const std::string& level) {
    if (level == "debug") return QUIVER_LOG_DEBUG;
    if (level == "info")  return QUIVER_LOG_INFO;
    if (level == "warn")  return QUIVER_LOG_WARN;
    if (level == "error") return QUIVER_LOG_ERROR;
    if (level == "off")   return QUIVER_LOG_OFF;
    throw std::runtime_error("Unknown log level: " + level);
}
```

### Example 4: argparse Mutually Exclusive Group
```cpp
auto& group = program.add_mutually_exclusive_group();
group.add_argument("--schema")
    .help("create database from schema file");
group.add_argument("--migrations")
    .help("create database from migrations directory");
```
When both are provided, argparse throws with a message like: `Argument '--migrations DIR' not allowed with '--schema FILE'`.
However, the user decision says: "Cannot use --schema and --migrations together" -- so we may need to check manually after parse and provide the custom message.

### Example 5: Database Construction (Three Modes)
```cpp
quiver::DatabaseOptions options{};
options.read_only = program.get<bool>("--read-only") ? 1 : 0;
options.console_level = parse_log_level(program.get<std::string>("--log-level"));

auto db_path = program.get<std::string>("database");
quiver::Database db = [&]() -> quiver::Database {
    if (program.is_used("--schema")) {
        return quiver::Database::from_schema(db_path, program.get<std::string>("--schema"), options);
    }
    if (program.is_used("--migrations")) {
        return quiver::Database::from_migrations(db_path, program.get<std::string>("--migrations"), options);
    }
    return quiver::Database(db_path, options);
}();
```

### Example 6: Migrations Directory File Collection
```cpp
// Note: from_migrations() takes a directory path directly.
// The C++ method handles directory traversal internally.
// The CLI just passes the --migrations value through.
// No file collection needed in CLI code.
```
**Correction from CONTEXT.md review:** The CONTEXT.md says "CLI collects all files in the directory... File paths passed directly to `from_migrations()`." However, examining the actual C++ API, `Database::from_migrations()` takes a single `const std::string& migrations_path` which is a directory path -- it handles directory traversal internally (calls `migrate_up()` which reads the directory). So the CLI just passes the directory path through. No per-file collection needed.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| getopt / manual parsing | argparse header-only library | 2023+ | Simpler code, auto-generated help |
| Boost.Program_options | argparse / CLI11 | 2020+ | No Boost dependency, header-only |

**Deprecated/outdated:**
- Nothing relevant. argparse v3.2 (Jan 2025) is the latest release.

## Key Findings from Codebase Investigation

### spdlog stdout vs stderr
The Database's logger (created in `src/database.cpp:51`) uses `spdlog::sinks::stdout_color_sink_mt()`. This writes to **stdout**, which will conflict with Lua script output. The fix is straightforward: change to `stderr_color_sink_mt` in `src/database.cpp` and update the include from `<spdlog/sinks/stdout_color_sinks.h>` to `<spdlog/sinks/stderr_color_sinks.h>`. This is a 2-line change in the library.

### from_migrations Takes a Directory Path
`Database::from_migrations(db_path, migrations_path, options)` at `src/database.cpp:239` takes a directory path as a string. It checks `fs::exists()` and `fs::is_directory()` internally, then calls `migrate_up()` which handles file discovery. The CLI does NOT need to enumerate files.

### LuaRunner Error Format
`LuaRunner::run()` at `src/lua_runner.cpp:982` uses `sol::script_pass_on_error` and throws `std::runtime_error("Lua error: " + err.what())`. The error message from sol2 includes file/line info like `[string "..."]:3: attempt to call a nil value` but does NOT include a Lua stack trace. This matches the requirement.

### Version Injection Pattern
The `quiver_c` target already uses `QUIVER_VERSION="${PROJECT_VERSION}"` as a compile definition (`src/CMakeLists.txt:117`). The CLI should use the same pattern.

### Build System Integration
The executable should be built unconditionally (not behind a `QUIVER_BUILD_*` option) since it's the primary deliverable of v0.5. It goes in `build/bin/quiver_lua.exe`.

## Open Questions

1. **argparse mutually exclusive group error message format**
   - What we know: argparse generates its own error message for mutually exclusive violations. The user wants "Cannot use --schema and --migrations together".
   - What's unclear: Whether argparse's default message is acceptable, or if we need manual post-parse checking.
   - Recommendation: Use manual checking after `parse_args()` with `program.is_used()`. This gives exact control over the error message wording. Avoid the mutually exclusive group feature since it auto-formats the error message in argparse's style.

2. **argparse --help output customization**
   - What we know: The user wants "flags only: no program description, no usage examples." argparse by default shows a usage line and description.
   - What's unclear: How much control argparse gives over the help format. The `ArgumentParser` constructor does not add a description by default (only if `add_description()` is called). The usage line (`Usage: quiver_lua [options]...`) is auto-generated.
   - Recommendation: Simply don't call `add_description()` or `add_epilog()`. The default help shows argument names and their help text, which satisfies "flags only."

3. **Script file path as truly optional positional**
   - What we know: argparse supports `nargs(argparse::nargs_pattern::optional)` for positional arguments. The decision says "script file path is optional in argparse (errors with 'no script provided' for now)."
   - What's unclear: Whether argparse allows accessing an optional positional that wasn't provided without throwing.
   - Recommendation: Use `.nargs(argparse::nargs_pattern::optional)` and check with `program.present<std::string>("script")`. If absent, print "No script provided" to stderr and exit 2.

## Sources

### Primary (HIGH confidence)
- **Codebase direct inspection** -- `src/database.cpp`, `src/lua_runner.cpp`, `include/quiver/database.h`, `include/quiver/options.h`, `include/quiver/c/options.h`, `src/CMakeLists.txt`, `CMakeLists.txt`, `cmake/Dependencies.cmake`
- **p-ranav/argparse GitHub README** -- constructor signature, mutually exclusive groups, `is_used()`, `nargs`, `flag()`, FetchContent setup
- **p-ranav/argparse source header** -- `ArgumentParser` constructor parameters (`program_name`, `version`, `default_arguments`, `exit_on_default_arguments`, `os`)

### Secondary (MEDIUM confidence)
- **argparse GitHub releases** -- v3.2 tag confirmed as latest (Jan 25, 2025)
- **spdlog sink API** -- `stderr_color_sink_mt` exists as direct replacement for `stdout_color_sink_mt` (same API, different stream)

### Tertiary (LOW confidence)
- None. All findings verified against codebase or official sources.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- argparse is already decided, codebase APIs verified by direct inspection
- Architecture: HIGH -- single main.cpp, all patterns verified against existing codebase conventions
- Pitfalls: HIGH -- spdlog stdout issue confirmed by reading `database.cpp:51`; BOM handling is well-understood; all other pitfalls verified against source

**Research date:** 2026-02-27
**Valid until:** 2026-03-27 (stable domain, no fast-moving dependencies)
