# Domain Pitfalls

**Domain:** Adding a standalone CLI executable (`quiver_lua`) to an existing C++ library project
**Researched:** 2026-02-27
**Confidence:** HIGH -- pitfalls derived from direct codebase inspection combined with well-known C++/CMake patterns

## Critical Pitfalls

Mistakes that cause build failures, runtime crashes, or major rework.

### Pitfall 1: Linking the Executable Against the Shared Library Without DLL Adjacency

**What goes wrong:** The new `quiver_lua` executable links against `libquiver` (shared library), but at runtime on Windows, the system cannot find `libquiver.dll` because it is not in the same directory as the `.exe` or on `PATH`. The executable crashes immediately with a missing DLL error dialog.

**Why it happens:** CMake's `target_link_libraries` resolves the import library (`.lib`) at link time, but Windows DLL resolution at runtime requires the `.dll` to be discoverable. The project already outputs everything to `build/bin/` via `CMAKE_RUNTIME_OUTPUT_DIRECTORY`, so this works *during development*. But forgetting to add a `POST_BUILD` copy command (as already done for `quiver_tests`, `quiver_c_tests`, and `quiver_benchmark` in `tests/CMakeLists.txt`) would break the executable if it were defined in a different output directory.

**Consequences:** Executable silently fails to start on Windows. Users see a cryptic system dialog ("The code execution cannot proceed because libquiver.dll was not found"), not a useful error message.

**Prevention:** Define the `quiver_lua` target in a location where `CMAKE_RUNTIME_OUTPUT_DIRECTORY` already places it alongside `libquiver.dll` in `build/bin/`. If the target is defined in `src/CMakeLists.txt` or a new `cli/CMakeLists.txt` subdirectory added from `src/CMakeLists.txt`, the output directory setting from the root `CMakeLists.txt` (line 36: `set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)`) will already apply. Alternatively, add an explicit `POST_BUILD` copy command as the tests do. The simplest approach: define the target in `src/CMakeLists.txt` alongside the library, which inherits the global output directory.

**Detection:** The `.exe` is built successfully but crashes immediately when run from a directory where the DLL is not present.

### Pitfall 2: Forgetting /bigobj for sol2 Template Instantiations in the CLI Target

**What goes wrong:** The `quiver_lua` executable's `main.cpp` includes `LuaRunner` which transitively pulls in sol2 headers. On MSVC, compilation fails with "fatal error C1128: number of sections exceeded object file format limit" because sol2 generates enormous template instantiations.

**Why it happens:** The existing `src/CMakeLists.txt` already applies `/bigobj` specifically to `lua_runner.cpp` (lines 51-55). However, the new `quiver_lua` target compiles its own `.cpp` file(s), and if those files include `<quiver/lua_runner.h>` -- which pulls sol2 through the Pimpl boundary -- this should not be an issue because `lua_runner.h` forward-declares `Impl` and does NOT include sol2 headers. But if the CLI's main.cpp inadvertently includes sol2 directly (e.g., for error handling), or if the compilation unit grows large enough, the same `/bigobj` issue appears.

**Consequences:** Build failure on MSVC with a confusing error about "sections exceeded."

**Prevention:** The CLI executable should NOT include sol2 headers directly. It should only include `<quiver/lua_runner.h>` and `<quiver/database.h>`, which use Pimpl and do not expose sol2 or sqlite3 internals. This is already the correct design. If build issues arise anyway (large single TU), apply `/bigobj` to the CLI source file as done for `lua_runner.cpp`. Keep the CLI's `main.cpp` minimal.

**Detection:** MSVC build error referencing section limits.

### Pitfall 3: Uncaught Exceptions in main() Producing Unhelpful Exit Behavior

**What goes wrong:** The C++ library throws `std::runtime_error` for all error conditions (three patterns: "Cannot ...", "... not found: ...", "Failed to ..."). The Lua layer adds `std::runtime_error("Lua error: ...")` via `LuaRunner::run()`. If `main()` does not catch these, the program calls `std::terminate()`, which on Windows produces a crash dialog or a large negative exit code, with no user-visible error message.

**Why it happens:** Developers focus on the happy path and forget that every `Database::from_schema()`, `Database::from_migrations()`, and `LuaRunner::run()` call can throw. The existing benchmark and sandbox executables do NOT have try-catch in `main()` because they are developer tools, not user-facing. Copying their pattern for a user-facing CLI is wrong.

**Consequences:** Users see no error message, just a crash or unhelpful exit code. Scripts calling `quiver_lua` cannot distinguish between different failure modes. On Windows, a crash dialog may appear requiring user interaction.

**Prevention:** Wrap the entire body of `main()` in a try-catch that catches `std::exception`, prints `e.what()` to `stderr`, and returns a non-zero exit code (e.g., `EXIT_FAILURE`). Add a catch-all `catch (...)` for unknown exceptions. Follow this pattern:

```cpp
int main(int argc, char* argv[]) {
    try {
        // parse args, open db, run script
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Error: unknown exception" << std::endl;
        return EXIT_FAILURE;
    }
}
```

**Detection:** Run the executable with invalid inputs (bad path, bad schema, syntax error in Lua) and check whether a clear error message appears on stderr and the exit code is non-zero.

### Pitfall 4: File Path Arguments with Spaces or Special Characters Failing Silently

**What goes wrong:** Users pass file paths containing spaces (e.g., `C:\My Documents\schema.sql`) or special characters. The argument parser may split these incorrectly, or the file read may fail because the path string is not properly handled.

**Why it happens:** On Windows, `argv` encoding depends on the compilation mode. MSVC on Windows uses the current code page for `main(int, char**)`, which means non-ASCII paths can be mangled. Even with proper encoding, spaces in paths are a classic issue if argument parsing is not robust.

**Consequences:** The executable fails to open the database, schema, or Lua script file with a confusing "File not found" error even though the file exists.

**Prevention:**
1. Use `std::filesystem::path` for all file arguments after parsing. It handles platform-specific path semantics correctly.
2. Validate that all three file paths (database, schema/migrations, script) exist before attempting to use them, and report clear errors with the full path shown.
3. Test with paths containing spaces, e.g., `"C:/path with spaces/script.lua"`.
4. On Windows, users invoke from cmd.exe or PowerShell which handle quoting differently -- this is OS-level, not something the executable controls, but documentation should mention quoting paths with spaces.

**Detection:** Test with `quiver_lua "C:\My Folder\db.sqlite" "C:\My Folder\schema.sql" "C:\My Folder\script.lua"`.

## Moderate Pitfalls

### Pitfall 5: Lua Script File Read Not Handling UTF-8 BOM

**What goes wrong:** A Lua script saved by a Windows text editor (e.g., Notepad, which historically adds UTF-8 BOM by default) has a three-byte BOM prefix (`\xEF\xBB\xBF`). When the script is read as a string and passed to `LuaRunner::run()`, the BOM bytes become part of the script text. Lua interprets these as invalid syntax or a garbled first token, producing a confusing parse error.

**Why it happens:** `std::ifstream` reads raw bytes. It does not strip BOMs. The BOM is invisible in text editors but present in the byte stream. The Lua parser does not expect or handle BOMs.

**Prevention:** After reading the file into a string, check if the first three bytes are `\xEF\xBB\xBF` and strip them before passing to `LuaRunner::run()`. This is a simple three-line check:

```cpp
std::string script = /* read file */;
if (script.size() >= 3 && script[0] == '\xEF' && script[1] == '\xBB' && script[2] == '\xBF') {
    script.erase(0, 3);
}
```

**Detection:** Save a test `.lua` file with BOM in Notepad, run it through `quiver_lua`, observe a syntax error on line 1.

### Pitfall 6: Argument Parsing Library Choice Causing Unnecessary Dependency Complexity

**What goes wrong:** Choosing a full-featured argument parsing library (CLI11, Boost.Program_options) adds FetchContent overhead, increases build time, and introduces a dependency that dwarfs the actual CLI logic. The `quiver_lua` CLI has exactly three positional arguments and one mode flag -- it does not need subcommands, validators, or configuration file integration.

**Why it happens:** Developers default to "use a library" for argument parsing without assessing whether the complexity is justified.

**Prevention:** Use a minimal header-only library. The best fit is [p-ranav/argparse](https://github.com/p-ranav/argparse) -- single header, C++17, 5K+ GitHub stars, no dependencies, supports positional arguments and optional flags cleanly. It matches the project's FetchContent pattern and minimalist philosophy. Alternative: hand-roll it with a 20-line function since the interface is so simple (three required paths + a mode flag), but argparse provides free `--help` generation and error messages, which are valuable for a user-facing tool.

### Pitfall 7: Not Validating File Existence Before Passing to Library

**What goes wrong:** The user provides a non-existent schema path or script path. The library function (`Database::from_schema()` or file read for Lua script) throws a `std::runtime_error` with an internal error message. While the top-level catch in `main()` will print it, the error message may be confusing because it comes from deep inside the library rather than from the CLI layer.

**Why it happens:** The CLI passes arguments directly to library functions without pre-validation. The library's error messages are designed for programmatic consumers, not end users.

**Prevention:** Validate all three file paths before calling any library functions:

```cpp
if (!std::filesystem::exists(script_path)) {
    std::cerr << "Error: script file not found: " << script_path << std::endl;
    return EXIT_FAILURE;
}
```

This gives the user a clear, CLI-appropriate error message instead of a library-internal one. Do the same for schema/migrations path and database path (for the parent directory).

### Pitfall 8: Confusing from_schema vs from_migrations Mode Selection

**What goes wrong:** The CLI needs to support both `Database::from_schema()` and `Database::from_migrations()` because both factory methods exist. If the argument interface is ambiguous (e.g., a single `--schema` flag that could mean either), users will not understand which mode they are in. Worse, passing a migrations directory to `from_schema()` or a single schema file to `from_migrations()` produces confusing errors.

**Why it happens:** Both factory methods take string paths but expect fundamentally different inputs (single `.sql` file vs. directory of ordered migration files).

**Prevention:** Make the mode explicit in the CLI interface. Use mutually exclusive flags or subcommands:
- `quiver_lua --schema schema.sql db.sqlite script.lua`
- `quiver_lua --migrations ./migrations/ db.sqlite script.lua`

The argument parser should enforce mutual exclusivity. Validate that `--schema` points to a file and `--migrations` points to a directory.

### Pitfall 9: The Executable Target Not Being Built by Default

**What goes wrong:** The new `quiver_lua` target is added but not included in the default build or in `build-all.bat`. Developers build the project and forget the CLI exists because it is never built or tested.

**Why it happens:** If the target is guarded behind an option flag (like `QUIVER_BUILD_C_API` guards `quiver_c`) or placed in a subdirectory that is not always added, it will only be built when explicitly requested.

**Prevention:** The CLI executable should be built unconditionally (like `quiver_benchmark` and `quiver_sandbox` in `tests/CMakeLists.txt`). It links against `libquiver` (not `libquiver_c`), so it has no optional dependency. Add it to `src/CMakeLists.txt` or a new `cli/CMakeLists.txt` that is always included. Update `build-all.bat` and `scripts/test-all.bat` if the CLI needs integration testing.

## Minor Pitfalls

### Pitfall 10: spdlog Console Output Interfering with CLI stderr

**What goes wrong:** The library uses spdlog for logging with a default level of `QUIVER_LOG_INFO`. When the CLI runs, spdlog info messages (e.g., "Opening database: ...") are printed to stderr, mixing with the CLI's own error output and the Lua script's `print()` output on stdout.

**Prevention:** Set `console_level` to `QUIVER_LOG_WARN` or `QUIVER_LOG_OFF` in the `DatabaseOptions` passed to the factory method. Optionally expose a `--verbose` flag that sets `QUIVER_LOG_DEBUG`.

### Pitfall 11: Lua print() Output Buffering on Windows

**What goes wrong:** Lua's `print()` function writes to stdout via C's `stdout` stream. On Windows, stdout may be fully buffered when redirected to a file or pipe, causing output to appear only after the program exits or the buffer fills. Interactive users see output immediately (line-buffered by default for consoles), but scripts piping output may miss partial output if the program crashes.

**Prevention:** This is a minor issue and the default behavior is acceptable for v0.5. If it becomes a problem, `setvbuf(stdout, nullptr, _IONBF, 0)` at the start of `main()` disables buffering entirely.

### Pitfall 12: Hardcoding Test Schema Paths Relative to __FILE__

**What goes wrong:** The existing test executables use `path_from(__FILE__, "../schemas/valid/...")` to find test schemas relative to the source file. If the CLI's integration tests follow this pattern, the paths break when the source file is moved or when running from a different working directory.

**Prevention:** For CLI integration tests, use absolute paths constructed from a known root (e.g., `CMAKE_SOURCE_DIR` passed as a compile definition) or use the existing test schema location pattern. Better: test the CLI as a subprocess invocation from a test, not by #including test utilities.

### Pitfall 13: Windows Console Code Page for Error Messages

**What goes wrong:** If database paths or error messages contain non-ASCII characters (e.g., accented characters in file paths), the Windows console may not display them correctly because the default code page is not UTF-8.

**Prevention:** This is a Windows-wide issue, not specific to Quiver. For v0.5, document that non-ASCII paths may display incorrectly in error messages on older Windows versions. On Windows 10+, users can enable UTF-8 globally. The executable can call `SetConsoleOutputCP(CP_UTF8)` at startup as an optional enhancement.

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| CMake target setup | DLL not adjacent to .exe (#1), not built by default (#9) | Place target in `src/CMakeLists.txt`, verify `CMAKE_RUNTIME_OUTPUT_DIRECTORY` covers it |
| Argument parsing | Over-engineered library (#6), schema/migrations confusion (#8) | Use p-ranav/argparse, enforce mutually exclusive `--schema`/`--migrations` flags |
| File reading | UTF-8 BOM corruption (#5), paths with spaces (#4) | Strip BOM, use `std::filesystem::path`, validate existence before use (#7) |
| Error handling | Uncaught exceptions (#3), spdlog noise (#10) | Top-level try-catch in main(), set log level to WARN by default |
| Build integration | Target not in build-all.bat (#9), /bigobj on MSVC (#2) | Unconditional target, keep main.cpp minimal to avoid sol2 exposure |
| Testing | Schema path assumptions (#12) | Test CLI as subprocess, use known absolute paths |

## Sources

- Direct codebase inspection of `CMakeLists.txt`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `lua_runner.cpp`, `export.h`, `CompilerOptions.cmake`, `Dependencies.cmake`, `Platform.cmake`
- [Why I Catch Exceptions in Main Function in C++](https://asawicki.info/news_1763_why_i_catch_exceptions_in_main_function_in_c)
- [CMake Discourse: Why is manual copying of .dll needed](https://discourse.cmake.org/t/why-is-manual-copying-of-dll-needed-when-linking-library/6971)
- [CMake Discourse: Linking shared library on Windows](https://discourse.cmake.org/t/linking-to-shared-library-built-within-my-project-works-under-linux-but-not-windows/8522)
- [p-ranav/argparse on GitHub](https://github.com/p-ranav/argparse)
- [CLI11 on GitHub](https://github.com/CLIUtils/CLI11)
- [Reading UTF File with BOM to UTF-8 in C++](https://vicidi.wordpress.com/2015/03/09/reading-utf-file-with-bom-to-utf-8-encoded-stdstring-in-c11-on-windows/)
- [Microsoft: Modern C++ best practices for exceptions](https://learn.microsoft.com/en-us/cpp/cpp/errors-and-exception-handling-modern-cpp?view=msvc-170)
