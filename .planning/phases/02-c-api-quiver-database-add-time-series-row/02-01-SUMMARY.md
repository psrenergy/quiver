---
phase: 02-c-api-quiver-database-add-time-series-row
plan: 01
subsystem: api
tags: [c-api, ffi, time-series, capi-11, capi-12]

# Dependency graph
requires:
  - phase: 01-cpp-database-add-time-series-row
    provides: Database::add_time_series_row C++ method (signature + upsert semantics)
provides:
  - quiver_database_add_time_series_row declaration in include/quiver/c/database.h
  - quiver_database_add_time_series_row implementation in src/c/database_time_series.cpp
  - Pattern-1 error relay through quiver_get_last_error for the new symbol
  - D-07-owned "Cannot add_time_series_row: unknown column type N" error string (the only wrapper-owned message)
affects:
  - 02-02 (C API tests for add_time_series_row — now unblocked)
  - 03-* (Phase 3 binding generators — symbol is discoverable at the FFI boundary)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Single-row columnar marshaling: literal adaptation of update_time_series_group with row loop dropped and std::vector<std::map> collapsed to a single std::map"

key-files:
  created: []
  modified:
    - include/quiver/c/database.h
    - src/c/database_time_series.cpp

key-decisions:
  - "D-02 signature applied byte-for-byte (8 parameters, no row_count, parameter order matches update)"
  - "D-04 placement: implementation lands immediately after quiver_database_update_time_series_group; declaration sits between update and read_time_series_row"
  - "D-05 error origins: all 'Cannot add_time_series_row: ...' Pattern-1 messages forwarded verbatim from C++; the only wrapper-crafted string is the D-07 'unknown column type' default branch"
  - "D-06 null-check shape: QUIVER_REQUIRE(db, collection, group) + conditional QUIVER_REQUIRE(column_names, column_types, column_data) — identical to update wrapper"
  - "Discretion (per CLAUDE.md 'Simple solutions over complex abstractions'): did NOT extract a shared convert_typed_column_value helper between update and add — literal duplication of the marshaling switch keeps both wrappers diff-friendly for review"
  - "Discretion: used [0] indexing (matching update's [r] with r=0) instead of *deref for visual parity with the sibling implementation"

patterns-established:
  - "Single-row variant of the columnar update wrapper: drop row_count, drop the outer row loop, replace std::vector<std::map> with one std::map, keep every other layer identical"

requirements-completed: [CAPI-11, CAPI-12]

# Metrics
duration: 9min
completed: 2026-05-19
---

# Phase 02 Plan 01: C API quiver_database_add_time_series_row Summary

**FFI declaration + implementation of `quiver_database_add_time_series_row` mirroring `quiver_database_update_time_series_group`'s columnar marshaling shape for the single-row case, with Pattern-1 error relay through `quiver_get_last_error`.**

## Performance

- **Duration:** ~9 min
- **Started:** 2026-05-19 (worktree spawn)
- **Completed:** 2026-05-19
- **Tasks:** 2/2
- **Files modified:** 2

## Accomplishments
- Declared `quiver_database_add_time_series_row` in `include/quiver/c/database.h` between `quiver_database_update_time_series_group` and `quiver_database_read_time_series_row` (per D-03), with the D-02 signature byte-for-byte.
- Implemented the wrapper in `src/c/database_time_series.cpp` immediately after `quiver_database_update_time_series_group` (per D-04). The body is a mechanical adaptation of the update wrapper: same `QUIVER_REQUIRE` shape, same four-case marshaling switch, no row loop, one `std::map<std::string, quiver::Value>` instead of a vector, passed straight to `db->db.add_time_series_row(collection, group, id, row)`.
- Confirmed `cmake --build build --config Debug` compiles cleanly with no new warnings, and `./build/bin/quiver_c_tests.exe` exits 0 (471/471 tests pass; targeted `*TimeSeries*` filter: 44/44 pass).

## Task Commits

Each task was committed atomically:

1. **Task 1: Declare quiver_database_add_time_series_row in the public C API header** — `3895752` (feat)
2. **Task 2: Implement quiver_database_add_time_series_row in src/c/database_time_series.cpp** — `ace7da3` (feat)

## Files Created/Modified
- `include/quiver/c/database.h` — Added 15-line declaration block (6 comment lines + 9-line `QUIVER_C_API` prototype) between the update and read_time_series_row declarations.
- `src/c/database_time_series.cpp` — Added 42-line wrapper implementation between `quiver_database_update_time_series_group` and the `// Time series free functions` comment block.

## Verification

**D-02 signature byte-for-byte match (declaration):**
```c
QUIVER_C_API quiver_error_t quiver_database_add_time_series_row(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* group,
                                                                int64_t id,
                                                                const char* const* column_names,
                                                                const int* column_types,
                                                                const void* const* column_data,
                                                                size_t column_count);
```
Parameter names + order: db, collection, group, id, column_names, column_types, column_data, column_count. No `row_count`. Confirmed.

**Shape-for-shape parity with `quiver_database_update_time_series_group` (implementation):**
- `QUIVER_REQUIRE(db, collection, group)` (D-06): present
- Conditional `QUIVER_REQUIRE(column_names, column_types, column_data)` guard under `column_count > 0` (D-06): present
- Four-case marshaling switch over `QUIVER_DATA_TYPE_{INTEGER,FLOAT,STRING,DATE_TIME}`: present
- `[0]` indexing (single-row adaptation of update's `[r]`): present
- No outer `for (size_t r ...)` row loop: verified (`grep -c "for (size_t r"` against the new function: 0)
- One `std::map<std::string, quiver::Value> row` (not a `std::vector<std::map>`): present
- Single call `db->db.add_time_series_row(collection, group, id, row)`: present
- `catch (const std::exception& e) { quiver_set_last_error(e.what()); return QUIVER_ERROR; }`: present

**D-07 wrapper-owned error string (only message crafted in this layer):**
`"Cannot add_time_series_row: unknown column type " + std::to_string(column_types[c])` — present in the `default:` branch (count: 1). All other error messages originate from `Database::add_time_series_row` (C++) and propagate verbatim via `e.what()`.

**Build + tests:**
- `cmake --build build --config Debug` — succeeded with no new warnings.
- `./build/bin/quiver_c_tests.exe` — 471 tests, 0 failures.
- `./build/bin/quiver_c_tests.exe --gtest_filter="*TimeSeries*"` — 44 tests, 0 failures.

## Decisions Made

Followed plan as specified. Discretion calls noted in plan:

- **Helper extraction skipped.** Did NOT introduce a shared `convert_typed_column_value` helper between `update_time_series_group` and `add_time_series_row`. Per CLAUDE.md "Simple solutions over complex abstractions" and CONTEXT.md guidance, kept a literal copy of the marshaling switch. The two wrappers now read identically side-by-side for review purposes; the duplication is intentional and minor (one switch block).
- **Indexing style: `[0]`.** Used `[0]` to match update's `[r]` shape (with r=0) rather than `*static_cast<...>(...)` deref. Visual parity with sibling code.

## Deviations from Plan

None — plan executed exactly as written. No Rule 1/2/3/4 deviations occurred. No new dependencies, no new error strings beyond the D-07-locked one, no API surface beyond the locked D-02 signature.

## Issues Encountered

None. The CMake `build/` directory had to be configured before the first build (`cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON`) — this is standard project setup, not an execution issue.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

**CAPI-11 + CAPI-12 verified — Plan 02-02 (C API tests) unblocked.** Symbol exported via `include/quiver/c/database.h` for Phase 3 binding generators per D-11.

- `quiver_database_add_time_series_row` is now linkable from `libquiver_c.dll` (proved by the test build succeeding).
- The declaration sits in the standard place for Julia/Dart/Python generators to pick up (Phase 3).
- No follow-up cleanup needed — the implementation matches the established columnar pattern; if review later wants the shared helper, it can land as a small cleanup phase without touching public API.

## Self-Check: PASSED

- `include/quiver/c/database.h` exists and contains the declaration (verified).
- `src/c/database_time_series.cpp` exists and contains the implementation (verified).
- Commit `3895752` exists in `git log` (Task 1).
- Commit `ace7da3` exists in `git log` (Task 2).
- `cmake --build build --config Debug` succeeded with no new warnings.
- `./build/bin/quiver_c_tests.exe` exits 0 (471/471 pass).

---
*Phase: 02-c-api-quiver-database-add-time-series-row*
*Completed: 2026-05-19*
