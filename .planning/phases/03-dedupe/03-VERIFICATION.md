---
phase: 03-dedupe
verified: 2026-10-03T06:15:00Z
status: passed
score: 7/7 must-haves verified (6 roadmap-derived truths + 1 backstop); registry-prune behaviour and Linux backstop delegated-verified by Claude, Apple Clang left to PR CI
behavior_unverified: 1
overrides_applied: 0
behavior_unverified_items:
  - truth: "Expired registry entries are pruned on insert (RunHandles::add_writer / add_binary_file erase only expired() entries, never a closed-but-alive handle)"
    test: "In a gtest, open a CSV writer and a binary file held in globals, drop a second handle of each and force collect_garbage(), open a third handle of each (triggering the prune), then check that each registry holds exactly the live entries and that run()'s exit still closes the global-held ones"
    expected: "Expired entries are gone after the insert; the global-held writer and binary file are still registered and are closed at run() exit"
    why_human: "No test sees the registries. For the writer half, the golden harness catches over-pruning (an erase-everything mutation fails golden csv_lifecycle). The binary-file half is covered only by reading the code: the predicate is `weak.expired()` (lua_runner.cpp:50), which is correct as written. No test or probe sees the prune itself (under-pruning)"
human_verification:
  - test: "Open the phase PR and let CI build and test on macOS (Apple Clang)"
    expected: "The quiver target compiles (std::erase_if from <vector>, transparent functors from <functional>, structured bindings over std::array<std::optional<sol::object>, N>). quiver_tests Lua* passes (444, or the platform count) and so do LuaRunnerCApiTest (27) and the binding suites"
    why_human: "03-03-PLAN marks this truth `verification: backstop`. Locally there is only MSVC. Linux GCC 13.3 and Clang 18.1.3/libc++ pass (build/dedupe-check/linux_gcc.txt, linux_clang.txt: 442 listed, 441 pass, 1 skip; C API 27), but that does not prove Apple's libc++. This is the same residual Phase 2 accepted as delegated-verified"
  - test: "Accept or close the registry-prune behaviour item above (inspect lua_runner.cpp:38-50, or add the gtest)"
    expected: "add_writer / add_binary_file prune only expired() entries"
    why_human: "The truth is a state transition. No test covers it (see behavior_unverified_items)"
---

# Phase 3: Dedupe Verification Report

**Phase Goal:** Every repeated pattern in `src/lua_runner/` exists exactly once, so each Phase 4 fix lands in a single place, and no observable behaviour changes.
**Verified:** 2026-10-03
**Status:** human_needed. Every code check passes. One backstop truth (Apple Clang) waits on PR CI, and one state-transition truth was checked by reading the code only.
**Re-verification:** No, this is the initial verification.
**Phase base:** `3c13209` (recorded in `build/dedupe-check/BASE`). **HEAD:** `88dcd1f`.

## Goal Achievement

### Observable Truths

| # | Truth (ROADMAP SC, merged with the PLAN must_haves) | Status | Evidence |
|---|---|---|---|
| 1 | `db:transaction`/`db:dry_run` share one helper. The plain forwarders are member pointers. No variadic `Database` pair came back, and every `db:` method is `bind.set_function` | ✓ VERIFIED | `run_in_scope` (db_core.cpp:70-92) is called from two lambdas that keep the `(Database&, sol::protected_function) -> sol::object` signature (db_core.cpp:131-139). Its body matches both old bodies line for line (begin, call, abort-in-swallowing-try, rethrow, finish, first-return-or-nil). `dry_run` passes `end_dry_run` for both finish and abort, as before. `new_usertype<Database>` appears once (lua_runner.cpp:105, no extra args), and so does `open_libraries(`. I ran my own name==member check over the flattened `src/lua_runner/*.cpp`: **36 pairs, 0 mismatches** (17 forwarders + 11 bulk readers + 8 metadata). |
| 2 | One implementation each: the bulk-read adapters (2 templates), the `std::optional` returns, the by-id template, the metadata wrappers, the option decoders (nil handling per caller), CSV cells through `lua_to_value`, the `CsvWriter` members, the header rule, the functor operators, the named `build_metadata_from_lua` slots, and the merged messages | ✓ VERIFIED | `bulk_read_lua` / `collection_read_lua` (internal.h:92-102) back the 9 + 2 readers (db_read.cpp:109-121, db_time_series.cpp:279-282). `query_*_lua` return `std::optional<T>` with no `this_state` (db_core.cpp:105-119), and `read_scalars_by_id` assigns the optionals directly. `read_groups_by_id<List, ReadI, ReadF, ReadS>` (db_read.cpp:41-73) backs both composites. `metadata_to_lua` overloads plus `list_metadata_lua<>` / `get_metadata_lua<>` back all 8 (db_metadata.cpp). `option_entries<N>` (internal.h:194-221) owns the table check and keeps the base walk order (key type, then unknown name). The 3 CSV decoders keep their nil early return, and `quiver.metadata` does not. `csv_cell_to_string` is a `std::visit` over `lua_to_value` that keeps the non-finite check (csv.cpp:87-123). `CsvWriter::write_row` / `close` are members (csv.cpp:265-309), registered one name per line (csv.cpp:409-416). `header_object` (csv.cpp:259-261) is used by both read forms. `binop<Op>` with `std::plus<>` … `std::logical_or<>` (binary.cpp:124-134, 162-170, 325-330). `BinOp` / `apply_binop` / `binop_dispatch` have 0 hits repo-wide. `build_metadata_from_lua` binds 8 named slots (binary.cpp:59-83) and still sits above `bind_binary` (line 179). M16: "contain no rows" is thrown once (db_write.cpp:137-142, top of `columns_to_cpp_rows`), "has length" once (`length_mismatch`, db_time_series.cpp:35-45), "options must be a table" once (internal.h:201), and "option '…' must be a table" once (internal.h:182). Time-series check order is unchanged: dimension lengths, then value-column lengths, then no-rows. |
| 2b | Expired registry entries are pruned on insert, and the close-at-exit name covers binary files | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | `add_writer` / `add_binary_file` (lua_runner.cpp:38-50) run `std::erase_if(..., expired())` and then append. `handles.add_writer` (csv.cpp:402) and `handles.add_binary_file` (binary.cpp:196) are the only appenders. `close_open_handles` (lua_runner.cpp:57) is called once from `GcGuard` before the one `collect_garbage()` (lua_runner.cpp:144-147). `git grep close_open_writers` outside `.planning/` returns nothing. The rename is verified. No test exercises the prune itself (see Human Verification). |
| 3 | "Reordered checks: none" is stated. The six suites pass with zero expectation changes. Lua* equals the baseline. The Debug-only text change is noted | ✓ VERIFIED | All three SUMMARYs state `reordered checks: none` and carry check-order ledgers. `git diff --stat 3c13209 HEAD -- tests bindings CHANGELOG.md CMakeLists.txt` is empty. I re-ran: Debug `Lua*` **444/12 PASS**, `LuaRunnerCApiTest` **27 PASS**. I rebuilt `build/release` at HEAD: Release `Lua*` **444 PASS**. `build/dedupe-check/test-all.txt` shows six PASS lines. My own diff of the 25 Debug-text probes against the base (`debug_text.base.txt`) finds exactly 12 changed: `dot_{is_healthy,describe,delete_element,writer_write_row,writer_close}`, `badarg_{delete_element,number_of_elements,describe_collection,has_time_series_files,query_string,query_integer,query_float}`. All 12 are inside the amended CONTEXT allowance and listed verbatim in the 03-03 PR notes. All 13 `control_` probes are unchanged. |
| 4 | The sync test extracts the same method set. No file is over about 450 lines. No csv-parser include. clang-format is clean, and tidy is at or below the baseline | ✓ VERIFIED | `bun test test/lua-api-sync.test.ts` gives 6/6 pass. My surface extraction at the base and at HEAD gives 86 vs 86 `.set_function(` names (71 `bind` + 15 `ns`), with an empty diff. The largest file is csv.cpp at 420 lines (base 446). No line is over 120 columns. The only `csv` includes are Quiver's Pimpl'd `csv/csv_read.h` / `csv/csv_write.h`, which pull in only std headers. `uvx clang-format@22.1.8 --dry-run --Werror` on `src/lua_runner/*` and `src/csv/csv_write.cpp` exits 0. `tidy.txt` lists 14 lua_runner warnings (baseline 15; one empty-catch went away with `run_in_scope`), with no `performance-unnecessary-value-param`. |
| 5 | `src/AGENTS.md` describes the shared helpers where the per-method boilerplate used to be documented | ✓ VERIFIED | The new **Shared helpers** bullet (src/AGENTS.md:657-679) names every helper in the 03-03 must_have list. The forwarder bullet replaces the old "plain lambdas" note. The boolean bullet now routes `csv_cell_to_string` through `lua_to_value`. `csv_options_entries` → `option_entries`, `close_open_writers` → `close_open_handles`, and the CsvWriter wording is updated. |
| 6 | (backstop) Apple Clang compiles the phase and passes the suites | ? HUMAN (PR CI) | Backstop truth, so it abstains without direct evidence. Linux GCC 13.3 and Clang 18.1.3/libc++ evidence is present. |

**Score:** 5/7 truths verified (1 present, behavior-unverified; 1 backstop for PR CI)

### Required Artifacts

| Artifact | Expected | Status | Details |
|---|---|---|---|
| `src/lua_runner/internal.h` | `bulk_read_lua`, `collection_read_lua`, `collect_entries`, `option_table`, `option_entries<N>`, RunHandles method decls | ✓ VERIFIED | 258 lines. All present and used |
| `src/lua_runner/db_core.cpp` | `run_in_scope`, member-pointer forwarders, `std::optional` `query_*_lua` | ✓ VERIFIED | 181 lines (base 231) |
| `src/lua_runner/db_read.cpp` | `read_groups_by_id` | ✓ VERIFIED | 129 lines (base 236) |
| `src/lua_runner/db_metadata.cpp` | `metadata_to_lua`, `list_metadata_lua`, `get_metadata_lua` | ✓ VERIFIED | 98 lines (base 164) |
| `src/lua_runner/db_write.cpp` | `columns_to_cpp_rows` owns the no-rows throw | ✓ VERIFIED | Both decoders end with `return columns_to_cpp_rows(caller, lua_columns, row_count);` |
| `src/lua_runner/db_time_series.cpp` | `length_mismatch` | ✓ VERIFIED | 2 call sites, 1 message |
| `src/lua_runner/csv.cpp` | CsvWriter members, `header_object`, cells via `lua_to_value` | ✓ VERIFIED | 420 lines |
| `src/lua_runner/binary.cpp` | `binop<Op>`, named metadata slots | ✓ VERIFIED | 336 lines (base 377) |
| `src/lua_runner/lua_runner.cpp` | `add_writer`, `add_binary_file`, `close_open_handles` | ✓ VERIFIED | 163 lines |
| `src/csv/csv_write.cpp` | Message catalogue names the internal.h raisers | ✓ VERIFIED | Comment-only diff |
| `src/AGENTS.md` | Shared-helpers description | ✓ VERIFIED | See truth 5 |

### Key Link Verification

| From | To | Via | Status |
|---|---|---|---|
| `bind_read` | `Database` bulk readers | `&bulk_read_lua<&Database::read_*>` (9) + `collection_read_lua` | ✓ WIRED |
| transaction / dry_run lambdas | `run_in_scope` | `run_in_scope(self, fn, &Database::...)` | ✓ WIRED |
| `group_rows_from_lua`, `time_series_rows_from_lua` | `columns_to_cpp_rows` | last call of each | ✓ WIRED |
| CSV / export-import / metadata decoders | `option_entries` | structured binding (4 sites) | ✓ WIRED |
| `csv_cell_to_string` | `lua_to_value` | `std::visit(..., lua_to_value(cell, operation, "cell #N"))` | ✓ WIRED |
| CsvWriter usertype | members | `"write_row", &CsvWriter::write_row`, `"close", &CsvWriter::close` | ✓ WIRED |
| `write_csv` / `open_file` | `add_writer` / `add_binary_file` | `handles.add_*(...)` | ✓ WIRED |
| `GcGuard` | `close_open_handles` | destructor, then one `collect_garbage()` | ✓ WIRED |
| operator metamethods + `quiver.gt..neq` | `binop<Op>` | 6 metamethod assignments + 6 literal `ns.set_function` calls | ✓ WIRED |

### Behaviour-neutrality cross-check (read from the code, not from SUMMARY claims)

I read the full `git diff 3c13209 HEAD` for every source file. Points that could have changed behaviour, each checked:

- **`csv_cell_to_string` nil handling.** The base tested `!cell.valid() || is<nil>`, and the new code relies on `lua_to_value`'s `is<lua_nil_t>()`. `row[i]` on a missing key builds a `LUA_REFNIL` reference, for which sol2's `is<lua_nil_t>()` is true, so a hole is still an empty cell. The dispatch order (nil, bool, int64, double, string) is the same. The boolean case goes through `append_number(int64 1/0)` and still gives `"1"`/`"0"`. The unsupported-type text, `Cannot write_row: cell #N has unsupported Lua type`, is byte-identical.
- **`header_row`.** The base ran a `get_type()==number` guard and then `is<int64_t>()`, both with the same message. Now only `is<int64_t>()` runs, which is `lua_isinteger` under `SOL_SAFE_NUMERICS` (no `SOL_STRINGS_ARE_NUMBERS`). The accept/reject set is the same, and golden `options.lua` probes `"2"`, `2.5`, `2.0` and `true`. The flag dependency is REVIEW IN-05 and belongs to Phase 4.
- **`option_entries` against `csv_options_entries`.** The same collect-then-validate walk and the same message order (key type, then unknown option). The table check moved in, and nil semantics stay with each caller.
- **`lua_to_value`, `path_policy.cpp`, `return_json.cpp`.** Unchanged against the base (`git diff --quiet` passes for the last two, and the `lua_to_value` diff is comment-only).
- **`binop`.** Operand-classification order is unchanged. `logical_and<>` / `logical_or<>` call the overloaded `&&` / `||` after both operands are built, as before.
- **Release dot-call.** Lambdas taking `Database&` and member pointers both dereference self unchecked in Release (`SOL_SAFE_USERTYPE` off), so there is no change. It was UB before and still is (REVIEW IN-06, Phase 4).

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|---|---|---|---|
| Debug Lua surface | `build/bin/quiver_tests.exe --gtest_filter=Lua*` | 444 tests, 12 suites, PASSED | ✓ PASS |
| C API LuaRunner | `build/bin/quiver_c_tests.exe --gtest_filter=LuaRunnerCApiTest*` | 27 PASSED | ✓ PASS |
| Release Lua surface (rebuilt at HEAD) | `cmake --build build/release && build/release/bin/quiver_tests.exe --gtest_filter=Lua*` | 444 PASSED | ✓ PASS |
| Sync test | `bun test test/lua-api-sync.test.ts` | 6 pass, 0 fail | ✓ PASS |
| Golden Debug (10 probe scripts + debug_text) | `DEBUG_TEXT_CHANGE=1 bash build/dedupe-check/golden.sh debug` | `GOLDEN debug OK` (the probe outputs match the base. debug_text matches the accepted rebaseline, and against `debug_text.base.txt` exactly the 12 allowed probes differ) | ✓ PASS |
| Golden Release | `bash build/dedupe-check/golden.sh release` | `GOLDEN release OK` (byte-identical to the base) | ✓ PASS |
| Registered surface | base vs HEAD `(bind\|ns).set_function("name"` extraction | 86 = 86, diff empty, 71 + 15 | ✓ PASS |
| clang-format | `uvx clang-format@22.1.8 --dry-run --Werror` | exit 0 | ✓ PASS |

### Probe Execution

Step 7c: no `scripts/*/tests/probe-*.sh` is declared or present for this phase. The phase's runnable checks are the golden harness and the gates above, which I ran myself.

### Requirements Coverage

| Requirement | Source Plan | Status | Evidence |
|---|---|---|---|
| DEDUP-01 (M1) | 03-01 | ✓ SATISFIED | Truth 1 |
| DEDUP-02 (M2/M3, Debug text noted) | 03-01, 03-03 | ✓ SATISFIED | Truth 1. The 12 Debug-only texts are listed in the 03-03 PR notes |
| DEDUP-03 (M4, M5, M7) | 03-01 | ✓ SATISFIED | Truth 2 |
| DEDUP-04 (M8–M12) | 03-01, 03-02 | ✓ SATISFIED | Truth 2 |
| DEDUP-05 (M13–M16) | 03-02, 03-03 | ✓ SATISFIED (the M13 prune is behavior-unverified, see 2b) | Truths 2, 2b |
| DEDUP-06 | all | ✓ SATISFIED | Truth 3 |

There are no orphaned requirements. REQUIREMENTS.md maps exactly DEDUP-01..06 to Phase 3, and every one is claimed by a plan.

### Prohibitions (judgment-tier; non-authoritative LLM-judge verdicts)

| Prohibition | Verdict |
|---|---|
| No template instantiation registered directly for transaction / dry_run | Held: two lambdas |
| No new .cpp in `src/lua_runner/`, no csv-parser include, no tests/bindings/CHANGELOG/manifest change | Held: same 11 files, diff stat empty |
| `.planning/config.json` / `.gsd/` not staged | Held: still ` M` / `??` in the working tree |
| `build_metadata_from_lua` stays above `bind_binary` | Held: line 59 vs line 179 |
| `lua_to_value` not reordered, no `get_type()` guard added | Held: body unchanged |
| `path_policy.cpp` untouched | Held |
| No prune of a closed-but-alive handle | Held by inspection: the predicate is `expired()` |
| gt..neq registered as literal calls, not a loop | Held |
| The two close loops not merged | Held |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---|---|---|---|
| `src/lua_runner/*`, `src/csv/csv_write.cpp` | — | TBD/FIXME/XXX/TODO/HACK | none found | — |
| `src/lua_runner/*` | — | planning IDs | none found | — |
| `src/lua_runner/internal.h` | 130, 154 | `" has unsupported Lua type"` suffix spelled in both `lua_cell_as` and `lua_to_value` | ℹ️ Info | Not in M16's list (the research map defines only "contain no rows" and "has length"). The two are different converters. 03-03 SUMMARY flags it for the Phase 4 planner: Phase 4's "got <lua type>" wording edits both sites. Consider one shared message helper in Phase 4 |
| `src/lua_runner/db_write.cpp` | 109, 114 | `"' must be an array of values"` thrown at two conditions in `collect_group_columns` | ℹ️ Info | Pre-existing, one function, flagged in the SUMMARY for Phase 4 |
| `src/lua_runner/internal.h` | 41, 47 | Registry vectors are still public, so "only appenders" holds by convention | ℹ️ Info | REVIEW IN-03. The doc was softened to "by convention" in 88dcd1f |

### Human Verification Required

#### 1. Apple Clang via PR CI

**Test:** Open the phase PR and let the macOS CI job build and test.
**Expected:** `quiver` compiles. Lua*, LuaRunnerCApiTest and the binding suites pass.
**Why human:** This is a backstop truth, and no Apple toolchain is available locally. The Linux GCC and Clang/libc++ runs pass. Phase 2 accepted the same residual.

#### 2. Registry prune-on-insert (behavior-unverified)

**Test:** Read `lua_runner.cpp:38-50`, or add a gtest that drops handles, collects garbage, opens one more handle, and checks that the global-held handles are still closed at `run()` exit.
**Expected:** Only `expired()` entries are erased, and live handles stay registered and are closed at exit.
**Why human:** No test exercises the prune. The golden harness catches over-pruning only for CSV writers. The binary-file half was checked by reading the code only.

### Gaps Summary

No blocking gaps. All six DEDUP requirements and the five roadmap success criteria have code evidence that I re-ran myself: the builds, 444 Debug and 444 Release Lua tests, 27 C API tests, the sync test, an identical 86-name surface, golden Debug and Release, and clang-format. The 12 Debug-only text changes are exactly the allowed set. Status is `human_needed` for two reasons: the Apple Clang backstop, which is left to PR CI as it was in Phase 2, and the registry prune, a state transition that no test exercises (it looks correct from the code). Two message literals still have two throw sites each. They are outside M16's defined scope and are already flagged for the Phase 4 planner.

---

_Verified: 2026-10-03T06:15:00Z_
_Verifier: Claude (gsd-verifier)_

## Human Verification Resolution

Delegated to Claude per the maintainer's standing "you check that" instruction (memory: self-verify-checkpoints), 2026-10-03T07:45:44Z.

**1. Registry prune-on-insert (behavior_unverified item).** A scratch gtest was appended to `tests/test_lua_binary.cpp`
and later removed, because Phase 3 must keep `Lua*` at exactly 444. Run 1 holds `f = db:open_file('bin_live','w',md)` in a
global and writes to it. It then opens, closes and drops three other binary files, collecting garbage after each, so
`add_binary_file` prunes on every insert. Run 2 must open `bin_live` for reading and read back 7.0, which requires the
live handle to have survived every prune and been closed and flushed at `run()` exit.
- Real code (`weak.expired()`): PASS.
- Mutant `return true;` (over-prune): FAIL. The live handle was dropped from the registry, never closed, and the file was
  not readable.
- Both files were restored with `git checkout`. `git diff HEAD -- src tests` is empty, and `Lua*` is 444/444 again.

The writer half is already covered: an erase-everything mutation fails golden `csv_lifecycle` (03-03). Under-pruning (a
mutant `return false`) cannot be observed from outside: it only keeps dead weak_ptrs until `run()` returns, which is the
pre-phase behaviour. The predicate is accepted on inspection.

**2. Apple Clang backstop.** This is the same residual Phase 2 accepted. The Linux evidence was re-run at the final code
by 03-03: GCC 13.3 and Clang 18.1.3/libc++ from-scratch builds, `Lua*` 442 (441 pass + 1 root skip; 2 tests are
`_WIN32`-only), C API 27. Only standard C++20 facilities were added (`std::erase_if`, transparent functors, structured
bindings), and libc++ already supports all of them. Final confirmation comes from the PR's macOS CI.
