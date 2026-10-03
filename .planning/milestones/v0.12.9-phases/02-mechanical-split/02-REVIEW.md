---
phase: 02-mechanical-split
reviewed: 2026-10-03T00:00:00Z
depth: standard
files_reviewed: 27
files_reviewed_list:
  - AGENTS.md
  - bindings/dart/AGENTS.md
  - bindings/dart/hook/build.dart
  - bindings/js/AGENTS.md
  - bindings/js/src/lua-api.ts
  - bindings/js/test/lua-api-sync.test.ts
  - bindings/julia/AGENTS.md
  - cmake/Platform.cmake
  - src/AGENTS.md
  - src/CMakeLists.txt
  - src/csv/csv_read.h
  - src/csv/csv_write.cpp
  - src/csv/csv_write.h
  - src/lua_runner/binary.cpp
  - src/lua_runner/csv.cpp
  - src/lua_runner/db_core.cpp
  - src/lua_runner/db_metadata.cpp
  - src/lua_runner/db_read.cpp
  - src/lua_runner/db_time_series.cpp
  - src/lua_runner/db_write.cpp
  - src/lua_runner/internal.h
  - src/lua_runner/lua_runner.cpp
  - src/lua_runner/path_policy.cpp
  - src/lua_runner/return_json.cpp
  - tests/AGENTS.md
  - tests/test_database_ui_metadata.cpp
  - tests/test_lua_runner_write_csv.cpp
findings:
  critical: 0
  warning: 2
  info: 3
  total: 5
status: issues_found
---

# Phase 02: Code Review Report

**Reviewed:** 2026-10-03
**Depth:** standard
**Files Reviewed:** 27
**Status:** issues_found

## Summary

I checked the split against the monolith at `47bec06` on four fronts: what is registered, what the function bodies do, how the pieces link, and how the runner is created and torn down. I found no regression in behaviour.

**What I checked, and how:**

- **Registered names.** I diffed the registered names programmatically. The old file had 17 `Database` methods passed as variadic pairs plus 69 `set_function` calls; the new folder has 86 `bind.`/`ns.` `set_function` calls. Both sets are the same 86 names, with no duplicates and none missing.
- **Function bodies.** I compared the code lines of the old and new files after stripping whitespace and comments. The only differences are signatures (`static` member to free function), the `[this]` captures becoming `[&handles]`/`[&db]`, the variadic `new_usertype<Database>` list becoming `set_function` calls, and clang-format re-wrapping. No logic changed.
- **Linkage.** `internal.h` contains only templates, `inline` functions, declarations and two struct definitions. Every non-shared helper sits in an anonymous namespace. `CsvWriter` is defined once, in `csv.cpp`, in the named `quiver::lua_internal` namespace. Moving the helpers from class scope to namespace scope switches argument-dependent lookup (ADL) on for their unqualified calls, so I grepped sol2 and `include/quiver` for each helper name. Nothing collides.
- **Self-containment.** Each TU, and `internal.h` on its own, passes `clang++ -std=c++20 -fsyntax-only -Wall -Wextra -Wpedantic` with no warnings. That run used the MSVC STL, so it does not prove the libstdc++ include set. By reading the includes, every symbol used comes from a direct include or a guaranteed one (`std::visit` through `quiver/value.h`, `std::ref` through `sol.hpp`).
- **Lifecycle.** `sizeof(LuaRunner)==sizeof(void*)` still holds: the header is unchanged and the `static_assert` in `tests/test_lua_runner_lifecycle.cpp` is still there. `RunHandles` now comes before `lua` in `Impl`, which reverses the old teardown order: the handles are now destroyed after the Lua state. This is safe, because no finalizer touches the handles. Closures capture objects that live inside the heap-allocated `Impl`, so a moved runner keeps working.
- **CI gates.** clang-format 22.1.8 `--dry-run --Werror` is clean, biome is clean, and `bun test test/lua-api-sync.test.ts` passes (6/6).

Both warnings are about guards the split weakened, not about runtime behaviour: two location comments that the move made wrong, and a sync test whose first pass now depends on a naming convention spread across seven functions. Items marked "pre-existing (moved verbatim)" are defects the move carried over unchanged.

## Warnings

### WR-01: Two directional comments now point at code that lives in another file

**File:** `src/lua_runner/db_core.cpp:224`, `src/lua_runner/csv.cpp:284`
**Issue:** In the monolith both comments sat in `bind_database()` right next to the `open_file`/`bin_to_csv`/`csv_to_bin` registrations. After the split, those registrations are in `binary.cpp`:
- `db_core.cpp:224` says `validate_migrations` is "db-scoped and sandboxed like the file I/O below". Nothing follows it in `bind_core`.
- `csv.cpp:284` says "db-scoped and sandboxed like the file I/O above". Nothing file-I/O precedes it in `bind_csv`.

Plan 02-04 set out to re-point every citation and missed these two. A reader following them finds nothing.

**Fix:** Name the gate instead of a direction:
```cpp
// db_core.cpp:224
// Migration round-trip validation — db-scoped and sandboxed through resolve_sandboxed_path (path_policy.cpp).
// csv.cpp:284
// CSV file reading/writing -- db-scoped and sandboxed through resolve_sandboxed_path (path_policy.cpp), like
// db:open_file in binary.cpp. The two reading entry points below ...
```

### WR-02: The sync test's first pass now depends on a parameter-name convention across seven binders, and nothing enforces it

**File:** `bindings/js/test/lua-api-sync.test.ts:12-26`
**Issue:** Pass 1 only finds `db:`/`quiver.*` names through `\b(bind|ns)\.set_function(`. Before the split there was one `bind` local in one function. Now seven free functions each take `sol::usertype<Database>&`, and the parameter name `bind` is only a convention documented in `src/AGENTS.md`.

Suppose a future binder or helper names that parameter `db_type` or `self_type`, or registers through a shared helper. Every method it registers then silently drops out of `dbMethods`. The `> 40` floor does not notice a handful going missing, and the stale-name check only catches names that are already documented. So an **undocumented new method passes**, which is exactly the drift this test exists to catch.

`readdirSync` is also non-recursive, so a later `src/lua_runner/<subdir>/` would be skipped just as silently.

**Fix:** Add one count guard so that every `set_function(` must be one the parser understood (today both counts are 86):
```ts
const allSetFns = CPP.match(/\.set_function\(/g)?.length ?? 0;
expect(setFns.length).toBe(allSetFns); // a set_function on any receiver but bind/ns would be unparsed
```
Also either read the directory with `{ recursive: true }`, or assert that `SRC_DIR` has no subdirectories.

## Info

### IN-01: The query helpers' by-value `sol::optional<sol::table>` parameters sit outside `db_core.cpp`'s NOLINT pair

**File:** `src/lua_runner/db_core.cpp:90-133` (NOLINT pair at 137/229)
**Issue:** `src/AGENTS.md` now says "each TU with by-value sol2 parameters gets one `NOLINTBEGIN/END(performance-unnecessary-value-param)` pair". In `db_core.cpp` the pair starts after `query_{string,integer,float}_lua`, whose `sol::optional<sol::table> parameters` is passed by value and only read. clang-tidy will still flag them. This is not a regression: the old `NOLINT` used the misspelled check name `performance-unnecessary-value-parameter`, so it suppressed nothing before.
**Fix:** Move the `NOLINTBEGIN` above `query_string_lua`, as `db_write.cpp` and `db_time_series.cpp` already do around their helpers.

### IN-02: Lambda-local `lua` shadows the binder's `sol::state& lua` parameter

**File:** `src/lua_runner/csv.cpp:291,325`; `src/lua_runner/binary.cpp:255,260,278`
**Issue:** `bind_csv` and `bind_binary` take `sol::state& lua`. Their `[]` lambdas each declare a local `sol::state_view lua(s)`. Before the split the local shadowed the `Impl::lua` member; now it shadows the enclosing parameter. There is no bug today, because those lambdas capture nothing. But if an edit drops the local and switches to `[&]`, the lambda would quietly use the main state instead of the calling coroutine's state.
**Fix:** Rename the binder parameter, e.g. `sol::state& state`, at the two `new_usertype` call sites.

### IN-03: Unchecked key conversion in four decoders — pre-existing (moved verbatim)

**File:** `src/lua_runner/db_write.cpp:47` (`table_to_element`), `src/lua_runner/db_time_series.cpp:39` (`lua_table_to_value_map`), `src/lua_runner/db_time_series.cpp:257` (`update_time_series_files_lua`), `src/lua_runner/binary.cpp:29` (`lua_table_to_dim_map`)
**Issue:** These four decoders call `pair.first.as<std::string>()` without first checking that the key is a string. `src/AGENTS.md` documents the same hazard as fixed in `csv_options_entries` and `collect_group_columns`:
- In Release (`SOL_SAFE_GETTER` off), a number key becomes its text and a boolean key becomes `""`.
- In Debug, it raises a raw sol2 panic with no Pattern 1 prefix.

For example, `db:create_element("C", {"x"})` writes a column named `"1"`. This belongs to the Phase 4 "Release type safety" work.
**Fix (Phase 4):** Add the same `get_type() != sol::type::string` guard that `collect_group_columns` uses, with a Pattern 1 message naming the caller.

---

_Reviewed: 2026-10-03_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
