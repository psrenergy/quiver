---
phase: 04-fixes-and-release-type-safety
plan: 04
subsystem: lua-binding
status: complete
tags: [sol2, lua_runner, release-safety, build-flags, perf, phase-gate]

requires:
  - phase: 04-fixes-and-release-type-safety
    provides: "every explicit Pattern 1 check of 04-01..04-03 (the flags land after them), the build/fixes-check/ harness, gate baseline lua=470"
provides:
  - "src/CMakeLists.txt: SOL_ALL_SAFETIES_ON=1, SOL_PRINT_ERRORS=0, SOL_SAFE_NUMERICS=1, SOL_NO_NIL=1, plus the perf fallback SOL_SAFE_GETTER=0 / SOL_SAFE_STACK_CHECK=0; no-op SOL_SAFE_FUNCTION=1 deleted"
  - "LuaRunnerTest.CaughtScriptErrorsWriteNothingToStderr and LuaRunnerTest.DotCallThrowsInsteadOfCrashing"
  - "Comments in internal.h / db_write.cpp / binary.cpp and src/AGENTS.md / tests/AGENTS.md worded for the final flag set"
  - "CHANGELOG [0.13.0] BREAKING entry for Release argument checks / dot-calls; section complete for Phase 4"
  - "STATE.md Phase 5 baseline: Lua* 472 / 12 suites (Windows), Linux 470 run + 1 skip, C API 27, at 7e5eccd"
affects: [05]

actuals:
  tokens: 9500
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "sol2 safeties on in every build as a backstop; explicit get_type() checks own every Pattern 1 message"
    - "Perf of a build flag measured by hand with interleaved before/after Release runs after one untimed warm-up"

key-files:
  created: []
  modified:
    - src/CMakeLists.txt
    - src/lua_runner/internal.h
    - src/lua_runner/binary.cpp
    - src/lua_runner/db_write.cpp
    - tests/test_lua_runner_errors.cpp
    - src/AGENTS.md
    - tests/AGENTS.md
    - CHANGELOG.md
    - .planning/STATE.md

key-decisions:
  - "The D-08 fallback landed: full SOL_ALL_SAFETIES_ON cost +16.4% on the 100k read_scalar_floats workload; with SOL_SAFE_GETTER=0 and SOL_SAFE_STACK_CHECK=0 it is -1.9% (read) / +0.5% (file:read)"
  - "With the fallback the getter is unchecked in every build, Debug included (an explicit =0 overrides sol2's debug default); lua_cell_as and the key checks are the guard, and the Debug golden output did not change"
  - "SOL_SAFE_FUNCTION_CALLS and SOL_SAFE_USERTYPE stay on: a dot-call raises sol2's self-argument text in Release instead of an access violation"

patterns-established:
  - "Red run saved under build/fixes-check/red/, gate before the commit, mutation after it reverted with git checkout"

requirements-completed: [SAFE-06]

duration: 41min
completed: 2026-10-03
---

# Phase 4 Plan 04: sol2 Safety Backstop and Phase Gate Summary

**Release builds now run sol2's argument and `self` checks (`SOL_ALL_SAFETIES_ON=1`) and never print a caught error to stderr (`SOL_PRINT_ERRORS=0`). A dot-call such as `db.commit()` raises `received nil for 'self' argument` instead of crashing with an access violation. Turning on every sol2 safety cost +16% on a bulk read, so the documented fallback (`SOL_SAFE_GETTER=0`, `SOL_SAFE_STACK_CHECK=0`) landed and brought it back within noise. The phase is green in Debug, Release, all six suites and both Linux toolchains.**

## Performance

- **Duration:** about 41 min
- **Started:** 2026-10-03T11:00 (local)
- **Completed:** 2026-10-03T11:41 (local)
- **Tasks:** 2 (1 fix commit, 1 STATE commit)
- **Files modified:** 9

## Commits (this plan)

| # | SHA | Subject | Gate line |
|---|-----|---------|-----------|
| 1 | `7e5eccd` | fix(04-04): turn on sol2's safety checks in every build | `GATE PASS lua=472 pairs=36` (no change flag; golden and debug text identical) |
| 2 | `b79cfff` | docs(04-04): record the Phase 5 test baseline | n/a (STATE.md only) |

Wave gate after commit 1: **`WAVE GATE PASS lua=472 tidy=14`** (Release `Lua*` 472 in 12 suites all pass, C API 27, `GOLDEN release OK`, GCC 14 syntax pass clean with the defines read from `src/CMakeLists.txt`, now including `SOL_ALL_SAFETIES_ON=1`, `SOL_PRINT_ERRORS=0`, `SOL_SAFE_GETTER=0`, `SOL_SAFE_STACK_CHECK=0`). Re-run after commit 2: exit 0.

## Perf (D-08)

Release `quiver_cli`, `build/perf/perf.db` (100k `Collection` elements + a 1000 x 100 x 10 binary file), one untimed warm-up of each binary per workload, then five interleaved pairs per workload (before, after, ...), Git Bash `date +%s%N`, same power state. Median is the 3rd of the sorted five. "Before" = `bd45db0` binaries copied to `build/perf-before/`. Workload scripts and data live in `build/perf/` (gitignored, not committed).

**Round 1: `SOL_ALL_SAFETIES_ON=1` + `SOL_PRINT_ERRORS=0` (no fallback)**

| Workload | Before (ms, run order) | After (ms, run order) | Before median | After median | Delta | Verdict |
|---|---|---|---|---|---|---|
| `read_floats` (20 x `read_scalar_floats` over 100k) | 607, 633, 635, 600, 752 | 737, 664, 811, 633, 796 | 633 | 737 | **+16.4%** | over 5% (every pair slower) |
| `file_read` (100k `r:read`, 1M cells) | 648, 470, 496, 598, 412 | 477, 552, 476, 500, 460 | 496 | 477 | -3.8% | within |

**Round 2: fallback added (`SOL_SAFE_GETTER=0`, `SOL_SAFE_STACK_CHECK=0`), re-measured once the same way; this is the kept result**

| Workload | Before (ms, run order) | After (ms, run order) | Before median | After median | Delta | Verdict |
|---|---|---|---|---|---|---|
| `read_floats` | 608, 638, 815, 631, 651 | 630, 639, 626, 608, 624 | 638 | 626 | -1.9% | within 5% |
| `file_read` | 482, 422, 415, 451, 419 | 460, 424, 420, 423, 464 | 422 | 424 | +0.5% | within 5% |

`SOL_SAFE_FUNCTION_CALLS` and `SOL_SAFE_USERTYPE` were never touched (`grep -cE 'SOL_SAFE_(FUNCTION_CALLS|USERTYPE)=0' src/CMakeLists.txt` = 0). The round-1 binaries are kept in `build/perf-after-full/`.

Final define set (`sed -n '/target_compile_definitions(quiver PRIVATE/,/)/p' src/CMakeLists.txt | grep -oE 'SOL_[A-Z_]+=[0-9]+' | sort`): `SOL_ALL_SAFETIES_ON=1`, `SOL_NO_NIL=1`, `SOL_PRINT_ERRORS=0`, `SOL_SAFE_GETTER=0`, `SOL_SAFE_NUMERICS=1`, `SOL_SAFE_STACK_CHECK=0`.

**Consequence noted:** sol2 honours an explicit `SOL_SAFE_GETTER=0` over both `SOL_ALL_SAFETIES_ON` and its Debug default (`version.hpp:324-334`), so the getter is now unchecked in Debug as well as Release. Every `.as<` site is guarded (audit below), and the Debug golden output plus `debug_text` stayed byte-identical, so no observable text changed. The three mixed-array tests now prove their fix in Debug too.

## Stderr and Dot-Call Evidence

| File / run | Build | Result |
|---|---|---|
| `red/flags-stderr.txt` | Debug, before the flags | `CaughtScriptErrorsWriteNothingToStderr` FAILED: both captures were `"[sol2] An exception occurred: Cannot commit: no active transaction\n"` (the pcall-caught case and the propagated case). `DotCallThrowsInsteadOfCrashing` **passed in Debug before the change** (Debug already had every safety on). |
| `red/flags-dotcall-release.txt` | Release, before the flags | `DotCallThrowsInsteadOfCrashing`: `SEH exception with code 0xc0000005 thrown in the test body` (gtest caught the access violation), `[  PASSED  ] 0 tests`, `exit=1`. The stderr test **passed in Release before the change** (Release never printed). |
| After the flags | Debug and Release | Both tests pass (2/2 in each). |
| Mutation (after commit, reverted) | Debug | Dropped only `SOL_PRINT_ERRORS=0`: the stderr test FAILED with the same `[sol2] An exception occurred: ...` captures (`build/fixes-check/mutation-print-errors.txt`). Reverted with `git checkout -- src/CMakeLists.txt`; `git diff --exit-code -- src` exited 0; rebuilt, 2/2 pass. |
| Release `debug_text.lua` through `build/release/bin/quiver_cli.exe` (same splice as `golden.sh`) | Release | exit 0, output starts with `{`, **byte-identical** to `baseline/debug_text.txt` (`cmp` silent). The five dot probes (`dot_delete_element`, `dot_describe`, `dot_is_healthy`, `dot_writer_close`, `dot_writer_write_row`) now print `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` in Release, as Debug did. |

**Tracer gate (delegated self-verification):** after the commit and the mutation revert, the tracer `<verify>` ran end to end: `GATE PASS lua=472 pairs=36`, the Release two-test filter 2/2, the four greps, and a clean `git diff -- src tests`. Per the standing preference, the verdict (Release no longer crashes on a dot-call and stderr stays silent, at an in-budget cost) is recorded as delegated rather than stopping for a human.

## `.as<` / `.get<` Audit (`src/lua_runner/`, 41 sites, none unguarded)

| Site | Guard |
|---|---|
| `binary.cpp:109` `o.as<Expression>()` | `o.is<Expression>()` (usertype check) |
| `binary.cpp:112` `o.as<BinaryFile&>()` | `o.is<BinaryFile>()` |
| `binary.cpp:129` `lhs.as<double>()`, `:132` `rhs.as<double>()` | `is_number(lhs)` / `is_number(rhs)` |
| `csv.cpp:61`, `:64` `pair.first.as<std::int64_t>()` | `pair.first.is<std::int64_t>()` on the same line (short-circuit) |
| `csv.cpp:161` `cell.as<std::string>()` | `cell.is<std::string>()` throw above |
| `csv.cpp:175` `value.as<std::string>()` | `get_type() != sol::type::string` throw above |
| `csv.cpp:353` `on_row_arg.as<sol::protected_function>()` | `get_type() != sol::type::function` throw above |
| `csv.cpp:377` `result.get<optional bool>(0)` | optional getter (strict boolean check, nullopt otherwise) |
| `db_core.cpp:23` `key.as<std::string>()` | `get_type() != sol::type::string` throw above |
| `db_core.cpp:83` `fn_arg.as<sol::protected_function>()` | `get_type() != sol::type::function` throw above |
| `db_core.cpp:100`, `lua_runner.cpp:167` `result.get<sol::object>(0)` | `sol::object` accepts any value |
| `db_core.cpp:109` `parameters.get<sol::object>(i)` | `sol::object` accepts any value; then `lua_to_value` |
| `db_write.cpp:63` `val.as<sol::table>()` | `val.get_type() == sol::type::table` |
| `db_write.cpp:114` `pair.first.as<std::string>()` | `get_type() != sol::type::string` throw above |
| `db_write.cpp:117`, `:120` `cell.first.as<int64_t>()` | `cell.first.is<int64_t>()` on line 117 (short-circuit / throw) |
| `db_write.cpp:154` `cell.first.as<int64_t>()` | validated upstream by `collect_group_columns` (lines 117-120) |
| `db_write.cpp:191` `target_label.as<std::string>()` | `get_type() != sol::type::string` throw above |
| `return_json.cpp:130` `as<std::int64_t>()` | `is<std::int64_t>()` break above |
| `return_json.cpp:157` / `:159` | `is<std::string>()` / `is<std::int64_t>()` branch |
| `return_json.cpp:201` `as<bool>()` | `get_type() == sol::type::boolean` |
| `return_json.cpp:203` / `:205` / `:207` | `is<std::int64_t>()` / `is<double>()` / `is<std::string>()` branch |
| `return_json.cpp:211` `as<sol::table>()` | `get_type() == sol::type::table` |
| `internal.h:125`, `:143` `as<bool>()` | `is_lua_boolean` (`get_type() == boolean`) |
| `internal.h:128`, `:237` `as<optional T>()` | optional getter: a mismatch is nullopt, then a Pattern 1 throw |
| `internal.h:146` / `:149` / `:152` | `is<int64_t>()` / `is<double>()` / `is<std::string>()` branch |
| `internal.h:165` `t.get<sol::object>(i)` | `sol::object`; then `lua_cell_as` |
| `internal.h:209` `o.as<sol::table>()` | `get_type() != sol::type::table` throw above (`require_table`) |
| `internal.h:218` `key.as<std::string>()` | `get_type() != sol::type::string` throw above (`lua_string_key`) |
| `internal.h:272` `entry.first.as<std::string>()` | `get_type() != sol::type::string` throw above (`option_entries`) |

No `src/lua_runner/` file beyond the planned three needed a fix.

## Phase Gate (Task 2)

- **Wave gate:** `WAVE GATE PASS lua=472 tidy=14` (output in `build/fixes-check/wave_0404.txt`); `n` equals the Debug gate count.
- **Six suites:** `bindings/dart/.dart_tool/hooks_runner` and `bindings/dart/.dart_tool/lib` deleted and confirmed absent (`ls`: No such file or directory) before the run; `tasklist /m libquiver.dll` and `/m libquiver_c.dll` found no holder. `scripts/test-all.bat` -> `build/fixes-check/test-all.txt`: C++ PASS, C API PASS, Julia PASS, Dart PASS, JavaScript PASS, Python PASS (`grep -Ec 'tests: +PASS'` = 6).
- **Linux** (`git archive HEAD` at `7e5eccd`, `ubuntu:24.04`, Debug, tests + C API):
  - GCC 13.3.0: `LISTED=470` (472 - 2 `_WIN32`-only), `[  PASSED  ] 469 tests.`, `[  SKIPPED ] 1 test.`, C API `[  PASSED  ] 27 tests.`, exit 0. The only build warnings are the pre-existing `-Wreturn-type` in `src/binary/time_properties.cpp:20,62`.
  - Clang 18.1.3 `-stdlib=libc++`: `LISTED=470`, 469 passed, 1 skipped, C API 27 passed, exit 0. Only warning: pre-existing `-Wself-assign-overloaded` in `tests/test_migrations.cpp:186`.
- **Static checks against `BASE` (`b39fe78`):**
  - `git grep -n 'SOL_SAFE_FUNCTION=' -- ':!.planning'` prints nothing.
  - `uv run python scripts/assert_version.py` exits 0 (all five at 0.13.0); the manifest `git diff --stat` is empty.
  - CHANGELOG `[0.13.0]`: `### Changed` with 5 `**BREAKING**` entries (table arguments, optional arguments, empty array in `update_element`, text-only `load`, Release argument checks / dot-call) and `### Fixed` with 4 entries (non-string keys, scoped-block argument check, commit-failure rollback, operand errors). Planning-ID regex count: 0.
  - `git diff --stat BASE HEAD -- bindings` lists only `bindings/js/src/lua-api.ts`; `git diff BASE HEAD -- 'tests/*.cpp' | grep -c '^-[^-]'` = 0; every `src/lua_runner/` file is at most 450 lines (binary.cpp 339, csv.cpp 416, db_core.cpp 205, db_metadata.cpp 91, db_read.cpp 129, db_time_series.cpp 281, db_write.cpp 311, internal.h 317, lua_runner.cpp 170, path_policy.cpp 63, return_json.cpp 225).

## Final Counts (Phase 5 baseline, recorded in STATE.md)

`quiver_tests --gtest_filter=Lua*` = **472 tests in 12 suites** (Windows Debug and Release), Linux **470 run, 469 pass + 1 skip**, `LuaRunnerCApiTest` = **27**, at `7e5eccd`.

## PR Notes for Phase 4

### Every commit of the phase

| SHA | Subject | Gate line |
|---|---|---|
| `70d2470` | fix(04-01): reject a non-table payload in the group writers | `GATE PASS lua=446 pairs=36` |
| `6014bb2` | fix(04-01): check every other table argument and report its Lua type | `GATE PASS lua=454 pairs=36` |
| `61f0297` | fix(04-01): reject non-string keys in element, row, dims and files tables | `GATE PASS lua=457 pairs=36` |
| `30a169f` | fix(04-01): reject wrong-typed optional arguments | `GATE PASS lua=460 pairs=36`; wave `WAVE GATE PASS lua=460 tidy=14` |
| `05242f5` | docs(04-01): complete release type safety plan | - |
| `6242813` | fix(04-02): reject a non-function in db:transaction and db:dry_run before opening the scope | `GATE PASS lua=462 pairs=36` |
| `d7c2478` | fix(04-02): roll back db:transaction when its commit fails | `GATE PASS lua=464 pairs=36` |
| `3705d29` | fix(04-02): pass an empty array in update_element through to the core | `GATE PASS lua=467 pairs=36`; wave `lua=467 tidy=14` |
| `3e22792` | docs(04-02): complete scoped blocks and empty arrays plan | - |
| `f3d6abd` | fix(04-03): load only text chunks | `GATE PASS lua=469 pairs=36` |
| `d81e2d8` | fix(04-03): name the operation in expression operand errors | `GATE PASS lua=470 pairs=36` |
| `87a8266` | refactor(04-03): remove the unreachable branches in the Lua bindings | `GATE PASS lua=470 pairs=36`; wave `lua=470 tidy=14` |
| `bd45db0` | docs(04-03): complete text-only load and operand error names plan | - |
| `7e5eccd` | fix(04-04): turn on sol2's safety checks in every build | `GATE PASS lua=472 pairs=36`; wave `lua=472 tidy=14` |
| `b79cfff` | docs(04-04): record the Phase 5 test baseline | - |

### Red files (`build/fixes-check/red/`)

`c1-groups.txt`, `c1-rest.txt`, `c2-keys.txt` (Debug only), `c5-optionals.txt`, `c5-optionals-release.txt`, `c6-fn.txt`, `c4-commit.txt`, `c7-empty.txt`, `load-text.txt`, `c8-operands.txt`, `flags-stderr.txt`, `flags-dotcall-release.txt`. Details per file in 04-01..04-04 SUMMARYs.

### Mutation results (each reverted, `git diff --exit-code -- src` clean)

| Plan | Mutation | Caught by |
|---|---|---|
| 04-01 | `require_table` accepts userdata | both group-writer tests FAILED |
| 04-01 | userdata-attribute check removed | `CreateElementRejectsUserdataAttribute` FAILED |
| 04-02 | `fn` check moved below `begin` | `TransactionBlockRejectsNonFunction`, `DryRunBlockRejectsNonFunction` FAILED |
| 04-02 | `finish` moved out of the `try` | `TransactionBlockCommitFailureRollsBack` FAILED |
| 04-02 | empty-array set replaced by a no-op | both update tests FAILED, create pin green |
| 04-03 | forced load mode `"bt"` | `LoadRefusesBinaryChunks` FAILED |
| 04-03 | `+` registered as `binop("sub")` | `OperandErrorsNameTheOperation` FAILED |
| 04-03 | lowercase dropped in `lua_data_type_name` | `golden.sh debug` diff in 3 probe files |
| 04-04 | `SOL_PRINT_ERRORS=0` dropped | `CaughtScriptErrorsWriteNothingToStderr` FAILED |

### Golden and debug-text re-baselines (D-18, aggregated)

| Commit | Key(s) | Old text | New text |
|---|---|---|---|
| `70d2470` | `write.txt` `vector_not_array` | `Cannot update_vector_group: column 'value_int' must be an array of values` | same + `, got number` |
| `6014bb2` | `options.txt` `{export_csv,import_csv,read_csv,read_csv_stream,write_csv}_{02,03,04,09}` | `Cannot <op>: options must be a table` | same + `, got number/string/userdata/boolean` |
| `6014bb2` | `options.txt` `enum_level1..3`, `header_number`, `metadata_none/number`, `metadata_labels_number` | `... must be a table` | same + `, got <type>` |
| `6014bb2` | `csv.txt` `row_none/number/string/userdata`; `binary.txt` `mapping_number`; `debug_text` `control_writer_write_row` | `... must be a table` | same + `, got <type>` |
| `30a169f` | `debug_text` `badarg_query_{string,integer,float}` (Debug-only sol2 signature) | `..., sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >)'` | `..., const sol::basic_object<sol::basic_reference<0> >&)'` |
| `6242813` | `debug_text` `control_transaction`, `control_dry_run` | `[string "..."]:72: stack index 2, expected function, received number: must be a function or table or a userdata (bad argument into '...')` | `Cannot transaction: fn must be a function, got number` / `Cannot dry_run: ...` |
| `d81e2d8` | `binary.txt` 30 keys `{add,sub,mul,div,band,bor}_{string,nn}`, `{gt,lt,gte,lte,eq,neq}_{nn,none,string}` | `Cannot build expression: operand must be an expression or a binary file` | `Cannot <op>: ..., got <type>` (`_nn` arithmetic: `Cannot expression: ..., got number`) |
| all others, incl. `7e5eccd` | none | - | byte-identical |

### Check-order ledger (phase)

Deliberate reorders only, all of decodes that used to share one call's argument list (compiler-dependent order): `file:write` (data, then dims), `db:bin_to_csv` (containment, then `aggregate`), `file:read` (dims, then `allow_nulls`), `expr:aggregate` / `aggregate_agents` (op, then parameter); and the scoped blocks' `fn` check moving ahead of `begin` (04-02). `db:open_file` keeps mode -> containment -> metadata (the metadata check is new, placed after `resolve_sandboxed_path`). Everything else unchanged. 04-04 moves no check.

### Tidy (`src/lua_runner/`, 14 unique, same set across the phase)

| File | Check | Count |
|---|---|---|
| binary.cpp | bugprone-unchecked-optional-access | 3 |
| binary.cpp | modernize-return-braced-init-list | 1 |
| binary.cpp | modernize-raw-string-literal | 1 |
| csv.cpp | readability-identifier-naming | 2 |
| db_core.cpp | bugprone-empty-catch | 1 |
| lua_runner.cpp | bugprone-empty-catch | 2 |
| return_json.cpp | readability-identifier-naming | 3 |
| return_json.cpp | bugprone-implicit-widening-of-multiplication-result | 1 |

`performance-unnecessary-value-param` = 0, `clang-diagnostic-error` = 0.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Accuracy] CMake comment reworded for the fallback**
- **Found during:** Task 1, after the fallback landed
- **Issue:** The planned comment said every sol2 safety is on in every build, which is false once `SOL_SAFE_GETTER=0` / `SOL_SAFE_STACK_CHECK=0` are added.
- **Fix:** The comment now says the safeties are on except the getter and stack check, gives the measured reason, and says never to turn off `SOL_SAFE_FUNCTION_CALLS` / `SOL_SAFE_USERTYPE`. Same commit.

**2. [Rule 2 - Docs] `binary.cpp` `rename_agents` comment also reworded**
- It said the unchecked getter misbehaved "in Release". With the fallback it does so in every build. One-line edit in a planned file.

**3. [Docs] CHANGELOG stderr sentence scoped to Debug**
- Release never printed the `[sol2]` line (the stderr test passed in Release before the change), so the entry says Debug builds no longer print it.

**4. [Note] Red-file acceptance wording**
- `flags-dotcall-release.txt` contains gtest's summary line `[  PASSED  ] 0 tests.`, and the crash surfaces as a caught SEH exception rather than a process kill. It has a nonzero exit (`exit=1`), and no test passed.

**Total deviations:** 4 (one comment accuracy fix, one doc rewording, one changelog scope fix, one acceptance-wording note). No scope creep.

## Issues Encountered

The perf budget was exceeded in round 1 (+16.4% on `read_floats`, every pair slower), so the documented fallback was applied and re-measured once.

## Known Stubs

None.

## Threat Flags

None. No new surface. T-04-16 (dot-call crash) is mitigated: red in Release, green after, and the five `debug_text` dot probes run in Release. T-04-17: `SOL_SAFE_FUNCTION_CALLS` is on and never disabled. T-04-18: `SOL_PRINT_ERRORS=0`, the stderr test and its mutation. T-04-19: the audit found no unguarded `.as<`, and the getter is unchecked anyway under the fallback, so no `luaL_error` longjmp comes from a getter. T-04-20: measured, fallback applied. T-04-21: the Dart caches were deleted and no process held the DLL.

## Next Phase Readiness

- Phase 5 starts from `Lua*` = 472 / 12 suites (Windows), Linux 470 + 1 skip, C API 27, at `7e5eccd`. CHANGELOG `[0.13.0]` holds every Phase 4 entry.
- The getter being unchecked in Debug is new. A future `.as<T>()` without a type guard now fails silently in Debug too, so CI no longer catches it. The `.as<` audit pattern above is the review check to keep.

## Self-Check: PASSED

- Commits found: `7e5eccd`, `b79cfff`.
- Red files exist: `red/flags-stderr.txt` (contains `FAILED` and `[sol2]`), `red/flags-dotcall-release.txt` (`exit=1`).
- Wave gate output saved at `build/fixes-check/wave_0404.txt`; six-suite output at `build/fixes-check/test-all.txt`; Linux logs at `build/fixes-check/linux_{gcc,clang}.txt`.
