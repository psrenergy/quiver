---
phase: 04-fixes-and-release-type-safety
verified: 2026-10-03T15:45:00Z
status: passed
score: 5/5 must-haves verified (criterion 5 baseline gap closed by orchestrator; Linux re-run at e6aa5c1; Apple Clang and PR perf note left to the PR)
behavior_unverified: 0
overrides_applied: 0
gaps:
  - truth: "ROADMAP SC5 (last clause): the resulting Lua gtest count (--gtest_filter=Lua*) and C API count are recorded as the baseline that Phase 5's runs must reproduce"
    status: partial
    reason: "STATE.md records Lua* = 472 (Windows) / 470 + 1 skip (Linux) at 7e5eccd. The review fixes (c666ced, b21f98d, bc38384) added 5 Lua tests, so the phase's resulting count is 477 on Windows (verified Debug and Release in this run). Phase 5 SC1 reproduces 'the count recorded at the end of Phase 4', so it would check against a wrong number."
    artifacts:
      - path: ".planning/STATE.md"
        issue: "Lines 120 (04-04 decision bullet) and 135 (Phase 5 baseline) say Lua* 472 / Linux 470 + 1 skip at 7e5eccd"
    missing:
      - "Record Lua* = 477 / 12 suites (Windows Debug and Release) at e6aa5c1, C API 27, full quiver_tests 1443, quiver_c_tests 543"
      - "Re-run Linux GCC 13 + Clang 18/libc++ after the review fixes and record the result (expected 476 run = 475 pass + 1 root skip), or mark the Linux number as 'expected, not re-run'"
human_verification:
  - test: "Open the Phase 4 PR and let the CI Release matrix (Windows, Linux GCC/Clang, macOS) run the Lua* and C API suites"
    expected: "All green. Lua* = 477 on Windows; on Linux 476 run (475 pass + 1 root skip)"
    why_human: "ROADMAP SC1 requires the new tests to pass 'in the CI Release matrix'. Locally only Windows Debug/Release ran after the review fixes; Linux was last run at 7e5eccd, before c666ced/b21f98d/bc38384"
  - test: "Put the perf measurement from 04-04-SUMMARY.md (Perf (D-08) tables: full flags +16.4% on read_floats; fallback -1.9% read_floats / +0.5% file:read) in the PR body"
    expected: "The PR reports before/after medians and the 5% budget verdict"
    why_human: "ROADMAP SC4 / SAFE-06 say the cost is 'reported in the PR'; no PR exists yet"
---

# Phase 4: Fixes and Release Type Safety Verification Report

**Phase Goal:** In Release builds, a wrong-type argument from an untrusted script raises a Pattern 1 error instead of undefined behaviour. The C4/C6/C7/C8 bugs are fixed, `load` is text-only, and each fix lands red-then-green with its own test and CHANGELOG line.
**Verified:** 2026-10-03
**Status:** gaps_found (one small documentation gap; the code is done)
**Re-verification:** No, this is the initial verification

## Goal Achievement

The code delivers the goal. A Release `quiver_cli` probe written for this verification (not taken from the tests) got a Pattern 1 `got <type>` error at every class of site. It also confirmed C4/C6/C7/C8, text-only `load`, and the dot-call backstop. The one failure is bookkeeping: the Phase 5 baseline in STATE.md predates the review fixes.

### Observable Truths (ROADMAP Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Pattern 1 `... got <lua type>` for non-table to every table parameter (incl. `table_to_element` cells, `collect_group_columns`), non-string keys at the 4 map-key sites, and wrong types at the 8 optional sites. Tests are new, red-then-green, cover every site, and pass in Debug + Release | ✓ VERIFIED (CI matrix pending, see Human) | `internal.h` `require_table`/`lua_string_key`/`optional_from_lua`/`lua_type_error`. Every bound table parameter is `const sol::object&`: no `sol::table` parameter remains in a bound signature (grep). Release CLI probe results: `columns must be a table, got userdata` with the group left at 2 rows; `element_table must be a table, got number`; `attribute 'some_integer' must be a value or a table, got userdata`; `column 'value_int' must be an array of values, got userdata`; `attribute name must be a string, got number`; `column name must be a string, got boolean`; `paths must be a table, got number`; `params must be a table, got number` (nil still OK); `aggregate must be a boolean, got number`. Tests: `GroupWritersRejectNonTableColumns`, `TimeSeriesGroupWritersRejectNonTableColumns`, `Create/UpdateElementRejectsNonTableElement`, `RowAndFilesWriters*`, `ReadWriteRejectNonTableArguments`, `ReadRejectsNonStringDimensionName`, `OptionalArgumentsRejectWrongTypes`, `AggregateParameterRejectsWrongType`, `QueryRejectsWrongTypedParams`, `MetadataRejectsNonTableArguments`, `SelectAndRenameAgentsRejectNonTable`, `NonTableArgumentsReportTheirType`. Red runs are saved in `build/fixes-check/red/` (c1-groups, c1-rest, c2-keys, c5-optionals[-release]). |
| 2 | `db:transaction`/`db:dry_run` reject a non-function before any side effect; a COMMIT failure rolls back and rethrows; an `end_dry_run` failure surfaces; bytecode `load` errors and string `load` works | ✓ VERIFIED | `db_core.cpp:80-98` `run_in_scope`: the type check comes before `begin`, `finish` is inside the `try`, and the catch does a best-effort `abort` then rethrows. Probe: `fn must be a function, got number`, then `in_tx=false` and `in_dry=false`. Bytecode `load` returns nil with `attempt to load a binary chunk (mode is 't')`, and `load("return 41 + 1")()` returns 42. Since CR-01, `run()` is also text-only (`lua_runner.cpp:157`, `sol::load_mode::text`). Tests: `TransactionBlockRejectsNonFunction` (including inside an open transaction and the `__call` table), `DryRunBlockRejectsNonFunction`, `TransactionBlockCommitFailureRollsBack`, `ScopedBlockFinishErrorsStillSurface`, `LoadRefusesBinaryChunks`, `LoadStillAcceptsTextChunks`, `RunRefusesBinaryChunks`. All pass in Debug and Release. |
| 3 | `{col = {}}` clears; a misspelled empty column throws; create skips; lua-api.ts text and sync test; expression errors name the op; D1 nil branch, `lua_data_type_name` default and `apply_binop` throw are gone | ✓ VERIFIED | `db_write.cpp:65-68` passes `std::vector<int64_t>{}` to the core. Probe: `after_clear=0,1` (the vector group is cleared and the set is untouched); a typo throws `array 'valu_int' does not match any vector, set, or time series table ...`; `create_typo_empty => OK`. `lua-api.ts:309-315` has the rule, and the JS suite (sync test included) passes. `binary.cpp` `to_expression(o, operation)` plus `binop<Op>("add"/"gt"/...)`. Probe: `Cannot gt: ... got string` and `Cannot ifelse: ... got number` (leftmost operand, WR-01). `apply_binop`/`BinOp` do not appear in `src/` (grep). `lua_data_type_name` is a lowercase of `data_type_to_string` with no switch. The files decoder uses `lua_cell_as` and has no nil branch. Tests: `UpdateElementEmptyArray*` (4), `CreateElementSkipsEmptyArray`, `OperandErrorsNameTheOperation`, `OperandErrorsReportTheLeftmostBadOperand`. |
| 4 | `SOL_ALL_SAFETIES_ON=1` + `SOL_PRINT_ERRORS=0` PRIVATE on `quiver` after the explicit checks; `SOL_SAFE_FUNCTION=1` and its AGENTS claim deleted; stderr test; perf measured, 5% budget, fallback only `SOL_SAFE_GETTER=0`/`SOL_SAFE_STACK_CHECK=0` | ✓ VERIFIED (PR report pending) | `src/CMakeLists.txt:77-84`. `SOL_SAFE_FUNCTION=1` does not appear outside `.planning/` (grep), and there is no `SOL_SAFE_FUNCTION_CALLS=0`/`SOL_SAFE_USERTYPE=0`. `CaughtScriptErrorsWriteNothingToStderr` and `DotCallThrowsInsteadOfCrashing` pass. Probe stderr was empty, and `db.commit()` gives `sol: received nil for 'self' argument ...`. The perf tables are in 04-04-SUMMARY.md, and no perf script is in the diff. Note: 7e5eccd is no longer literally the phase's *last* commit, because the review fixes c666ced/b21f98d/bc38384 came after it. SAFE-06's own wording ("landing after SAFE-01..04") still holds, and none of the three fixes depends on or is masked by the backstop. |
| 5 | CHANGELOG `[0.13.0] — unreleased` + compare link; the required BREAKING/Fixed entries with no planning IDs; manifests at 0.13.0; nearest AGENTS.md updated (root sandbox says text-only `load`); six suites green; counts recorded as the Phase 5 baseline | ✗ PARTIAL (FAILED clause) | CHANGELOG lines 1-75 have every required entry: BREAKING for C1/C5, C7 (with the D-12 round trip and the shared-column/`date_time` edges), text-only `load` + `run()`, and the Release dot-call with the raw sol2 text; Fixed for C2, C4, C6, C8. The link is at line 1369. The phase diff adds no planning IDs (grep). All five manifests are at 0.13.0. Root `AGENTS.md:86-88`. **Six suites re-run in this verification at HEAD e6aa5c1: all PASS** (quiver_tests 1443, C API 543, Julia/Dart/JS/Python PASS). **FAILED clause:** STATE.md records 472 as the Phase 5 baseline, but HEAD runs 477 (see Gaps). |

**Score:** 4/5 truths verified (0 present-but-behavior-unverified)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/lua_runner/internal.h` | type-check helpers | ✓ VERIFIED | `lua_type_name`, `lua_type_error`, `require_table` (uses `get_type`), `lua_string_key`, `optional_from_lua<T>` (luaL_opt semantics) |
| `src/lua_runner/db_write.cpp` | `table_to_element`/`collect_group_columns` checks; empty array reaches the core | ✓ VERIFIED | `require_table(columns, caller, "columns")`, `"element_table"`, `std::vector<int64_t>{}` |
| `src/lua_runner/db_core.cpp` | `run_in_scope` fix; query params optional | ✓ VERIFIED | `lua_type_error(operation, "fn", "a function", fn_arg)`, `optional_from_lua<sol::table>` ×3 |
| `src/lua_runner/binary.cpp` | optional sites; operand naming | ✓ VERIFIED | `optional_from_lua<BinaryMetadata>` after `resolve_sandboxed_path`; `binop<Op>(name)`; operands decoded into locals in argument order |
| `src/lua_runner/lua_runner.cpp` | text-only `load` wrapper; `run()` text mode | ✓ VERIFIED | `safe_script` wrapper forcing `"t"`, installed right after the dofile/loadfile nil-out; `sol::load_mode::text` in `run()` |
| `src/CMakeLists.txt` | sol2 flags | ✓ VERIFIED | see Truth 4 |
| `CHANGELOG.md` | `[0.13.0]` section and link | ✓ VERIFIED | see Truth 5 |
| `.planning/STATE.md` | Phase 5 baseline | ✗ STALE | 472 recorded, 477 actual |

### Key Link Verification

| From | To | Via | Status |
|------|----|-----|--------|
| 6 group writers | `collect_group_columns` | `group_rows_from_lua`/`time_series_rows_from_lua` → `require_table(columns, ...)` | ✓ WIRED (the probe got the error through `update_vector_group` and `update_time_series_group`) |
| create/update/update_by_label/metadata_from_element | `table_to_element` | `require_table(values, caller, "element_table")` | ✓ WIRED |
| `open_file` | `optional_from_lua<BinaryMetadata>` | after containment, before `BinaryFile::open_file` | ✓ WIRED (order pins in `OptionalArgumentsRejectWrongTypes`) |
| transaction/dry_run lambdas | `run_in_scope` | `const sol::object& fn` | ✓ WIRED |
| Impl ctor | global `load` | `lua.safe_script` wrapper | ✓ WIRED (no `.set_function(`, so the sync test count is unaffected) |
| sol2 defines | every `src/lua_runner/` TU | `target_compile_definitions(quiver PRIVATE ...)` | ✓ WIRED |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Release type checks, C4/C6/C7/C8, load, dot-call (30 probes) | `build/release/bin/quiver_cli.exe --schema tests/schemas/valid/collections.sql p.db probe.lua` | Every case gave the expected Pattern 1 text; group intact after a userdata payload; `in_tx=false`; stderr empty; exit 0 | ✓ PASS |
| Lua* Debug | `build/bin/quiver_tests.exe --gtest_filter=Lua*` | 477/477, 12 suites | ✓ PASS |
| Lua* Release | `build/release/bin/quiver_tests.exe --gtest_filter=Lua*` | 477/477 | ✓ PASS |
| C API Lua runner Debug/Release | `quiver_c_tests --gtest_filter=LuaRunnerCApiTest*` | 27/27 both | ✓ PASS |
| Six suites (one run) | `scripts/test-all.bat` at e6aa5c1 | C++ 1443, C API 543, Julia, Dart, JS, Python all PASS | ✓ PASS (Dart hook caches were not deleted for this run, to avoid mutating state; the executor's 7e5eccd run did delete them) |

### Probe Execution

Step 7c: SKIPPED. The phase declares no `scripts/*/tests/probe-*.sh`, and none exist. The executor's `build/fixes-check/gate.sh` is a gitignored scratch harness, not a declared probe.

### Requirements Coverage

| Requirement | Source Plan | Status | Evidence |
|-------------|-------------|--------|----------|
| SAFE-01 | 04-01 | ✓ SATISFIED | Truth 1; all 20 parameter conversions are `sol::object`; value-level sites (userdata cell, column value) covered |
| SAFE-02 | 04-01 | ✓ SATISFIED | `lua_string_key` at `lua_table_to_value_map`, `table_to_element`, `lua_table_to_dim_map`, `update_time_series_files_lua` |
| SAFE-03 | 04-01 | ✓ SATISFIED | 8 sites: open_file md, bin_to_csv aggregate, file:read allow_nulls, aggregate, aggregate_agents, query_string/integer/float |
| SAFE-04 | 04-02 | ✓ SATISFIED | Truth 2 |
| SAFE-05 | 04-01, 04-02 | ✓ SATISFIED | Every message from `require_table`/`lua_string_key`/`optional_from_lua`/`run_in_scope` goes through `lua_type_error`. Messages still without the suffix (IN-01: `on_row`, option *value* checks, `option key must be a string`, the `collect_group_columns` key check, `target_label`, and the D-04 converter texts) are outside SAFE-01..04's sites, and CONTEXT D-04/D-16 and the 04-01 prohibitions deliberately keep them. They are not a SAFE-05 failure; they are a consistency debt (see Anti-Patterns). |
| SAFE-06 | 04-04 | ✓ SATISFIED | Truth 4 (PR report pending, see Human) |
| SAFE-07 | 04-03 (+CR-01 fix) | ✓ SATISFIED | Truth 2; both `load` and `run()` are text-only and tested |
| FIX-01 | 04-02 | ✓ SATISFIED | `TransactionBlockCommitFailureRollsBack` |
| FIX-02 | 04-02 (+WR-02 tests) | ✓ SATISFIED | Truth 3 |
| FIX-03 | 04-03 (+WR-01 fix) | ✓ SATISFIED | Truth 3 |

No orphaned requirements: REQUIREMENTS.md maps exactly these 10 IDs to Phase 4, and every plan's `requirements` field accounts for them.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/lua_runner/*` | — | No TBD/FIXME/XXX in phase-touched files | — | — |
| `tests/AGENTS.md` | 146 | Says "every other sol2 safety is on in both builds", but `SOL_SAFE_STACK_CHECK=0` is also set (IN-02) | ℹ️ Info | Doc inaccuracy; fold into Phase 5 DOC-02 |
| `CHANGELOG.md` | 34-44 | The empty-array entry omits `quiver.metadata_from_element` (an empty `labels`/`dimensions` now reports `Number of labels must be positive, got 0` instead of `missing array 'labels'`) (IN-03) | ℹ️ Info | A user-visible error-text change with no entry; fold into Phase 5 DOC-03/04 |
| `src/lua_runner/csv.cpp`, `db_core.cpp`, `internal.h`, `db_write.cpp` | csv 351/173/241, db_core 39, internal 270, db_write 107/189 | Type errors outside SAFE-01..04 still lack `got <type>` (IN-01) | ℹ️ Info | Two message shapes for type errors. A deliberate, documented scope cut (D-04/D-16), not a gap |
| `AGENTS.md` | 88 | Line runs past the wrap width ("...not bytecode. The enabled standard libraries are") | ℹ️ Info | Cosmetic |
| `src/lua_runner/db_core.cpp` | 85 | `fn(std::ref(self))` sits outside the `try` (IN-05, pre-existing) | ℹ️ Info | An OOM during the argument push would leave the scope open. Pre-existing and out of scope |

Getter audit (because `SOL_SAFE_GETTER=0` now applies to Debug too): every `.as<T>()`/`.get<T>()` in `src/lua_runner/` on a script-controlled value is preceded by a `get_type()`/`is<T>()` check or goes through `sol::optional<T>` (checked). The guarded sites are `binary.cpp:109,112,129,132`; `csv.cpp:61,64,161,175,353,377`; `db_core.cpp:23,83`; `db_write.cpp:63,114,117,120,154,191`; `return_json.cpp:130,157,159,201-211`; and the `internal.h` helpers. No unguarded site was found.

### Human Verification Required

1. **CI Release matrix.** Open the PR and confirm the CI Release matrix is green (ROADMAP SC1). Linux was last run locally at 7e5eccd, before the three review-fix commits; it should now show 476 run (475 pass + 1 root skip).
2. **Perf report in the PR.** Copy the 04-04-SUMMARY.md perf tables into the PR body (SAFE-06: "reported in the PR").

### Gaps Summary

There is one gap, and it is bookkeeping only. STATE.md's Phase 5 baseline (Lua* 472 / Linux 470 + 1 skip, at 7e5eccd) predates the code-review fixes, which added 5 Lua tests. The phase's actual resulting count is **477** (Windows Debug and Release, verified here). The other counts are C API 27, quiver_tests 1443 and quiver_c_tests 543. Phase 5 must reproduce "the count recorded at the end of Phase 4", so the recorded number must be corrected. The Linux number should be re-measured or marked as expected. No code change is needed. Every code-level must-have is verified, both by the test suite and by an independent Release CLI probe.

---

_Verified: 2026-10-03_
_Verifier: Claude (gsd-verifier)_

## Gap Closure and Human Verification Resolution

Resolved by the orchestrator at 2026-10-03T15:37:09Z, per the maintainer's standing "you check that" instruction (memory: self-verify-checkpoints).

**Gap 1 (criterion 5: stale Phase 5 baseline): closed.** STATE.md now records `Lua*` = 477 / 12 suites (Windows Debug
and Release) at `e6aa5c1`. Linux was re-run from scratch at `e6aa5c1` (`git archive` into ubuntu:24.04): GCC 13.3 and
Clang 18.1.3/libc++ both ran `Lua*` 475 (474 pass + 1 root skip), with `LuaRunnerCApiTest` 27/27, full `quiver_tests` 1440
(1437 + 3 skip) and `quiver_c_tests` 543/543. That is 475, not the 476 estimated above: 477 minus the 2 `_WIN32`-only
`DeviceNamePathIsReportedWithPrefix` tests. No code change was needed.

**Human item 1 (CI Release matrix): passed, delegated (Claude).** Covered by the Linux re-run above (both toolchains,
after the review fixes), the Windows Release `Lua*` 477 run and the six-suite run. The residual is Apple Clang, the same one
Phases 2 and 3 accepted; PR CI confirms it.

**Human item 2 (perf tables in the PR body): ship-time action.** The tables from 04-04-SUMMARY are recorded in STATE.md as
a PR note so that `/gsd-ship` / the PR author copies them in. It is a reporting step, not a verification.
