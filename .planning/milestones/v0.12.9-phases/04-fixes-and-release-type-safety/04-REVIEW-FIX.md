---
phase: 04-fixes-and-release-type-safety
fixed_at: 2026-10-03T00:00:00Z
review_path: .planning/phases/04-fixes-and-release-type-safety/04-REVIEW.md
iteration: 1
findings_in_scope: 3
fixed: 3
skipped: 0
status: all_fixed
---

# Phase 4: Code Review Fix Report

**Fixed at:** 2026-10-03
**Source review:** .planning/phases/04-fixes-and-release-type-safety/04-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 3 (critical_warning: CR-01, WR-01, WR-02)
- Fixed: 3
- Skipped: 0
- Info findings IN-01..IN-06 are out of scope and were not touched.

**Where verification ran:** everything ran in the main checkout on `rs/runner`, with no worktree (as instructed). The numbers can be reproduced from this tree.

## Fixed Issues

### CR-01: `LuaRunner::run` still executes precompiled bytecode

**Files modified:** `src/lua_runner/lua_runner.cpp`, `tests/test_lua_runner_errors.cpp`, `CHANGELOG.md`, `AGENTS.md`, `src/AGENTS.md`
**Commit:** c666ced
**Applied fix:**
- `run()` now calls `safe_script(script, sol::script_pass_on_error, sol::detail::default_chunk_name(), sol::load_mode::text)`.
- New test `LuaRunnerTest.RunRefusesBinaryChunks`. A first `run()` returns hex-encoded `string.dump` output, because a JSON result must be UTF-8. The test decodes it and passes the raw bytecode to `run()`. It expects `attempt to load a binary chunk (mode is 't')`.
- The CHANGELOG `[0.13.0]` BREAKING entry now covers both `load` and the script given to `run()` (and therefore `quiver_cli`), and tells the reader to ship Lua source, not bytecode. The sandbox decision in root `AGENTS.md` and the `string.dump` note in `src/AGENTS.md` say the same.

**Red / green / mutation:**
- Red: on the pre-fix tree the test failed with `a binary chunk should not run`, because the bytecode executed.
- Green: the test passes after the fix.
- Mutation: after the commit, setting the mode back to `sol::load_mode::any` made the test fail again (`a binary chunk should not run`). Restored afterwards.

### WR-01: The reported operand depends on the compiler (`binop`, `ifelse`)

**Files modified:** `src/lua_runner/binary.cpp`, `tests/test_lua_expression.cpp`
**Commit:** b21f98d
**Applied fix:**
- In the both-expression branch of `binop`, and in `quiver.ifelse`, the `to_expression` decodes are now locals in argument order (`a`, `b` / `cond`, `then_value`, `else_value`). The leftmost bad operand is reported in every build. The operands are passed by `const Expression&`, so no `std::move` is needed.
- New test `LuaExpressionTest.OperandErrorsReportTheLeftmostBadOperand`. `quiver.gt('a', {})` must report `got string`, and `quiver.ifelse(5, {}, 'x')` must report `got number`. The test does not depend on the build type.

**Red / green / mutation:**
- Red: on the pre-fix tree (MSVC Debug) both assertions failed. The errors were `Cannot gt: ... got table` and `Cannot ifelse: ... got string`, i.e. the rightmost bad operand.
- Mutation: after the commit, reversing the order of the locals produced the same two wrong messages, and the test failed. Restored afterwards.
- Golden: `golden.sh debug` and `golden.sh release` are unchanged (`GOLDEN ... OK`). No golden probe passes two bad operands.

### WR-02: Two destructive behaviours in the empty-array BREAKING entry have no test

**Files modified:** `tests/test_lua_runner_update.cpp`, `CHANGELOG.md`
**Commit:** bc38384
**Applied fix:** tests only, with no production change. Three new tests:
- `UpdateElementEmptyArrayClearsEveryGroupSharingTheColumn` (`relations.sql`): `{ parent_ref = {} }` clears both `Child_vector_refs` and `Child_set_parents`. `mentor_id` and `score` are untouched.
- `UpdateElementEmptyDateTimeClearsEveryTimeSeriesGroup` (`multi_time_series.sql`): `{ date_time = {} }` clears both the `temperature` and the `humidity` groups.
- `UpdateElementRoundTripOfReadVectorsById` (`collections.sql`): the `read_vectors_by_id` -> `update_element` round trip.
  - When `value_int` is `{1, 2}` and `value_float` is all NULL, the call throws `must have the same length` and the data is unchanged.
  - When both columns are all NULL, the round trip deletes the group's rows.

**CHANGELOG correction:**
- The old sentence ("clears that group when it is the group's only column in the call") did not cover the second round-trip case, where two columns are in the call and both are empty.
- It now reads "clears that group when no other column of the group in the call is non-empty (so rows whose cells are all NULL are deleted)".
- It also says that `{ date_time = {} }` clears every time-series group, since they all share `date_time`.
- The behaviour itself is the decided rule and was not changed.

**Red / green / mutation:**
- Red: no production change was made, so there was no pre-fix failure. The tests pass on the current tree.
- Mutation: after the commit, putting back the old Lua behaviour in `table_to_element` (`continue` instead of `element.set(k, std::vector<int64_t>{})`) made all three tests fail. Restored and rebuilt afterwards.

## Gates

- `bash build/fixes-check/gate.sh` returned `GATE PASS` after each fix: `lua=473`, then `474`, then `477` (472 plus the 5 new tests). The C API run passed 27/27, the JS `lua-api-sync` test passed, `.set_function(` stayed at 86, and `GOLDEN debug OK`.
- clang-format 22.1.8 `--dry-run --Werror` reported nothing on every touched C++ file.
- Release (`cmake --build build/release`): Lua* passed 477/477, `LuaRunnerCApiTest*` passed 27/27, and `GOLDEN release OK`.
- The first WR-02 gate run failed on the planning-ID regex, because the test label `"C1"` matched `\b[CM][0-9]{1,2}\b`. The labels were renamed to `Parent 1` / `Child 1` / `Sensor 1` before the commit.

---

_Fixed: 2026-10-03_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
