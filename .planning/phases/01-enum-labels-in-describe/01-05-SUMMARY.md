---
phase: 01-enum-labels-in-describe
plan: 05
subsystem: testing
tags: [gtest, c-api, lua, sol2, describe, utf-8, ffi-boundary]

# Dependency graph
requires:
  - phase: 01-01
    provides: "src/ui_config.{h,cpp} parser, Database::has_ui_config(), tests/test_ui_fixture.h open_ui_fixture helper, tests/schemas/ui/{enum_basic,malformed,no_ui_dir}/"
  - phase: 01-02
    provides: "tests/schemas/ui/{bess_like,foresight_like,htd_like,no_enum,empty_enum,unknown_keys,format_table,orphan_collection}/, tests/schemas/ui/README.md (the authoritative fixture-literal table)"
  - phase: 01-03
    provides: "src/database_describe.cpp full scalar-line rendering (unit/[hidden]/label/vocabulary clauses, UI config header, collection label)"
  - phase: 01-04
    provides: "src/ui_config.cpp parser tolerances (unknown keys, format table form, empty enum.toml, PascalCase id)"
provides:
  - "tests/test_c_api_database_describe.cpp -- DatabaseCApiDescribe suite (6 tests), proving the enum/unit/hidden/label/header/non-ASCII rendering survives the char** marshalling boundary"
  - "tests/test_lua_runner_describe.cpp -- 6 new LuaRunnerDescribe cases, proving the same rendering survives the sol2 boundary"
affects: [01-06]

# Actuals (#2632)
actuals:
  tokens: 3150
  tasks: 2
  commits: 2

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "C API describe tests build the database through quiver_database_from_schema itself (never through the C++ test_ui_fixture.h helper, which returns a quiver::Database), writing the sqlite file inside the shared tests/schemas/ui/<fixture>/ directory so the ui/ sidecar sibling is found (D-28)"
    - "A non-ASCII literal asserted from a Lua script is built as a separate C++ std::string with hex-byte escapes (\\xC3\\xAF) and spliced into the script text via string concatenation, since a raw string literal (R\"LUA(...)LUA\") cannot itself contain a C++ escape sequence"
    - "Lua boundary assertions use string.find(needle, 1, true) (plain-find mode) throughout, because the asserted literals contain {, }, (, ) and -, all Lua pattern metacharacters"

key-files:
  created:
    - tests/test_c_api_database_describe.cpp
  modified:
    - tests/test_lua_runner_describe.cpp
    - tests/CMakeLists.txt

key-decisions:
  - "Task 1's C API suite intentionally does not use tests/test_ui_fixture.h's open_ui_fixture, because that helper returns a quiver::Database (C++), not a quiver_database_t* handle -- the plan requires building every database through the C API itself, so the fixture directory path is computed with the same path_from helper and combined with a case-specific sqlite filename inline."
  - "Task 2's new cases use plain TEST(...) rather than TEST_F(LuaRunnerTest, ...), per the plan -- LuaRunnerTest's system-temp-directory sandbox does not put the sqlite file inside a directory with a ui/ sibling, which D-28 requires."

requirements-completed: [DESC-07]

coverage:
  - id: D1
    description: "The C API suite asserts the exact enum rendering strings (declared vocabulary, histogram, unit/hidden/label, header, no-sidecar) against the shared tests/schemas/ui/ fixtures, proving the report survives the char** marshalling boundary unaltered"
    requirement: "DESC-07"
    verification:
      - kind: unit
        ref: "tests/test_c_api_database_describe.cpp#DatabaseCApiDescribe.DeclaredVocabularyListCrossesTheBoundary"
        status: pass
      - kind: unit
        ref: "tests/test_c_api_database_describe.cpp#DatabaseCApiDescribe.HistogramLabelsCrossTheBoundary"
        status: pass
      - kind: unit
        ref: "tests/test_c_api_database_describe.cpp#DatabaseCApiDescribe.UnitHiddenAndLabelCrossTheBoundary"
        status: pass
      - kind: unit
        ref: "tests/test_c_api_database_describe.cpp#DatabaseCApiDescribe.HeaderLineCrossesTheBoundary"
        status: pass
      - kind: unit
        ref: "tests/test_c_api_database_describe.cpp#DatabaseCApiDescribe.NoHeaderWithoutSidecar"
        status: pass
    human_judgment: false
  - id: D2
    description: "The Lua suite asserts the exact enum rendering strings through db:describe, db:describe_collection and db:summarize_collection against the same fixtures, using string.find(..., 1, true) so braces/parens/dashes are matched literally"
    requirement: "DESC-07"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_describe.cpp#LuaRunnerDescribe.DescribeCollectionRendersDeclaredVocabulary"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_describe.cpp#LuaRunnerDescribe.SummarizeCollectionRendersEnumLabels"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_describe.cpp#LuaRunnerDescribe.DescribeRendersHeaderAndCollectionLabel"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_describe.cpp#LuaRunnerDescribe.HiddenAndUnitRenderedInLua"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_describe.cpp#LuaRunnerDescribe.NoHeaderWithoutSidecarInLua"
        status: pass
    human_judgment: false
  - id: D3
    description: "Both suites assert the exact byte sequence of the non-ASCII Foresight labels (L9 Seasonal Naïve, bytes Seasonal Na\\xC3\\xAFve; L10 Regresión Lineal, bytes Regresi\\xC3\\xB3n Lineal) end to end through describe_collection(\"EconomicDriver\"), catching a truncating or re-encoding char**/sol2 bug at the layer that would introduce it"
    requirement: "DESC-07"
    verification:
      - kind: unit
        ref: "tests/test_c_api_database_describe.cpp#DatabaseCApiDescribe.NonAsciiLabelBytesSurviveMarshalling"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_describe.cpp#LuaRunnerDescribe.NonAsciiLabelSurvivesSol2"
        status: pass
    human_judgment: false

# Metrics
duration: 25min
completed: 2026-09-19
status: complete
---

# Phase 1 Plan 5: Enum Labels in Describe (C API and Lua Boundary Proofs) Summary

**Six new C API gtest cases and six new plain-`TEST` Lua cases prove the describe/describe_collection/summarize_collection rendering built in waves 1–3 survives the `char**` and sol2 marshalling boundaries byte-for-byte, including the two accented Foresight labels, with zero new C symbols and zero `db:` methods added.**

## Performance

- **Duration:** ~25 min
- **Tasks:** 2/2 completed
- **Files modified:** 3 (1 created, 2 modified)

## Accomplishments

- `tests/test_c_api_database_describe.cpp` (`DatabaseCApiDescribe`, 6 tests) builds every database through `quiver_database_from_schema` itself — never through `test_ui_fixture.h`'s C++ helper — writing the sqlite file inside the shared `tests/schemas/ui/<fixture>/` directory (`capi_declared.sqlite`, `capi_histogram.sqlite`, `capi_unit_hidden.sqlite`, `capi_header.sqlite`, `capi_unicode.sqlite`, `capi_no_sidecar.sqlite`) so `ctest -j` cannot race and the `ui/` sidecar sibling is always found (D-28).
- Asserts, verbatim from `tests/schemas/ui/README.md`'s authoritative literal table: the declared vocabulary list with zero elements (L3), the histogram (L1), the unit/hidden/label scalar lines (L4, L5), the `UI config: ... (locale: en)` header, and — the assertion the C++ core suite cannot substitute for — both non-ASCII Foresight labels (L9, L10) via `describe_collection("EconomicDriver")` on `foresight_like`, written as explicit `\xC3\xAF` / `\xC3\xB3` byte escapes rather than raw source characters (no `/utf-8` flag for MSVC).
- Six new plain `TEST(LuaRunnerDescribe, ...)` cases added to `tests/test_lua_runner_describe.cpp` beside (not replacing) the three existing `LuaRunnerTest` cases, using `open_ui_fixture` (file-backed, sidecar beside the db) instead of `LuaRunnerTest`'s temp-dir sandbox, and `string.find(needle, 1, true)` throughout so `{`, `}`, `(`, `)` and `-` are matched literally rather than as Lua patterns.
- The Lua non-ASCII case builds the two accented literals as separate C++ `std::string`s with hex-byte escapes and splices them into the Lua script text via string concatenation — a `R"LUA(...)LUA"` raw string literal cannot itself carry a C++ escape sequence, so the byte-safe literal has to be built outside it and joined in.
- Confirmed no new C API symbol (`git diff --name-only <this plan's own commits> -- src/ include/ bindings/` is empty) and no new `db:` method (`bun test bindings/js/test/lua-api-sync.test.ts` passes unchanged, 6/6).

## Task Commits

1. **Task 1: C API exact-string enum rendering suite** - `260c842` (test)
2. **Task 2: Lua exact-string enum rendering cases** - `c644041` (test)

## Files Created/Modified

- `tests/test_c_api_database_describe.cpp` - `DatabaseCApiDescribe` suite (6 tests)
- `tests/test_lua_runner_describe.cpp` - 6 new `LuaRunnerDescribe` cases appended after the existing `LuaRunnerTest` cases
- `tests/CMakeLists.txt` - registers the new C API test file in `quiver_c_tests`' source list

## Decisions Made

- Task 1 deliberately avoids `tests/test_ui_fixture.h`'s `open_ui_fixture` because that helper returns a `quiver::Database` (C++), not a `quiver_database_t*` handle — the plan's whole point is proving the *C API's* marshalling, so every database in this file is opened via `quiver_database_from_schema` directly, with the fixture directory path computed via `quiver::test::path_from` (the same helper `test_utils.h` already exposes).
- Task 2's new cases use bare `TEST(...)` rather than `TEST_F(LuaRunnerTest, ...)`, matching the plan's explicit instruction: `LuaRunnerTest`'s system-temp-directory sandbox does not place the sqlite file inside a directory with a `ui/` sibling, which D-28 requires.

## Deviations from Plan

None — plan executed exactly as written. One observation worth recording: the plan's acceptance criterion `git diff --name-only master...HEAD -- src/ include/ bindings/` is empty` is written as if this were the first plan off `master`, but this branch (`rs/enums`) is several plans deep (01-01 through 01-04 already committed src/ changes that are part of this phase's legitimate scope, and `master` has not merged any of it yet — confirmed `git merge-base master HEAD` equals `master`'s own tip). Taken literally against the distant `master` branch, that diff is never empty on this branch regardless of what any later plan does. The check was therefore run in the sense the constraint actually cares about — did *this plan's own two commits* touch `src/`, `include/`, or `bindings/` — via `git diff --name-only HEAD~1..HEAD` (Lua commit) and by construction (Task 1's commit added only a new test file plus one CMakeLists.txt line), both empty.

## Issues Encountered

None blocking.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- All 16 fixture literals in `tests/schemas/ui/README.md` relevant to DESC-07's C++-testable boundaries (C API, Lua) are now asserted at those boundaries; L1, L3, L4, L5, L6, L9 and L10 are each asserted at both the C API and the Lua boundary in this plan.
- `quiver_tests.exe` (1160/1160) and `quiver_c_tests.exe` (563/563) both fully green after this plan's changes.
- No blocker for 01-06 (the four FFI bindings — Julia/Dart/Python/JS): this plan added no `src/`, `include/`, or `bindings/` change, and the fixture corpus is untouched (`git status --porcelain -- tests/schemas/` empty at completion), so 01-06 has the same fixtures and the same renderer to assert against.

## Self-Check: PASSED
