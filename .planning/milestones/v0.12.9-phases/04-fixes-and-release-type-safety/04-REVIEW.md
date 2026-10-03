---
phase: 04-fixes-and-release-type-safety
reviewed: 2026-10-03T00:00:00Z
depth: standard
files_reviewed: 23
files_reviewed_list:
  - AGENTS.md
  - CHANGELOG.md
  - bindings/js/src/lua-api.ts
  - src/AGENTS.md
  - src/CMakeLists.txt
  - src/lua_runner/binary.cpp
  - src/lua_runner/csv.cpp
  - src/lua_runner/db_core.cpp
  - src/lua_runner/db_metadata.cpp
  - src/lua_runner/db_time_series.cpp
  - src/lua_runner/db_write.cpp
  - src/lua_runner/internal.h
  - src/lua_runner/lua_runner.cpp
  - tests/AGENTS.md
  - tests/test_lua_binary.cpp
  - tests/test_lua_expression.cpp
  - tests/test_lua_runner_create.cpp
  - tests/test_lua_runner_errors.cpp
  - tests/test_lua_runner_query.cpp
  - tests/test_lua_runner_time_series.cpp
  - tests/test_lua_runner_transaction.cpp
  - tests/test_lua_runner_update.cpp
  - tests/test_lua_runner_write_csv.cpp
findings:
  critical: 1
  warning: 2
  info: 6
  total: 9
status: issues_found
---

# Phase 4: Code Review Report

**Reviewed:** 2026-10-03
**Depth:** standard
**Files Reviewed:** 23
**Status:** issues_found

## Summary

I reviewed the Phase 4 diff (`b39fe78..HEAD`) at standard depth. I traced every changed decoder and
read the core functions it calls (`prepare_group_data`, `validate_group_columns`,
`BinaryMetadata::from_element`, and the transaction and dry-run methods). I checked the sol2
defaults in the fetched sol2 sources and ran probes against the built Debug and Release
`quiver_cli`.

Most of the phase holds up:
- `require_table`, `lua_string_key` and `optional_from_lua` sit where sol2's old up-front check fired. The containment-before-options order is preserved in `open_file`, `bin_to_csv`, `export_csv`/`import_csv` and `write_csv`.
- `run_in_scope` checks `fn` before `begin`. It never rolls back a transaction it did not open, because `begin` throws outside the `try`. It also rethrows the original error.
- The `load` wrapper forwards env correctly: a missing env means the global environment and an explicit `nil` stays nil. A script cannot reach the original `load`, because `debug` is not loaded.
- No unguarded `.as<T>()` is left on a script-controlled value under `SOL_SAFE_GETTER=0`.
- No test is gated on a build type.

**The critical finding:** the top-level script still loads bytecode. `LuaRunner::run` calls `safe_script` with sol2's default `load_mode::any`, so a precompiled chunk passed as the script runs. I reproduced this in Debug and Release with the shipped `quiver_cli`. The phase's text-only guarantee therefore covers `load` only, not the sandbox's main entry point.

## Critical Issues

### CR-01: `LuaRunner::run` still executes precompiled bytecode, so the text-only gate can be bypassed

**File:** `src/lua_runner/lua_runner.cpp:157`

**Issue:** `impl_->lua.safe_script(script, sol::script_pass_on_error)` uses sol2's default `load_mode mode = load_mode::any` (`build/_deps/sol2-src/include/sol/state_view.hpp:382-383`). The `load` wrapper (lines 106-109) forces mode `"t"`, but the script passed to `run()` is loaded with no mode restriction. Its rationale ("Lua does not verify bytecode, so a crafted binary chunk could read and write host memory") applies equally to the script.

Reproduced:
```
luac.exe -o s.luac s.lua          # s.lua: return "bytecode ran"
quiver_cli.exe t.db s.luac        # Debug:   "bytecode ran", exit 0
build/release/bin/quiver_cli.exe t.db s.luac   # Release: "bytecode ran", exit 0
```
`quiver_cli` reads the script with `std::ios::binary` (`src/cli/main.cpp:11`), and `LuaRunner::run(const std::string&)` takes arbitrary bytes, so the C++ API and the CLI are both open.

The FFI bindings are not affected only by accident. `quiver_lua_runner_run` takes a NUL-terminated `const char*`, and byte 5 of every Lua 5.4 bytecode header is `0x00` (`1b 4c 75 61 54 00 ...`).

Root `AGENTS.md:86-87` ("a precompiled binary chunk is refused") and the CHANGELOG's text-only entry both describe a sandbox guarantee that does not hold at the entry point. No test covers it: `LoadRefusesBinaryChunks` only exercises `load`.

**Fix:**
```cpp
auto result = impl_->lua.safe_script(
    script, sol::script_pass_on_error, sol::detail::default_chunk_name(), sol::load_mode::text
);
```
Add a test that builds bytecode in a scratch `sol::state` (`string.dump(function() return 1 end)`) and passes it to `lua.run(...)`. Expect the error to contain `attempt to load a binary chunk (mode is 't')`. Then extend the AGENTS.md sandbox sentence and the CHANGELOG entry to cover the script itself.

## Warnings

### WR-01: The reported operand depends on the compiler: `binop` and `ifelse` decode several operands inside one argument list

**File:** `src/lua_runner/binary.cpp:134`, `src/lua_runner/binary.cpp:324`

**Issue:** `Op{}(to_expression(lhs, operation), to_expression(rhs, operation))` and `quiver::ifelse(to_expression(c, ...), to_expression(t, ...), to_expression(e, ...))` evaluate their arguments in an unspecified order. Before this phase every operand error had identical text, so the order was invisible. Now the message ends in `got <type>`, so the order becomes observable.

The MSVC Debug build reports the rightmost bad operand:
- `quiver.gt('a', {})` reports `Cannot gt: ... got table`.
- `quiver.ifelse(5, {}, 'x')` reports `... got string`.

Clang evaluates left to right and would report `got string` and `got number`. This contradicts the policy the phase applied everywhere else (`file:write` decodes `data` first "so data is the one reported", and the src/AGENTS.md text says the hoisting means "which bad argument wins no longer depends on the compiler"). `OperandErrorsNameTheOperation` does not catch it, because it never passes two bad operands of different types.

**Fix:** Hoist the decodes into locals in argument order:
```cpp
// binop, both-expression branch
auto a = to_expression(lhs, operation);
auto b = to_expression(rhs, operation);
return Op{}(std::move(a), std::move(b));

// ifelse
auto cond = to_expression(c, "ifelse");
auto then_ = to_expression(t, "ifelse");
auto else_ = to_expression(e, "ifelse");
return quiver::ifelse(cond, then_, else_);
```
Add one assertion such as `quiver.gt('a', {})`, which should report `got string`.

### WR-02: Two destructive behaviours in the empty-array BREAKING entry have no test

**File:** `src/lua_runner/db_write.cpp:65-68`, `CHANGELOG.md:34-43`, `tests/test_lua_runner_update.cpp`

**Issue:** The CHANGELOG and the `lua-api.ts` reference both state: "A column name shared by several groups clears every one of them." This follows from `prepare_group_data` routing an empty array to every table in `find_all_tables_for_column`. The same mechanism means `{ date_time = {} }` in `update_element` clears every time-series group of the collection, because they all share `date_time`.

Neither path is tested:
- `UpdateElementEmptyArrayClearsGroup` covers only single-group vector and set columns.
- `UpdateElementEmptyArrayErrors` covers only the length mismatch and the typo case.
- The documented `read_vectors_by_id -> update_element` round trip (D-12) is not exercised either.

These are the data-destroying edges of a BREAKING change in a phase whose rule is "each fix lands with its own test". A later change to the core's routing could silently change them.

**Fix:** Add one test on `relations.sql` (`parent_ref` is shared by a vector and a set group): `db:update_element("Child", 1, { parent_ref = {} })` must clear both groups. Add one time-series case asserting that `{ date_time = {} }` clears every time-series group. Or, if clearing them all is not intended, reject an empty array that matches more than one table.

## Info

### IN-01: The argument type errors now come in two shapes

**File:** `src/lua_runner/csv.cpp:351`, `src/lua_runner/csv.cpp:173`, `src/lua_runner/csv.cpp:241`, `src/lua_runner/db_core.cpp:39`, `src/lua_runner/internal.h:270`, `src/lua_runner/db_write.cpp:189`

**Issue:** `db:transaction`/`db:dry_run` report `fn must be a function, got number`, while the model those checks were copied from reports `Cannot read_csv_stream: on_row must be a function` with no type. The option-value checks (`separator`, `date_time_format`, `header_row`) and `option key must be a string` also lack the `got <type>` suffix that D-01 defines as the one shape.

**Fix:** Route these checks through `lua_type_error` when their pinned texts are next re-baselined. The substring tests stay green.

### IN-02: `tests/AGENTS.md` overstates the sol2 flag set

**File:** `tests/AGENTS.md:146`

**Issue:** The file says "every other sol2 safety is on in both builds", but `SOL_SAFE_STACK_CHECK=0` is also set (`src/CMakeLists.txt`). The CMake comment justifies disabling the stack check only through the joint +16% measurement. Only the getter was shown to need it.

**Fix:** Change the sentence to "every other sol2 safety except the stack check". Optionally measure `SOL_SAFE_STACK_CHECK=1` on its own and turn it back on if it fits the budget.

### IN-03: The `quiver.metadata_from_element` behaviour change is not in the CHANGELOG

**File:** `src/lua_runner/db_write.cpp:65-68`, `src/lua_runner/binary.cpp:272-274`

**Issue:** `table_to_element` also backs `quiver.metadata_from_element`. An empty array there used to be dropped, which produced `Cannot from_element: missing array 'labels'`. It is now passed through, so the same call reports `Number of labels must be positive, got 0` (and likewise for `dimensions`). The CHANGELOG entry only mentions `create_element`/`update_element`.

**Fix:** Add a sentence to the empty-array entry, or to the type-check entry that already lists `quiver.metadata_from_element`.

### IN-04: The Lua `data_type` strings now depend on `data_type_to_string`

**File:** `src/lua_runner/db_metadata.cpp:13-21`

**Issue:** The Lua metadata `data_type` value used to be an explicit switch. It is now derived by lowercasing `data_type_to_string`, which `describe()` also uses for its text output. Renaming a core display string would silently change a Lua API value. The file also relies on a transitive include for `quiver/data_type.h`.

**Fix:** Add `#include "quiver/data_type.h"` and a test that pins the four Lua spellings. A test may already pin them indirectly; if so, nothing else is needed.

### IN-05: The failure paths in `run_in_scope` (pre-existing)

**File:** `src/lua_runner/db_core.cpp:85-97`

**Issue:** There are two gaps, both pre-existing:
- `fn(std::ref(self))` sits outside the `try`. A C++ exception from sol2's argument push (OOM) would leave the scope open without calling `abort`.
- For `dry_run`, a failed `end_dry_run` in the `abort` slot is swallowed. If `end_dry_run` throws "rollback left the transaction open", the dry-run flag stays set. The script sees only the callback's error and its later `commit()` calls are silently absorbed.

**Fix:** Move the call inside the `try`. For `dry_run`, consider letting an abort failure surface, for example by appending it to the rethrown message.

### IN-06: The CHANGELOG has no `[0.12.9]` section or link (pre-existing)

**File:** `CHANGELOG.md:77`, `CHANGELOG.md:1366`

**Issue:** `v0.12.9` is tagged and the new link compares `v0.12.9...v0.13.0`, but the file jumps from `[0.13.0]` to `[0.12.8]`, and the link block has no `[0.12.9]` entry.

**Fix:** Add the missing section and its compare link, or record why 0.12.9 has none.

---

_Reviewed: 2026-10-03_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
