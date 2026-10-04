---
phase: 08-typed-expression-parameters-in-lua
reviewed: 2026-10-04T00:00:00Z
depth: standard
files_reviewed: 8
files_reviewed_list:
  - AGENTS.md
  - CHANGELOG.md
  - bindings/js/src/lua-api.ts
  - src/AGENTS.md
  - src/lua_runner/binary.cpp
  - src/lua_runner/expression.cpp
  - src/lua_runner/internal.h
  - tests/test_lua_expression.cpp
findings:
  critical: 0
  warning: 2
  info: 3
  total: 5
status: issues_found
---

# Phase 8: Code Review Report

**Reviewed:** 2026-10-04
**Depth:** standard
**Files Reviewed:** 8
**Status:** issues_found

## Summary

Reviewed `git diff 1bea16a..HEAD` for the eight files. The code change is sound. I checked it three ways:

- **Reading sol2.** With `SOL_DERIVED_CLASSES`, the checker in `stack_check_unqualified.hpp` and the getter in `stack_get_unqualified.hpp` read `class_check`/`class_cast` from the derived metatable. `usertype_storage.hpp:1099` registers both on every usertype regardless of base form, so a `const AbstractExpression&` parameter accepts both `BinaryFile` (including the `d::u<BinaryFile>` shared_ptr metatable) and `Expression`, with correct pointer adjustment.
- **Probing `quiver_cli` at HEAD.** The Debug build was up to date. `operand_error` gives the documented message on every path I tried:
  - Missing, nil, string, table, boolean or userdata operands.
  - number/number.
  - `quiver.gt(1, e, 3)`.
  - Direct metamethod calls.
  - `e + 1` with a Lua integer is accepted through the `double` candidate.
  - `"1" + e` is refused, the same as the old `is_number`.
- **Running tests.** `LuaExpression*:LuaBinary*` passes 72/72, and the lua-api sync test passes 6/6.

Other checks that came back clean:
- The "too many arguments" branch only fires after every operand position has validated, so the `expected N, got N` case cannot happen.
- Every sol2 TU picks up the traits through `internal.h`, which includes sol2 itself before the specializations.
- The Linux GCC/Clang logs in `build/typed-check/` show no new warnings.

Neither warning was introduced by this phase. Both are pre-existing behaviours that this phase makes newly reachable from files and documents in the agent-facing reference, without saying anything about them.

## Warnings

### WR-01: `f == g`, `e == e2`, `f < g` silently evaluate to `true`, and the reference now invites comparing files

**File:** `bindings/js/src/lua-api.ts:886-895` (behaviour from sol2's automagic registration on the usertypes built in `src/lua_runner/binary.cpp:~170` / `src/lua_runner/expression.cpp:172`)
**Issue:** sol2's `insert_default_registrations` sees `operator==`/`operator<`/`operator<=` on `const AbstractExpression&` (they return `Expression`). It binds `__eq`/`__lt`/`__le` through `comparsion_operator_wrap`, which pushes the resulting `Expression` userdata, and Lua coerces that to `true`. Verified at HEAD:

```
fa == fb  => true    (two distinct files with different data)
fa <  fb  => true
e  == (fa + 1.0) => true
```

So `if fa == fb then` or `if e1 < e2 then` always takes the branch, with no error. This predates the phase and is tracked as the deferred EQ-01 decision; `eq.sh` pins `[true,true,false]`. What is new is the text this phase added to the shipped LLM prompt payload: "A binary file **is** an expression: every operator ... takes a file handle directly". An agent reading that will reasonably write `f == g` or `e < 3.0`-style comparisons on handles, and nothing tells it those operators are the trap.
**Fix:** The minimal, in-scope fix is one sentence in `LUA_DB_API_REFERENCE` next to the operators list:
```
`==`, `<`, `<=` on files or expressions do NOT compare values or identity (they always yield true);
use quiver.eq/lt/lte/gt/gte/neq for element-wise comparison.
```
The real fix is EQ-01. In `bind_expression_operators`, set `type[sol::meta_function::equal_to]` to an identity comparison, and make `less_than`/`less_than_or_equal_to` throw `Cannot lt: use quiver.lt for element-wise comparison`. That is for the maintainer to decide; it is not a drive-by change.

### WR-02: `f:save` on a file open for writing reports `Cannot open_file`, and the new test pins the wrong operation name

**File:** `tests/test_lua_expression.cpp:769-773` (message from `src/expression/expression.cpp:66`, `f->open('r')`)
**Issue:** The root AGENTS.md rule says Pattern 1 validators thread the operation through, so `{operation}` is the public method the user called. `w:save('expr_out')` raises `Cannot open_file: file is already open for writing`, but the script never called `open_file`. The message was already reachable through `quiver.expression(w):save(...)`. This phase adds a direct entry point (`w:save`), advertises it in the CHANGELOG ("refuses ... a file open for writing"), and pins the misnamed text in `FileSaveGuards`. A later fix to the core message now has to change this test, and the pin makes the wrong name look intended. The 08-01 summary records the decision not to touch the core, so this is a conscious deferral, but the deferral is not written down anywhere a reader of the test or the CHANGELOG would see it.
**Fix:** Either check for a writer-held input before `open('r')` in `AbstractExpression::save` and throw `Cannot save: input file '<path>' is open for writing` (one check in the shared core path, which covers `expr:save`, `f:save`, Julia and the C API at once), or, if the deferral stands, assert only the stable tail (`"file is already open for writing"`) in the test so the operation prefix is not frozen.

## Info

### IN-01: A directly called `__unm` / `__bnot` with one operand reports `got nil`

**File:** `src/lua_runner/expression.cpp:100-107`
**Issue:** `unary_metamethod` uses arity 2 because Lua passes the operand twice. So `getmetatable(e).__unm(e)`, a natural direct call, is refused with `Cannot unm: operand must be an expression or a binary file, got nil`. `__unm(e, 5)` reports `got number`. Both describe a valid call as a bad operand. Only direct metamethod calls reach this (FA-4 in the 08-01 summary), and `-e` / `~e` are correct.
**Fix:** Leave it, or give the typed set a one-argument candidate `[f](const AbstractExpression& o) { return f(o); }` so a direct call with one operand works.

### IN-02: The traits invariant is stated as "includes internal.h first" and is enforced only by a gitignored harness

**File:** `src/lua_runner/internal.h:34-40`, `src/AGENTS.md` (new sol2 traits bullet)
**Issue:** Both texts say every sol2 TU "includes this header first". That is not literally true: `expression.cpp`, `csv.cpp` and `lua_runner.cpp` include other headers before it. The invariant that actually matters is that the specializations are visible before any sol2 instantiation for `BinaryFile`/`Expression`/`AbstractExpression`. A future TU that includes `<sol/sol.hpp>` directly and touches these types without `internal.h` is an ill-formed-NDR ODR violation that compiles cleanly. The only check (`grep -L internal.h` in `build/typed-check/gate.sh`) lives in `build/`, which is not in CI.
**Fix:** Reword to "before any sol2 use of these types". Optionally make it structural by having every TU get sol2 through one header that also carries the traits. `internal.h` already includes `<sol/sol.hpp>`, so dropping the direct `#include <sol/sol.hpp>` lines from the TUs and noting that sol2 is only reached via `internal.h` would do it.

### IN-03: Root AGENTS.md still names only `expr:save` in the sandbox decision and still describes building from a file

**File:** `AGENTS.md:~107-110` (Lua sandbox design decision), `AGENTS.md:~818-833` (Lua expression paragraph)
**Issue:** The sandbox list in the root design decision still reads `... db:write_csv, expr:save`. `src/AGENTS.md` and `lua-api.ts` were updated to "`save` on a file or an expression". The expression paragraph still says "build from a file with `quiver.expression(file)`" and lists methods only as `expr:...`. That breaks the Self-Updating rule. The 08-01 summary defers the root decisions and cross-layer rows to Phase 9.
**Fix:** In Phase 9 (or now), change the sandbox list to "`save` on a file or an expression", and state in the expression paragraph that a file handle takes the six expression methods directly.

---

_Reviewed: 2026-10-04_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
