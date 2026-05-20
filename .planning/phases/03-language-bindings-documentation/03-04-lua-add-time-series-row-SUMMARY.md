---
phase: "03"
plan: "04"
subsystem: lua-binding
tags: [lua, time-series, add-time-series-row, binding]
dependency_graph:
  requires: [01-01, 01-02, 02-01, 02-02]
  provides: [LUA-11]
  affects: [src/lua_runner.cpp, tests/test_lua_runner.cpp]
tech_stack:
  added: []
  patterns: [sol2-set_function, lua_table_to_value_map]
key_files:
  created: []
  modified:
    - src/lua_runner.cpp
    - tests/test_lua_runner.cpp
decisions:
  - "Reused lua_table_to_value_map directly — one-line forward, no helper variant needed"
  - "build_wt/ created in worktree for isolated build (main repo build uses hardcoded paths)"
metrics:
  duration: "~15 minutes"
  completed: "2026-05-20"
  tasks_completed: 3
  files_modified: 2
---

# Phase 03 Plan 04: Lua Add Time Series Row Summary

## One-liner

Lua binding `db:add_time_series_row(collection, group, id, row_table)` exposed through LuaRunner via `add_time_series_row_lua` static function and sol2 `bind.set_function` registration.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Hand-add add_time_series_row_lua + bind.set_function | 6c5718e | src/lua_runner.cpp |
| 2 | Add LuaRunnerTest.AddTimeSeriesRow* GTest cases | b5b81e2 | tests/test_lua_runner.cpp |
| 3 | Rebuild and run quiver_tests.exe | (no commit — build/test verification) | build_wt/bin/quiver_tests.exe |

## Changes Made

### src/lua_runner.cpp

Added `add_time_series_row_lua` static function immediately after `update_time_series_group_lua` (around line 806):

```cpp
static void add_time_series_row_lua(Database& db,
                                    const std::string& collection,
                                    const std::string& group,
                                    int64_t id,
                                    sol::table row) {
    db.add_time_series_row(collection, group, id, lua_table_to_value_map(row));
}
```

Registered the binding at line 156, immediately after `update_time_series_group`:

```cpp
bind.set_function("add_time_series_row", &add_time_series_row_lua);
```

### tests/test_lua_runner.cpp

Added three GTest cases placed immediately after `LuaRunnerTest.UpdateTimeSeriesGroup`:

- `LuaRunnerTest.AddTimeSeriesRowInsert` — inserts one row via Lua and reads it back from C++
- `LuaRunnerTest.AddTimeSeriesRowUpsert` — calls `add_time_series_row` twice with same `date_time`; asserts final value is the second write (99.0)
- `LuaRunnerTest.AddTimeSeriesRowMultiDim` — uses `multi_dim_time_series.sql` (Resource collection, `date_time + block` composite PK); inserts `{ date_time, block, load, flag }` and asserts all columns round-trip

## Test Results

```
[==========] Running 3 tests from 1 test suite.
[  PASSED  ] LuaRunnerTest.AddTimeSeriesRowInsert (33 ms)
[  PASSED  ] LuaRunnerTest.AddTimeSeriesRowUpsert (9 ms)
[  PASSED  ] LuaRunnerTest.AddTimeSeriesRowMultiDim (8 ms)
[  PASSED  ] 3 tests.
```

Full suite: **856 tests, all passed**, no regressions.

## Deviations from Plan

### Build deviation: worktree-local build directory required

The existing `build/` directory in the main repo points its `build.ninja` to hardcoded source paths under the main repo (`C:/Development/DatabaseTmp/quiver3/src/...`), not the worktree. Running `cmake --build build --config Debug` from the worktree would compile the main repo's unmodified source files.

**Fix:** Configured a fresh `build_wt/` directory inside the worktree (`cmake -S . -B build_wt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON`). The first link of `libquiver.dll` produced mingw "stripping non-representable symbol" errors and failed; the second `cmake --build build_wt` invocation succeeded (ninja recovered and completed the remaining link steps). All subsequent builds were incremental and clean.

**Files modified:** None beyond the planned files.
**Commit:** No deviation commit — build infrastructure is not tracked.

## Threat Surface Scan

No new network endpoints, auth paths, file access patterns, or schema changes introduced. The binding reuses `lua_table_to_value_map` unchanged — no new value coercion code. Error messages surface from C++ `Database::add_time_series_row` unchanged through sol2's default exception handler, consistent with CLAUDE.md policy.

## Self-Check: PASSED

- [x] `src/lua_runner.cpp` contains `add_time_series_row_lua` (line 809) and `bind.set_function("add_time_series_row", ...)` (line 156)
- [x] `tests/test_lua_runner.cpp` contains `LuaRunnerTest, AddTimeSeriesRowInsert`, `AddTimeSeriesRowUpsert`, `AddTimeSeriesRowMultiDim`
- [x] Commit 6c5718e exists
- [x] Commit b5b81e2 exists
- [x] `build_wt/bin/quiver_tests.exe --gtest_filter=LuaRunnerTest.AddTimeSeriesRow*` exits 0, 3 passed
- [x] Full `build_wt/bin/quiver_tests.exe` exits 0, 856 passed
