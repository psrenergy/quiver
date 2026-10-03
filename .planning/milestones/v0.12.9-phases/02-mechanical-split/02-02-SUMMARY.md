---
phase: 02-mechanical-split
plan: 02
subsystem: lua-runner
status: complete
tags: [lua, sol2, cmake, refactor, de-class]
requires: [02-01 folder move, target-wide /bigobj, folder-reading sync test]
provides: [quiver::lua_internal namespace, RunHandles, seven binders, one Database usertype, internal.h, return_json.cpp, path_policy.cpp]
affects: [02-03, 02-04]
tech-stack:
  added: []
  patterns: [binders take sol::usertype<Database>& bind and the quiver table as ns, file-local helpers in anonymous namespaces nested in quiver::lua_internal, header holds only templates/inline/structs/declarations]
key-files:
  created: [src/lua_runner/internal.h, src/lua_runner/return_json.cpp, src/lua_runner/path_policy.cpp]
  modified: [src/lua_runner/lua_runner.cpp, src/CMakeLists.txt]
decisions:
  - "lua_runner.cpp is laid out as the future files end to end (return_json, shared, path_policy, db_metadata, db_read, db_write, db_time_series, db_core, binary, csv, root), so plan 03's extractions are pure range cuts"
  - "The 'Conversion helpers' banner was dropped and the 'Expression subsystem bindings' banner became a one-line comment inside bind_binary, since both labelled regions that no longer exist"
metrics:
  duration: 20min
  completed: 2026-10-03
actuals:
  tokens: 33000
  tasks: 2
  commits: 2
---

# Phase 2 Plan 02: De-class Impl and extract internal.h, return_json.cpp, path_policy.cpp Summary

`LuaRunner::Impl` now holds only `db`, `handles` (a `lua_internal::RunHandles`) and the Lua state. All of its former static members are free functions in `quiver::lua_internal`. The constructor creates the one `Database` usertype and passes it to seven binders. All 71 `db:` methods register through `bind.set_function`. `internal.h`, `return_json.cpp` and `path_policy.cpp` are pure cuts out of the de-classed file.

## Commits

| Task | Commit | Files |
|------|--------|-------|
| 1 (tracer): de-class Impl in place, RunHandles, seven binders, 17 pairs to `bind.set_function` | c898a63 | src/lua_runner/lua_runner.cpp |
| 2: extract internal.h, return_json.cpp, path_policy.cpp | 53eae34 | src/lua_runner/{internal.h, return_json.cpp, path_policy.cpp, lua_runner.cpp}, src/CMakeLists.txt |

**Reviewing c898a63:** `git show -w --color-moved=zebra --color-moved-ws=allow-indentation-change c898a63`. Only these lines should be uncolored: namespace wrappers, binder headers, the `bind.set_function("name", ...)` heads of the 17 converted pairs, the three captures, the `handles.` qualifications, the two dropped `lua["quiver"]` lookups, the new Impl constructor and `run()` tail, the five NOLINT pairs, and the comment words listed below. Lines that clang-format re-wrapped to fit after the re-indent are also uncolored.

## Test counts (observed)

| After | Debug `Lua*` listed / suites / passed | Debug C API | sync test | surface diff | Release `Lua*` | Release `LuaRunner_Lifecycle.*` | Release C API |
|-------|------|------|------|------|------|------|------|
| c898a63 | 444 / 12 / 444 | 27 passed | 6 pass | empty (107 entries) | 444 passed | 4 listed, 4 passed | 27 passed |
| 53eae34 | 444 / 12 / 444 | 27 passed | 6 pass | empty (107 entries) | 444 passed | 4 passed | 27 passed |

`static_assert(sizeof(LuaRunner) == sizeof(void*))` compiles in both builds. MSVC Debug and Release built with zero warnings in `src/lua_runner/`. The only Release warnings are the two pre-existing C4715 in `src/binary/time_properties.cpp`.

## File sizes after this plan (`wc -l`, clang-format 22.1.8)

| File | Lines |
|------|-------|
| src/lua_runner/internal.h | 225 |
| src/lua_runner/return_json.cpp | 225 |
| src/lua_runner/path_policy.cpp | 63 |
| src/lua_runner/lua_runner.cpp | 2100 (plan 03 cuts it down) |

## Acceptance checks (Task 1)

- `new_usertype<Database>`: 1. `open_libraries(`: 1. Both totals are still 1 across the folder after Task 2.
- `bind.set_function(` = 71 and `ns.set_function(` = 15. after.txt has 71 `db` entries, and the diff against bab557e's before.txt is empty.
- Binder signatures: the five-binder grep prints 5, and the `bind_csv` and `bind_binary` greps print 1 each.
- `[this]` = 0, `[&handles]` = 2, `[&db]` = 1. `lua["quiver"]` = 0.
- Impl members are `Database& db;`, `lua_internal::RunHandles handles;` and `sol::state lua;`, in that order. The constructor token order matches the plan exactly, from `open_libraries(` through `lua["db"] = &db`.
- `impl.handles.close_open_writers();` = 1 and `impl.lua.collect_garbage();` = 1. `gc_guard{` (line 2508) comes before `auto result` (line 2510).
- `^static` = 0. NOLINTBEGIN = 5 and NOLINTEND = 5, all named `performance-unnecessary-value-param`, in db_write, db_time_series, db_core, binary and csv.
- The planning-ID grep over `src/lua_runner` prints nothing. clang-format `--dry-run --Werror` is clean.

As an extra check, I compared the old and new file as sorted multisets of trimmed lines. Every difference is one of: a clang-format re-wrap, a converted pair head, a capture, a `handles.` qualification, a namespace or binder scaffold line, a NOLINT line, or one of the comment edits below.

## Acceptance checks (Task 2)

- `ls src/lua_runner` lists exactly `internal.h`, `lua_runner.cpp`, `path_policy.cpp` and `return_json.cpp`.
- In internal.h: the guard count is 1, seven binder declarations, `struct RunHandles {` = 1, `^static`/`^namespace {` = 0, and no csv include.
- `weakly_canonical` appears only in `path_policy.cpp`. `append_json` appears 0 times in `lua_runner.cpp`, and the `encode_return_json` definition appears once in `return_json.cpp`.
- Every `.cpp` in the folder includes `"lua_runner/internal.h"`. No file uses the bare `"internal.h"`.
- CMake lists four `lua_runner/` entries in the required order and has no GLOB.
- Pure move: every line removed from `lua_runner.cpp` appears verbatim in one of the three new files. The only line added to `lua_runner.cpp` is the `#include "lua_runner/internal.h"`.

## Comment edits (Task 1, words that named something relocated)

- CsvWriter comments: "Impl keeps a weak_ptr" became "RunHandles keeps a weak_ptr" (two places).
- `csv_row_cells_from_lua` comment: "csv_cell_to_string below" became "above". `csv_cell_to_string` moved above its caller, as planned.
- `csv_cell_to_string` comment: "csv_row_cells_from_lua above" became "below".
- `read_csv_options_from_lua` comment: "see relation_target_from_lua below" lost "below". That function now lives in the write section.
- CsvWriter usertype comment: "BinaryFile below is the same" became "BinaryFile is the same". The binary section now comes first.
- The "Conversion helpers" banner was dropped. The "Expression subsystem bindings" banner became a one-line comment inside `bind_binary`.
- The GcGuard rationale comment is unchanged. Every name it uses (`close_open_writers()`, "member of Impl") is still accurate.

## Note for the PR

`CsvWriter`'s sol2 registry key changes from the `quiver::LuaRunner::Impl::CsvWriter` spelling to `quiver::lua_internal::CsvWriter`. Only sol2's Debug type text shows it. Scripts cannot see it because the `debug` library is not loaded, and no test pins it.

## Tracer gate

Per the user's self-verify preference, I verified the tracer myself instead of opening a human checkpoint. I re-ran its `<verify>` end to end (Debug suites, sync test, surface diff, then Release `Lua*`, `LuaRunner_Lifecycle.*` and C API), and all passed. After that I did the expansion task.

## Deviations from Plan

None. Both tasks ran as written. I also ran the Release `Lua*`, lifecycle and C API suites after Task 2, which the plan requires only for Task 1. They passed.

## Notes for later plans

- The `is_lua_boolean` comment says "Every boolean test in this file goes through this one predicate". It now lives in `internal.h`, so after plan 03 "this file" should read "this folder". Task 2 forbade editing moved lines, so I left it for 02-04 or the Phase 5 docs pass.
- `lua_runner.cpp` still includes every header the monolith did. Plan 03's extractions should trim each TU to the headers it actually uses.

## Known Stubs

None.

## Self-Check: PASSED

- FOUND: src/lua_runner/internal.h, src/lua_runner/return_json.cpp, src/lua_runner/path_policy.cpp, src/lua_runner/lua_runner.cpp
- FOUND: c898a63, 53eae34
