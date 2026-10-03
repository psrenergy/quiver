---
phase: 04-fixes-and-release-type-safety
plan: 03
subsystem: lua-binding
status: complete
tags: [sol2, lua_runner, sandbox, load, expression, pattern-1-errors, refactor]

requires:
  - phase: 04-fixes-and-release-type-safety
    provides: "lua_type_error (04-01), the build/fixes-check/ harness, the 04-02 gate baseline lua=467"
provides:
  - "Text-only load: a lua.safe_script wrapper in LuaRunner::Impl forcing mode \"t\" (env forwarded through ...)"
  - "to_expression(o, operation) + binop<Op>(operation) factory: every operand error names the operation"
  - "lua_data_type_name as the ASCII lowercase of data_type_to_string; update_time_series_files without its nil branch"
  - "AGENTS.md / src/AGENTS.md / lua-api.ts say load takes text chunks only; CHANGELOG BREAKING load entry + Fixed operand entry"
affects: [04-04]

actuals:
  tokens: 9800
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - "A sandbox global is replaced by a Lua wrapper installed with lua.safe_script (never set_function), holding the original only as an upvalue"
    - "A shared operator body that must name its caller is a factory (template <typename Op> auto binop(const char*)) returning a capturing lambda"

key-files:
  created: []
  modified:
    - src/lua_runner/lua_runner.cpp
    - src/lua_runner/binary.cpp
    - src/lua_runner/db_time_series.cpp
    - src/lua_runner/db_metadata.cpp
    - tests/test_lua_runner_errors.cpp
    - tests/test_lua_expression.cpp
    - bindings/js/src/lua-api.ts
    - AGENTS.md
    - src/AGENTS.md
    - CHANGELOG.md

key-decisions:
  - "load always passes mode \"t\" to the original, ignoring the caller's mode; string.dump stays (its output is inert)"
  - "Operand errors use Lua's metamethod event names (add, sub, mul, div, unm, band, bor, bnot) and the quiver.* function names, through lua_type_error (adds ', got <type>')"
  - "The arithmetic _nn golden probes become 'Cannot expression:' (the probe calls quiver.expression on a Lua-computed number), not 'Cannot <op>:' as the plan listed"

patterns-established:
  - "Red run saved under build/fixes-check/red/, gate before each commit, mutation after the commit reverted with git checkout"

requirements-completed: [SAFE-05, SAFE-07, FIX-03]

duration: 27min
completed: 2026-10-03
---

# Phase 4 Plan 03: Text-only load, Operand Error Names and Dead Branches Summary

**Lua `load` now refuses precompiled bytecode in every build (a wrapper forcing mode `"t"`, installed before any script runs). Every Expression operator and `quiver.*` helper names the called operation in its operand error (`Cannot add: operand must be an expression or a binary file, got string`). The two unreachable branches FIX-03 listed are gone, and the golden harness shows that commit changes no output.**

## Performance

- **Duration:** about 27 min
- **Started:** 2026-10-03T13:33Z
- **Completed:** 2026-10-03T14:00Z
- **Tasks:** 2 (2 fix commits + 1 refactor commit)
- **Files modified:** 10

## Commits

| # | SHA | Subject | Gate line |
|---|-----|---------|-----------|
| 1 | `f3d6abd` | fix(04-03): load only text chunks | `GATE PASS lua=469 pairs=36` (no change flag) |
| 2 | `d81e2d8` | fix(04-03): name the operation in expression operand errors | `GATE PASS lua=470 pairs=36` (with `GOLDEN_CHANGE=1`, then promoted) |
| 3 | `87a8266` | refactor(04-03): remove the unreachable branches in the Lua bindings | `GATE PASS lua=470 pairs=36` (no change flag) |

Wave gate after commit 3: **`WAVE GATE PASS lua=470 tidy=14`**. In Release, `Lua*` runs 470 tests in 12 suites and all pass. The C API `LuaRunnerCApiTest` runs 27. `GOLDEN release OK`. The GCC 14 syntax pass is clean. The Release `LuaRunnerTest.Load*:LuaExpressionTest.OperandErrorsNameTheOperation` filter passes 3/3. The JS sync test passes, and the `.set_function(` count stays 86. The surface is unchanged. No existing test line was removed: `git diff f3d6abd~1 HEAD -- 'tests/*.cpp' | grep -c '^-[^-]'` = 0. Commit 3's diff touches only `db_metadata.cpp` and `db_time_series.cpp`.

## Red Runs

| File | Build | Failing assertions |
|------|-------|--------------------|
| `red/load-text.txt` | Debug | `LoadRefusesBinaryChunks` FAILED at `no mode: a binary chunk should not load`, so `load(string.dump(f))` returned a function. **Pin `LoadStillAcceptsTextChunks` was green before the fix and green after it.** It covers no mode, explicit mode + env, nil mode + env, no env (globals), explicit nil env (loads, call errors), and a reader yielding text pieces. |
| `red/c8-operands.txt` | Debug | `OperandErrorsNameTheOperation`: all 11 expectations failed, each with `Cannot build expression: operand must be an expression or a binary file` (e + 'x', e - 'x', e * {}, e / 'x', e & 'x', e \| 'x', quiver.gt(1, 2), quiver.eq(e, 'x'), quiver.abs('x'), quiver.ifelse(e, e, 'x'), quiver.expression(5)). |

The refactor commit has no red run by design: unreachable code has no observable behaviour, and the golden comparison is the proof.

## Mutations (after commit, then reverted)

- **Commit 1:** forced mode changed to `"bt"`. `LoadRefusesBinaryChunks` FAILED (`no mode: a binary chunk should not load`), and the pin stayed green (`build/fixes-check/mutation-load-text.txt`). Reverted with `git checkout -- src/lua_runner/lua_runner.cpp`, and `git diff --exit-code -- src/lua_runner` exited 0.
- **Commit 2 (extra):** addition registered as `binop<std::plus<>>("sub")`. `OperandErrorsNameTheOperation` FAILED: `e + 'x'` reported `Cannot sub: ...` (`mutation-c8-operands.txt`). Reverted the same way.
- **Commit 3:** dropped the lowercase step in `lua_data_type_name`. `golden.sh debug` exited 1 with diffs in `read_all_types.txt`, `read_collections.txt` and `write_multi_dim.txt` (43 `string:INTEGER/REAL/TEXT` lines) (`mutation-fix03.txt`). Reverted, rebuilt, and `GOLDEN debug OK`.
- **Tracer gate (delegated self-verification):** after commit 1 and the mutation revert, the tracer `<verify>` ran end to end: gate PASS, the three-test filter 3/3, `safe_script(` = 2, and a clean `git diff -- src/lua_runner`. Per the standing preference, the verdict (a binary chunk is refused and string-form `load` is intact) is recorded as delegated rather than stopping for a human. Release confirmed it at the wave gate.

## Golden Changes (commit 2 only; promoted with `golden.sh debug --capture`, previous baseline kept in `build/fixes-check/baseline-pre-c8`)

All 30 changed keys are in `binary.txt`, and nothing else changed (`debug_text` unchanged). Each old value is `{"err":"string:Cannot build expression: operand must be an expression or a binary file",...`. The new value replaces only that text:

| Keys | New text |
|------|----------|
| `add_string`, `sub_string`, `mul_string`, `div_string`, `band_string`, `bor_string` | `Cannot <add\|sub\|mul\|div\|band\|bor>: operand must be an expression or a binary file, got string` |
| `add_nn`, `sub_nn`, `mul_nn`, `div_nn`, `band_nn`, `bor_nn` | `Cannot expression: operand must be an expression or a binary file, got number` (see Deviation 2) |
| `gt_nn`, `lt_nn`, `gte_nn`, `lte_nn`, `eq_nn`, `neq_nn` | `Cannot <op>: ..., got number` |
| `gt_none`, `lt_none`, `gte_none`, `lte_none`, `eq_none`, `neq_none` | `Cannot <op>: ..., got nil` |
| `gt_string`, `lt_string`, `gte_string`, `lte_string`, `eq_string`, `neq_string` | `Cannot <op>: ..., got string` |

A script checked every pair: the prefix and suffix around the replaced text are identical, and the key ends in `_nn`, `_none` or `_string` (review diff in `build/fixes-check/c8-golden-diff.txt`).

## `load({})` Text Change (known, unpinned)

The function name stays `'load'`, contrary to research's `'?'` prediction. The **position prefix** changes: the error now points at the wrapper chunk's line instead of the script's line.

- Before: `[string "local ok, e = pcall(function() return load({}..."]:1: bad argument #1 to 'load' (function expected, got table)`
- After: `[string "..."]:3: bad argument #1 to 'load' (function expected, got table)`

## Check-Order Ledger

| Function | Old order | New order |
|---|---|---|
| `binop<Op>` (each operator / comparison) | number on left only -> `Op(double, to_expression(rhs))`; number on right only -> `Op(to_expression(lhs), double)`; else both through `to_expression` (lhs, then rhs) | unchanged; only `operation` is threaded into `to_expression` |
| `quiver.ifelse` | `to_expression(c)`, `(t)`, `(e)` as function arguments (unspecified evaluation order) | unchanged (same three calls, each now named `ifelse`, so whichever fails first reports the same operation) |
| `update_time_series_files_lua` | require_table -> per entry: key check -> nil test -> `lua_cell_as<std::string>` | require_table -> per entry: key check -> `lua_cell_as<std::string>` |

## Tidy (wave gate, `src/lua_runner/`: 14 unique, same set as 04-02)

| File | Check | Count |
|---|---|---|
| binary.cpp | bugprone-unchecked-optional-access | 3 |
| binary.cpp | modernize-return-braced-init-list | 1 (now `:112`, `to_expression`) |
| binary.cpp | modernize-raw-string-literal | 1 |
| csv.cpp | readability-identifier-naming | 2 |
| db_core.cpp | bugprone-empty-catch | 1 |
| lua_runner.cpp | bugprone-empty-catch | 2 |
| return_json.cpp | readability-identifier-naming | 3 |
| return_json.cpp | bugprone-implicit-widening-of-multiplication-result | 1 |

`performance-unnecessary-value-param` = 0, `clang-diagnostic-error` = 0.

## `src/lua_runner/` Line Counts (all at most 450)

binary.cpp 339, csv.cpp 416, db_core.cpp 205, db_metadata.cpp 91, db_read.cpp 129, db_time_series.cpp 281, db_write.cpp 311, internal.h 318, lua_runner.cpp 170, path_policy.cpp 63, return_json.cpp 225.

## Accomplishments

- SAFE-07: `load` refuses a binary chunk given as a string or through a reader, with mode omitted, `"b"` or `"bt"`. It returns `nil, "attempt to load a binary chunk (mode is 't')"` in Debug and Release. String-form `load` behaves as stock Lua, and the none-vs-nil `env` distinction is preserved. The wrapper's `safe_script(` sits after `lua["loadfile"]` (line 105) and before `create_named_table("quiver")` (line 110).
- SAFE-05 (complete with 04-01 and 04-02): the expression operand errors now end in `, got <lua type>` like every other type error.
- FIX-03: operand errors name the public operation, the unreachable `update_time_series_files` nil branch is removed, and the `lua_data_type_name` switch with its fallback is removed. `apply_binop` was already gone (`grep -rn apply_binop src` = 0).
- 3 new tests in existing files and suites (Lua* 467 -> 470, still 12 suites).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Test compile/logic] Load test fixes before the red run**
- **Found during:** Task 1
- **Issue:** The message `(mode is 't')"` closed the `R"(` raw string early. The first helper signature `refused(f, err, how)` also shifted its arguments when `load` succeeded, because a successful `load` returns one value. The first red therefore failed for the wrong reason (`concatenate a nil value (local 'how')`).
- **Fix:** Used `R"lua( ... )lua"` and put `how` first, as `refused(how, load(...))`. The recorded red is the intended failure.

**2. [Rule 1 - Plan expectation] Six arithmetic `_nn` golden keys say `Cannot expression:`**
- **Found during:** Task 2, commit 2's golden review
- **Issue:** The plan listed the `add/sub/mul/div/band/bor` `_nn` keys as `Cannot <op>:`. The probe is `quiver.expression(op(2.0, 3.0))`: Lua computes the number itself, and then `quiver.expression(5)` rejects it. So the operation the script called is `expression`, and the new text is the correct name for it.
- **Fix:** None needed in code. All six were checked to read exactly `Cannot expression: ..., got number`, then promoted with the rest.

**3. [Rule 2 - Cleanup] Unused `<stdexcept>` include dropped from `db_metadata.cpp`**
- Its only use was the deleted fallback throw. Same commit (`87a8266`). The golden output and the gate are unchanged.

**4. [Docs wrap] lua-api.ts Standard library paragraph rewrapped**
- The paragraph was rewrapped after the edit so the line lengths stay even. Only line breaks moved, and the stdlib list sentence is intact (the sync test passes).

**Total deviations:** 4 (one test fix before the red run, one plan expectation miss, one cleanup, one rewrap). No scope creep. `db_read.cpp` defaults untouched.

## Issues Encountered

None beyond the deviations above.

## Known Stubs

None.

## Threat Flags

None. No new surface. T-04-11..T-04-15 are mitigated as planned. T-04-11 has the wrapper and the `"bt"` mutation. T-04-12 has the pin covering none, nil and explicit env. For T-04-13, the original `load` is held only as an upvalue, and the wrapper is installed before `quiver`/`db` exist. T-04-14 has 30 golden keys, all reviewed. T-04-15 is the byte-identical refactor plus the lowercase mutation.

## Next Phase Readiness

- 04-04 (SAFE-06 flags) starts from `GATE PASS lua=470 pairs=36` and `WAVE GATE PASS lua=470 tidy=14`. The golden baseline is `build/fixes-check/baseline/` as of `d81e2d8`, unchanged by `87a8266`.

## Self-Check: PASSED

- Commits found: `f3d6abd`, `d81e2d8`, `87a8266`.
- Red files exist and contain `FAILED`: `red/load-text.txt`, `red/c8-operands.txt`.
- All 10 key files exist. The wave gate output is saved at `build/fixes-check/wave_0403.txt`.
