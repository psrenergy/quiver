---
phase: 06-quiver-file-layout
verified: 2026-10-04T14:45:00Z
status: passed
score: 5/5 roadmap success criteria verified (plus all plan must-haves)
behavior_unverified: 0
overrides_applied: 0
---

# Phase 6: Quiver File Layout Verification Report

**Phase Goal:** Each Lua name is registered in the `src/lua_runner/` file named after the core file that implements its C++ method, so a change to a core file has an obvious Lua counterpart, and no script can tell that anything moved.
**Verified:** 2026-10-04
**Status:** passed
**Re-verification:** No, this is the initial verification.

Base is `21ba6f8`. `git diff da6f67b 21ba6f8 -- src tests bindings include` is empty, so the roadmap's `da6f67b` reference and the base are the same for every compared artifact. The golden and name baselines were captured at 12:57-12:58 and the first phase commit `8fb84cc` was made at 13:06, so they were taken before any source change.

## Goal Achievement

### Observable Truths (Roadmap Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | The folder holds exactly 19 files, no `db_*.cpp`, each listed in `QUIVER_SOURCES`, none over ~450 lines; names are placed after the core files; `return_json`/`path_policy` diffs are empty; `lua_runner.cpp`/`internal.h` change only binder and shared-helper lines | VERIFIED | `ls src/lua_runner` gives 19 files; the largest is `csv.cpp` at 416 lines. The CMake block lists the 19 files in byte order. I mapped every `bind.set_function` name to the `src/database*.cpp` file that defines `Database::<name>(`: every mapped name sits in the Lua file of the same name, and the unmapped ones are composites, `transaction`/`dry_run`, csv and binary. `git diff 21ba6f8` is empty for return_json and path_policy. The `lua_runner.cpp` diff touches only `lua_internal::bind_` lines. The `internal.h` diff adds the binder declarations, the `metadata_to_lua` declarations, the `list_/get_metadata_lua` templates and the `parse_csv_options` declaration. |
| 2 | 14 binders called in the LAYOUT-02 order; `bind_binary` returns the `sol::usertype<BinaryFile>` passed to `bind_expression`; constructor order is kept; one `new_usertype<Database>` and one `open_libraries(` | VERIFIED | `lua_runner.cpp:113-126` calls database, create, read, update, delete, describe, metadata, query, time_series, csv_export, csv_import, csv, binary, expression. `auto binary_file_type = lua_internal::bind_binary(...)` is passed to `bind_expression`. Grep finds 1 `new_usertype<Database>` (lua_runner.cpp:112) and 1 `open_libraries(` (:92). The five usertypes are each registered once. |
| 3 | No observable change: 86 `set_function` (71 bind + 15 ns), sorted names identical to the base, all test counts pass, binding suites pass, golden output byte-identical in Debug and Release | VERIFIED | I derived the name list from `git archive 21ba6f8` myself and it matches HEAD: 86 entries, no duplicates. Per file: 71 bind and 15 ns. My re-runs: `gate.sh` printed `GATE PASS lua=477 pairs=36` (Lua* 477/12 suites, LuaRunnerCApiTest 27, SandboxedPathTest 11, lua-api sync 6 pass, `GOLDEN debug OK`). `phase_gate.sh` printed `PHASE GATE PASS lua=477 tidy=14 gcc=ok` (quiver_tests 1454 and quiver_c_tests 543 in Debug and Release, `GOLDEN release OK`). `scripts/test-all.bat` reported all six suites PASS: Julia, Dart 444, JS 241, Python 350. `git diff 21ba6f8 -- tests` is empty. |
| 4 | NOLINT pair per file with by-value sol2 args; clang-format clean; tidy within the 14-warning baseline; rename-only files moved with `git mv` | VERIFIED | Four NOLINT pairs (csv, time_series, update, expression), with at most one per file and balanced. Tidy reports `performance-unnecessary-value-param` 0, so no uncovered file. The tidy (check, line) pairs are identical to the base, 14 warnings. clang-format 22.1.8 `--dry-run --Werror` passes inside gate.sh. Commit `818eb95` contains only the three 100% renames plus the CMake lines. `git log --follow` reaches `da6f67b` for database_read, database_metadata, database_time_series and database_update. |
| 5 | Old-name grep goes from 24 to 0; root and `src/` AGENTS.md list the new files; the `lua-api.ts` header names `bind_database` through `bind_expression` | VERIFIED | `git grep` at `21ba6f8` returns 24 lines and at HEAD returns 0. The `src/AGENTS.md` file map lists the 14 binder files in call order, and the Layout bullet is updated. Root AGENTS.md cites `binary.cpp` + `expression.cpp` and the new layout. The `lua-api.ts:4` header now names `bind_database` through `bind_expression`. Every cited `lua_runner/*` file exists, which phase_gate.sh checks. |

**Score:** 5/5 roadmap truths verified (0 present, behavior-unverified)

The 06-01/02/03 plan must-have truths are covered by the rows above plus these checks:
- `metadata_to_lua` is declared in internal.h and defined once in `database_metadata.cpp`. The templates exist once, in internal.h.
- `parse_csv_options` is defined in `database_csv_export.cpp` and declared in internal.h. `table_to_element` is in `database_create.cpp`.
- The per-file counts match the plan (database 13, create 1, read 15, update 8, delete 2, describe 3, metadata 6, query 3, time_series 12, csv_export 1, csv_import 1, csv 3, binary 3+3, expression 12 ns).
- `build/build.ninja` and `build/release/build.ninja` contain no `lua_runner/db_` (phase gate).
- The BinaryFile runtime metatable still has `__lt/__le/__eq/__add/__unm/__band/__bor/__bnot`, and `surface.txt` is byte-identical to the base in Debug and Release.
- `binary.cpp:8-10` keeps the `expression.h` include with its explanatory comment.
- Bisectability: I extracted each of the 8 non-doc commits with `git archive` and checked it statically. At every commit the registered names equal the base, the CMake lua_runner block equals the file list, and binder calls equal binder definitions. The SUMMARY records a per-commit `GATE PASS`. I did not rebuild each intermediate commit, because that would need checking out and modifying the shared tree.
- The backstop truth (an interrupted phase cannot pass) holds: `phase_gate.sh` runs the old-name grep and the cited-file check on the final tree.

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/lua_runner/database.cpp` | bind_database, run_in_scope | VERIFIED | 94 lines, 13 names |
| `src/lua_runner/database_create.cpp` | bind_create, table_to_element | VERIFIED | 102 lines, 1 name |
| `src/lua_runner/database_read.cpp` | bind_read incl. number_of_elements | VERIFIED | git-mv rename; 15 names |
| `src/lua_runner/database_update.cpp` | bind_update + group decoder | VERIFIED | git-detected rename of db_write; 8 names |
| `src/lua_runner/database_delete.cpp` | bind_delete | VERIFIED | 2 names |
| `src/lua_runner/database_describe.cpp` | bind_describe | VERIFIED | 3 names |
| `src/lua_runner/database_metadata.cpp` | bind_metadata + metadata_to_lua | VERIFIED | 6 names |
| `src/lua_runner/database_query.cpp` | bind_query | VERIFIED | 3 names |
| `src/lua_runner/database_time_series.cpp` | bind_time_series incl. metadata pair | VERIFIED | 12 names |
| `src/lua_runner/database_csv_export.cpp` / `_import.cpp` | export_csv / import_csv | VERIFIED | 1 name each |
| `src/lua_runner/binary.cpp` | bind_binary returns usertype | VERIFIED | `sol::usertype<BinaryFile> bind_binary(` |
| `src/lua_runner/expression.cpp` | bind_expression | VERIFIED | 12 ns names, operators on Expression + BinaryFile |
| `src/lua_runner/internal.h` | 14 declarations in order | VERIFIED | Order checked by phase_gate.sh |
| `src/AGENTS.md`, `AGENTS.md`, `bindings/js/src/lua-api.ts` | New layout described | VERIFIED | See truth 5 |

### Key Link Verification

| From | To | Via | Status |
|------|----|-----|--------|
| lua_runner.cpp | every binder file | `lua_internal::bind_*` calls (14) | WIRED |
| lua_runner.cpp | binary.cpp → expression.cpp | `auto binary_file_type = bind_binary(...)`, passed to `bind_expression` | WIRED |
| src/CMakeLists.txt | all 19 files | explicit `QUIVER_SOURCES` entries | WIRED |
| database_csv_import.cpp | database_csv_export.cpp | `parse_csv_options` declared in internal.h | WIRED |
| database_update.cpp | database_create.cpp | `table_to_element` declared in internal.h | WIRED |
| database_time_series.cpp | internal.h templates | `list_metadata_lua<&Database::list_time_series_groups>` | WIRED |
| expression.cpp | path_policy.cpp | `resolve_sandboxed_path` for expr:save (the 10 sandboxed operations are unchanged per gate.sh) | WIRED |

### Data-Flow Trace (Level 4)

Not applicable. This phase is a source move with no new data paths. The runtime surface and golden probes stand in for it: they are byte-identical to the base.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Per-commit gate on HEAD | `bash build/layout-check/gate.sh` | `GATE PASS lua=477 pairs=36`, `GOLDEN debug OK`, novel lines 44 (all scaffolding) | PASS |
| Phase gate | `bash build/layout-check/phase_gate.sh` | `PHASE GATE PASS lua=477 tidy=14 gcc=ok` | PASS |
| All six suites | `scripts/test-all.bat` | C++, C API, Julia, Dart, JS, Python all PASS | PASS |
| Names vs base (independent of harness) | `git archive 21ba6f8` + set_function extraction | 86 = 86, diff empty, harness `names-base.txt` matches | PASS |

### Probe Execution

None declared. The phase uses the `build/layout-check` harness rather than `scripts/*/tests/probe-*.sh`. The harness was re-run in this verification (see above).

### Requirements Coverage

| Requirement | Source Plan | Status | Evidence |
|-------------|-------------|--------|----------|
| LAYOUT-01 | 06-01, 06-02, 06-03 | SATISFIED | Truth 1 |
| LAYOUT-02 | 06-01, 06-02, 06-03 | SATISFIED | Truth 2 |
| LAYOUT-03 | 06-01, 06-02, 06-03 | SATISFIED | Truth 3 |
| LAYOUT-04 | 06-01, 06-02, 06-03 | SATISFIED | Truth 4 |
| LAYOUT-05 | 06-03 | SATISFIED | Truth 5 |

No orphaned requirements. REQUIREMENTS.md maps exactly LAYOUT-01..05 to Phase 6.

### Prohibitions (judgment tier)

| Prohibition | Verdict | Evidence |
|-------------|---------|----------|
| No green by moving the gates (tests, counts, baselines, GOLDEN_CHANGE) | Held | `git diff 21ba6f8 -- tests` is empty. The baselines' mtimes (12:57-13:00) predate the first code commit (13:06). Golden passes without the override env vars. |
| No cleanup inside moved code | Held | All 44 novel lines in `novel-lines.txt` are includes, binder signatures/declarations, binder calls, `return binary_file_type;`, or the 2-line include comment. No logic line is new, and the tidy pairs are unchanged. |
| No change to BinaryFile/Expression comparison behaviour (EQ-01) | Held | The `surface.txt` metatable keys and the golden binary probe are byte-identical to the base. |

The 06-03 `<human-check>` asks for a review of `novel-lines.txt`. I reviewed it myself and every line is an allowed kind, so it is resolved.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (none) | — | TBD/FIXME/XXX/TODO/HACK | — | grep over `src/lua_runner` returns nothing |
| `src/lua_runner/binary.cpp` / `src/AGENTS.md:666-670` | 8-10 | WR-01 (review): sol2 automatic `__eq/__lt/__le` on BinaryFile/Expression always return true | Info (out of scope) | This predates the phase and was deliberately preserved. It is deferred to the EQ-01 decision. No success criterion is violated. |
| `src/AGENTS.md` | 656-657 | IN-01 (review): "order of the core files they mirror" doesn't match the core listing order | Info | The call order is the one LAYOUT-02 prescribes. Only the doc wording is slightly loose. |

### Human Verification Required

None.

### Gaps Summary

None. I checked every roadmap success criterion and every plan must-have against the code, the git history and fresh gate/test runs, not against SUMMARY claims. WR-01 is pre-existing behaviour that the phase was required to preserve, so it is not a gap here.

---

_Verified: 2026-10-04_
_Verifier: Claude (gsd-verifier)_
