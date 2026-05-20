---
phase: "03"
fixed_at: 2026-05-20T13:15:00Z
review_path: .planning/phases/03-language-bindings-documentation/03-REVIEW.md
iteration: 1
findings_in_scope: 14
fixed: 4
skipped: 10
status: partial
---

# Phase 03: Code Review Fix Report

**Fixed at:** 2026-05-20T13:15:00Z
**Source review:** .planning/phases/03-language-bindings-documentation/03-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope (Critical + Warning): 14
- Fixed: 4
- Skipped: 10

Scope filter: `critical_warning` (CR-* and WR-* only; Info findings excluded).

The phase introduced `add_time_series_row` across four bindings (Julia, Dart, Python, Lua) plus docs. Several review findings sit on code the phase did NOT touch (pre-existing `update_time_series_files!` bugs, `_marshal_time_series_columns` from Phase 2, stale `read_time_series_table` / `delete_time_series!` references in `docs/time_series.md` outside the "Inserting data" section). Those findings are skipped with explicit pre-existing notes so they can be tracked separately.

CR-03 (cross-binding Int -> Float homogeneity) is skipped per **D-03** in `03-CONTEXT.md` ("Python strict typing per binding idiom") — the documented design decision is that Python and the other strict bindings deliberately do not coerce; overriding it would require a milestone-level policy revision rather than a review-fix.

## Fixed Issues

### CR-02: Python `add_time_series_row` rejects `bool` values

**Files modified:** `bindings/python/src/quiverdb/database.py`
**Commit:** 912d25e
**Applied fix:** Replaced `type(v) is int / float / str` dispatch with `isinstance` dispatch and an explicit `isinstance(v, bool)` branch tested **before** `isinstance(v, int)`. `True`/`False` now marshal as INTEGER `1`/`0` (mirrors the `_marshal_params` policy already documented in the same file: *"bool is subclass of int, handled here"*). Updated the docstring to record the new behavior and note D-03 (no Int -> Float coercion is retained intentionally).

### WR-02: Lua `add_time_series_row` test coverage missing negative case

**Files modified:** `tests/test_lua_runner.cpp`
**Commit:** 47a4ea9
**Applied fix:** Added `LuaRunnerTest.AddTimeSeriesRowMissingDimErrors` (placed immediately after `AddTimeSeriesRowMultiDim`). The test omits the required `date_time` dimension column and asserts `std::runtime_error` surfaces from the C++ layer through sol2 and `LuaRunner::run`. Brings Lua coverage parity with Julia (`Missing Dim Error`), Dart, and Python suites which already exercise the negative path. Used `EXPECT_THROW({...}, std::runtime_error)` matching the file's existing error-test style; no gmock dependency introduced.

### WR-03: Lua `lua_table_to_value_map` silently drops unsupported value types

**Files modified:** `src/lua_runner.cpp`
**Commit:** 1d11c9b
**Applied fix:** Added an `else` branch to the type-dispatch ladder in `lua_table_to_value_map` that throws `std::runtime_error("Cannot lua_table_to_value_map: column '<name>' has unsupported Lua type")` for any value that isn't nil / int64_t / double / std::string. Phase 3 introduced `add_time_series_row_lua` which makes the silent-drop more user-visible (single-row calls + typos now surface immediately instead of producing confusing downstream "missing column" errors). Pattern matches the existing default-branch throws in `read_scalars_by_id_lua` / `read_vectors_by_id_lua`. Existing tests pass values matching the supported types only, so no regression risk for `update_time_series_group_lua`. Also added a small comment documenting that `std::map<string, Value>` makes column order alphabetical (addresses WR-10's "fragility" concern as a free comment).

### WR-07: Julia `Add Row > Multi-Dim` test missing `flag` value column

**Files modified:** `bindings/julia/test/test_database_time_series.jl`
**Commit:** 2088492
**Applied fix:** Added `flag = 0` to the kwargs of `Quiver.add_time_series_row!` in the `Multi-Dim` subtest, with a matching `@test result["flag"][1] == 0` read-back assertion. The `multi_dim_time_series.sql` schema declares `flag INTEGER` (nullable). The previous test only exercised `load`, asymmetric with the Dart suite which passes `'flag': 0` (line 651 of `database_time_series_test.dart`). Both bindings now exercise the multi-value-column path identically. Added a code comment explaining the parity with the Dart suite.

## Skipped Issues

### CR-01: docs/time_series.md references three non-existent Julia functions

**File:** `docs/time_series.md:163-170, 215-225, 245-252`
**Reason:** Out of phase scope — Phase 3's docs scope was deliberately limited to the "Inserting data" section per the executive plan (`03-05-docs-add-time-series-row-SUMMARY.md` records this explicitly: *"Out-of-scope observations (deferred, not fixed): docs/time_series.md 'Reading data' section contains stale references ... explicitly deferred"*). The stale `read_time_series_table`, `update_time_series_row!`, `delete_time_series!`, and the wrong `read_time_series_row` signature all sit in the "Reading data" / "Updating data" / "Deleting data" sections which were intentionally not modified in this phase.
**Recommended follow-up:** Track as a separate "docs reconciliation" phase. The fix is mechanical but spans roughly 90 lines of documentation that need rewriting against the actual shipped API surface (`read_time_series_group`, `update_time_series_group!`, no whole-group delete primitive, real `read_time_series_row` signature without the positional type argument).
**Original issue:** "Doc shows three function calls that have no implementation anywhere in `bindings/julia/src/`. Users following the doc will get UndefVarError immediately."

### CR-03: Cross-binding inconsistency on Int -> Float coercion

**Files:** Multiple — `bindings/julia/src/database_update.jl`, `bindings/dart/lib/src/database_update.dart`, `bindings/python/src/quiverdb/database.py`, `src/lua_runner.cpp`
**Reason:** Skipped per design decision **D-03** ("Python strict typing per binding idiom") documented in `03-CONTEXT.md`. The strict typing in Python, Dart, and Lua is a deliberate per-binding-idiom choice, not an oversight. Julia's coercion is also deliberate and idiomatic (Julia number promotion is a language feature). Overriding D-03 across four bindings would be a milestone-scope homogeneity policy revision, not a review-fix.
**Recommended follow-up:** If the team wants to revisit homogeneity, open a separate planning round to (a) pick one rule per the principle in CLAUDE.md, (b) add a coercion-policy section to CLAUDE.md, (c) propagate the rule to all four bindings, and (d) update tests pinning the chosen behavior. WR-04 and WR-05 (test/doc coverage of the chosen policy) become trivial once the policy is set.
**Original issue:** "Julia auto-coerces Int -> Float when schema is FLOAT; Python/Dart/Lua reject. Homogeneity violation per CLAUDE.md principle."

### CR-04: Julia `update_time_series_files!` has incomplete `GC.@preserve`

**File:** `bindings/julia/src/database_update.jl:206-238`
**Reason:** Out of phase scope — REVIEW.md flags this as pre-existing code ("Pre-existing code, but the phase doesn't fix it"). Phase 3 did not modify `update_time_series_files!`. The bug exists in the milestone-baseline file and predates the `add_time_series_row` work.
**Recommended follow-up:** Track as an independent bug-fix phase (or a follow-up GSD `/gsd-quick` task). The fix is well-specified in REVIEW.md (extend `GC.@preserve` to include `column_ptrs` and `path_ptrs`, or wrap everything in a `refs::Vector{Any}`). Also note: the cited `path_cstrings = []` type-instability is the same untyped-vector issue and should be fixed in the same commit.
**Original issue:** "GC.@preserve block omits column_ptrs and path_ptrs whose pointer() is passed to C; vectors could be collected under GC pressure."

### WR-01: Julia `update_time_series_files!` silently no-ops on empty dict

**File:** `bindings/julia/src/database_update.jl:207-209`
**Reason:** Out of phase scope — pre-existing semantic inconsistency in `update_time_series_files!`, not introduced or modified by Phase 3. Should be addressed together with CR-04 since both touch the same function.
**Recommended follow-up:** Decide the empty-dict semantic (no-op vs. clear-all) and document it. The Dart wrapper has the identical early-return pattern and should change in lockstep.
**Original issue:** "update_time_series_files! treats empty paths as 'do nothing'; sibling update_time_series_group! treats empty kwargs as 'clear all rows'. Opposite semantics, identical surface."

### WR-04: Dart test missing int-for-float case

**File:** `bindings/dart/test/database_time_series_test.dart:585-664`
**Reason:** Skipped — this finding is downstream of CR-03 (the test only matters once the coercion policy is set). With CR-03 skipped per D-03 (strict typing intentional in Dart), the test would either (a) duplicate existing strict-typing coverage that's implicit in the current type-dispatch logic, or (b) need to assert the schema-mismatch error message, which the C++ layer produces and `update_time_series_group` tests already exercise. Adding it now would pre-commit Dart to a specific failure mode without the policy decision being made.
**Recommended follow-up:** Bundle this with the CR-03 follow-up — once homogeneity policy is set, add a Dart test that pins the chosen behavior, and add similar pin-tests in Python and Lua.
**Original issue:** "Dart addTimeSeriesRow tests use already-correctly-typed values; the int-for-REAL case the Julia binding's coercion is designed to handle is untested in Dart."

### WR-05: Python docstring "No Int->Float coercion" not documented in CLAUDE.md

**File:** `bindings/python/src/quiverdb/database.py:1189-1196`
**Reason:** Skipped — same downstream-of-CR-03 reasoning as WR-04. The docstring now references D-03 explicitly (updated as part of the CR-02 commit `912d25e`), so the per-binding documentation is at least consistent with the design decision recorded in `03-CONTEXT.md`. A CLAUDE.md policy section is appropriate once CR-03 is resolved milestone-scope.
**Recommended follow-up:** When CR-03 is revisited, add the policy section to CLAUDE.md ("Type coercion: bindings do NOT coerce int -> float; pass `42.0` for FLOAT columns") and tighten/remove the per-binding docstring duplication.
**Original issue:** "Python docstring asserts 'No Int->Float coercion' as binding policy but CLAUDE.md is silent on coercion."

### WR-06: Julia `add_time_series_row!` duplicates `update_time_series_group!` marshaling

**File:** `bindings/julia/src/database_update.jl:123-204` vs `15-121`
**Reason:** Skipped — the duplication is real but extracting the shared helper safely requires a refactor that crosses 80 lines of GC-sensitive FFI marshaling code with no Julia test coverage for negative paths beyond the four `Add Row` cases already in place. The current code is correct; the refactor would invalidate `GC.@preserve refs` reasoning that has been validated by 1056 passing Julia tests. CLAUDE.md does say "Simple solutions over complex abstractions" but the helper extraction here would introduce a heterogeneous container (vector-of-vectors for `update`, vector-of-single-element for `add`) and lose readability of the scalar-kwargs path.
**Recommended follow-up:** If the team wants to refactor, do it in a dedicated GSD phase with a before/after `time` benchmark to confirm no marshaling regression, and broader Julia integration tests covering DateTime/string/Int/Real branches for both `update_time_series_group!` and `add_time_series_row!`. Not blocking for v1.1.
**Original issue:** "~80 lines duplicated; future schema-coercion bug fix in one wouldn't propagate to the other."

### WR-08: Python `add_time_series_row` does no schema validation; `update_time_series_group` does

**File:** `bindings/python/src/quiverdb/database.py:1189-1238` vs `1533-1598`
**Reason:** Out of phase scope — `_marshal_time_series_columns` (the function violating "Error Messages: All error messages are defined in the C++/C API layer") is Phase 2 code, not Phase 3. The asymmetric validation pattern existed before `add_time_series_row` was added. The right fix is REVIEW.md's option 1 (*"Remove the binding-side validation in `_marshal_time_series_columns` and let C++ produce the canonical error"*) but it touches both write paths and Phase 2 tests.
**Recommended follow-up:** Track as a Python-binding cleanup phase: remove the `ValueError("Row N is missing ...")` / `TypeError("Column '...' expects float, got int")` craftings in `_marshal_time_series_columns`, let C++ produce the messages, and align `add_time_series_row`'s error surface with `update_time_series_group`'s. Then verify Phase 2 tests still pass against C++-sourced messages.
**Original issue:** "_marshal_time_series_columns crafts Python-side error messages, violating CLAUDE.md 'Error messages defined in C++/C API'."

### WR-09: Dart `addTimeSeriesRow` does not validate empty row map

**File:** `bindings/dart/lib/src/database_update.dart:159-223`
**Reason:** Skipped — the finding itself states *"The current state is fine if you pick option 2 [let the C++ message through]; just flagging for awareness."* CLAUDE.md "All error messages are defined in the C++/C API layer" makes option 2 the canonical choice. No code change is needed; the empty-row case produces the canonical `Cannot add_time_series_row: row missing required '<dim>' column` C++ error.
**Recommended follow-up:** None. The Dart wrapper is consistent with project policy. If a tighter caller-side error is desired in the future, document it explicitly in CLAUDE.md as a binding-side allowed-validations carve-out.
**Original issue:** "Empty row map produces 'row missing required date_time column' from C++ — confusing because the wrapper invited the empty call."

### WR-10: Lua `lua_table_to_value_map` iteration order is hash-based

**File:** `src/lua_runner.cpp:216-232, 814`
**Reason:** Skipped (largely no-op) — the finding itself says *"No code change required. Consider adding a comment in lua_table_to_value_map documenting that order doesn't matter to the consumer."* The fix for WR-03 already added that comment to the function as a free side-effect of the change (the comment lives at lines 217-219 documenting the alphabetical-order invariant). So WR-10's recommended documentation action is satisfied incidentally by the WR-03 commit `1d11c9b`.
**Recommended follow-up:** None. The fragility note is now in the source.
**Original issue:** "Lua tables iterated via for-each use hash order; consumers happen not to depend on it but the invariant isn't documented."

---

_Fixed: 2026-05-20T13:15:00Z_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
