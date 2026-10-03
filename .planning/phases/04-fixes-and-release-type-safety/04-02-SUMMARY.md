---
phase: 04-fixes-and-release-type-safety
plan: 02
subsystem: lua-binding
status: complete
tags: [sol2, lua_runner, transactions, dry-run, empty-arrays, pattern-1-errors]

requires:
  - phase: 04-fixes-and-release-type-safety
    provides: "lua_type_error (04-01) and the build/fixes-check/ harness"
provides:
  - "run_in_scope(self, operation, fn_arg, begin, finish, abort): type check before begin; error-or-finish inside one best-effort undo"
  - "table_to_element passes an empty array to the core as std::vector<int64_t>{}"
  - "lua-api.ts: empty-array rule and commit-failure rollback; CHANGELOG [0.13.0] two Fixed entries + one BREAKING entry"
affects: [04-03, 04-04]

actuals:
  tokens: 4900
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "A Lua function argument is a const sol::object& checked with get_type() == sol::type::function before any side effect (callable tables refused)"
    - "Scoped block: the callback's error and the closing call share one try; the catch undoes best-effort and rethrows the original"

key-files:
  created: []
  modified:
    - src/lua_runner/db_core.cpp
    - src/lua_runner/db_write.cpp
    - tests/test_lua_runner_transaction.cpp
    - tests/test_lua_runner_update.cpp
    - tests/test_lua_runner_create.cpp
    - bindings/js/src/lua-api.ts
    - CHANGELOG.md
    - src/AGENTS.md

key-decisions:
  - "db:transaction / db:dry_run check fn before begin; a callable table is refused (same rule as read_csv_stream's on_row)"
  - "A failed COMMIT at the end of db:transaction rolls back best-effort and rethrows the COMMIT error; dry_run's failed end_dry_run gets one swallowed retry and still surfaces"
  - "An empty Lua element array reaches the core as an empty int64 vector: create_element skips it, update_element clears its group (BREAKING)"

patterns-established:
  - "Red run saved under build/fixes-check/red/, gate before each commit, mutation after the commit reverted with git checkout"

requirements-completed: [SAFE-04, FIX-01, FIX-02]

duration: 18min
completed: 2026-10-03
---

# Phase 4 Plan 02: Scoped Blocks and Empty Arrays Summary

**`db:transaction` / `db:dry_run` reject a non-function with `Cannot <op>: fn must be a function, got <type>` before opening anything. A failed COMMIT at the end of `db:transaction` now rolls back and rethrows. An empty array in Lua `update_element` now reaches the core and clears its group (BREAKING), as it already did in C++, Python and JS.**

## Performance

- **Duration:** about 18 min
- **Started:** 2026-10-03T13:14Z
- **Completed:** 2026-10-03T13:32Z
- **Tasks:** 3 (3 fix commits)
- **Files modified:** 8

## Commits

| # | SHA | Subject | Gate line |
|---|-----|---------|-----------|
| 1 | `6242813` | fix(04-02): reject a non-function in db:transaction and db:dry_run before opening the scope | `GATE PASS lua=462 pairs=36` (with `DEBUG_TEXT_CHANGE=1`) |
| 2 | `d7c2478` | fix(04-02): roll back db:transaction when its commit fails | `GATE PASS lua=464 pairs=36` (no change flag) |
| 3 | `3705d29` | fix(04-02): pass an empty array in update_element through to the core | `GATE PASS lua=467 pairs=36` (no change flag) |

Wave gate after commit 3: **`WAVE GATE PASS lua=467 tidy=14`**. Release `Lua*` = 467 tests in 12 suites, all pass. C API `LuaRunnerCApiTest` = 27. `GOLDEN release OK`. The GCC 14 syntax pass is clean. Each commit lists only its planned files, and no existing test line was removed (`git diff HEAD~3 HEAD -- 'tests/*.cpp' | grep -c '^-[^-]'` = 0). `git diff HEAD~1 HEAD --stat -- src | grep -c src/database` = 0, so the core is untouched. `git diff --stat BASE HEAD -- bindings` lists only `bindings/js/src/lua-api.ts`.

## Red Runs

| File | Build | Failing assertions |
|------|-------|--------------------|
| `red/c6-fn.txt` | Debug | `db:transaction(5)` and `db:transaction("x")` (inside an open transaction) got sol2's raw `stack index 2, expected function, received number/string: must be a function or table or a userdata (bad argument into ...)`. `db:transaction(setmetatable({}, { __call = ... }))` **did not throw** (`expected script to throw`), because the callable table was accepted. `db:dry_run(5)` got the same raw text. 2/2 FAILED. |
| `red/c4-commit.txt` | Debug | `TransactionBlockCommitFailureRollsBack`: the error text matched, but `db.in_transaction()` was `true` and `SELECT COUNT(*) FROM Child` was `1`. 1 FAILED. **Pin `ScopedBlockFinishErrorsStillSurface` was green before the fix and is green after it** (`Cannot commit: no active transaction`, `Cannot end_dry_run: no active dry run`). |
| `red/c7-empty.txt` | Debug | `UpdateElementEmptyArrayClearsGroup`: `{ value_int = {} }` threw `Cannot update_element: element must have at least one attribute to update`. `UpdateElementEmptyArrayErrors`: `{ typo = {} }` threw the same text instead of the unmatched-array error. `{ value_int = {}, value_float = {1.5, 2.5} }` **did not throw**: it rewrote the group with `value_float` and NULLed `value_int`, so `read_vector_integers_by_id` no longer returned `{1, 2}`. 2 FAILED. **Pin `CreateElementSkipsEmptyArray` was green before the fix and is green after it.** |

## Mutations (after commit, then reverted)

- **Commit 1:** moved the `fn` type check below `(self.*begin)()`. Both `TransactionBlockRejectsNonFunction` (`db.in_transaction()` Actual: true at line 208) and `DryRunBlockRejectsNonFunction` FAILED. Reverted with `git checkout -- src/lua_runner/db_core.cpp`, and `git diff --exit-code -- src/lua_runner` exited 0.
- **Commit 2:** moved `(self.*finish)()` back out of the `try`. `TransactionBlockCommitFailureRollsBack` FAILED (`in_transaction()` true, Child count 1). The pin stayed green. Reverted the same way.
- **Commit 3 (extra):** replaced the empty-array `element.set(k, std::vector<int64_t>{})` with a no-op. Both update tests FAILED, and the create pin passed. Reverted the same way.
- **Tracer gate (delegated self-verification):** after commit 1 and the mutation revert, the tracer `<verify>` was re-run end to end. The Debug and Release `TransactionBlock*:DryRun*` filters passed 10/10. `lua_type_error(operation, "fn", "a function", fn_arg)` occurs once, and `sol::protected_function fn)` occurs 0 times. Per the standing preference, the verdict (a non-function is rejected before any scope opens, in both builds) is recorded as delegated rather than stopping for a human.

## Debug-Text Change (commit 1 only, promoted with `golden.sh debug --capture`)

| Key | Before | After |
|-----|--------|-------|
| `control_transaction` | `[string "-- schema: collections.sql..."]:72: stack index 2, expected function, received number: must be a function or table or a userdata (bad argument into 'sol::basic_object<sol::basic_reference<0> >(quiver::Database&, sol::basic_protected_function<sol::basic_reference<0>,0,sol::basic_reference<0> >)')` | `Cannot transaction: fn must be a function, got number` |
| `control_dry_run` | `[string "-- schema: collections.sql..."]:73: stack index 2, expected function, received number: must be a function or table or a userdata (bad argument into '...')` (same signature) | `Cannot dry_run: fn must be a function, got number` |

The new text has no `[string ...]:NN:` prefix, because a C++ exception carries no Lua position (the same as every other Pattern 1 key in `debug_text`). No other key changed. Every golden probe output, including `core.lua`'s `scoped_blocks` keys `tx_err`, `dry_err` and `nested`, stayed byte-identical through all three commits. Commits 2 and 3 ran the gate with no change flag. The pre-change baseline is kept as `build/fixes-check/baseline-pre-scoped`.

## Check-Order Ledger

| Function | Old order | New order |
|---|---|---|
| `run_in_scope` (`db:transaction`, `db:dry_run`) | sol2's typed `protected_function` parameter check (Debug only; Release did none), then begin, call, error -> abort + rethrow, then finish **outside** any try | argument check (`get_type() == function`, `db_core.cpp:80-82`), then begin (`:84`), then call, then error-or-finish inside one `try` (`:86-91`) with a best-effort abort and rethrow (`:92-98`) |

Nothing else moved. `table_to_element` keeps key -> userdata check -> table -> `require_dense_array` -> dispatch. Only the empty branch now sets the array instead of dropping it.

## Tidy (wave gate, `src/lua_runner/`: 14 unique, same set as 04-01)

| File | Check | Count |
|---|---|---|
| binary.cpp | bugprone-unchecked-optional-access | 3 |
| binary.cpp | modernize-return-braced-init-list | 1 |
| binary.cpp | modernize-raw-string-literal | 1 |
| csv.cpp | readability-identifier-naming | 2 |
| db_core.cpp | bugprone-empty-catch | 1 (now `:95`, the inner abort catch) |
| lua_runner.cpp | bugprone-empty-catch | 2 |
| return_json.cpp | readability-identifier-naming | 3 |
| return_json.cpp | bugprone-implicit-widening-of-multiplication-result | 1 |

`performance-unnecessary-value-param` = 0, `clang-diagnostic-error` = 0.

## `src/lua_runner/` Line Counts (all at most 450)

binary.cpp 334, csv.cpp 416, db_core.cpp 205, db_metadata.cpp 98, db_read.cpp 129, db_time_series.cpp 286, db_write.cpp 311, internal.h 318, lua_runner.cpp 163, path_policy.cpp 63, return_json.cpp 225.

## Accomplishments

- SAFE-04: a non-function argument to either scoped block raises Pattern 1 before any transaction or dry run opens. Inside an open transaction, the type error is reported instead of `transaction already active`.
- SAFE-05 (the `fn` message): it ends in `, got <lua type>` through `lua_type_error`. SAFE-05 continues in 04-03 and is not marked complete.
- FIX-01: a failed COMMIT inside `db:transaction` leaves no open transaction and none of the block's rows, and its error surfaces.
- FIX-02: `{ col = {} }` on update clears the group, a misspelled empty column throws, an empty column beside a non-empty one throws the length error, and `create_element` still skips it. `lua-api.ts` and the CHANGELOG BREAKING entry state both round-trip outcomes and the fan-out.
- 7 new tests in existing files and suites (Lua* 460 -> 467, still 12 suites).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Formatting] `run_in_scope(self, "<op>", fn, ...)` is not on one line**
- **Found during:** Task 1
- **Issue:** The one-line call is 121 columns, so clang-format 22.1.8 (enforced by the gate) splits it one argument per line. The plan's verify grep `run_in_scope\(self, "(transaction|dry_run)", fn,` therefore prints 0, not 2.
- **Fix:** Kept the clang-format output. A multiline check confirmed both calls pass `self`, the operation name and `fn` in that order (see the `git diff` of `6242813`).
- **Commit:** `6242813`

**2. [Rule 1 - Test compile] Raw string terminated by the Lua text**
- **Found during:** Task 2
- **Issue:** `VALUES ('orphan', 999)")` contains `)"`, which ends `R"( ... )"` early.
- **Fix:** Used `R"lua( ... )lua"` (already used in the Lua tests) before the red run.

**3. [Rule 2 - Docs] Stale comment above `require_dense_array`**
- It said `table_to_element` "skips it outright when the hole is cell 1". After the fix such an array reads as empty, so the comment now says that. Same commit (`3705d29`).

**4. [Rule 1 - Docs] lua-api.ts wrap**
- The first wrap split `does not match any vector, set, or time series table` across two lines, which failed the plan's acceptance grep. Rewrapped before the commit, so only the final text is committed.

**Total deviations:** 4 (one formatter-forced, one test compile, two doc fixes). No scope creep.

## Issues Encountered

None beyond the deviations above.

## Known Stubs

None.

## Threat Flags

None. No new surface. T-04-07..T-04-10 are mitigated as planned: commit-failure rollback with its mutation, the check before begin with its mutation, the BREAKING entry with the fan-out and both round-trip outcomes, and the finish-error pins green before and after.

## Next Phase Readiness

- 04-03 (C8 / text-only `load` / FIX-03) starts from `GATE PASS lua=467 pairs=36` and `WAVE GATE PASS lua=467 tidy=14`. The golden baseline is `build/fixes-check/baseline/` as of `6242813` (unchanged by `d7c2478` and `3705d29`).
- SAFE-05 is still open (04-03).

## Self-Check: PASSED

- Commits found: `6242813`, `d7c2478`, `3705d29`.
- Red files exist and contain `FAILED`: `red/c6-fn.txt`, `red/c4-commit.txt`, `red/c7-empty.txt`.
- Files exist: all 8 key files. The wave gate output is saved at `build/fixes-check/wave_0402.txt`.
