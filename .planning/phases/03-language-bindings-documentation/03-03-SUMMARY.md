---
phase: "03"
plan: "03"
subsystem: python-binding
tags: [python, cffi, time-series, add_time_series_row, bindings]
dependency_graph:
  requires: [02-01, 02-02]
  provides: [PY-11]
  affects: [bindings/python/src/quiverdb/_c_api.py, bindings/python/src/quiverdb/database.py, bindings/python/tests/test_database_time_series.py]
tech_stack:
  added: []
  patterns: [cffi-abi-mode, keepalive-list, strict-type-dispatch, dict-unpacking-kwargs]
key_files:
  created: []
  modified:
    - bindings/python/src/quiverdb/_c_api.py
    - bindings/python/src/quiverdb/database.py
    - bindings/python/tests/test_database_time_series.py
    - bindings/python/tests/conftest.py
decisions:
  - "Inline marshaling (no _marshal_time_series_columns reuse) per D-03 and simple-over-abstract principle"
  - "strict type(v) is ... dispatch — no Int-to-Float coercion, mirrors update wrapper policy"
  - "TestAddTimeSeriesRow class matches existing class-based test convention in file"
  - "multi_dim_ts_db fixture added to conftest.py for multi-dim composite PK test"
metrics:
  duration: "~45 minutes"
  completed: "2026-05-20"
  tasks: 4
  files: 4
---

# Phase 3 Plan 03: Python add_time_series_row Summary

**One-liner:** Python `Database.add_time_series_row(**kwargs)` wrapper with strict CFFI marshaling, keepalive ownership, and 4-test coverage (insert, upsert, dict-unpacking, multi-dim).

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Hand-add cdef to _c_api.py | dc55230 | `bindings/python/src/quiverdb/_c_api.py` |
| 2 | Add add_time_series_row method | 2541ff2 | `bindings/python/src/quiverdb/database.py` |
| 3 | Add test functions + fixture | f6ebfd8 | `bindings/python/tests/test_database_time_series.py`, `conftest.py` |
| 4 | Run Python test suite | (verified) | 225 passed |

## What Was Built

### Task 1: _c_api.py cdef
Added `quiver_database_add_time_series_row` declaration to the time-series section of `_c_api.py`, placed between `update_time_series_group` and `free_time_series_data`. 8 parameters, no `row_count` parameter (matches single-row C API signature).

### Task 2: database.py wrapper
`Database.add_time_series_row(self, collection: str, group: str, id: int, **kwargs) -> None` added after `update_time_series_group` method. Key implementation details:
- Strict `type(v) is int/float/str` dispatch (no Int-to-Float coercion per D-03)
- Per-column single-element CFFI arrays: `ffi.new("int64_t[]", [v])`, `ffi.new("double[]", [v])`, `ffi.new("char*[]", [str_buf])`
- `keepalive: list` owns all CFFI allocations across the call boundary
- 8-argument CFFI call: `lib.quiver_database_add_time_series_row(ptr, collection, group, id, col_names, col_types, col_data, col_count)` — no `row_count`
- `**kwargs` shape enables dict unpacking natively

### Task 3: Tests
Added `TestAddTimeSeriesRow` class with 4 test methods to `test_database_time_series.py`:
- `test_add_time_series_row_insert`: insert one row via kwargs, read back, assert presence
- `test_add_time_series_row_upsert`: insert same PK twice, assert value overwrite
- `test_add_time_series_row_dict_unpacking`: `db.add_time_series_row(..., **row_dict)` round-trip
- `test_add_time_series_row_multi_dim`: composite PK (date_time+block) via `multi_dim_time_series.sql`

Added `multi_dim_ts_db` fixture to `conftest.py` using `tests/schemas/valid/multi_dim_time_series.sql`.

### Task 4: Test Suite
All 225 tests passed (28.49s). The 4 new `TestAddTimeSeriesRow` tests all passed at lines 176-179 of test output.

## Deviations from Plan

None - plan executed exactly as written.

## Verification

- `_c_api.py` contains `quiver_database_add_time_series_row` cdef (8 parameters, no `row_count`)
- `database.py` exposes `def add_time_series_row(self, collection, group, id, **kwargs)` 
- No "Cannot add_time_series_row:" string in bindings/python/ (zero matches on grep)
- 225 tests passed via PowerShell with `build\bin` prepended to PATH for DLL discovery

## Self-Check: PASSED

- `bindings/python/src/quiverdb/_c_api.py` - modified (cdef added)
- `bindings/python/src/quiverdb/database.py` - modified (method added at line 1189)
- `bindings/python/tests/test_database_time_series.py` - modified (TestAddTimeSeriesRow added)
- `bindings/python/tests/conftest.py` - modified (multi_dim_ts_db fixture added)
- Commits: dc55230, 2541ff2, f6ebfd8 exist in git log
