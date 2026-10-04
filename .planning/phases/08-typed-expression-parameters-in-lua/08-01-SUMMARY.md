---
phase: 08-typed-expression-parameters-in-lua
plan: 01
subsystem: lua_runner
status: complete
tags: [lua, sol2, expression, binary, typed-operands, breaking]
requires:
  - "Phase 7: quiver::AbstractExpression base of BinaryFile and Expression (C++)"
provides:
  - "sol2 base traits: BinaryFile and Expression are AbstractExpression in Lua"
  - "typed operand overload sets with one operand_error fallback"
  - "six expression methods on Lua files (save, get_metadata, aggregate, aggregate_agents, select_agents, rename_agents)"
affects:
  - "08-02 (Release build, f:read/f:write benchmark of traits vs runtime tag, C4702 check, six suites)"
  - "Phase 9 docs (root AGENTS.md AbstractExpression decision, cross-layer rows)"
tech-stack:
  added: []
  patterns:
    - "SOL_BASE_CLASSES / SOL_DERIVED_CLASSES compile-time inheritance traits at global scope in internal.h"
    - "sol::overload typed candidates ending in a sol::variadic_args fallback that only words the error"
    - "MSVC-only #pragma warning(push/disable 4702/pop) around a TU's includes"
key-files:
  created: []
  modified:
    - src/lua_runner/internal.h
    - src/lua_runner/expression.cpp
    - src/lua_runner/binary.cpp
    - tests/test_lua_expression.cpp
    - CHANGELOG.md
    - bindings/js/src/lua-api.ts
    - src/AGENTS.md
    - AGENTS.md
decisions:
  - "Compile-time traits form (not the runtime base-classes tag) lands as the starting point; 08-02 benchmarks the tag against it"
  - "w:save on a writer pins the core's existing `Cannot open_file: file is already open for writing` text; the core is not changed"
  - "No alias keeps Lua e:metadata(); an old script gets Lua's own `attempt to call a nil value (method 'metadata')`"
  - "Expression methods keep ignoring extra arguments; only the 12 quiver.* expression functions and operator metamethods are strict"
metrics:
  duration: 17min
  completed: 2026-10-04
actuals:
  tokens: 11700
  tasks: 3
  commits: 3
---

# Phase 8 Plan 1: Typed Expression Parameters in Lua Summary

In Lua, sol2 now type-checks every expression operand as `const AbstractExpression&` through compile-time base traits, a raw `db:open_file` handle takes all six expression methods directly, a wrong operand still gets the pinned Pattern 1 text, and extra arguments raise `Cannot <op>: too many arguments (expected N, got M)`.

## Gate record

- BASE: `1bea16a10e2a634f224bde93b4285f98be7e33c3`
- At base (`GATE_AT_BASE=1`): `GATE PASS lua=477 luaexpr=28 tests=1463 capi=543`
- After Task 1 (`5c7469a`): `GATE PASS lua=478 luaexpr=29 tests=1464 capi=543` (GOLDEN debug OK, SURFACE DELTA OK, EQ debug OK)
- After Task 2 (`d384e72`): `GATE PASS lua=490 luaexpr=41 tests=1476 capi=543`
- After Task 3 (`9d250f2`): `GATE PASS lua=490 luaexpr=41 tests=1476 capi=543` (plus SYNC OK, see deviations)
- N = 13 new LuaExpressionTest tests. Final Windows Debug counts: LuaExpressionTest 41, Lua* 490 in 12 suites, quiver_tests 1476, ExpressionFixture 125, SandboxedPathTest 11, quiver_c_tests 543, ExpressionCApiFixture 73, LuaRunnerCApiTest 27, all passing.

## Commits

| Task | Commit | Message |
|------|--------|---------|
| 1 (tracer) | 5c7469a | feat(08-01): type Lua expression operands as AbstractExpression and give files the expression methods |
| 2 | d384e72 | test(08-01): pin file expression methods, save guards, arity and operand edges in Lua |
| 3 | 9d250f2 | docs(08-01): changelog, Lua reference and AGENTS.md for typed expression operands |

## What changed

- `src/lua_runner/internal.h`: global-scope forward declarations of `AbstractExpression`, `BinaryFile`, `Expression` and the three trait lines (`SOL_BASE_CLASSES` x2, `SOL_DERIVED_CLASSES` x1); the old in-block `class BinaryFile;` moved into that block.
- `src/lua_runner/expression.cpp` (238 lines): `operand_error` fallback; `binop<Op>`, `unary_metamethod`, `unary_function` overload sets; the six methods as shared lambdas on `const AbstractExpression& self`, listed in the `Expression` usertype and attached to `BinaryFile` through the indexer; `"metadata"` renamed `"get_metadata"`; MSVC C4702 push/disable/pop around the includes. The two old operand helpers and the `sol::object` binop are gone.
- `src/lua_runner/binary.cpp`: only the `"get_metadata"` entry removed (now shared).
- Tests: three `:metadata()` calls renamed; 13 new tests (FileAggregateAgents, ExtraArgumentsThrow, OperandErrorsForMissingAndMixedOperands, SameFileOnBothSides, FileAggregate, FileSelectAgents, FileRenameAgents, FileSaveKeepsFileOpen, FileSaveGuards, FileGetMetadata, ExpressionGetMetadata, ExpressionOfFileIsAnExpression, FileAndExpressionKeepTableIndex). The two pinned operand-error bodies are byte-identical to BASE, with no diff hunk inside them (gate hunk check).
- Docs: CHANGELOG `[0.13.0]` gains two BREAKING Lua entries and an `### Added` entry; `LUA_DB_API_REFERENCE` documents files as expressions, the six methods, `e:get_metadata()` and the arity rule (lua-api sync test 6 pass, biome clean); src/AGENTS.md file map, layout, shared-helpers, sandbox and a new sol2 traits bullet; root AGENTS.md `expr:get_metadata()`.

## Golden surface delta (reviewed) and baseline recapture

`GOLDEN_CHANGE=1 golden.sh debug` showed only `surface.txt` changing: BinaryFile gained `aggregate`, `aggregate_agents`, `rename_agents`, `save`, `select_agents`; Expression lost `metadata` and gained `get_metadata`. `surface_delta.sh` printed `SURFACE DELTA OK` (every other usertype, the quiver table, every other golden file including `binary.txt`, and the Debug text byte-identical to the previous baseline; both usertypes keep `__index:table`). The baseline was recaptured once (`golden.sh debug --capture`, CAPTURED debug) in Task 1; the previous baseline is kept at `build/typed-check/baseline-phase7/`. `eq.sh debug` still prints `[true,true,false]`.

## Mutation checks (adversarial self-verification, reverted, never committed)

- (a) Trait lines deleted from internal.h: 40 of 41 LuaExpressionTest tests fail, including FileAggregateAgents, FileAggregate, FileSaveKeepsFileOpen and OperatorMetamethodsOnFileAndExpression (without the traits even an Expression is not an AbstractExpression to sol2). Only OperandErrorsReportTheLeftmostBadOperand (no expression operand) passes. Restored with `git checkout -- src/lua_runner/internal.h`.
- (b) `number_operands < arity` dropped from operand_error: OperandErrorsNameTheOperation fails (`quiver.gt(1, 2)` reports too many arguments instead of `got number`). Restored with `git checkout -- src/lua_runner/expression.cpp`.
- `git diff --quiet -- src` exited 0 after both reverts; the gate passed again (lua=490).

## Tracer gate

Auto mode was off, so the workflow called for a human-verify checkpoint after the tracer commit. Per the user's standing instruction to self-verify tracer and human-verify gates adversarially, the check was delegated: the tracer's end-to-end gate passed, then Task 2's 12 tests and the two mutation checks above served as the adversarial verification. Verdict: `source: delegated (Claude, adversarial)`, passed.

## Flagged assumptions (restated)

- **FA-1:** expression *methods* (`save`, `get_metadata`, `aggregate`, `aggregate_agents`, `select_agents`, `rename_agents`) keep ignoring extra arguments like every other Lua method; only the twelve `quiver.*` expression functions and the operator metamethods are strict. Whether methods should be strict is unresolved.
- **FA-2:** no alias keeps `e:metadata()`; an old script fails with Lua's `attempt to call a nil value (method 'metadata')`. The CHANGELOG BREAKING line and the reference are the only migration aids.
- **FA-3 outcome:** the traits form works at runtime. The first Step 5 run passed all three tests (FileAggregateAgents and both pinned operand-error tests) on the first build, and a raw file is accepted as `const AbstractExpression&`. No switch to the runtime tag was needed.
- **FA-4:** a directly called `__unm` / `__bnot` with extra arguments reports `expected 2` (Lua passes a unary metamethod its operand twice). Not pinned, not documented.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] A Task 1 comment broke the lua-api sync test**
- **Found during:** Task 3 (running `bun test test/lua-api-sync.test.ts`)
- **Issue:** A comment in `bind_expression` contained the text `new_usertype<Expression>`. The sync test's second pass treats any line that matches it as opening that usertype, so it reset the parsed Expression method list to empty, and "parse found the binding surface" failed. The Task 1 gate did not run the sync test, so commit `5c7469a` shipped with the sync test red.
- **Fix:** Reworded the comment to "from the Expression usertype above" in the Task 3 commit. Added the sync test to `build/typed-check/gate.sh` (it now prints `SYNC OK`), and noted the trap in the src/AGENTS.md layout bullet.
- **Files modified:** src/lua_runner/expression.cpp (comment only)
- **Commit:** 9d250f2

**2. [Wording] lua-api.ts arity sentence wrapping**
- The too-many-arguments text is kept on one line so the literal `too many arguments (expected N, got M)` stays greppable (acceptance criterion). No behaviour change.

**3. [Minor] Test spelling in two positive scripts**
- The plan wrote `(fa + 1.0):save('expr_out')` and `(fa - fa):save('expr_zero')`. The tests assign the expression to a local first (`local sum = fa + 1.0; sum:save(...)`), which avoids Lua's ambiguous-call parse of a line that starts with `(`. The semantics are the same.

No ROADMAP wording deviations beyond these. The C++ core, C API, Julia, Dart and Python are untouched. The gate's scope check confirms that `src/lua_runner` changed only in internal.h, expression.cpp and binary.cpp.

## Known Stubs

None.

## Threat Flags

None. The new `f:save` entry point is the one the plan's threat model already covers (T-08-01..03, T-08-06), and FileSaveGuards and FileSaveKeepsFileOpen pin each mitigation.

## Self-Check: PASSED

- Files: src/lua_runner/internal.h, src/lua_runner/expression.cpp, src/lua_runner/binary.cpp, tests/test_lua_expression.cpp, CHANGELOG.md, bindings/js/src/lua-api.ts, src/AGENTS.md, AGENTS.md all present and modified; build/typed-check/{BASE, gate.sh, surface_delta.sh, baseline-phase7/} present (gitignored).
- Commits 5c7469a, d384e72, 9d250f2 present in `git log`.
