---
phase: 02-mechanical-split
plan: 03
subsystem: lua-runner
status: complete
tags: [lua, sol2, cmake, refactor, split]
requires: [02-02 de-classed lua_runner.cpp laid out as the future files end to end, internal.h]
provides: [db_metadata.cpp, db_read.cpp, db_write.cpp, db_time_series.cpp, db_core.cpp, csv.cpp, binary.cpp, final 11-entry QUIVER_SOURCES lua_runner/ block]
affects: [02-04]
tech-stack:
  added: []
  patterns: [one binder per TU, includes trimmed per TU to the headers it uses, file-local helpers in anonymous namespaces nested in quiver::lua_internal]
key-files:
  created: [src/lua_runner/db_metadata.cpp, src/lua_runner/db_read.cpp, src/lua_runner/db_write.cpp, src/lua_runner/db_time_series.cpp, src/lua_runner/db_core.cpp, src/lua_runner/csv.cpp, src/lua_runner/binary.cpp]
  modified: [src/lua_runner/lua_runner.cpp, src/CMakeLists.txt]
decisions:
  - "Each new TU includes only the headers it uses from the old include list (picked by scanning its code for each header's identifiers); lua_runner.cpp keeps quiver/lua_runner.h, csv/csv_write.h, lua_runner/internal.h, quiver/binary/binary_file.h, quiver/database.h, sol and <memory>/<stdexcept>/<string>"
  - "csv.cpp is 446 lines after clang-format, so the csv.cpp line-budget fallback was not needed and internal.h is untouched"
metrics:
  duration: 10min
  completed: 2026-10-03
actuals:
  tokens: 44000
  tasks: 3
  commits: 3
---

# Phase 2 Plan 03: Per-domain binder extraction Summary

The seven per-domain binder sections were cut out of `src/lua_runner/lua_runner.cpp` into their own files with no edits. `lua_runner.cpp` now holds only the `RunHandles` member bodies, `LuaRunner::Impl` and the `LuaRunner` members (144 lines, down from 2100). `QUIVER_SOURCES` lists all 11 `lua_runner/` entries explicitly, in alphabetical order.

## Commits

| Task | Commit | Files |
|------|--------|-------|
| 1 (tracer): db_metadata.cpp, db_read.cpp | e80cba0 | src/lua_runner/{db_metadata.cpp, db_read.cpp, lua_runner.cpp}, src/CMakeLists.txt |
| 2: db_write.cpp, db_time_series.cpp, db_core.cpp | 85b5ec1 | src/lua_runner/{db_write.cpp, db_time_series.cpp, db_core.cpp, lua_runner.cpp}, src/CMakeLists.txt |
| 3: binary.cpp, csv.cpp; lua_runner.cpp include trim | 1dae9f8 | src/lua_runner/{binary.cpp, csv.cpp, lua_runner.cpp}, src/CMakeLists.txt |

**Reviewing:** read each commit with `git show --color-moved=zebra --color-moved-ws=allow-indentation-change <hash>`. The only uncoloured lines should be includes, the `namespace quiver::lua_internal {` / closing lines, and the CMake entries. In 1dae9f8 the dropped include lines in `lua_runner.cpp` are also uncoloured.

**Pure-move check (each commit):** I sorted the lines removed from `lua_runner.cpp` and compared them with the lines of the new files. Every removed line appears verbatim in a new file. In Task 3 the only lines with no match are the seven dropped root includes (`quiver/options.h`, `quiver/value.h`, `<chrono>`, `<filesystem>`, `<map>`, `<string_view>`, `<type_traits>`). The new files add only include lines, blank lines and the namespace open/close lines. No commit added a line to `lua_runner.cpp`.

## Test counts (observed, Debug, after each commit)

| After | `Lua*` listed / suites / passed | C API listed / passed | sync test | surface diff | clang-format 22.1.8 |
|-------|------|------|------|------|------|
| e80cba0 | 444 / 12 / 444 | 27 / 27 | 6 pass | empty | clean |
| 85b5ec1 | 444 / 12 / 444 | 27 / 27 | 6 pass | empty | clean |
| 1dae9f8 | 444 / 12 / 444 | 27 / 27 | 6 pass | empty | clean (all 11 files) |

The Debug build of every commit produced no warning or error output.

## File sizes (`wc -l src/lua_runner/*`, after 1dae9f8)

| File | Lines |
|------|-------|
| binary.cpp | 377 |
| csv.cpp | 446 |
| db_core.cpp | 231 |
| db_metadata.cpp | 164 |
| db_read.cpp | 236 |
| db_time_series.cpp | 289 |
| db_write.cpp | 311 |
| internal.h | 225 |
| lua_runner.cpp | 144 |
| path_policy.cpp | 63 |
| return_json.cpp | 225 |

No file is over 450 lines. **csv.cpp line-budget fallback: not needed.** The file is 446 lines, so neither step (comment collapse or moving `CsvWriter` to `internal.h`) was applied.

## Registrations per file

| File | `bind.set_function` | other |
|------|------|------|
| db_core.cpp | 22 | |
| db_read.cpp | 14 | |
| db_write.cpp | 11 | |
| db_time_series.cpp | 10 | |
| db_metadata.cpp | 8 | |
| csv.cpp | 3 | `CsvWriter` usertype |
| binary.cpp | 3 | 15 `ns.set_function`; `BinaryMetadata`, `BinaryFile`, `Expression` usertypes |
| lua_runner.cpp | 0 | the one `new_usertype<Database>` |
| **total** | **71** | |

## Acceptance checks (Task 3, final state)

- `ls src/lua_runner` lists exactly the 11 planned files, and `src/lua_runner.cpp` does not exist.
- `new_usertype<Database>` appears once, in `lua_runner.cpp`. `open_libraries(` appears once. `lua_runner.cpp` contains no `bind.set_function` and no `void bind_` definition.
- `struct CsvWriter {` appears once, in `csv.cpp`, before the first anonymous namespace (awk depth 0).
- `NOLINTBEGIN(performance-unnecessary-value-param)` appears once each in binary, csv, db_core, db_time_series and db_write, and each has one matching NOLINTEND. The other six files contain no NOLINT.
- `Element table_to_element(...) {` is defined once, in `db_write.cpp`. `internal.h` holds only the declaration.
- `grep -rn 'csv\.hpp' src/lua_runner` prints nothing. `lua_runner.cpp` does not include `csv/csv_read.h`. Only `csv.cpp` includes it.
- CMake has 11 `^    lua_runner/` lines in the planned order and no GLOB.
- The planning-ID `git grep` over `src/lua_runner` prints nothing.
- Each commit's `git diff --name-only HEAD~1 HEAD` lists only that task's `<files>`.

## Tracer gate

Per the user's self-verify preference, I verified the tracer myself instead of opening a human checkpoint. I rebuilt and re-ran the full `<verify>` after e80cba0, confirmed the new objects were compiled, and ran the pure-move comparison. Everything passed, so I did the expansion tasks.

## Deviations from Plan

None. The plan ran as written. Includes were picked by scanning each TU's code for the identifiers of every header in the old list, then adjusted by hand for `lua_runner.cpp`. The scan missed `csv/csv_write.h` and `quiver/binary/binary_file.h`, which the `RunHandles` bodies use only through `->` calls, so I added both back as the plan requires.

## Notes for 02-04

- The `is_lua_boolean` comment in `internal.h` still says "this file". It should say "this folder". This plan did not cover it.
- SPLIT-05 (tidy baseline) and SPLIT-06 (Julia/Dart/Python/JS suites) stay open for 02-04's gate. Before the Dart suite runs, the stale Dart hook cache needs clearing (RESEARCH Pitfall 6).

## Known Stubs

None.

## Self-Check: PASSED

- FOUND: src/lua_runner/{binary,csv,db_core,db_metadata,db_read,db_time_series,db_write}.cpp
- FOUND: e80cba0, 85b5ec1, 1dae9f8
