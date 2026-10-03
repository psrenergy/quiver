---
phase: 03-dedupe
reviewed: 2026-10-03T00:00:00Z
depth: standard
files_reviewed: 11
files_reviewed_list:
  - src/AGENTS.md
  - src/csv/csv_write.cpp
  - src/lua_runner/binary.cpp
  - src/lua_runner/csv.cpp
  - src/lua_runner/db_core.cpp
  - src/lua_runner/db_metadata.cpp
  - src/lua_runner/db_read.cpp
  - src/lua_runner/db_time_series.cpp
  - src/lua_runner/db_write.cpp
  - src/lua_runner/internal.h
  - src/lua_runner/lua_runner.cpp
findings:
  critical: 0
  warning: 0
  info: 8
  total: 8
status: issues_found
---

# Phase 3: Code Review Report

**Reviewed:** 2026-10-03
**Depth:** standard
**Files Reviewed:** 11
**Status:** issues_found (Info only)

## Summary

I reviewed `git diff 3c13209..HEAD -- src` commit by commit against the locked constraints in
03-CONTEXT.md. I found no behaviour regression. Every item in the focus list was traced:

- **Check order and message text.** The checks run in the same order in `option_entries` (the table check moved inside it, still after each caller's nil early-return; `quiver.metadata` still lets nil reach it), in `option_table` + `collect_entries` (type check, then collect, at each `enum_labels` level and for `header`), in `columns_to_cpp_rows` (the no-rows throw is its first statement, so it still fires after every decoder-specific check: dimension presence, length, nil-in-dimension, value-column length) and in `read_groups_by_id`'s default branch. Every message string matches the old one byte for byte.
- **`std::optional` returns.** sol2's `std::optional` pusher sends `nullopt` as one `nil`, so `query_*` still return exactly one value, and assigning it to a `read_scalars_by_id` key leaves the key absent. Lua 5.4's `luaH_newkey` does not insert a nil value. This is the same as the old `sol::make_object(..., lua_nil)`. `header_object` assigns nil to `result.header`, which likewise leaves the key absent.
- **Template adapters and member pointers.** All 17 member pointers name non-overloaded members with no default arguments (checked against `include/quiver/database.h`), so sol2 sees the same arity. `bulk_read_lua`, `collection_read_lua`, `list_metadata_lua`, `get_metadata_lua` and `read_groups_by_id` each bind the member their registration names.
- **`lua_to_value` vs the old `csv_cell_to_string`.** A row hole (`row[i]`) is a `LUA_REFNIL` object, and sol2's `is<lua_nil_t>()` answers true for it (`object_base.hpp:55-56`). That covers what the old `!cell.valid()` arm caught. The dispatch order is the same: boolean, then int64, double, string. A boolean writes `"1"`/`"0"` through the int64 overload. An integer-valued float (`2.0`) still takes the double branch under `SOL_NUMBER_PRECISION_CHECKS`. A non-finite value is rejected before `append_number` with the same row/cell text. An unsupported value gets the same `cell #N has unsupported Lua type` text.
- **Prune-on-insert.** Only `expired()` entries are dropped. A closed handle that is still alive stays, so `path_has_open_writer` and the WRITE-08 truncate path are unchanged. `std::erase_if` keeps the order of the survivors, so `close_open_handles` closes in the same order as before.
- **`binop<Op>`.** The transparent functors resolve the same ADL `quiver::` overloads the old `apply_binop` switch called, with the same argument categories. `logical_and<>`/`logical_or<>` reach the overloaded `&&`/`||`, which evaluate both operands. In C++20, a non-rewritten `==`/`!=` candidate still beats a reversed one. NaN semantics live in `ExpressionBinary` and are untouched. `number op number` still throws from `to_expression`.

Verification I ran: a Debug build is current, and `quiver_tests --gtest_filter=Lua*` passes 444/444 across 12 suites. `quiver_c_tests` `LuaRunner*` passes 27/27. `bun test test/lua-api-sync.test.ts` passes 6/6. I rebuilt Release, and `build/dedupe-check/golden.sh release` printed `GOLDEN release OK` (its probes cover booleans, NaN, `2.0`, min/max integers, nil holes, `header_row` of `"2"`/`2.5`/`2.0`/`true`, and boolean option keys). clang-format 22.1.8 is clean on every touched file, and the largest file is 420 lines (`csv.cpp`).

What is left is documentation accuracy in the new `src/AGENTS.md` text, one robustness note on a merged check, and pre-existing defects carried over unchanged.

## Narrative Findings (AI reviewer)

## Info

### IN-01: AGENTS.md overstates what prune-on-insert achieves

**File:** `src/AGENTS.md:859-862` (also `src/lua_runner/lua_runner.cpp:34-36`)
**Issue:** The text says "a long script that opens and drops many handles does not grow the registry until `run()` returns". An entry is pruned only once the Lua GC has actually collected its handle, and only at the next `add_*` call. A script that drops handles faster than the GC collects them still grows the list. So does a script that closes handles but keeps them alive, for example by collecting them in a table, because closed-but-alive entries are kept on purpose. The code is correct. The prose promises a stronger bound than the code gives.
**Fix:** Reword to: "...which first prune entries whose handle the GC has already collected (`expired()`), never a closed-but-alive one, so the registry holds only handles that are still alive plus any dropped since the last collection, not every handle the run ever opened."

### IN-02: Three imprecise statements in the new "Shared helpers" bullet

**File:** `src/AGENTS.md:664`, `src/AGENTS.md:669`, `src/AGENTS.md:677`
**Issue:**
- Line 664: "`query_*_lua` and `read_scalars_by_id` return `std::optional`". `read_scalars_by_id_lua` returns a `sol::table`. Only the per-attribute values it assigns are `std::optional`.
- Line 669: "`lua_to_value` is the one write-path dispatch". This conflicts with the bullet at line 735, which correctly says the 1/0 mapping lives in *two* converters (`lua_to_value` and `lua_cell_as<T>`).
- Line 677: "`length_mismatch` (`db_time_series.cpp`) their length message". "their" reads as both group decoders. Only the time-series decoder has a length check. The vector/set decoder derives its row count from the maximum extent and has no length message.
**Fix:** Line 664: "`query_*_lua` return `std::optional` and `read_scalars_by_id` assigns `std::optional` values, so a NULL is `nil` and an absent key." Line 669: "`lua_to_value` is the one `Value`-typed write dispatch, CSV cells included". Line 677: "...and `length_mismatch` (`db_time_series.cpp`) is the time-series decoder's one length message."

### IN-03: "The only way into the run-handle registries" is a convention, not enforced

**File:** `src/lua_runner/internal.h:41,47`; `src/AGENTS.md:672`
**Issue:** `RunHandles::open_writers` and `open_binary_files` are still public data members. A later binder could `push_back` directly and skip the prune without any compile error. The doc states this as a guarantee.
**Fix:** Make the two vectors `private:`. `path_has_open_writer`, `add_*` and `close_open_handles` are already members, and nothing outside `RunHandles` touches the vectors any more (grep confirms only `lua_runner.cpp` does). Alternatively, soften the doc to "are appended only through".

### IN-04: The `is_lua_boolean` comment's list of `lua_to_value` callers misses CSV cells

**File:** `src/lua_runner/internal.h:104-107`
**Issue:** "The Value mapping itself lives in lua_to_value (scalars, row upserts, query parameters, group cells)". After M10, `csv_cell_to_string` also goes through `lua_to_value`. The list predates that change and is now incomplete.
**Fix:** Add "CSV cells" to the list. Or drop the list and point to the `src/AGENTS.md` boolean bullet, which is now the complete list.

### IN-05: The merged `header_row` check now depends on a build flag for string rejection

**File:** `src/lua_runner/csv.cpp:236-242`
**Issue:** The old code ran `get_type() != number` first and then `is<int64_t>()`. The merged single `is<int64_t>()` behaves the same only while `SOL_STRINGS_ARE_NUMBERS` is off. Under that flag, sol2's integer checker switches to `lua_tointegerx` (`stack_check_unqualified.hpp:132-136`), so a quoted `"2"` would pass. The old explicit type guard did not depend on that flag. Behaviour is unchanged today (the golden `options.lua` probes `"2"`, `2.5`, `2.0` and `true`), and the comment documents the dependency.
**Fix:** No change is needed for Phase 3. If Phase 4 touches sol2 macros (`SOL_ALL_SAFETIES_ON`), re-run those `header_row` probes. Or restore the one-line `get_type() != sol::type::number` pre-check, which uses the same message text.

### IN-06 (pre-existing): A dot-call on any `db:`/`w:` method segfaults the process in Release

**File:** `src/lua_runner/db_core.cpp:123-138` (all member-pointer and lambda registrations), `src/lua_runner/csv.cpp:409-416`
**Issue:** I confirmed this directly. `return db.commit()` inside `pcall` makes `build/release/bin/quiver_cli.exe` exit 139 (segmentation fault). Debug reports sol2's "received nil for 'self' argument". With `SOL_SAFE_USERTYPE` off, sol2 dereferences a null self pointer. The old lambdas (`Database& self`) did the same, so this is not a Phase 3 regression, and the harness's `debug_text.lua` already notes it as undefined behaviour in Release. An untrusted script can still take down the host process with one typo.
**Fix:** This is Phase 4 (`SOL_ALL_SAFETIES_ON`, or at least `SOL_SAFE_USERTYPE=1` in `src/CMakeLists.txt`). Add a Release test that pins a Pattern-style error for `db.commit()`.

### IN-07 (pre-existing): `lua_table_to_dim_map` converts keys without checking their type

**File:** `src/lua_runner/binary.cpp:30`
**Issue:** `pair.first.as<std::string>()` runs on every key with no `get_type() == string` check. This is the same hazard `option_entries` and `collect_group_columns` guard against. In Release a boolean key gives an unchecked `lua_tolstring` nullptr, and in Debug a sol2 panic. A number key is silently spelled as text. This was carried over unchanged, and it is the Phase 4 "key type checks" item (IN-03 from Phase 2).
**Fix:** In Phase 4, add `if (pair.first.get_type() != sol::type::string) throw std::runtime_error("Cannot " + caller + ": dimension key must be a string");` before the conversion.

### IN-08 (pre-existing): The write_csv message catalogue misses one throw

**File:** `src/csv/csv_write.cpp:9-11,26-44`
**Issue:** The catalogue says it lists "Every throw the db:write_csv / w:write_row / w:close feature can raise, wherever it lives". It leaves out `"Cannot write_csv: file is already open for writing: <path>"` (`src/lua_runner/csv.cpp:394`), which `tests/test_lua_runner_write_csv.cpp:1438` pins. This phase rewrote the paragraph header right above the list, but the omission predates the phase.
**Fix:** Add `//   "Cannot write_csv: file is already open for writing: <original_path>"   (two live writers on one resolved path)` to the csv.cpp-raised block.

---

_Reviewed: 2026-10-03_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
