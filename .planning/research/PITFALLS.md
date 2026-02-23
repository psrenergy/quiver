# Pitfalls Research

**Domain:** Replacing a hand-rolled CSV writer with a third-party library, and consolidating C API option headers, in a multi-layer FFI system (C++20 / C API / Julia / Dart / Lua)
**Researched:** 2026-02-22
**Confidence:** HIGH — based on direct inspection of all affected source files, both FFI generator configurations, and the full test suites

---

## Critical Pitfalls

### Pitfall 1: CRLF Line Ending Contamination from Library Defaults

**What goes wrong:**
The hand-rolled writer opens the output file in `std::ios::binary` mode and emits a bare `'\n'` per row. The test `ExportCSV_LFLineEndings` explicitly asserts `EXPECT_EQ(content.find("\r\n"), std::string::npos)`. Many CSV libraries default to CRLF line endings (RFC 4180 section 2 technically recommends CRLF). On Windows, a stream opened without `std::ios::binary` silently converts every `'\n'` to `'\r\n'` at the OS level. Either behavior breaks all 38 CSV tests that search for `"field\n"` substrings.

**Why it happens:**
Library authors reasonably comply with RFC 4180's CRLF recommendation, or they delegate to the platform stream. A developer integrating the library either uses default settings or forgets to open the stream in binary mode, and suddenly all substring searches for `"...\n"` fail because the actual output contains `"...\r\n"`.

**How to avoid:**
Before selecting a library, verify it exposes explicit line-ending control (a `set_line_terminator('\n')` option or the ability to inject a pre-opened binary-mode `std::ostream`). This is a hard requirement, not a preference. The `ExportCSV_LFLineEndings` test will catch this on Windows but not on Linux/macOS, so run the full test suite on Windows — the primary build target — during the library evaluation.

**Warning signs:**
- Library documentation says "RFC 4180 compliant" without specifying line-ending override capability
- Library opens its own output stream internally and does not accept a caller-provided `std::ostream`
- Tests pass on Linux/macOS but fail on Windows (every row assertion off by one character)

**Phase to address:** Library selection phase — verify line-ending control before committing to any library.

---

### Pitfall 2: Float Formatting Divergence from `%g` Semantics

**What goes wrong:**
The current writer uses `snprintf(buf, sizeof(buf), "%g", value)` producing `9.99`, `19.5`, `1.1`, `2.2`, `3.3`, `22.5`, `23` (trailing zeros stripped, scientific notation suppressed). Multiple tests assert exact strings: `"Item1,Alpha,1,9.99,...\n"`, `"Item1,1.1\n"`, `"Item1,2024-01-01T11:00:00,23,55\n"`. Many CSV libraries use `std::to_string` (six decimal places), `std::format("{}", v)` (implementation-defined), or `operator<<` on a stream (which for `23.0` gives `"23"` but for `1.1` may give `"1.1"` or `"1.10000000000000009"` depending on stream precision).

**Why it happens:**
`%g` is C-style and not the default in any C++ iostream or CSV library. The crucial case is `23.0` which `%g` renders as `"23"` — dropping both the decimal point and trailing zero. Most C++ formatters produce `"23"` from `(double)23.0` too, but `1.1` is a different story: IEEE 754 makes `1.1` slightly non-round, and `%g` with default precision 6 truncates to `"1.1"` while a high-precision formatter would give `"1.1000000000000001"`.

**How to avoid:**
Preserve the existing `snprintf` / `%g` float-formatting path in `value_to_csv_string`. Use the library only for field quoting and row assembly; supply pre-formatted strings. Do not let the library serialize `double` values directly. This approach is safe and makes the library a thin row-writing mechanism rather than a value serializer.

**Warning signs:**
- `ExportCSV_TimeSeriesGroupExport` asserts `"Item1,2024-01-01T11:00:00,23,55\n"` — if the library formats `(double)23.0` as `"23.0"` or `"23.000000"`, this test fails
- `ExportCSV_VectorGroupExport` asserts `"Item1,1.1\n"` — high-precision formatting breaks this

**Phase to address:** Library selection — add float formatting to the evaluation checklist before integration begins.

---

### Pitfall 3: NULL Field Misrepresentation

**What goes wrong:**
The current writer converts SQL NULL values to empty unquoted strings. The test `ExportCSV_NullValues_EmptyFields` asserts the exact string `"Item1,Alpha,,,,\n"`. Some CSV libraries emit `"NULL"`, an empty quoted field `""`, or configure a null sentinel. Any of these break the test.

**Why it happens:**
SQL NULL has no standard CSV representation. Library authors make different choices: unquoted empty is what the existing code produces and what most parsers expect for missing data, but some libraries quote empty strings or substitute a user-configurable sentinel.

**How to avoid:**
Keep the NULL-to-empty-string conversion entirely in the existing `value_to_csv_string` function (`std::nullptr_t` branch returns `""`). Never pass a null or unset value to the library's field API. The library receives only pre-converted `std::string` values.

**Warning signs:**
- Library has a `set_null_value()` or `null_representation` configuration option — check what its default produces
- Library's field API accepts `std::optional<std::string>` and formats `std::nullopt` differently than an empty string

**Phase to address:** Integration — verify NULL handling on the first test run after the library is wired in.

---

### Pitfall 4: Over-Quoting of Clean Fields Breaks Substring Searches

**What goes wrong:**
The current writer only quotes fields containing a comma, double-quote, newline, or carriage return. Fields like `Alpha`, `1`, `9.99` appear unquoted. Tests use `content.find("Item1,Alpha,1,9.99,...")` — if the library quotes all string fields unconditionally, `Alpha` becomes `"Alpha"` in the output and the substring search returns `npos`, failing the test.

**Why it happens:**
Quoting all fields is RFC 4180-valid but more conservative than necessary. Libraries aimed at safety-first interchange default to it. The existing test design relies on minimal quoting (quote only when the field contains a special character).

**How to avoid:**
Choose a library with a configurable quoting strategy that can be set to "quote only when necessary." If the library only supports quote-always, use it purely for row assembly (joining pre-escaped field strings with commas and appending a newline) while continuing to pre-escape fields via the existing `csv_escape()` function. Do not pass values through both the existing escaper and the library's auto-quoting, or fields will be double-quoted.

**Warning signs:**
- Library has no "minimal quoting" mode
- Library always wraps string fields in double quotes
- After integration, `ExportCSV_ScalarExport_HeaderAndData` fails because `"Item1"` appears as `"""Item1"""`

**Phase to address:** Library selection — test minimal quoting mode with a clean field before committing to any library.

---

### Pitfall 5: `csv.h` Include in Test File Overlooked During Header Deletion

**What goes wrong:**
When `include/quiver/c/csv.h` is deleted and its contents merged into `include/quiver/c/options.h`, there are exactly two include sites for `csv.h`:

1. `src/c/database_csv.cpp` line 2: `#include "quiver/c/csv.h"`
2. `tests/test_c_api_database_csv.cpp` line 6: `#include <quiver/c/csv.h>`

The migration sweep naturally focuses on production code in `src/`. The test file is a separate compilation unit and is easy to miss. The result is a build error in the C API test binary.

**Why it happens:**
Header consolidation migrations typically proceed by following the dependency tree from the production source. Test files are not part of that tree — they are separate compilation units that include headers directly and are only discovered if the search explicitly covers `tests/`.

**How to avoid:**
Before deleting `csv.h`, run a codebase-wide grep for `csv.h` across all file types (`*.cpp`, `*.h`, `*.jl`, `*.dart`). The current output would find exactly the two sites above. Update both. Then delete the file. Optionally, leave a one-build-cycle compatibility stub `csv.h` containing only `#include "options.h"` to catch any missed sites at compile time — then remove the stub in the same PR once the build is confirmed clean.

**Warning signs:**
- Build of `quiver_c_tests.exe` fails with `fatal error: quiver/c/csv.h: No such file or directory`
- Only `src/` was searched for includes before deletion

**Phase to address:** Header consolidation phase — run the grep before touching any file.

---

### Pitfall 6: Julia Generator Produces Duplicate Struct Definitions

**What goes wrong:**
The Julia generator in `bindings/julia/generator/generator.jl` uses `readdir(include_dir)` to collect all `.h` files in `include/quiver/c/`. It then builds a single Clang context from all of them. If `csv.h` remains as a compatibility stub that `#include`s `options.h`, the generator processes both files. Depending on how Clang.jl deduplicates, it may emit `quiver_csv_export_options_t` twice in the generated `c_api.jl`, causing a Julia `UndefVarError` or "cannot redefine" error at module load time.

**Why it happens:**
The `readdir`-based approach treats every `.h` file as an independent entry point. Include guards prevent duplicate C declarations, but the Clang.jl generator may still emit the struct once per entry-point header that references it, depending on its deduplication strategy.

**How to avoid:**
Delete `csv.h` completely — do not leave a compatibility stub. The generator's `readdir` pattern means any remaining file will be processed as an entry point. After deletion, regenerate Julia bindings and confirm `quiver_csv_export_options_t` appears exactly once in `c_api.jl`.

**Warning signs:**
- `c_api.jl` contains `struct quiver_csv_export_options_t` defined more than once
- Julia test startup fails with "cannot redefine constant C.quiver_csv_export_options_t"
- `csv.h` stub was left "for safety" without considering the generator's readdir enumeration

**Phase to address:** Header consolidation phase — regenerate Julia bindings immediately after deletion and diff `c_api.jl`.

---

### Pitfall 7: Dart ffigen Struct Silently Disappears from Bindings

**What goes wrong:**
`bindings/dart/pubspec.yaml` (and the duplicate `ffigen.yaml`) configures ffigen with an `include-directives` whitelist: `common.h`, `database.h`, `element.h`, `lua_runner.h`. Notice `csv.h` is NOT in the whitelist. `quiver_csv_export_options_t` currently reaches the generated `bindings.dart` transitively because `database.h` includes `csv.h`. After the merge, the struct moves to `options.h`. Since `database.h` also includes `options.h`, the struct continues to flow through transitively — but only if the `#include "options.h"` in `database.h` is preserved. If that line is accidentally removed or if the merge leaves the struct defined only in a file that is not transitively reachable from `database.h`, the struct silently vanishes from `bindings.dart` without a compile error.

**Why it happens:**
The ffigen `include-directives` filter emits types from whitelisted headers and their transitive includes. If the transitive include chain breaks, the type disappears from output rather than producing an error — it simply is not seen by the filter.

**How to avoid:**
After running ffigen following the header merge, diff `bindings.dart` against the previous version. Confirm `quiver_csv_export_options_t` is still present (currently at line 3105) with all six fields in the correct order. Verify `database.h` retains `#include "options.h"` after the merge. This diff is a mandatory step, not an optional check.

**Warning signs:**
- `bindings.dart` no longer contains `class quiver_csv_export_options_t extends ffi.Struct`
- Dart compilation fails with "Undefined name 'quiver_csv_export_options_t'" in `database_csv.dart`
- ffigen ran without errors but the struct count in `bindings.dart` decreased

**Phase to address:** Header consolidation phase — diff `bindings.dart` immediately after ffigen regeneration.

---

### Pitfall 8: Dart `.dart_tool/` Cache Retains Stale Bindings After Header Changes

**What goes wrong:**
The Dart binding uses a build hook that compiles the C++ DLL from source. After renaming or deleting a header file, the hooks runner may use a cached build artifact that still references the old header. The Dart test suite passes locally (using the stale DLL) but fails on a clean machine or CI. This is the same scenario documented in `CLAUDE.md`: "When C API struct layouts change, clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` to force a fresh DLL rebuild."

**Why it happens:**
The hooks runner uses a hash of build inputs to decide whether to rebuild. Deleting a header file may not invalidate the hash if the hash is computed only from file contents that remain, or if the hash is stale from a previous run.

**How to avoid:**
After any header file rename or deletion, manually clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` before running Dart tests. Add this step to the migration checklist for the header consolidation phase. The `test.bat` script handles PATH setup but does not clear this cache automatically.

**Warning signs:**
- Dart tests pass locally but fail on a clean CI build
- `libquiver_c.dll` in `.dart_tool/` is older than the last source change timestamp
- Dart test errors reference types or functions that should no longer exist

**Phase to address:** Header consolidation phase — clear cache as the first step after any header deletion.

---

### Pitfall 9: Both Dart Generator Config Files Must Stay In Sync

**What goes wrong:**
The Dart binding has two separate ffigen configuration sources: `bindings/dart/pubspec.yaml` (the `ffigen:` section) and `bindings/dart/ffigen.yaml`. They currently contain identical content. After the header merge, only one file is updated — leading to inconsistent behavior depending on which config ffigen reads. This can produce a regenerated `bindings.dart` that still references `csv.h` in some entry-point list, or that uses a stale `include-directives` whitelist.

**Why it happens:**
The duplication exists by design (one for IDE tooling, one for CLI invocation), but creates a maintenance trap when headers change. There is no single source of truth.

**How to avoid:**
Treat both files as a single unit: edit both simultaneously when any header path changes. After the merge, verify both files have identical content for the ffigen configuration keys. The migration checklist should include "update both `pubspec.yaml` and `ffigen.yaml`" as a single atomic step.

**Warning signs:**
- `pubspec.yaml` lists `csv.h` in `entry-points` but `ffigen.yaml` does not (or vice versa)
- `dart run ffigen` produces different output depending on working directory
- One file references the old `csv.h` path after deletion

**Phase to address:** Header consolidation phase.

---

### Pitfall 10: New CMake Library Dependency Creates Extra DLL on Windows

**What goes wrong:**
Quiver builds two DLLs on Windows: `libquiver.dll` and `libquiver_c.dll`. The Dart binding notes "both must be in PATH" and `test.bat` handles this automatically. Adding a third-party CSV library via `FetchContent` or `find_package` may introduce a third DLL that the test runner does not add to PATH, causing Dart (and potentially Julia) tests to fail with "DLL not found" at runtime.

**Why it happens:**
CMake `FetchContent` integration behavior depends on whether the upstream library exports a `SHARED` or `STATIC` CMake target. Header-only libraries and libraries that compile to static archives are transparent. A library that produces a `SHARED` target adds a DLL that must be discoverable at runtime.

**How to avoid:**
Prefer a header-only or static-linkage CSV library. The simplest outcome: zero new DLLs. If a shared library is unavoidable, update `bindings/dart/test/test.bat` and `bindings/julia/test/test.bat` to add the new DLL's directory to PATH, following the existing pattern. Verify the CMake target type before committing to a library.

**Warning signs:**
- `build/bin/` contains a new `.dll` not present before library integration
- Dart tests fail with "The specified module could not be found" on Windows
- Library's `CMakeLists.txt` uses `add_library(csvlib SHARED ...)` or defaults to shared on Windows

**Phase to address:** Library selection — confirm CMake target type and deployment footprint before evaluation.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Keep `value_to_csv_string` pre-formatting and use library only for row/field assembly | Preserves all float/null/enum/datetime logic; library does minimal work | Library's read path (for future CSV import) may expect to own field parsing, requiring a future re-integration | Acceptable for v0.5 scope — import is explicitly out of scope |
| Leave `csv.h` as a compatibility stub `#include`ing `options.h` | No need to hunt all include sites immediately | Julia generator processes both files; risk of duplicate struct definitions in `c_api.jl` | Never — delete completely and fix all include sites in the same commit |
| Skip FFI generator re-run after header merge | Saves one build step | Bindings reference old struct field layout or missing type; Dart/Julia call wrong C function signature | Never — regenerate bindings in the same PR as the header change |
| Leave `.dart_tool/` cache intact after header deletion | Faster local iteration | Stale DLL passes locally, fails on CI or clean build | Never — clear the cache as the first step |
| Update only `pubspec.yaml` and not `ffigen.yaml` (or vice versa) | Half the work | One config remains stale; ffigen behavior depends on invocation method | Never — both files must be updated atomically |

---

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| CSV library line endings | Use library defaults (often CRLF or platform-native) | Require explicit LF-only configuration or inject binary-mode stream; verify before selecting library |
| CSV library float serialization | Let library format `double` values directly | Pre-format with `snprintf("%g", value)` in `value_to_csv_string`, pass `std::string` to library |
| CSV library quoting | Use library's auto-quote mode on all fields | Verify "quote only when necessary" mode exists; or pass pre-escaped strings and disable library quoting |
| Header merge — Julia generator | Assume `readdir` deduplicates correctly when stub left behind | Delete `csv.h` entirely; regenerate and confirm single struct definition in `c_api.jl` |
| Header merge — Dart ffigen | Assume transitive includes always flow through the whitelist | Diff `bindings.dart` before and after; confirm `quiver_csv_export_options_t` with all 6 fields is present |
| Header merge — Dart cache | Assume Dart's build hook invalidates correctly | Manually clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` after any header file deletion |
| Header merge — two Dart configs | Update one ffigen config file | Both `pubspec.yaml` and `ffigen.yaml` must be updated to identical state |

---

## "Looks Done But Isn't" Checklist

- [ ] **Line endings:** `ExportCSV_LFLineEndings` passes on Windows (not just Linux); binary mode confirmed
- [ ] **Float format:** `ExportCSV_TimeSeriesGroupExport` produces `"23"` not `"23.0"`; `ExportCSV_VectorGroupExport` produces `"1.1"` not `"1.10000000000000009"`
- [ ] **NULL fields:** `ExportCSV_NullValues_EmptyFields` produces exact string `"Item1,Alpha,,,,\n"` with no quoting on empty fields
- [ ] **Quoting:** `ExportCSV_ScalarExport_HeaderAndData` shows `Alpha` not `"Alpha"` in output
- [ ] **All `csv.h` include sites updated:** `grep -r "csv.h"` across entire repo returns zero hits
- [ ] **Julia `c_api.jl`:** Contains exactly one `struct quiver_csv_export_options_t` definition after regeneration
- [ ] **Dart `bindings.dart`:** Contains `class quiver_csv_export_options_t extends ffi.Struct` with all 6 fields after regeneration
- [ ] **Dart cache cleared:** `.dart_tool/hooks_runner/` and `.dart_tool/lib/` cleared before first post-merge Dart test run
- [ ] **Both Dart configs consistent:** `pubspec.yaml` and `ffigen.yaml` are identical for all ffigen-relevant keys
- [ ] **No new DLLs on Windows:** `build/bin/` contains the same DLLs as before library integration
- [ ] **All 19 C++ CSV tests pass:** `quiver_tests.exe` -- `DatabaseCSV.*` suite green
- [ ] **All 19 C API CSV tests pass:** `quiver_c_tests.exe` -- `DatabaseCApiCSV.*` suite green
- [ ] **Julia CSV tests pass:** `bindings/julia/test/test.bat` -- `TestDatabaseCSV` suite green
- [ ] **Dart CSV tests pass:** `bindings/dart/test/test.bat` -- `DatabaseCSVTest` suite green

---

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| CRLF contamination discovered after library commit | LOW | Configure library for LF-only or open stream in binary mode; re-run tests; one-line change |
| Float format mismatch in tests | LOW | Pre-format with `snprintf("%g")` before passing to library; library receives `std::string` not `double` |
| NULL field quoted or sentineled by library | LOW | Pre-convert NULL to empty string in `value_to_csv_string` before library receives it |
| Over-quoting of clean fields | LOW | Configure library's minimal-quoting mode, or pass pre-escaped strings and disable library quoting |
| `csv.h` include missed in test file | LOW | Add `#include <quiver/c/options.h>` to `tests/test_c_api_database_csv.cpp`; rebuild |
| Julia duplicate struct in `c_api.jl` | LOW | Delete `csv.h` stub entirely; regenerate Julia bindings |
| Dart `bindings.dart` missing struct | LOW | Verify `database.h` includes `options.h`; re-run ffigen; diff output |
| Dart stale `.dart_tool/` cache | LOW | Clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/`; re-run Dart tests |
| Dart configs out of sync | LOW | Diff `pubspec.yaml` vs `ffigen.yaml`; make identical; re-run ffigen |
| New shared DLL not in PATH | MEDIUM | Switch to static library; or update both `test.bat` files to add new DLL directory to PATH |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| CRLF line ending contamination | Library selection — require LF override | `ExportCSV_LFLineEndings` passes on Windows |
| Float formatting divergence | Library selection — evaluate float output | `ExportCSV_VectorGroupExport` and `ExportCSV_TimeSeriesGroupExport` produce exact expected strings |
| NULL field misrepresentation | Integration — first test run | `ExportCSV_NullValues_EmptyFields` produces exact `"Item1,Alpha,,,,\n"` |
| Over-quoting of clean fields | Library selection — test minimal quoting | `ExportCSV_ScalarExport_HeaderAndData` shows no quoted clean fields |
| `csv.h` include in test file missed | Header consolidation — grep all include sites before deletion | `cmake --build build` succeeds with zero compile errors |
| Julia generator duplicate struct | Header consolidation — delete `csv.h` entirely, then regenerate | Exactly one `quiver_csv_export_options_t` in `c_api.jl` |
| Dart bindings struct disappears | Header consolidation — diff `bindings.dart` after ffigen | `quiver_csv_export_options_t` with 6 fields present in `bindings.dart` |
| Dart stale `.dart_tool/` cache | Header consolidation — clear cache before Dart test run | Dart tests pass on clean checkout without prior build artifacts |
| Dart dual config out of sync | Header consolidation — update both files atomically | `diff pubspec.yaml ffigen.yaml` for ffigen section shows no differences |
| New DLL breaks runtime linking | Library selection — verify CMake target type | `build/bin/` contains no new DLLs; Dart and Julia tests pass without PATH changes |

---

## Sources

- Direct inspection of `src/database_csv.cpp` — hand-rolled writer: binary mode, `'\n'` per row, `%g` float format, NULL-to-empty, minimal quoting
- Direct inspection of `tests/test_database_csv.cpp` and `tests/test_c_api_database_csv.cpp` — 38 tests, all using exact substring assertions; `ExportCSV_LFLineEndings` explicitly checks for absence of `"\r\n"`
- Direct inspection of `bindings/julia/generator/generator.jl` — `readdir(include_dir)` enumerates all `.h` files; no filtering
- Direct inspection of `bindings/dart/pubspec.yaml` and `bindings/dart/ffigen.yaml` — `include-directives` whitelist does not include `csv.h`; `quiver_csv_export_options_t` reaches output only via transitive include from `database.h`
- Direct inspection of `bindings/dart/lib/src/ffi/bindings.dart` lines 3105-3118 — current struct layout with 6 fields
- Direct inspection of `include/quiver/c/database.h` lines 5-7 — `#include "csv.h"` and `#include "options.h"` both present
- Direct inspection of `src/c/database_csv.cpp` line 2 and `tests/test_c_api_database_csv.cpp` line 6 — both include `quiver/c/csv.h` (the only two sites)
- `CLAUDE.md` note: "When C API struct layouts change, clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` to force a fresh DLL rebuild"
- RFC 4180 section 2: CRLF as the standard record terminator (source of library default behavior)

---
*Pitfalls research for: Quiver v0.5 — CSV Library Swap and C API Header Consolidation*
*Researched: 2026-02-22*
