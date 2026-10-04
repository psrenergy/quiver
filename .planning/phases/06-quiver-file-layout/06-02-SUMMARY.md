---
phase: 06-quiver-file-layout
plan: 02
subsystem: lua_runner
status: complete
tags: [lua, refactor, file-layout, sol2]
requires:
  - 06-01 (build/layout-check harness, database_describe/read/metadata/time_series files)
provides:
  - src/lua_runner/database.cpp (bind_database, run_in_scope; git mv of db_core.cpp)
  - src/lua_runner/database_query.cpp (bind_query)
  - src/lua_runner/database_csv_export.cpp (bind_csv_export, parse_csv_options)
  - src/lua_runner/database_csv_import.cpp (bind_csv_import)
  - src/lua_runner/database_create.cpp (bind_create, table_to_element, require_dense_array)
  - src/lua_runner/database_update.cpp (bind_update; git mv of db_write.cpp)
  - src/lua_runner/database_delete.cpp (bind_delete)
affects: [06-03]
tech-stack:
  added: []
  patterns: [git mv the largest remainder so history follows, binder call lands at its final slot in one step]
key-files:
  created:
    - src/lua_runner/database_query.cpp
    - src/lua_runner/database_csv_export.cpp
    - src/lua_runner/database_csv_import.cpp
    - src/lua_runner/database_create.cpp
    - src/lua_runner/database_delete.cpp
  modified:
    - src/lua_runner/database.cpp (renamed from db_core.cpp)
    - src/lua_runner/database_update.cpp (renamed from db_write.cpp)
    - src/lua_runner/internal.h
    - src/lua_runner/lua_runner.cpp
    - src/CMakeLists.txt
decisions:
  - "NOLINT: no performance-unnecessary-value-param pair in database.cpp, database_query.cpp, database_csv_export.cpp, database_csv_import.cpp, database_create.cpp, database_delete.cpp (none takes a sol2 argument by value); database_update.cpp keeps the pair its moved text carries"
  - "parse_csv_options is named in lua_internal, defined in database_csv_export.cpp and declared in internal.h; string_key stays anonymous beside it"
metrics:
  duration: 11min
  completed: 2026-10-04
actuals:
  tokens: 8200
  tasks: 3
  commits: 3
---

# Phase 6 Plan 02: Carve the core and write binders Summary

Split the two multi-domain binder files into seven files named after the core files they bind: `db_core.cpp` became `database.cpp` (13 lifecycle/transaction/dry-run/migration names) plus `database_query.cpp`, `database_csv_export.cpp` and `database_csv_import.cpp`; `db_write.cpp` became `database_update.cpp` (8 names) plus `database_create.cpp` and `database_delete.cpp`. Every body moved verbatim, and every binder call went straight to its final LAYOUT-02 slot.

## Commits

| Task | Commit | Gate |
|---|---|---|
| 1 export_csv / import_csv -> `database_csv_export.cpp` / `database_csv_import.cpp` | `f1540c4` | `GATE PASS lua=477 pairs=36`, `GOLDEN debug OK`, sandbox list unchanged (10) |
| 2 `git mv db_core.cpp database.cpp` (`bind_database`) + `database_query.cpp` | `fba0703` | `GATE PASS lua=477 pairs=36`, `GOLDEN debug OK` |
| 3 `git mv db_write.cpp database_update.cpp` (`bind_update`) + `database_create.cpp` + `database_delete.cpp` | `f07f1a1` | `GATE PASS lua=477 pairs=36`, `GOLDEN debug OK` |

Registrations per binder after the plan (11 of 14 final): database 13, create 1, read 15, update 8, delete 2, describe 3, metadata 6, query 3, time_series 12, csv_export 1, csv_import 1. Totals unchanged: 86 = 71 `bind` + 15 `ns`.

Binder call order in `lua_runner.cpp`: `bind_database bind_create bind_read bind_update bind_delete bind_describe bind_metadata bind_query bind_time_series bind_csv_export bind_csv_import bind_csv bind_binary` (LAYOUT-02 minus `bind_expression`, which 06-03 adds). `internal.h` declares them in the same order.

## Renames

- `db_core.cpp -> database.cpp`: detected, 63% similarity.
- `db_write.cpp -> database_update.cpp`: detected, 69% similarity; `git log --follow` reaches `da6f67b`.

## Novel lines

29 lines under `src/lua_runner` since BASE match no removed line (10 of them from 06-01). The 19 from this plan are binder declarations, definitions and calls, the `parse_csv_options` declaration, and two includes in the new `database_create.cpp` (`quiver/element.h`, kept by `database_update.cpp` too, and `<variant>` for the moved `std::visit`). None is logic.

## Deviations from Plan

**1. [Acceptance wording] `git grep bind_core|bind_write -- src` prints 1 each, not 0.** Both hits are the `src/AGENTS.md` file map (lines 52 and 54), which 06-03 rewrites. No code file, declaration, call or CMake line of either retired binder remains (`ls src/lua_runner | grep -c '^db_'` = 0, `grep -c 'lua_runner/db_' src/CMakeLists.txt` = 0).

**2. clang-format moved `quiver/database.h` above `lua_runner/internal.h` in `database.cpp`.** It treats `quiver/database.h` as the main header of `database.cpp`. This is formatter output, and the gate's `--dry-run --Werror` check requires it.

**3. Blank line dropped in Task 1.** The blank line between the dry-run block and the query registrations in the transitional `db_core.cpp` went away with the CSV registrations. Task 2 then moved those query lines out, so nothing of it is left at HEAD.

Otherwise none. No test, baseline or pinned count changed, and no GOLDEN_CHANGE/DEBUG_TEXT_CHANGE run. `path_policy.{h,cpp}`, `return_json.cpp` and `tests` match BASE. LAYOUT-* requirements are left Pending for 06-03.

## Self-Check: PASSED

- FOUND: src/lua_runner/database.cpp, database_query.cpp, database_csv_export.cpp, database_csv_import.cpp, database_create.cpp, database_update.cpp, database_delete.cpp
- MISSING (as intended): src/lua_runner/db_core.cpp, src/lua_runner/db_write.cpp
- FOUND commits: f1540c4, fba0703, f07f1a1
