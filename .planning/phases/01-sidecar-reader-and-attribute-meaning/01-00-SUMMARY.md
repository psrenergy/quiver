---
phase: 01-sidecar-reader-and-attribute-meaning
plan: 00
subsystem: testing
tags: [cpp, googletest, cmake, fixtures, from_migrations]

# Dependency graph
requires: []
provides:
  - "UiTempTreeFixture: per-test temp-dir builder for a migrations/ tree + sibling ui/ tree"
  - "UiConfigTest / DatabaseUiMetadataTest gtest suite names reserved for later plans in this phase"
  - "reservoir_schema() helper (Configuration + HydroPlant tables) shared across this phase's tests"
  - "SAFE-01 no-sidecar baseline test (NoUiDirReportsUnchanged)"
affects: ["01-01", "01-02"]

# Actuals (#2632)
actuals:
  tokens: 1470
  tasks: 1
  commits: 1

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Temp-dir migrations+ui fixture (extends MigrationsTestFixture idiom from test_migrations.cpp)"

key-files:
  created:
    - tests/test_database_ui_metadata.cpp
  modified:
    - tests/CMakeLists.txt

key-decisions:
  - "Moved the *NoUiDir* baseline test from plan 02 (per 01-VALIDATION.md row 1-02-03) to this plan, as task 1-00-01, because the fixture and baseline must exist before any production code is written — recorded per the plan's own note."
  - "summarize_collection's scalar-loop assertion targets its actual output shape (\"    - initial_volume_type: \") rather than the write_collection_section literal line (\"(INTEGER) NOT NULL\") the plan's action text names for all three reports — summarize_collection never renders that declaration form (src/database_describe.cpp), so a literal match there would always fail. describe()/describe_collection() (which share write_collection_section) still assert the literal line verbatim; all three still assert the absence of the three clause markers, which is the must_haves-level requirement."

requirements-completed: [SAFE-01]

coverage:
  - id: D1
    description: "UiTempTreeFixture builds a migrations/ tree and sibling ui/ tree from caller-supplied file contents in a per-test temp dir, cleaned in both SetUp and TearDown"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.NoUiDirReportsUnchanged"
        status: pass
    human_judgment: false
  - id: D2
    description: "With no ui/ sidecar present, describe/describe_collection/summarize_collection render with no ; label, ; enum or ; tooltip clause, and the existing scalar declaration line is unchanged"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.NoUiDirReportsUnchanged"
        status: pass
    human_judgment: false

duration: 25min
completed: 2026-09-20
status: complete
---

# Phase 01 Plan 00: Temp-Dir UI Fixture and SAFE-01 Baseline Summary

**Per-test temp-dir fixture builds a `migrations/`+sibling-`ui/` tree from caller-supplied strings, and one gtest proves `describe`/`describe_collection`/`summarize_collection` render unchanged with no `ui/` sidecar — before any production code exists.**

## Performance

- **Duration:** ~25 min
- **Tasks:** 1
- **Files modified:** 2 (1 created, 1 modified)

## Accomplishments
- `UiTempTreeFixture` (base class) with `write_migration`, `write_ui_file`, `migrations_dir()`, `ui_dir()` (via `weakly_canonical(...).parent_path() / "ui"`, avoiding the raw-`parent_path()` trailing-slash trap), and `open_tree()`
- `UiConfigTest` / `DatabaseUiMetadataTest` empty derived fixtures — the two gtest suite names later plans' filters target
- `reservoir_schema()` shared helper: `Configuration` + `HydroPlant` (`hm3_initial`, `initial_volume_type`, `reservoir_type`, `discount_rate`), all `STRICT`
- `DatabaseUiMetadataTest.NoUiDirReportsUnchanged` — the SAFE-01 anchor test, green against today's build with zero production changes
- Registered `test_database_ui_metadata.cpp` in `tests/CMakeLists.txt`, alphabetically between `test_database_transaction.cpp` and `test_database_update.cpp`

## Task Commits

1. **Task 1-00-01: Temp-dir migrations+ui fixture and the no-sidecar baseline test** - `ef1a7f1` (test)

## Files Created/Modified
- `tests/test_database_ui_metadata.cpp` - fixture + SAFE-01 baseline test
- `tests/CMakeLists.txt` - registers the new test file in `quiver_tests`

## Decisions Made
- Kept the `*NoUiDir*` baseline test here (plan 00) rather than plan 02, per the plan's own note reconciling 01-VALIDATION.md row `1-02-03` — the fixture must exist before any production code, and plan 02 retains the stronger SAFE-02 byte-identity comparison.
- Adjusted the summarize_collection assertion to its real output shape rather than the write_collection_section literal line the plan's action text named for "all three reports" — see key-decisions above. No production behavior was changed; this is purely a test-assertion correction against verified current output (`src/database_describe.cpp`).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Corrected the summarize_collection assertion in the plan's action text**
- **Found during:** Task 1-00-01
- **Issue:** The plan's action text says all three reports (`describe`, `describe_collection`, `summarize_collection`) should contain the literal line `    - initial_volume_type (INTEGER) NOT NULL`. Reading `src/database_describe.cpp` (the plan's own `read_first` reference) shows `summarize_collection` has its own scalar loop that renders `"    - " << name << ": " << non_null_count << " non-null, " << null_count << " null"` — it never emits the `(TYPE) ... NOT NULL` declaration form. Asserting the literal line against `summarize_collection`'s output would always fail (a false SAFE-01 anchor, not a real one).
- **Fix:** `describe()`/`describe_collection()` (which share `write_collection_section`) assert the literal line verbatim, as specified. `summarize_collection` instead asserts its own actual unchanged-shape prefix (`"    - initial_volume_type: "`). All three still assert the absence of the three clause markers (`; label`, `; enum`, `; tooltip`), which is the must_haves-level SAFE-01 requirement and the one that matters for this baseline.
- **Files modified:** tests/test_database_ui_metadata.cpp
- **Verification:** `./build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*NoUiDir*` — 1 test, 0 failures
- **Committed in:** ef1a7f1 (Task 1-00-01 commit)

---

**Total deviations:** 1 auto-fixed (1 bug in the plan's own assertion spec, caught before it could ship a test that couldn't pass)
**Impact on plan:** No scope creep; no production code touched. The fix keeps the SAFE-01 truth ("no clause marker when no ui/ dir exists, for all three reports") intact while correcting only the incidental literal-line target for the one report where it was factually wrong.

## Issues Encountered
- No pre-existing `build/` directory in this checkout (the plan's `<build_and_test>` context assumed one). Configured fresh with `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON -DFETCHCONTENT_BASE_DIR=<sibling checkout>/build/_deps` to reuse an already-populated FetchContent cache from a sibling checkout of the same repo (same dependency versions, same Debug/tests/C-API config) rather than re-cloning sqlite3/lua/sol2/googletest/tomlplusplus/etc. from scratch. Full build succeeded (207/207 targets); no repo files were modified by this step.

## Next Phase Readiness
- The fixture, both reserved gtest suite names, and `reservoir_schema()` are ready for plan 01 (sidecar reader) and plan 02 (rendering) to build on directly.
- Full `quiver_tests.exe` suite: 1237/1237 passing, no regressions from adding this file.
- `tests/test_database_lifecycle.cpp` is byte-for-byte unmodified (`git diff --stat` empty); no path was created under `tests/schemas/ui/`.

---
*Phase: 01-sidecar-reader-and-attribute-meaning*
*Completed: 2026-09-20*
