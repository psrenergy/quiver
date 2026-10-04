---
phase: 06-quiver-file-layout
plan: 03
subsystem: lua_runner
status: complete
tags: [lua, refactor, file-layout, sol2, docs]
requires:
  - 06-02 (11 of 14 binders at their final slots, build/layout-check harness)
provides:
  - src/lua_runner/expression.cpp (bind_expression: Expression usertype, operators on Expression and BinaryFile, 12 quiver.* expression functions)
  - src/lua_runner/binary.cpp (bind_binary returns the BinaryFile usertype)
  - AGENTS.md / src/AGENTS.md file map and Layout bullet for the 14-binder layout
  - build/layout-check/phase_gate.sh (gitignored)
affects: [07, 08]
tech-stack:
  added: []
  patterns: [binder returns a usertype the next binder extends, kept include documented where sol2 trait detection depends on it]
key-files:
  created:
    - src/lua_runner/expression.cpp
  modified:
    - src/lua_runner/binary.cpp
    - src/lua_runner/internal.h
    - src/lua_runner/lua_runner.cpp
    - src/CMakeLists.txt
    - AGENTS.md
    - src/AGENTS.md
    - bindings/js/src/lua-api.ts
    - bindings/js/test/lua-api-sync.test.ts
decisions:
  - "bind_binary returns sol::usertype<BinaryFile> and drops its Database& parameter (only expr:save used it); bind_expression takes the usertype and db"
  - "binary.cpp keeps quiver/expression/expression.h with a comment: sol2 derives BinaryFile's __lt/__le/__eq from the Expression operators"
  - "binary.cpp carries no NOLINT pair (tidy performance-unnecessary-value-param 0 confirms); expression.cpp carries the one pair the moved text had"
  - "No CHANGELOG entry and no version bump: no user-visible change (golden Debug+Release and runtime surface identical)"
metrics:
  duration: 35min
  completed: 2026-10-04
actuals:
  tokens: 14000
  tasks: 3
  commits: 2
---

# Phase 6 Plan 03: Expression split, docs and phase gate Summary

`binary.cpp` split in two: `bind_binary` keeps file I/O, BinaryMetadata, BinaryFile and `quiver.metadata*` and now returns the `BinaryFile` usertype; new `expression.cpp` holds `bind_expression` (Expression usertype, the operator metamethods on both usertypes, the 12 `quiver.*` expression functions). Docs now describe the 14-binder core-file layout, and the phase gate passes on the final tree.

## Commits (whole phase)

| Plan | Commit | Message |
|---|---|---|
| 06-01 | `8fb84cc` | refactor: register describe names in lua_runner/database_describe.cpp |
| 06-01 | `818eb95` | refactor: rename lua_runner read, metadata and time-series files after their core files |
| 06-01 | `e0b4b1d` | refactor: register number_of_elements and time-series metadata beside their core files |
| 06-01 | `4322804`, `1146e28` | docs: 06-01 summary/state |
| 06-02 | `f1540c4` | refactor: register export_csv and import_csv in their own lua_runner files |
| 06-02 | `fba0703` | refactor: split the core binder into lua_runner/database.cpp and database_query.cpp |
| 06-02 | `f07f1a1` | refactor: split the write binder into lua_runner/database_create, database_update and database_delete |
| 06-02 | `0dadeb6`, `0961ead` | docs: 06-02 summary/state |
| 06-03 | `756de71` | refactor(06-03): move the expression bindings from lua_runner/binary.cpp to expression.cpp |
| 06-03 | `bb33917` | docs(06-03): describe the core-file layout of src/lua_runner |

## Evidence

- **Base:** `21ba6f89fdac3c18e814d24e7015c1bd4a9e8e7e` (no baseline recaptured, no GOLDEN_CHANGE/DEBUG_TEXT_CHANGE run).
- **Per-commit gate:** `GATE PASS lua=477 pairs=36` after Task 1, after Task 2, and twice in a row on the final commit (identical lines).
- **Phase gate:** `PHASE GATE PASS lua=477 tidy=14 gcc=ok`.
  - Debug and Release: `quiver_tests` 1454 listed / 1454 pass, `quiver_c_tests` 543 / 543; Lua* 477 in 12 suites, SandboxedPathTest 11, LuaRunnerCApiTest 27.
  - `GOLDEN debug OK` and `GOLDEN release OK` (runtime surface probe and Debug-only sol2 text probe included).
  - Tidy over 17 files: 14 unique warnings, `performance-unnecessary-value-param` 0, `clang-diagnostic-error` 0, pairs identical to the base.
  - GCC 14 `-fsyntax-only` over all `src/lua_runner/*.cpp`: clean (also run on binary/expression/lua_runner after Task 1).
  - Placement equals `expected-placement.txt` (86 lines); 19 files; CMake block byte-sorted with the same 19; usertypes Database, BinaryMetadata, BinaryFile, Expression, CsvWriter once each; binder calls and declarations in LAYOUT-02 order; `return_json.cpp`, `path_policy.{h,cpp}`, `tests/`, `CHANGELOG.md` unchanged; every changed `lua_runner.cpp` line is a `lua_internal::bind_` line; 0 `lua_runner/db_` in both build.ninja files; old-name grep 0 (24 at base); every cited `lua_runner/` file exists; `assert_version.py` OK (0.13.0).
- **BinaryFile metatable** still has `__lt __le __eq __add __unm __band __bor __bnot` (8, runtime surface).
- **Six suites:** `scripts/test-all.bat` - C++, C API, Julia, Dart, JavaScript, Python all PASS, `All tests PASSED`. lua-api sync test 6 pass, 0 fail.

### The 14 tidy pairs (unchanged from base)

```
bugprone-empty-catch                                } catch (...) {
bugprone-empty-catch                                } catch (const std::exception&) {   (x2)
bugprone-implicit-widening-of-multiplication-result constexpr std::size_t kMaxReturnBytes = 64UL * 1024UL * 1024UL;
bugprone-unchecked-optional-access                  t["frequency"] / t["initial_value"] / t["parent_dimension_index"] (3)
modernize-raw-string-literal                        throw std::runtime_error("Cannot open_file: mode must be \"r\" or \"w\"");
modernize-return-braced-init-list                   return Expression(o.as<BinaryFile&>());   (now in expression.cpp)
readability-identifier-naming                       CsvWriter(...) ctor, kMaxReturnDepth, kMaxWidth, kMaxReturnBytes, kHex (5)
```

### Per-file line counts (max 416, limit 450)

database_delete 13, database_describe 14, path_policy.h 18, database_csv_import 24, database_query 48, path_policy.cpp 64, database_metadata 70, database_csv_export 79, database 94, database_create 102, database_read 130, expression 176, lua_runner 180, binary 203, return_json 225, database_update 232, database_time_series 284, internal.h 352, csv 416.

### Novel lines (44, all classified)

- Includes (5): `binary_file.h`, `binary_metadata.h`, `expression.h` (expression.cpp), `element.h`, `<variant>` (database_create.cpp, 06-02).
- The expression.h comment in binary.cpp (2).
- Shared-helper declarations (3): `parse_csv_options`, two `metadata_to_lua` (06-01/06-02).
- Binder calls (10): `bind_create/csv_export/csv_import/database/delete/describe/query/update`, `auto binary_file_type = ...bind_binary(...)`, `bind_expression(...)`.
- Binder signatures and declarations (20), including the clang-format-wrapped `bind_binary` parameter lines (`sol::state& state,`, `sol::usertype<Database>& bind,`, `sol::table& ns,`, `RunHandles& handles`, `sol::usertype<BinaryFile> bind_binary(`).
- `return binary_file_type;` (1).

No other line; no logic changed. Reordered checks: none. No Lua-visible change (golden + runtime surface identical).

## Deviations from Plan

**1. [Rule 1 - Harness bug] phase_gate.sh exited silently on its first run.** `old=$(git grep ... | wc -l)` under `set -o pipefail` failed when git grep found nothing (exit 1), so `set -e` stopped the script after the placement step with no message. Fixed with `|| true`; the full gate was then rerun end to end and passed. Harness only (gitignored), no repo file affected.

**2. Tidy filter regex** `src.lua_runner.` instead of `src[\\/]lua_runner[\\/]`, as 06-01 recorded.

**3. AGENTS.md wording** uses the plan's text "(`database.cpp` and `database_*.cpp`, named after `src/database*.cpp`)"; `database_*.cpp` there is a glob in a sentence about `src/lua_runner/`, not a single file name. Every concrete Lua binder file cited outside the src/AGENTS.md file map carries the `lua_runner/` prefix.

## No CHANGELOG entry

The root AGENTS.md Changelog rule covers user-visible changes; the phase proves none (golden Debug+Release, runtime surface, registered names and all six suites unchanged), so CHANGELOG.md and the five version manifests stay at 0.13.0.

## Self-Check: PASSED

- FOUND: src/lua_runner/expression.cpp, src/lua_runner/binary.cpp, build/layout-check/phase_gate.sh
- FOUND commits: 756de71, bb33917
