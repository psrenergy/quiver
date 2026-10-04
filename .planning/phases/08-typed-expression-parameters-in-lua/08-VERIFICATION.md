---
phase: 08-typed-expression-parameters-in-lua
verified: 2026-10-04T23:30:00Z
status: passed
score: 5/5 roadmap success criteria verified (39/39 plan must-have truths, 8/8 requirements)
behavior_unverified: 0
overrides_applied: 0
deferred:
  - truth: "Root AGENTS.md sandbox decision still lists `expr:save` (not `save` on a file or an expression) and the Lua expression paragraph still says to build from a file with `quiver.expression(file)` (review IN-03)"
    addressed_in: "Phase 9"
    evidence: "Phase 9 SC4: 'The root AGENTS.md has an AbstractExpression Design Decision and updated binary cross-layer rows (get_metadata on files and expressions, the expression methods on files in Lua and Julia)'"
  - truth: "`f == g`, `f < g`, `e == e2` are always true (review WR-01)"
    addressed_in: "Deferred decision EQ-01 (ROADMAP Phase 7 'Settled')"
    evidence: "ROADMAP: 'fixing it for files and expressions together is deferred as EQ-01'; Phase 8 plan pins [true,true,false] unchanged"
---

# Phase 8: Typed Expression Parameters in Lua Verification Report

**Phase Goal:** In Lua a binary file is an expression: sol2 type-checks every expression operand as `AbstractExpression`, a wrong operand still gets the pinned Pattern 1 text, and a file accepts every expression method without `quiver.expression`.
**Verified:** 2026-10-04
**Status:** passed
**Re-verification:** No (initial verification)

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Both usertypes register `AbstractExpression` as base; no `new_usertype<AbstractExpression>`; all 12 `quiver.*` functions, every operator metamethod and every expression method take `const AbstractExpression&`; `to_expression`/`is_number` gone; no `sol::object` operand parameter | VERIFIED | `internal.h:38-40` has the three trait lines at global scope; `git grep new_usertype<AbstractExpression> -- src` empty; `git grep -nwE 'to_expression\|is_number' -- src/lua_runner` empty; no `sol::bases<`/`base_classes` in src. `expression.cpp`: `binop` / `unary_metamethod` / `unary_function` / `ifelse` candidates all take `const AbstractExpression&`; remaining `sol::object` uses are line 63 (fallback reading its args) and `parameter`/`labels`/`mapping` (non-operand). Every sol2-using TU includes `internal.h` (only `internal.h` itself lacks it). |
| 2 | 13 pinned expectations pass with bodies unmodified; new tests pin `quiver.abs(e, 99)` and `quiver.gt(e, f, 3)` too-many-arguments texts; CHANGELOG BREAKING line | VERIFIED | Extracted both pinned bodies from `1bea16a` and HEAD: `cmp` IDENTICAL, 13 expectations. Diff hunks in the test file are at lines 235, 299 (metadata renames) and 580 (append after the pinned body). `ExtraArgumentsThrow` pins both texts plus ifelse/expression/`__add`/`__mul`. CHANGELOG `[0.13.0]` has the BREAKING arity line. |
| 3 | Raw-file methods each tested; `f:save` keeps three guards and leaves file open; `e:get_metadata()` works, `e:metadata()` raises; `quiver.expression(f)` returns an Expression; CHANGELOG BREAKING + Added | VERIFIED | Tests `FileAggregate`, `FileAggregateAgents`, `FileSelectAgents`, `FileRenameAgents`, `FileSaveKeepsFileOpen`, `FileSaveGuards`, `FileGetMetadata`, `ExpressionGetMetadata`, `ExpressionOfFileIsAnExpression` read and confirmed substantive. `save` lambda routes through `resolve_sandboxed_path(db, "save", path)` for both usertypes. `binary.cpp` only lost its own `get_metadata` (now shared). Ran `LuaExpressionTest.*`: 41/41 pass Debug and Release. |
| 4 | Release benchmark before/after recorded with landed form; no C4702 or new warning in Release `src/lua_runner` | VERIFIED | `build/typed-check/bench.txt` six medians, `perf-runs.txt` 30 lines; tag +2.3% on `f:read` so traits stay per the LUA-06 rule (`FORM` = traits); recorded in 08-02-SUMMARY, STATE.md line 174 and `src/AGENTS.md`. Re-ran `release_warnings.sh` myself: `RELEASE WARNINGS tus=17 total=0 c4702=0`; mutation evidence `release-warnings-no-pragma.txt` shows the guard is load-bearing (2 unique C4702). |
| 5 | `LUA_DB_API_REFERENCE` documents files as expressions, methods on files, `get_metadata`, arity rule; no `:metadata()`; sync test passes; counts recorded; six suites green | VERIFIED | `lua-api.ts` diff adds all four topics; `grep ':metadata()'` returns nothing (rc=1). Re-ran `bun test test/lua-api-sync.test.ts`: 6 pass. Re-ran counts: Lua* 490 Debug and Release, LuaRunnerCApiTest 27/27. `build/typed-check/test-all.txt` shows all six suites PASS. |

**Score:** 5/5 truths verified (0 present, behavior-unverified)

Plan-level truths (08-01: 26, 08-02: 13) were checked against the same evidence. The behavior-dependent ones (save leaves the file open, refused saves leave handles intact, operand error leaves runner usable, leftmost bad operand, bad operand outranks arity) each have a named passing test. The three `backstop` truths are confirmed by explicit evidence: the traits are compile-time explicit specializations (nothing registered per run), `expression.cpp` has no `static`/`thread_local` state, and `LUA_DB_API_REFERENCE` is an `export const` string.

### Deferred Items

| # | Item | Addressed In | Evidence |
|---|------|-------------|----------|
| 1 | Root AGENTS.md sandbox list and expression paragraph still describe `expr:save` / `quiver.expression(file)` only (IN-03) | Phase 9 | SC4: root AGENTS.md updated cross-layer rows, expression methods on files in Lua and Julia |
| 2 | `==`/`<` between files or expressions always true (WR-01) | EQ-01 (deferred decision) | ROADMAP Phase 7 "Settled" note |

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/lua_runner/internal.h` | forward decls + 3 trait lines | VERIFIED | lines 26-40 |
| `src/lua_runner/expression.cpp` | `operand_error` fallback, overload sets, 6 shared methods, C4702 guard | VERIFIED | 238 lines (<=450); MSVC push/disable 4702/pop around includes only |
| `src/lua_runner/binary.cpp` | usertype minus its own `get_metadata` | VERIFIED | 2-line deletion only |
| `tests/test_lua_expression.cpp` | 13 new tests, 3 renames | VERIFIED | 41 tests listed and passing |
| `CHANGELOG.md` | 2 BREAKING + 1 Added | VERIFIED | under `[0.13.0]` |
| `bindings/js/src/lua-api.ts` | files-as-expressions, methods, get_metadata, arity | VERIFIED | sync test 6 pass |
| `src/AGENTS.md`, `.planning/STATE.md` | benchmark + landed form | VERIFIED | STATE.md:174; src/AGENTS.md:866 |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `internal.h` | every sol2 TU in `src/lua_runner` | traits visible before use | WIRED | every file that includes sol2 includes `lua_runner/internal.h` |
| `expression.cpp` | `internal.h` | `lua_type_error(operation, "operand", "an expression or a binary file", o)` | WIRED | line 74 |
| `expression.cpp` | `binary.cpp` | `binary_file_type["save"]` etc. on the returned usertype | WIRED | lines 193-199 |
| `expression.cpp` | `path_policy.h` | `resolve_sandboxed_path(db, "save", path)` | WIRED | line 137 |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Lua expression suite (Debug) | `quiver_tests --gtest_filter=LuaExpressionTest.*` | 41 passed | PASS |
| Lua expression suite (Release) | `build/release/bin/quiver_tests --gtest_filter=LuaExpressionTest.*` | 41 passed | PASS |
| Lua* counts | `--gtest_list_tests` Debug / Release | 490 / 490 | PASS |
| C API Lua runner | `quiver_c_tests --gtest_filter=LuaRunnerCApiTest.*` | 27 passed | PASS |
| lua-api sync | `bun test test/lua-api-sync.test.ts` | 6 pass | PASS |
| Release warnings | `build/typed-check/release_warnings.sh` | tus=17 total=0 c4702=0 | PASS |
| Settled equality | `build/abstract-check/eq.sh debug` | EQ debug OK | PASS |
| Golden surface | `build/layout-check/golden.sh debug` | GOLDEN debug OK | PASS |

Linux GCC 13 / Clang 18 evidence (not rerun): `build/typed-check/linux_{gcc,clang}.txt` show LISTED=488, FULL_LISTED=1472, CAPI 543, exit=0.

### Probe Execution

Step 7c: no `scripts/*/tests/probe-*.sh` declared or present for this phase; SKIPPED.

### Requirements Coverage

| Requirement | Source Plan | Status | Evidence |
|-------------|-------------|--------|----------|
| LUA-01 | 08-01 | SATISFIED | SC1 evidence |
| LUA-02 | 08-01, 08-02 | SATISFIED | pinned bodies byte-identical, pass Debug/Release/Linux, golden `binary.txt` unchanged |
| LUA-03 | 08-01 | SATISFIED | `ExtraArgumentsThrow`, CHANGELOG BREAKING |
| LUA-04 | 08-01 | SATISFIED | 7 raw-file tests incl. guards and file-stays-open |
| LUA-05 | 08-01 | SATISFIED | `ExpressionGetMetadata`, `ExpressionOfFileIsAnExpression`, CHANGELOG BREAKING |
| LUA-06 | 08-02 | SATISFIED | interleaved benchmark, rule applied, traits landed, recorded |
| LUA-07 | 08-01, 08-02 | SATISFIED | re-run: total=0 c4702=0; guard shown load-bearing |
| LUA-08 | 08-01 | SATISFIED | lua-api.ts diff, sync 6 pass |

No orphaned requirements: REQUIREMENTS.md maps exactly LUA-01..08 to Phase 8, all claimed.

### Anti-Patterns Found

None. No TBD/FIXME/XXX/TODO/HACK in any added line outside `.planning`. Scope: only the 8 planned files changed outside `.planning` (no change under `include/`, `src/expression`, `src/binary`, `src/c`, other bindings).

### Code Review Findings Weighed

- **WR-01** (`==`/`<` always true): pre-existing, the deferred EQ-01 decision; not a Phase 8 gap. The reference lists only `+ - * /`, unary `-` and `& | ~` as operators, so it does not claim `==` works. The review's one-sentence warning for the reference is still a cheap improvement worth considering.
- **WR-02** (`w:save` reports `Cannot open_file: ...`): the plan made this choice on purpose (keep the core's text, change no core code), and SC3 only requires the save to be refused, which it is. Advisory: the misnamed operation is now pinned by a test.
- **IN-01..03**: informational; IN-03 deferred to Phase 9 SC4.

### Human Verification Required

None.

### Gaps Summary

No gaps. Every roadmap success criterion and plan truth is backed by code and passing tests that I re-ran in Debug and Release. The two review warnings are pre-existing behaviour that the plan deliberately left alone, not missed goals.

---

_Verified: 2026-10-04_
_Verifier: Claude (gsd-verifier)_
