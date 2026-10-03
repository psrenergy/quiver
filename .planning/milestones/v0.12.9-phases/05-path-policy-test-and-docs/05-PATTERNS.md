# Phase 5: Path-Policy Test and Docs - Pattern Map

**Mapped:** 2026-10-03
**Files analyzed:** 5 code/build files + the DOC-01 sweep (19 files) + 9 doc files
**Analogs found:** 5 / 5 for the code files; the sweep and the docs follow existing house style

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `src/lua_runner/path_policy.h` (new) | internal header | utility | `src/csv/csv_read.h` (guard + header comment style); declaration from `src/lua_runner/internal.h:302` | exact |
| `src/lua_runner/path_policy.cpp` (include line only) | utility | file-I/O (path resolve) | itself | n/a |
| `src/lua_runner/internal.h` (drop decl, add include) | internal header | n/a | its own include block `:4-6` | exact |
| `tests/test_sandboxed_path.cpp` (new) | test | file-I/O | `tests/test_lua_runner.h` `LuaSandboxTest` fixture; `tests/test_lua_binary.cpp:423` (device-name prefix assert) | exact |
| `tests/CMakeLists.txt` | config | build | its own `add_executable(quiver_tests ...)` list `:3-54` | exact |
| `src/CMakeLists.txt` | config | build | the `lua_runner/internal.h` listing entry | exact |
| DOC-01 sweep files (src/, tests/, bindings tests, AGENTS.md, `.gitattributes`) | comments | n/a | Phase 4 / write_csv commits that already stripped IDs (below) | exact |
| `bindings/js/src/lua-api.ts` | docs (shipped string) | n/a | "Filesystem sandbox" bullet `:111-117` | exact |
| AGENTS.md x8, `CHANGELOG.md` | docs | n/a | existing sections | exact |

## Pattern Assignments

### `src/lua_runner/path_policy.h` (new)

**Guard / layout analog:** `src/csv/csv_read.h:1-2` and `internal.h:1-2` use `QUIVER_SRC_<DIR>_<FILE>_H`:
```cpp
#ifndef QUIVER_SRC_LUA_RUNNER_INTERNAL_H
#define QUIVER_SRC_LUA_RUNNER_INTERNAL_H

#include "quiver/database.h"
```
**Declaration to move** (`internal.h:302`, inside `namespace quiver::lua_internal`):
```cpp
std::string resolve_sandboxed_path(const Database& db, const std::string& operation, const std::string& path);
```
**Comment to move with it** (`path_policy.cpp:10-12`):
```cpp
// Resolves a script-supplied path against the database file's directory and enforces that the
// result stays strictly inside it (subdirectories allowed). Returns the resolved absolute path.
// `operation` is the public method name the user called (threaded into Pattern 1 messages).
```
Use the RESEARCH.md draft verbatim (§Code Examples). Rules: no sol2 include; comments must not spell `.set_function(`, `new_usertype<`, `open_libraries(` (sync test greps `src/lua_runner/`).

### `src/lua_runner/path_policy.cpp`
Line 1 `#include "lua_runner/internal.h"` -> `#include "lua_runner/path_policy.h"`. Body untouched (messages at `:18`, `:47`, `:55-57` are the test's expected text).

### `src/lua_runner/internal.h`
Add `#include "lua_runner/path_policy.h"` to the project-include block (`:4-6`, quoted includes before `<sol/sol.hpp>`); delete `:302`. Let clang-format 22.1.8 order includes.

### `tests/test_sandboxed_path.cpp` (new)

**Fixture analog:** `tests/test_lua_runner.h:26-44`
```cpp
class LuaSandboxTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        sandbox = std::filesystem::temp_directory_path() /
                  (std::string("quiver_lua_") + info->test_suite_name() + "_" + info->name());
        std::filesystem::remove_all(sandbox);
        std::filesystem::create_directories(sandbox);
    }
    void TearDown() override { std::filesystem::remove_all(sandbox); }
    std::string db_path() const { return (sandbox / "test.db").string(); }
    std::filesystem::path sandbox;
};
```
Derive: `class SandboxedPathTest : public LuaSandboxTest { ... };` (header includes only public quiver headers, so no sol2).

**Database open idiom:** `tests/test_database_errors.cpp:12`
```cpp
quiver::Database db(path, {.read_only = false, .console_level = quiver::LogLevel::Off});
```
**Device-name assertion analog:** `tests/test_lua_binary.cpp:423` (`DeviceNamePathIsReportedWithPrefix`, `#ifdef _WIN32`): assert the exact prefix + non-empty localized OS reason, never the reason text.

**Expected messages** (verbatim from `path_policy.cpp`):
- `"Cannot " + op + ": database is in-memory, file operations are unavailable"`
- `"Cannot " + op + ": cannot resolve path '" + path + "': " + <os reason>`
- `"Cannot " + op + ": path '" + path + "' escapes the database directory '" + weakly_canonical(root).string() + "'"`

Full draft: RESEARCH.md §Code Examples (11 tests Windows / 10 elsewhere). Build expectations from `fs::weakly_canonical(sandbox)`; symlink test uses `std::error_code` overload + `GTEST_SKIP()`.

### `tests/CMakeLists.txt`
Analog: alphabetical list `:3-54`. Insert `test_sandboxed_path.cpp` between `test_row_result.cpp` (`:52`) and `test_schema_validator.cpp` (`:53`); append `${CMAKE_SOURCE_DIR}/src/lua_runner/path_policy.cpp` with the static-link comment; add `target_include_directories(quiver_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)` after the `target_link_libraries(quiver_tests ...)` block (`:56`). Exact text in RESEARCH.md.

### `src/CMakeLists.txt`
List `lua_runner/path_policy.h` next to `lua_runner/internal.h` (listing only). Delete Dart hook caches before the six-suite run.

## Shared Patterns

### DOC-01: replacing a planning ID (house style from commits already in the tree)
Rule: drop the tag when the sentence already states the reason; otherwise substitute the reason in a few words, or name the pinning test. Never leave a dangling clause.

Before/after examples from git history (`src/lua_runner/csv.cpp` / `src/csv/csv_write.cpp`):
```cpp
-    // Lua-visible handle behind db:write_csv (LUA-11, D-35). Wraps the internal writer; sol2 owns
+    // Lua-visible handle behind db:write_csv. Wraps the internal writer; sol2 owns
```
```cpp
-    // The collect-then-validate walk every strict options decoder shares (LUA-09/D-17): read_csv,
+    // The collect-then-validate walk every strict options decoder shares: read_csv,
```
```cpp
-    // Collect a nested option table's entries before any is checked (the same LUA-09 rule), after
+    // Collect a nested option table's entries before any is checked (the same collect-then-validate rule), after
```
```cpp
-            // D-34: to_chars' shortest round-trip form, with no synthetic decimal point -- a whole
+            // to_chars' shortest round-trip form, with no synthetic decimal point -- a whole
```
The third shows the substitution case: the ID carried meaning ("the same rule"), so its name replaces it. Re-wrap to the file's width (~100 cols C++, clang-format enforces 120). Section-header comments in tests (`// CSV-01: ...`) keep the text after the colon. Pin-by-test-name form: `(pinned by LuaRunner_WriteCsv.UnclosedWriterIsFlushedWhenRunReturns)`.

Embedded Lua `--` comments in raw strings: edit in place, keep the line count identical. Stale `close()` comments (`test_lua_runner_write_csv.cpp:1121,1532,1554`) get the true reason (run-exit flush runs after the in-script `db:read_csv`). Diagnostic strings (19) may drop IDs per D-10; test names and compared values never change. Gate: RESEARCH.md PCRE `G`.

### `lua-api.ts` bullet style
Analog `bindings/js/src/lua-api.ts:111-117`:
```
- **Filesystem sandbox.** Every file-touching operation (\`db:export_csv\`, ...) resolves
  relative paths against the directory containing the database file and rejects anything outside it
  ...
```
Bold lead sentence ending in `.`, two-space continuation indent, ~100-col wrap, every backtick escaped `\``, no `${`, no `db:`/`quiver.` before a non-method word. Insert the "What the sandbox does not limit." bullet right after it (text in RESEARCH.md §DOC-04). Keep `Loaded standard libraries: ...` sentence verbatim. Verify: sync test + `bunx biome check src/lua-api.ts`.

### Error messages
Pattern 1 only; the test asserts existing text, invents none.

## No Analog Found
None. `test_sandboxed_path.cpp` is the first test to include a `src/` header and compile a `src/` TU; that is new, but each piece has an analog above.

## Metadata
**Analog search scope:** `src/lua_runner/`, `src/csv/`, `tests/`, `bindings/js/src/lua-api.ts`, git history of ID-stripping commits
**Pattern extraction date:** 2026-10-03
