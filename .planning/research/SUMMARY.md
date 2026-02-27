# Project Research Summary

**Project:** quiver_lua CLI executable
**Domain:** Standalone CLI executable wrapping an existing C++ library
**Researched:** 2026-02-27
**Confidence:** HIGH

## Executive Summary

The `quiver_lua` executable is a thin CLI shell that runs Lua scripts against a Quiver SQLite database. The project is well-understood: all core capabilities (Database, LuaRunner, error handling, logging) already exist in `libquiver`. The only new work is wiring a CLI argument parser to the existing public C++ API in a single `main.cpp` file. This is deliberately a small, scoped deliverable — the research consistently confirms minimal scope is the right call.

The recommended approach is to add one new dependency (p-ranav/argparse v3.2, header-only) via the existing FetchContent pattern, write a single `main.cpp` under `tests/tools/`, and define one new CMake executable target. The executable accepts three positional arguments (database, schema, script) plus optional flags (`--migrations`, `--log-level`, `--read-only`). No modifications to `libquiver`, `LuaRunner`, or the C API are needed. The entire implementation should be achievable in one focused phase.

The primary risks are Windows-specific CMake/DLL issues (a solved problem in this codebase, just needs applying to the new target) and error handling correctness (the existing benchmark and sandbox executables are not user-facing and do not catch exceptions — the new CLI must). Both risks have clear, low-effort mitigations. There are no novel technical challenges.

## Key Findings

### Recommended Stack

The project's existing stack is sufficient for everything except argument parsing. `libquiver` provides all database and Lua capabilities; linking against it transitively brings SQLite3, sol2, spdlog, and Lua. The one new dependency is p-ranav/argparse v3.2: a header-only, MIT-licensed, C++17-compatible argument parser that integrates via FetchContent identically to all existing Quiver dependencies.

**Core technologies:**
- p-ranav/argparse v3.2: CLI argument parsing — header-only, zero link-time cost, CMake target `argparse::argparse`, Python-like API that matches Quiver's clean-code philosophy. Chosen over CLI11 (over-engineered for ~5 arguments) and cxxopts (more verbose for positional args).
- libquiver (existing): All database operations and Lua script execution — no changes required.
- CMake FetchContent (existing): Dependency management — argparse follows the exact same pattern as sqlite3, spdlog, sol2, rapidcsv, googletest.

### Expected Features

The feature scope is narrow. The tool does one thing: open a database and run a Lua script. Everything else is configuration.

**Must have (table stakes):**
- Positional `<database>` and `<script>` arguments — universal expectation from every script runner (`sqlite3 db.db`, `lua script.lua`)
- `--schema PATH` / `--migrations PATH` mutually exclusive flags — Quiver databases require initialization; both factory methods must be reachable
- Open-existing fallback — when neither flag is provided, open the database file directly (most common case for re-running scripts)
- Error messages to stderr, meaningful exit codes (0 success, 1 runtime error, 2 usage error) — POSIX convention
- `--help` and `--version` — generated automatically by argparse at zero cost
- Script file validation before execution — check existence, report clearly

**Should have (differentiators):**
- `--log-level` flag — maps to existing `DatabaseOptions.console_level`; default should be `warn` to avoid spdlog info noise mixing with script output
- `--read-only` flag — maps to existing `DatabaseOptions.read_only`; safety net for analysis scripts

**Defer (v2+):**
- REPL / interactive mode — out of scope, different UX paradigm (readline, prompt loop)
- Script arguments (`-- arg1 arg2`) — out of scope, requires Lua `arg` table
- Inline execution (`-e "code"`) — out of scope, shell quoting complexity
- Multiple script execution — use shell scripting instead
- Colorized output — Windows compatibility concerns, spdlog already handles its own coloring

### Architecture Approach

The architecture is maximally simple: one new file, one new CMake target, one new dependency. `main.cpp` parses arguments, reads the script file into a string, constructs `DatabaseOptions`, calls the appropriate factory method, creates a `LuaRunner`, calls `run()`, and handles errors. All logic resides in the existing C++ layer. The CLI is a thin shell that surfaces errors but never crafts its own.

**Major components:**
1. `main.cpp` (NEW) — CLI entry point: arg parsing, file reading, DB opening, Lua execution, error handling, exit codes
2. `libquiver.dll` (EXISTING, UNCHANGED) — `Database` and `LuaRunner` classes; all library logic stays here
3. `argparse::argparse` (NEW DEPENDENCY) — compile-time only, header-only, zero runtime footprint

**Placement:** `tests/tools/main.cpp` with the CMake target defined in `tests/CMakeLists.txt` alongside `quiver_benchmark` and `quiver_sandbox`. This inherits `CMAKE_RUNTIME_OUTPUT_DIRECTORY` pointing to `build/bin/`, placing the executable alongside `libquiver.dll` automatically.

### Critical Pitfalls

1. **DLL not adjacent to executable on Windows** — Every existing executable uses `POST_BUILD` DLL copy commands. The new target must do the same, or it must be placed where `CMAKE_RUNTIME_OUTPUT_DIRECTORY` already puts it alongside `libquiver.dll`. Placing the target in `tests/CMakeLists.txt` resolves this automatically.

2. **Uncaught exceptions producing unhelpful exit behavior** — The existing benchmark and sandbox executables do not catch exceptions (they are developer tools). The user-facing CLI must wrap all operations in a top-level try-catch that prints `e.what()` to stderr and returns `EXIT_FAILURE`. Add a catch-all `catch (...)` for unknown exceptions.

3. **UTF-8 BOM in Lua scripts from Windows editors** — `std::ifstream` does not strip BOMs. Strip the three-byte BOM (`\xEF\xBB\xBF`) from the beginning of the script string before passing to `LuaRunner::run()`. Three lines of code.

4. **File paths with spaces or non-ASCII characters** — Use `std::filesystem::path` for all file arguments. Validate existence of all paths before invoking any library functions and report the full path in error messages.

5. **spdlog info noise mixing with script output** — Default `console_level` in `DatabaseOptions` is `QUIVER_LOG_INFO`. The CLI should default to `QUIVER_LOG_WARN` unless the user passes `--log-level debug` or `--log-level info`.

## Implications for Roadmap

The entire feature set fits in a single phase. There are no architectural dependencies that require sequenced phases. The implementation order within the phase is dictated by dependencies in `main.cpp`, not by separate deliverables.

### Phase 1: quiver_lua CLI Executable

**Rationale:** All prerequisites exist. `libquiver` is fully built, `LuaRunner` is functional, and the C++ public API is stable. The only work is the CLI shell. One phase is correct — splitting into multiple phases would create artificial milestones for a ~100-line file.

**Delivers:** A working `quiver_lua.exe` that users can invoke from the command line to run Lua scripts against Quiver databases.

**Addresses (from FEATURES.md):**
- All table stakes features: positional args, schema/migrations flags, open-existing fallback, error handling, help/version, script validation
- Differentiator features: `--log-level`, `--read-only`

**Implementation order within phase (dependency-driven):**
1. CMake setup — argparse FetchContent in `cmake/Dependencies.cmake`, `quiver_lua` target in `tests/CMakeLists.txt`
2. Argument parsing — argparse setup, positional args (database, schema, script), optional flags, help/version
3. Database opening — `--schema`/`--migrations` mutual exclusivity, open-existing fallback
4. Script loading — file read, BOM strip, existence validation
5. Execution — `LuaRunner(db).run(script)`
6. Error handling — top-level try-catch, exit code mapping, stderr output, spdlog default level

**Avoids (from PITFALLS.md):**
- DLL adjacency: inherit `CMAKE_RUNTIME_OUTPUT_DIRECTORY` from `tests/CMakeLists.txt` placement
- Uncaught exceptions: top-level try-catch wraps all operations
- UTF-8 BOM: strip before `run()`
- Path issues: `std::filesystem::path`, pre-validation
- spdlog noise: default to `QUIVER_LOG_WARN`

### Phase Ordering Rationale

- Single phase is correct because there are no external dependencies to wait on and no architectural risk that requires staged delivery.
- Implementation order within the phase is dictated by the data flow in `main.cpp`: you cannot open a database before parsing arguments, cannot run a script before opening a database.
- Anti-patterns to avoid are well-understood (REPL, script args, inline execution, subcommands) and are explicitly deferred, keeping the phase clean.

### Research Flags

Phases with standard patterns (skip research-phase):
- **Phase 1 (CLI Executable):** Patterns are fully established. CMake FetchContent, argparse API, `Database`/`LuaRunner` usage, and Windows DLL handling are all documented in existing project code or in official library docs. No additional research needed before implementation.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | argparse v3.2 version and CMake target name verified from official GitHub. FetchContent pattern verified from existing `cmake/Dependencies.cmake`. No uncertainty. |
| Features | HIGH | Derived from `PROJECT.md` scope decisions, existing `Database`/`LuaRunner` API, and established CLI conventions (sqlite3, lua interpreter, POSIX). All features map directly to existing library capabilities. |
| Architecture | HIGH | Verified directly from `tests/CMakeLists.txt` (benchmark/sandbox patterns), `cmake/Dependencies.cmake` (FetchContent pattern), `include/quiver/lua_runner.h`, and `include/quiver/options.h`. Confidence is not inferred — it is observed. |
| Pitfalls | HIGH | DLL adjacency issue is a known Windows CMake pattern verified in existing targets. Exception handling gap is observed directly in benchmark/sandbox source. BOM and path issues are well-documented Windows C++ problems. |

**Overall confidence:** HIGH

### Gaps to Address

- **Default log level selection:** Research recommends `QUIVER_LOG_WARN` as the CLI default to avoid spdlog info noise. Confirm this is the right UX choice during implementation — if users expect to see database activity by default, `QUIVER_LOG_INFO` is appropriate. Low-stakes decision, easy to change.
- **Schema argument structure:** ARCHITECTURE.md shows three positional args (`database`, `schema`, `script`) plus a `--migrations` flag as the mode selector. FEATURES.md shows two positional args (`database`, `script`) with `--schema PATH` and `--migrations PATH` as optional flags (allowing no-schema open). These two designs differ and need a final decision during implementation. Recommendation: go with FEATURES.md's two-positional design (database, script) plus optional `--schema`/`--migrations` flags, as it handles the open-existing case more naturally.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection: `tests/CMakeLists.txt`, `cmake/Dependencies.cmake`, `include/quiver/lua_runner.h`, `include/quiver/options.h`, `include/quiver/c/options.h` — verified CMake patterns, API surface, and option types
- [p-ranav/argparse GitHub](https://github.com/p-ranav/argparse) — v3.2 release confirmed Jan 25, 2025; `argparse::argparse` CMake target verified from official CMakeLists.txt
- [SQLite CLI Documentation](https://sqlite.org/cli.html) — argument structure reference
- [Lua Standalone Interpreter](https://www.lua.org/manual/5.4/lua.html) — argument structure reference
- [POSIX Exit Status Conventions](https://en.wikipedia.org/wiki/Exit_status) — 0 success, 1 error, 2 usage error

### Secondary (MEDIUM confidence)
- [CLI11 on GitHub](https://github.com/CLIUtils/CLI11) — evaluated and rejected as over-engineered for this use case
- [jarro2783/cxxopts GitHub](https://github.com/jarro2783/cxxopts) — evaluated and rejected; less natural for positional arguments
- [CMake Discourse: DLL linking on Windows](https://discourse.cmake.org/t/linking-to-shared-library-built-within-my-project-works-under-linux-but-not-windows/8522) — DLL adjacency pattern confirmation
- [Microsoft: Modern C++ exception handling](https://learn.microsoft.com/en-us/cpp/cpp/errors-and-exception-handling-modern-cpp?view=msvc-170) — top-level try-catch rationale

### Tertiary (LOW confidence)
- UTF-8 BOM handling: [Reading UTF File with BOM in C++](https://vicidi.wordpress.com/2015/03/09/reading-utf-file-with-bom-to-utf-8-encoded-stdstring-in-c11-on-windows/) — the BOM stripping pattern is simple and standard; the source is secondary but the technique is well-known

---
*Research completed: 2026-02-27*
*Ready for roadmap: yes*
