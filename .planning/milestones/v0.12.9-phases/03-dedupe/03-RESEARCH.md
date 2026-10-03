# Phase 3: Dedupe - Research

**Researched:** 2026-10-03
**Domain:** C++20 / sol2 v3.5.0 binding-layer refactor (`src/lua_runner/`), zero behaviour change
**Confidence:** HIGH (every target shape was compiled on MSVC 14.51, clang 22.1.3 and GCC 14; every sol2 runtime claim was executed in a standalone probe against the vendored sol2/Lua build)

## Summary

Phase 3 collapses 15 duplicated patterns (M1, M3-M5, M7-M16) in `src/lua_runner/` into single helpers, with no new translation unit and no observable behaviour change. All target shapes were tried before planning: `template <auto Read>` adapters over `Database` member pointers (const and non-const), the 17 member-pointer forwarders, `std::optional` returns, an `option_entries` that deduces `N` from a braced list and returns a `std::array` for structured bindings, transparent functors (`std::greater<>`, `std::logical_and<>`, ...) on `Expression`, function-pointer metamethods, and `CsvWriter` member pointers in the variadic usertype. All of them compile cleanly on MSVC `/W4`, clang `-Wall -Wextra` and GCC 14 `-Wall -Wextra`. clang-format 22.1.8 still puts the `CsvWriter` usertype one argument per line, so Pass 2 of the sync test still sees `write_row` and `close`.

The locked statement "only the Debug dot-call text changes" is **too narrow**. sol2's Debug bad-argument message embeds the full C++ signature of the bound callable (verified: `bad argument into 'void(quiver::Database&, const std::basic_string<...>&, __int64)'`). So M3 (member pointers drop `quiver::Database&` from the signature) and M5 (return type changes from `sol::basic_object<...>` to `std::optional<...>`, and `sol::this_state` goes away) also change Debug-only bad-argument text for those 20 methods. No test pins any of this text (grep over `tests/` and the binding tests: zero hits for `bad argument`, `expected userdata`, `'self'`). Release has no such check at all, so nothing changes there. The planner should record the wider list in the SUMMARY/PR rather than drop M3/M5. M1 avoids it by keeping its two lambdas with the current signature.

Several dedupe targets have **no test that would catch a wrong mapping**:
- `quiver.gte/lt/eq/neq` are untested; only `gt` and `lte` are.
- A number on the left of every operator except `*` is untested.
- `quiver.metadata`'s only accessor test uses identical `dimensions` and `time_dimensions`, so swapping those two slots passes.
- The exact text of `option key must be a string` is not asserted; tests check only the prefix.

The test count must stay 444, so the cheapest guard is a **golden-output harness**, kept in gitignored `build/dedupe-check/`. It runs Lua scripts through `quiver_cli` (which prints the script's JSON return) at the phase base and after every commit, with both the Debug and the Release binaries, and diffs the output. Add a mechanical "registered name equals member name" grep as well.

**Primary recommendation:** Run 3 sequential plans, waves 1 → 2 → 3, because `internal.h` is touched by all three. Use one commit per M-item. Each commit is gated by build, `Lua*`=444, C API=27, the sync test, the surface diff, clang-format 22.1.8 and the golden diff. Keep M1 as two lambdas over one helper. Return `std::optional` from M5 and accept the extra Debug-only signature-text change.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Lua → C++ argument marshalling (adapters, option decoders, cell conversion) | Lua binding layer (`src/lua_runner/`, sol2) | — | Thin-binding rule: logic stays in the C++ core. These helpers only convert values. |
| Transaction / dry-run sequencing (M1) | Lua binding layer | `Database` core (public `begin_*`/`commit`/`rollback`/`end_dry_run`) | Root decision "Dry runs live on Database": the helper only sequences public calls. |
| Error text for business rules | `Database` core | — | Unchanged; no core file is touched. |
| Pattern 1 text for Lua-shape errors (options, cells, keys) | Lua binding layer | — | Locally crafted (pre-FFI marshalling). M9/M10/M16 merge sites but keep the text byte-identical. |
| Run-scoped handle registry (M13) | Lua binding layer (`RunHandles`, `lua_runner.cpp`) | — | Owned by `LuaRunner::Impl`. |
| Expression operator dispatch (M14) | Lua binding layer | `Expression` core operators | Functors call the existing C++ operator overloads. |

## User Constraints (from CONTEXT.md)

<user_constraints>
### Locked Decisions

#### Behaviour neutrality (locked: PROJECT.md Constraints, DEDUP-06)
- Zero behaviour change. Every check order that decides which error a call reports stays byte-for-byte. Each plan's SUMMARY states "reordered checks: none" (the phase PR repeats it).
- No test expectation changes. Gate counts: Windows `quiver_tests --gtest_filter=Lua*` = 444 / 12 suites in Debug and Release, and C API `LuaRunnerCApiTest` = 27. Linux GCC/Clang `Lua*` = 442 (2 `_WIN32`-only tests) plus 1 root skip. The six suites (`scripts/test-all.bat`) must pass.
- The only allowed observable text change is the Debug-only sol2 dot-call error that comes from moving forwarders to member pointers (M3). It is recorded in the SUMMARY and the PR, and no test may pin it.
- Error message wording stays identical, including where M16 merges two throw sites into one.

#### Registration shape (locked: sync-test contract)
- Every `db:` method stays registered as `bind.set_function("name", ...)` on the parameter named `bind`. Every `quiver.*` function stays registered as `ns.set_function("name", ...)` with a literal name, one per call (M14 keeps literal `ns.set_function("gt", …)` calls).
- The sync test (hardened in Phase 2, e5b00b7) must still extract the same method set: 71 `db:` + 15 `quiver.*`, with the `.set_function(` count equal to the parsed count. `CsvWriter` member-pointer lines (M11) must fit the 120-column limit as one method per line in the variadic usertype.
- Template adapters keep the line shape `bind.set_function("read_scalar_strings", &bulk_read_lua<&Database::read_scalar_strings>)` (M4).

#### File layout and build cost (from Phase 2 results)
- No new `src/lua_runner/*.cpp` TU unless a helper has nowhere else to live. Phase 2 measured sol2 CPU time across the Lua files rising from 61 s to 223 s, because each TU parses sol2 separately. Shared helpers go into `internal.h` (templates/inline) or stay in the TU that uses them.
- Every `src/lua_runner/` file stays at about 450 lines or less (`csv.cpp` is at 446 now, so M10/M11/M12 should shrink it).
- csv-parser headers are never included from `src/lua_runner/`. clang-format 22.1.8 must be clean. `scripts/tidy.bat` may report fewer warnings than the Phase 2 baseline of 15, never more.

#### Plan granularity
- One phase PR into master (roadmap decision), split into small commits: one commit per M-item or per tight cluster, each green on its own (build + `Lua*` + sync test), so the series bisects.
- Suggested waves: (1) the registration/forwarder/adapter cluster M1, M3, M4, M5, M7, M8 in `db_*.cpp`; (2) the CSV cluster M9, M10, M11, M12 in `csv.cpp`; (3) the binary/registry cluster M13, M14, M15 in `binary.cpp`/`lua_runner.cpp`, plus M16 and the AGENTS.md update. The planner may regroup by file overlap.

### Claude's Discretion
- The exact helper names and signatures (e.g. `bulk_read_lua`, `group_list_lua`, `header_object`, the M1 transaction helper), as long as the registration shapes above hold.
- Whether to fold in Phase 2 review items IN-01 (move the `db_core.cpp` NOLINT region up to cover the `query_*_lua` helpers) and IN-02 (rename the lambda-local `lua` that shadows the `bind_csv`/`bind_binary` parameter). Both are behaviour-neutral cleanups in files this phase already touches. IN-03 (key type checks) is Phase 4.
- The M13 rename target for the close-at-exit function (it must cover binary files too).

### Deferred Ideas (OUT OF SCOPE)
- M6 (`value_to_lua_object` duplicates sol2's `std::variant` pusher): optional per the research map, and only after a macOS CI run, because platform-default sol2 macros already caused trouble once (the `SOL_NO_NIL` story). Not in DEDUP-01..06; it stays deferred.
- Precompiled headers / reducing the sol2 parse cost: compile time is explicitly out of scope (PROJECT.md). Note it only.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| DEDUP-01 | `db:transaction` and `db:dry_run` share one helper (M1) | §M1: one `run_in_scope` helper taking three member pointers; two lambdas kept so the Debug signature text does not change |
| DEDUP-02 | Every `db:` method registered with `bind.set_function`; plain forwarders become member pointers (M2/M3); Debug dot-call text change noted | §M3: the 17 forwarders were compiled on 3 compilers. M2 is confirmed: 86 `.set_function(` = 71 + 15, and `new_usertype<Database>("Database")` has no pairs. The exact before/after Debug text is recorded |
| DEDUP-03 | Bulk readers via two adapter templates (M4); `query_*` and by-id composites return `std::optional` (M5); one `read_vectors/sets_by_id` template (M7) | §M4/M5/M7: shapes compiled; `std::optional` → nil and nullopt → absent key verified at runtime |
| DEDUP-04 | Metadata wrappers (M8); option decoders with per-caller nil (M9); `csv_cell_to_string` via `lua_to_value` (M10); `CsvWriter` members (M11); one header rule (M12) | §M8-M12: equivalence arguments, line budgets, clang-format layout verified |
| DEDUP-05 | Prune expired on insert + rename close-at-exit (M13); functors (M14); named slots (M15); merged messages (M16) | §M13-M16: prune semantics, functor mapping table, structured-binding slots, single throw sites |
| DEDUP-06 | "reordered checks: none"; every suite passes with no expectation changes | §Validation Architecture: per-commit gate, golden harness, mutation checks, the wider Debug-text list |
</phase_requirements>

## Project Constraints (from AGENTS.md / src/AGENTS.md)

No `CLAUDE.md` or `.claude/skills` exists. Binding directives taken from `AGENTS.md` and `src/AGENTS.md`:

- **Self-Updating:** keep `src/AGENTS.md` current. Each commit updates the sentence it invalidates (see §Docs to update). The final commit describes the shared helpers (success criterion 5).
- **Error Messages:** text is defined in C++. All three patterns stay byte-identical (`Cannot {op}: ...`).
- **Philosophy:** clean over defensive; delete unused code, do not deprecate. Logic stays in C++, bindings stay thin.
- **Layout bullet (src/AGENTS.md:633-645):**
  - `internal.h` holds only templates, `inline` functions and declarations.
  - Other helpers live in an anonymous namespace nested in `quiver::lua_internal`.
  - A type registered as a usertype stays in the named namespace (sol2 keys usertypes by demangled name).
  - Comments must not spell the Database usertype call or the stdlib-opening call (greps count both).
  - One `NOLINTBEGIN/END(performance-unnecessary-value-param)` pair per TU that has by-value sol2 parameters.
  - Files stay at about 450 lines or fewer.
- **sol2 build settings:** `SOL_SAFE_NUMERICS`, `SOL_SAFE_FUNCTION` and `SOL_NO_NIL` stay PRIVATE on `quiver`. Write `sol::lua_nil`, never `sol::nil`.
- **Write policy:** `is<int64_t>()` comes before `is<double>()` everywhere. Booleans go through `is_lua_boolean` and become INTEGER 1/0.
- **Do Not Fix:** do not "simplify" load-bearing workarounds. Do not drive-by fix lint in untouched files.
- **clang-format is pinned to 22.1.8.** The `clang-format` on PATH is **22.1.3** (VS LLVM) [VERIFIED: `clang-format --version`]. Run the pinned one with `uvx --from clang-format==22.1.8 clang-format`.
- **Never stage `.planning/config.json` or `.gsd/`** (orchestrator instruction).

## Standard Stack

No new dependencies. Everything used is already vendored or in the C++20 standard library.

### Core
| Library | Version | Purpose | Why |
|---------|---------|---------|-----|
| sol2 | v3.5.0 (FetchContent, `cmake/Dependencies.cmake`) [CITED: AGENTS.md "Dependencies"] | Member-pointer and function-pointer registration, `std::optional` pusher | Already the binding layer |
| Lua | 5.4.8 [CITED: AGENTS.md] | `t[k] = nil` on an absent key creates nothing (M5/M12 rely on this) | Probe-verified (below) |
| C++20 `<functional>` transparent functors | std | M14: `std::plus<>`, `std::minus<>`, `std::multiplies<>`, `std::divides<>`, `std::greater<>`, `std::less<>`, `std::greater_equal<>`, `std::less_equal<>`, `std::equal_to<>`, `std::not_equal_to<>`, `std::logical_and<>`, `std::logical_or<>` | `operator()` returns `decltype(l op r)`, so `Expression` comes back unchanged |
| C++20 `std::erase_if(vector, pred)` | std | M13 prune | Header-only and stable (keeps the order of survivors) |
| C++20 `std::array` + structured bindings | std | M9/M15 named option slots | Compiled on MSVC/clang/GCC |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `std::array` return + structured bindings (M15) | A by-name lookup (`entry("labels")`) | Makes a slot swap impossible rather than visible, but adds a runtime lookup and a "name not in allowed list" logic error. Not worth it. |
| Function-pointer metamethods `&binop<std::plus<>>` (M14) | Keep the lambdas, passing `std::plus<>{}` | Both are behaviour-identical, because `sol::object` parameters accept any value, so no Debug bad-argument text can differ. Function pointers drop ~15 lines and the by-value `sol::object` params that tidy would otherwise flag outside a NOLINT pair. |
| `template <auto Begin, auto Finish, auto Abort>` registered as `&run_in_scope<...>` (M1) | — | **Rejected:** it changes the Debug bad-argument signature text for `db:transaction(5)`. Keep the lambdas. |

**Installation:** none.

## Package Legitimacy Audit

No external packages are installed in this phase. The audit is not applicable, and there are no `[SLOP]` or `[SUS]` packages.

## Architecture Patterns

### System Architecture Diagram

```
Lua script ──▶ sol2 call trampoline ──(Debug: arg/self checks; Release: none)──▶ bound callable
                                                                                   │
   ┌──────────────────────────────────────────────────────────────────────────────┤
   │ forwarders (M3): &Database::x ──────────────────────────────▶ Database (core) │
   │ adapters (M4/M7/M8): template<auto MemberPtr> ─▶ (db.*MemberPtr)(…) ─▶ to_lua_table / metadata_to_lua ─▶ sol::table
   │ query_* (M5): std::optional<T> ─▶ sol2 optional pusher ─▶ value | nil
   │ scoped blocks (M1): run_in_scope(begin, finish, abort) ─▶ fn(db) ─▶ commit | rollback / end_dry_run
   │ option decoders (M9): is-nil? (per caller) ─▶ option_entries (table check → collect → key checks) ─▶ named slots
   │ write_row (M10/M11): CsvWriter::write_row ─▶ type check ─▶ closed? ─▶ csv_max_integer_key ─▶ lua_to_value ─▶ format ─▶ width ─▶ csv_write::Writer
   │ read_csv[_stream] (M12): sandbox ─▶ options ─▶ Reader ─▶ header_object (nil | table)
   │ registries (M13): add_writer / add_binary_file (prune expired, append) ─▶ close_open_handles at run() exit
   └ operators (M14): binop<Functor>(lhs, rhs) ─▶ number/expr routing ─▶ Functor{}(l, r) ─▶ Expression operator
```

### File-to-item map (current line numbers)

| Item | File:lines (current) | Target shape | Line delta |
|------|----------------------|--------------|-----------|
| M1 | `db_core.cpp:147-163` (transaction), `167-183` (dry_run) | one `run_in_scope` in the anon namespace + two 3-line lambdas | ≈ -14 |
| M2 | already done: `lua_runner.cpp:86` `lua.new_usertype<Database>("Database")` (no pairs) | confirm only | 0 |
| M3 | `db_core.cpp:140-146,164-166,208-218`; `db_write.cpp:288-296`; `db_time_series.cpp:271-273` | `bind.set_function("x", &Database::x)` ×17 | ≈ -12 |
| M4 | `db_read.cpp:14-77,179-211` (10 fns), `db_time_series.cpp:237-240` | `bulk_read_lua<auto Read>` (2-arg) + `collection_read_lua<auto Read>` (1-arg) in `internal.h` | ≈ -110 |
| M5 | `db_core.cpp:90-133` (`query_*_lua`), `db_read.cpp:85-100` (ternaries) | return `std::optional<T>`; assign the optional straight into the table | ≈ -25 |
| M7 | `db_read.cpp:110-162` | `read_groups_by_id<auto List, auto I, auto F, auto S>` + two named one-line wrappers (kept for `read_element_by_id`) | ≈ -20 |
| M8 | `db_metadata.cpp:30-148` | overloads `metadata_to_lua(lua, const ScalarMetadata&)` / `(lua, const GroupMetadata&)` + `list_metadata_lua<auto List>` + `get_metadata_lua<auto Get>` | ≈ -60 |
| M9 | `internal.h:152-187`; `db_core.cpp:18-33,45-53`; `csv.cpp:199-277`; `binary.cpp:56-71,341-344` | `collect_entries`, `option_table`, `option_entries<N>` (table check inside) in `internal.h` | ≈ -20 |
| M10 | `csv.cpp:74-128` | `std::visit` over `lua_to_value(cell, operation, "cell #N")` | ≈ -15 |
| M11 | `csv.cpp:20-44,395-442` | `CsvWriter::write_row(const sol::object&)`, `CsvWriter::close()`; variadic usertype with member pointers | ≈ 0 |
| M12 | `csv.cpp:308-316` and `350-352` | `header_object(lua, header) -> sol::object` (nil when empty) | ≈ -8 |
| M13 | `internal.h:37-51`, `lua_runner.cpp:16-60,118,126`, `csv.cpp:27,386-388`, `binary.cpp:233` | `RunHandles::add_writer` / `add_binary_file` (prune `expired()`), rename `close_open_writers` → `close_open_handles` | ≈ +12 |
| M14 | `binary.cpp:97-163,187-208,364-371` | `template <typename Op> Expression binop(const sol::object&, const sol::object&)` | ≈ -60 |
| M15 | `binary.cpp:56-82` | `const auto& [version, initial_datetime, unit, labels, dimensions, dimension_sizes, time_dimensions, frequencies] = option_entries(...)` | ≈ 0 |
| M16 | `db_time_series.cpp:141-144,159-171`; `db_write.cpp:120-146,229-234` | "contain no rows" moves into `columns_to_cpp_rows`; one `length_mismatch(...)` in `db_time_series.cpp` | ≈ -10 |

Projected sizes after the phase (all ≤ 450): `csv.cpp` ≈ 400, `binary.cpp` ≈ 310, `db_write.cpp` ≈ 300, `db_time_series.cpp` ≈ 270, `internal.h` ≈ 270, `db_core.cpp` ≈ 175, `lua_runner.cpp` ≈ 160, `db_read.cpp` ≈ 110, `db_metadata.cpp` ≈ 90 [ASSUMED: estimates from the edits above; measure with `wc -l`].

### M1: one scoped-block helper

The current bodies differ only in which `Database` member runs after the callback (`db_core.cpp:147-183`, read this session).

```cpp
// anon namespace, db_core.cpp. Phase 4's C4 (commit failure) and C6 (non-function) land here, once.
sol::object run_in_scope(
    Database& self,
    const sol::protected_function& fn,
    void (Database::*begin)(),
    void (Database::*finish)(),
    void (Database::*abort)()
) {
    (self.*begin)();
    auto result = fn(std::ref(self));
    if (!result.valid()) {
        sol::error err = result;
        try {
            (self.*abort)();
        } catch (...) {
        }
        throw std::runtime_error(err.what());
    }
    (self.*finish)();
    if (result.return_count() > 0) {
        return result.get<sol::object>(0);
    }
    return sol::make_object(result.lua_state(), sol::lua_nil);
}
// inside bind_core: keep the lambda signature byte-identical, so Debug bad-argument text does not change
bind.set_function("transaction", [](Database& self, sol::protected_function fn) {
    return run_in_scope(self, fn, &Database::begin_transaction, &Database::commit, &Database::rollback);
});
bind.set_function("dry_run", [](Database& self, sol::protected_function fn) {
    return run_in_scope(self, fn, &Database::begin_dry_run, &Database::end_dry_run, &Database::end_dry_run);
});
```
- **Keep the explicit return type.** The current lambdas declare `-> sol::object`. These return whatever `run_in_scope` returns, which is `sol::object`, so the deduced type is the same.
- **Tidy delta:** the two `bugprone-empty-catch` hits (`db_core.cpp:154,174`) become one, so the baseline falls from 15 to 14 (allowed).
- **Layout:** clang-format 22.1.8 output was verified. The lambda stays on one `set_function(` line, with a one-line body.

### M3: member-pointer forwarders

All 17 targets have exactly one declaration in `include/quiver/database.h` (no overloads, no default arguments) [VERIFIED: database.h:42,44,52,55,223,299,307,311,313,316,338-341,348-350 read this session]. They are:

`is_healthy`, `current_version`, `path`, `begin_transaction`, `commit`, `rollback`, `in_transaction`, `begin_dry_run`, `end_dry_run`, `in_dry_run`, `number_of_elements`, `describe`, `describe_collection`, `summarize_collection`, `delete_element`, `delete_element_by_label`, `has_time_series_files`.

`path()` returns `const std::string&`. sol2 pushes a copy, and the probe confirms `db:path_mp()` returns the string.

**Debug text change, recorded verbatim (from the probe and `quiver_cli` at HEAD):**
- Dot call **before** (lambda): `stack index 1, expected userdata, received no value: value is not a valid userdata (bad argument into 'std::basic_string<...>(quiver::Database&)')`
- Dot call **after** (member pointer): `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` (sol2's own spelling of "preceeded")
- **Also changes (not in CONTEXT's list):** the wrong-argument text. Today `db:delete_element("Collection", "x")` gives `... (bad argument into 'void(quiver::Database&, const std::basic_string<...>&, __int64)')`. After M3 the signature loses `quiver::Database&`, giving `'void(const std::basic_string<...>&, __int64)'` (probe: `touch_mp` gave `'void(const std::basic_string<...>&, __int64)'`).
- **Release:** neither form checks anything (`call.hpp:487-498`: self check only `#if SOL_IS_ON(SOL_SAFE_USERTYPE)`, which is off in Release). Both forms dereference the userdata identically, so Release is unchanged, and a dot call is still UB there (Phase 4 / C1 territory).

### M4: two adapters (internal.h, next to `to_lua_table`)

```cpp
// The 9 two-argument bulk readers (read_{scalar,vector,set}_{integers,floats,strings}).
template <auto Read>
sol::table bulk_read_lua(Database& db, const std::string& collection, const std::string& attribute, sol::this_state s) {
    sol::state_view lua(s);
    return to_lua_table(lua, (db.*Read)(collection, attribute));
}
// The two one-argument readers (read_element_ids, list_time_series_files_columns).
template <auto Read>
sol::table collection_read_lua(Database& db, const std::string& collection, sol::this_state s) {
    sol::state_view lua(s);
    return to_lua_table(lua, (db.*Read)(collection));
}
```
- **Signature:** the parameter list and the return type are byte-identical to today's `read_*_lua` functions, so even the Debug bad-argument text is unchanged.
- **Const-ness:** works for non-const (`read_element_ids`, `read_scalar_*`) and const (`list_time_series_files_columns`) members [VERIFIED: compiled on MSVC/clang/GCC].
- **Layout:** the longest registration (`list_time_series_files_columns`) is 121 columns, so clang-format wraps it to a 3-line call. Pass 1 is multiline-safe. Every other line fits on one line (verified with clang-format 22.1.8).
- **Where:** both templates go in `internal.h`. `collection_read_lua` is used by `db_read.cpp` and `db_time_series.cpp`, and keeping the pair together gives AGENTS.md one place to describe.

### M5: `std::optional` returns

```cpp
std::optional<std::string> query_string_lua(Database& db, const std::string& sql, sol::optional<sol::table> parameters) {
    return db.query_string(sql, parameters ? lua_table_to_values("query_string", *parameters) : std::vector<Value>{});
}
// read_scalars_by_id (db_read.cpp:85-100):
case DataType::Integer:
    result[attribute.name] = db.read_scalar_integer_by_id(collection, attribute.name, id);
    break;
```
- **Probe-verified:**
  - `select('#', f())` stays `1` and the value is `nil` for `nullopt`.
  - Some value → `42` with `math.type` = `integer`.
  - Assigning `nullopt` to a fresh table key leaves the key **absent** (a `pairs` count of 1 out of 2 assignments). Lua 5.4 skips inserting a nil value.
- **Keep three named functions,** not a template. The operation name is a literal inside each one (the `lua_table_to_values` caller), and Phase 4's `optional_from_lua` lands at these three sites.
- **Debug text:** the bad-argument signature text changes, because the return type becomes `std::optional<...>` and `sol::this_state` goes away. Recorded at HEAD: `db:query_string({})` gives `... (bad argument into 'sol::basic_object<sol::basic_reference<0> >(quiver::Database&, const std::basic_string<...>&, sol::optional<...>, sol::this_state)')`. Note it in the PR alongside M3.
- **IN-01:** move `db_core.cpp:137`'s `NOLINTBEGIN` above `query_string_lua` (`db_core.cpp:90`) in this commit. Harmless today: tidy does not flag them, because `*parameters` is a non-const use. A clang-tidy probe confirmed it flags `sol::optional<sol::table>` by value when the body only reads `bool(parameters)`.

### M7: one by-id composite template

```cpp
template <auto List, auto ReadIntegers, auto ReadFloats, auto ReadStrings>
sol::table read_groups_by_id(Database& db, const std::string& operation, const std::string& collection, int64_t id, sol::state_view lua);
sol::table read_vectors_by_id_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
    return read_groups_by_id<&Database::list_vector_groups, &Database::read_vector_integers_by_id,
                             &Database::read_vector_floats_by_id, &Database::read_vector_strings_by_id>(
        db, "read_vectors_by_id", collection, id, s);
}
```
- **Wrappers:** keep the two named wrappers with today's exact signature. `read_element_by_id_lua` (`db_read.cpp:164-177`) calls both, and the Debug signature text stays identical.
- **Error text:** keep the `default:` throw with `"Cannot " + operation + ": unknown data type " + std::to_string(...)`. It is byte-identical, though unreachable, because `DataType` is `enum class DataType { Integer, Real, Text, DateTime };` [VERIFIED: include/quiver/data_type.h:10].
- **Routing:** keep the documented by-column-name routing unchanged (the per-column `_by_id` readers).

### M8: metadata wrappers

```cpp
sol::table metadata_to_lua(sol::state_view& lua, const ScalarMetadata& attribute);  // was scalar_metadata_lua
sol::table metadata_to_lua(sol::state_view& lua, const GroupMetadata& metadata);    // was group_metadata_lua
template <auto List>
sol::table list_metadata_lua(Database& db, const std::string& collection, sol::this_state s);   // 4 list fns
template <auto Get>
sol::table get_metadata_lua(Database& db, const std::string& collection, const std::string& name, sol::this_state s); // 4 get fns
// bind.set_function("list_scalar_attributes", &list_metadata_lua<&Database::list_scalar_attributes>);
```
- **Rename:** this also settles the map's rename note (`list_*_metadata_lua` disappear).
- **Order change:** `list_scalar_metadata_lua` calls `db` before `create_table` today, and the vector/set lists do it after. The difference is unobservable, because `create_table` has no effect a script can see.
- **Stays put:** `lua_data_type_name`'s unreachable `default:` stays; FIX-03 removes it in Phase 4.

### M9: option decoders (internal.h)

```cpp
inline std::vector<std::pair<sol::object, sol::object>> collect_entries(const sol::table& t);  // the 3 for_each copies
inline sol::table option_table(const sol::object& value, const std::string& operation, const std::string& what);
//   throws "Cannot <op>: option '<what>' must be a table"  -- db_core.cpp:26 and csv.cpp:219 spell this identically
template <std::size_t N>
std::array<std::optional<sol::object>, N> option_entries(
    const sol::object& options, const std::string& operation, const std::string_view (&allowed)[N]);
//   1. options.get_type() != table -> "Cannot <op>: options must be a table"  (moved in from the 4 callers)
//   2. collect_entries, then per entry: key type ("option key must be a string"), unknown ("unknown option '<k>'")
```
- **Nil stays per caller.** The three CSV decoders keep their early return, `if (!options.valid() || options.get_type() == sol::type::lua_nil) { return result; }`. `quiver.metadata` has none, so nil falls into `option_entries`' table check and yields today's exact text, `Cannot metadata: options must be a table` (`binary.cpp:58`).
- **Order:** nil → table → keys, as today. Reordered checks: none.
- **`header_row`:** drop the `get_type() != number` check (`csv.cpp:257-259`) and keep `is<int64_t>()` (same message). Under `SOL_SAFE_NUMERICS` the integral check is exactly `lua_isinteger` (`stack_check_unqualified.hpp`, the `SOL_NUMBER_PRECISION_CHECKS` branch). It is false for strings, booleans, `2.5` and `2.0`, and it does not depend on the build type. The probe confirms `'12'`, `true` and `2.0` all return false.
- **Callers:**
  - `rename_agents` (`binary.cpp:341-344`) uses `collect_entries` and keeps its own "mapping must be a table" check and text.
  - `parse_csv_options`' local `table_entries` (`db_core.cpp:20-33`) becomes `collect_entries(option_table(...))`.
- **Braced lists:** a braced list deduces `N`, so callers write `option_entries(options, op, {"separator", "header"})` (compiled on all three compilers).
- **Docs:** rename `csv_options_entries` → `option_entries` in `src/AGENTS.md:271,698` and in the comment at `binary.cpp:35`.

### M10: `csv_cell_to_string` through `lua_to_value`

```cpp
std::string csv_cell_to_string(const sol::object& cell, const std::string& operation, std::int64_t index, std::int64_t row_index) {
    return std::visit(
        [&](const auto& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            std::string out;
            if constexpr (std::is_same_v<T, std::string>) {
                return value;
            } else if constexpr (!std::is_same_v<T, std::nullptr_t>) {
                if constexpr (std::is_same_v<T, double>) {
                    if (!std::isfinite(value)) {
                        throw std::runtime_error("Cannot " + operation + ": row " + std::to_string(row_index) +
                                                 " cell #" + std::to_string(index) + " is not a finite number");
                    }
                }
                quiver::utils::append_number(value, out);
            }
            return out;
        },
        lua_to_value(cell, operation, "cell #" + std::to_string(index)));
}
```

Equivalence, branch by branch. `Value` is `std::variant<std::nullptr_t, int64_t, double, std::string>` [VERIFIED: include/quiver/value.h:10]. `lua_to_value` checks in the order nil → boolean → int64 → double → string → throw [VERIFIED: internal.h:120-137].

| Input | Today (`csv.cpp:91-127`) | After |
|-------|--------------------------|-------|
| missing key / nil (`row[i]`) | `!valid() \|\| is<nil>` → `""` | `is<nil>` → `nullptr` → `""` |
| boolean | `"1"`/`"0"` | `int64_t{1}/{0}` → `append_number` → `"1"`/`"0"` |
| integer | `append_number(int64)` | same |
| float | finite check → `append_number(double)` | same; same message text |
| string | verbatim | verbatim |
| other | `Cannot <op>: cell #N has unsupported Lua type` | `lua_to_value`: `"Cannot " + caller + ": " + what + " has unsupported Lua type"`, with `what = "cell #N"` → identical |

The one input on which the two differ is a stateless, default-constructed `sol::object`. The probe gives `default.is_nil=0` but `missing.is_nil=1`. `row[i]` (the only caller, `csv.cpp:145`) always yields a state-backed object, so no script can produce that input.

Per-cell cost: the `"cell #N"` string is now built for every cell. It is under 16 characters, so SSO applies and there is no heap allocation (`lua_table_to_vector` already does the same).

Update the comment block at `csv.cpp:74-84` (the "do not reorder" warning now belongs to `lua_to_value`) and `src/csv/csv_write.cpp:26-31` (the unsupported-type message is now raised by `lua_to_value` in `internal.h`). Also rewrite `src/AGENTS.md:713` ("`csv_cell_to_string` writes a boolean as the text 1/0").

### M11: `CsvWriter` behaviour as members

```cpp
struct CsvWriter {
    std::shared_ptr<quiver::csv_write::Writer> writer;
    std::int64_t next_row_index = 1;
    std::size_t header_width = 0;
    CsvWriter(std::shared_ptr<quiver::csv_write::Writer> w, std::size_t header_width_);
    void write_row(const sol::object& row);   // body = today's lambda, csv.cpp:399-439, `self.` dropped
    void close();                             // writer->close("close")
};
// define the two members after the anon namespace (they call csv_row_cells_from_lua), before bind_csv.
state.new_usertype<CsvWriter>(
    "CsvWriter",
    sol::no_constructor,
    "write_row",
    &CsvWriter::write_row,
    "close",
    &CsvWriter::close
);
```
- **Layout:** clang-format 22.1.8 keeps one argument per line (verified). On one line the call would be 130 columns with `lua.` and 132 with `state.`, so the margin is about 10 columns.
- **Same behaviour:** probe-verified. `w:write_row()` with no argument still reaches the member and throws `Cannot write_row: row must be a table`. Check order is unchanged: type, then closed, then cells, then width.
- **What changes:** only `w.write_row(...)` / `w.close()` (dot calls) switch to the sol2 self text in Debug. A `const sol::object&` parameter accepts any value, so no bad-argument text exists to change.
- **Fold in IN-02:** rename `bind_csv`'s `sol::state& lua` parameter to `state` (`internal.h:206`, `csv.cpp:283,395`). The lambda-locals `sol::state_view lua(s)` at `csv.cpp:291,325` stop shadowing it.
- **Docs:** `src/AGENTS.md:145` ("lives entirely in the Lua-layer `CsvWriter` wrapper") still holds; reword it to "`CsvWriter::write_row`".

### M12: one header rule

```cpp
// nil (absent key / falsy argument) when the file has no header (header_row = 0), else the name list.
sol::object header_object(sol::state_view& lua, const std::vector<std::string>& header) {
    return header.empty() ? sol::object(sol::lua_nil) : sol::object(to_lua_table(lua, header));
}
result["header"] = header_object(lua, reader.header());          // read_csv, before result["rows"] as today
const sol::object header_table = header_object(lua, reader.header());  // read_csv_stream
```
- **Proof:** assigning a nil `sol::object` to a fresh key leaves no entry (probe: `nil_obj_table` → 1 key of 2). That matches today's `if (!header.empty())`.
- **Comment:** the stale "forward-looking" comment is **already gone** (Phase 2 02-01). `grep -rn "forward-looking" src/lua_runner` returns nothing. Merge the two "must not diverge" comments (`csv.cpp:309-312,344-349`) into the helper's comment.

### M13: registry hygiene

```cpp
// RunHandles (internal.h decl, lua_runner.cpp body)
void RunHandles::add_writer(const std::string& resolved_path, const std::shared_ptr<csv_write::Writer>& writer) {
    std::erase_if(open_writers, [](const auto& entry) { return entry.second.expired(); });
    open_writers.emplace_back(resolved_path, writer);
}
void RunHandles::add_binary_file(const std::shared_ptr<BinaryFile>& file) {
    std::erase_if(open_binary_files, [](const auto& weak) { return weak.expired(); });
    open_binary_files.push_back(file);
}
void RunHandles::close_open_handles();   // renamed from close_open_writers (closes CSV writers AND binary files)
```
- **Prune `expired()` only, never closed-but-alive** (Pitfall 10). Both readers of the registry already skip expired entries (`path_has_open_writer` via `lock()`, `lua_runner.cpp:26`; the close loops via `lock()`, `:40,51`), so pruning them cannot be observed. `erase_if` keeps the order of survivors, so the order of closes at exit is unchanged.
- **Take `const std::shared_ptr&`:** `write_csv` still needs `writer` afterwards for `make_unique<CsvWriter>(std::move(writer), ...)`. By-value would also trip `performance-unnecessary-value-param`.
- **Rename sites:** `internal.h:44,50`; `lua_runner.cpp:38,118,126`; `csv.cpp:27,386`; `src/AGENTS.md:819,829,833`. No test or other AGENTS.md names the function [VERIFIED: repo-wide grep excluding build/.planning].
- **Not required:** the two close loops (`lua_runner.cpp:39-59`) are the same pattern, but DEDUP-05 does not require merging them. Leave them (their element types and close calls differ).

### M14: functors replace `BinOp`/`apply_binop`

```cpp
// Every Expression binary operator for the three operand combos; Op is a transparent functor, so overload
// resolution picks the Expression operator. number/number routes through to_expression, which throws.
template <typename Op>
Expression binop(const sol::object& lhs, const sol::object& rhs) {
    const bool lnum = is_number(lhs);
    const bool rnum = is_number(rhs);
    if (lnum && !rnum) {
        return Op{}(lhs.as<double>(), to_expression(rhs));
    }
    if (!lnum && rnum) {
        return Op{}(to_expression(lhs), rhs.as<double>());
    }
    return Op{}(to_expression(lhs), to_expression(rhs));
}
type[sol::meta_function::addition] = &binop<std::plus<>>;      // in bind_expression_operators
ns.set_function("gte", &binop<std::greater_equal<>>);           // literal name, one per call
```

Mapping table, which must be exact:

| Name | Functor |
|------|---------|
| addition | `plus<>` |
| subtraction | `minus<>` |
| multiplication | `multiplies<>` |
| division | `divides<>` |
| bitwise_and | `logical_and<>` |
| bitwise_or | `logical_or<>` |
| `gt` | `greater<>` |
| `lt` | `less<>` |
| `gte` | `greater_equal<>` |
| `lte` | `less_equal<>` |
| `eq` | `equal_to<>` |
| `neq` | `not_equal_to<>` |

- **Unchanged:** `unary_minus` and `bitwise_not` keep their lambdas (`binary.cpp:201,207`).
- **Operators exist:** the C++ `Expression` operators exist for all three combos of all twelve (src/AGENTS.md Expression section), and `std::logical_and<>` calls the overloaded `&&` (compiled with `(double, Expression)`, `(Expression, double)` and `(Expression, Expression)` on all three compilers).
- **Short-circuit:** none to lose; the overloaded `&&`/`||` evaluate both operands, as `apply_binop` did.
- **Evaluation order:** the unspecified argument-evaluation order is the same as today. In the last branch both `to_expression` calls throw the same text.
- **`apply_binop`'s unreachable throw** (`binary.cpp:150`) disappears. FIX-03 lists it for Phase 4, so tell the Phase 4 planner that item is already done.
- **Includes:** add `#include <functional>`. Remove `enum class BinOp`.
- **Debug text:** a `sol::object` parameter accepts anything, including none (`quiver.gt(1)` at HEAD gives `Cannot build expression: operand must be an expression or a binary file`). So no bad-argument text can change.

### M15: named slots

```cpp
const auto& [version, initial_datetime, unit, labels, dimensions, dimension_sizes, time_dimensions, frequencies] =
    option_entries(t, "metadata", {"version", "initial_datetime", "unit", "labels", "dimensions",
                                   "dimension_sizes", "time_dimensions", "frequencies"});
el.set("labels", metadata_array<std::string>(labels, "labels"));
```
- **Guarantee:** each use site now shows its slot name next to its key string. The one remaining swap risk is the binding order versus the allowed list, two adjacent lines. That is a visible review check, not a type error, so be honest about the guarantee in the PR.
- **Pass 2 hazard:** the wrapped list lines (`"initial_datetime",`) match Pass 2's bare-name regex. They are safe only because `build_metadata_from_lua` sits **above** the first `new_usertype` in `binary.cpp` (`current` is `""` at file start). Do not move it below `bind_binary`'s usertypes. The surface diff catches this.

### M16: merged messages

- **"contain no rows"** (`db_time_series.cpp:166-171`, `db_write.cpp:229-234`, identical text) moves to the top of `columns_to_cpp_rows` (`db_write.cpp:120`). Both callers call `columns_to_cpp_rows` immediately after today's check, so the check order is unchanged:
  - In the time-series path it still runs after the dimension and longer-than-dimension checks.
  - `join_column_names` becomes anon-namespace in `db_write.cpp`, so remove its declaration from `internal.h:219`.
- **"has length X but expected Y"** (`db_time_series.cpp:141-144` for dimension mismatch, `:159-162` for a value column too long) becomes one `std::runtime_error length_mismatch(caller, column, length, expected)` in `db_time_series.cpp`'s anon namespace, used as `throw length_mismatch(...)`. The two predicates differ (`!=` and `>`) and stay as they are.

### Anti-Patterns to Avoid
- **A template registered directly for M1** (`&run_in_scope<...>`). It changes the Debug bad-argument text for `db:transaction(5)`.
- **Pruning closed-but-alive writers** (M13). Prune `expired()` only.
- **Reordering `lua_to_value`** or adding a `get_type()` guard to it (M10). Every write path depends on its order now.
- **Table-driven loops for `gt/lt/…` or the metamethods** (M14). The sync test needs a literal `ns.set_function("gt", …)` per name.
- **A new TU for helpers.** It costs sol2 parse time (Phase 2: 61 s → 223 s CPU).
- **Including csv-parser** (`csv.hpp`, `internal/csv_*`) anywhere under `src/lua_runner/`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Operator dispatch by enum | `BinOp` + switch | `<functional>` transparent functors | They forward to the real overloads; there is no enum to keep in sync |
| Optional → nil marshalling | `has_value() ? make_object : lua_nil` | Return `std::optional<T>` (sol2 pusher) | Probe-verified: same arity and same absent key |
| Vector pruning loop | manual iterator erase | `std::erase_if` (C++20) | Stable, one line |
| Two-way dispatch copy | a second `is<…>` ladder | `lua_to_value` + `std::visit` | One ordering for every write path |

## Runtime State Inventory

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None. No database, file or registry key stores any renamed symbol (`close_open_writers`, `csv_options_entries` are C++ identifiers only) [VERIFIED: repo grep] | none |
| Live service config | None. There is no external service | none |
| OS-registered state | None | none |
| Secrets/env vars | None | none |
| Build artifacts | (1) The Dart native-assets hook cache `bindings/dart/.dart_tool/hooks_runner/` and `.dart_tool/lib/` hold their own Release build of `libquiver` (Pitfall 12). (2) `build/release/` must be rebuilt before the Release gates. (3) The golden baseline must be captured from base-commit binaries **before** the first code commit | Delete both Dart cache dirs before the final six-suite run. Rebuild `build/release` at base and per wave |

## Common Pitfalls

### Pitfall 1: The Debug text change is wider than CONTEXT says
**What goes wrong:** the PR claims "only the dot-call text changed", but M3/M5 also change Debug bad-argument signatures.
**How to avoid:** list both in every affected SUMMARY and the PR: dot-call (17 `db:` methods plus `CsvWriter:write_row/close`) and bad-argument signature text (17 forwarders plus 3 `query_*`). Keep M1, M4, M7 and M8 signatures byte-identical, as shown above.
**Warning signs:** a test or doc that quotes `bad argument into` (none today).

### Pitfall 2: A wrong-but-compiling mapping passes every test
**What goes wrong:** for example `"gte", &binop<std::greater<>>`, `"read_scalar_floats", &bulk_read_lua<&Database::read_scalar_integers>`, or a swap of the `dimensions`/`time_dimensions` slots.
**Why:** there are no Lua tests for `gte/lt/eq/neq`. The metadata test uses the same values for both dimension lists. And Lua's `1 == 1.0` hides integer/float swaps in `assert`s.
**How to avoid:** use the golden harness (below; it emits `math.type` and distinct dimension lists), plus a grep check that every `set_function("X", &…&Database::Y…)` has `X == Y` (see the Validation section).

### Pitfall 3: Pass 2 loses `CsvWriter` methods
**What goes wrong:** the usertype joins onto one line. Today it would be 130 columns on one line, so it still wraps.
**How to avoid:** per commit, require `bun run build/split-check/surface.ts src/lua_runner | diff build/split-check/before.txt -` to be empty. The sync test's `CsvWriter` floor guard also fails.

### Pitfall 4: Check-order drift in M9/M16
**What goes wrong:** the table check moves before the nil check, or "no rows" moves before the length check.
**How to avoid:** use the orders spelled out above. Pins: `options must be a table` (9 tests), `contain no rows` (time-series and update), `has length` (both sites, `test_lua_runner_time_series.cpp:341,540`).

### Pitfall 5: The wrong clang-format
**What goes wrong:** PATH has 22.1.3. Using it can reflow differently from CI's 22.1.8.
**How to avoid:** always run `uvx --from clang-format==22.1.8 clang-format`.

### Pitfall 6: Stale natives in the six-suite run
**How to avoid:** delete `bindings/dart/.dart_tool/hooks_runner` and `.dart_tool/lib`, close Julia/Python/Dart hosts, then run `cmd //c 'scripts\test-all.bat'` (the Phase 2 02-04 recipe).

## Code Examples

All shapes above were compiled from `scratchpad/probe/compile.cpp`. It includes the real `lua_runner/internal.h`, `quiver/database.h` and `quiver/expression/expression.h` and registers every target shape. Compilers:
- **MSVC 14.51:** `-W4 -permissive- -std:c++20`, clean.
- **clang 22.1.3:** `-fsyntax-only -Wall -Wextra`, clean.
- **GCC 14:** Docker `gcc:14`, `-fsyntax-only -Wall -Wextra`, clean.

Runtime behaviour came from `scratchpad/probe/runtime.cpp`, standalone sol2 and `lua-5.4d.lib` with the same `SOL_*` defines, Debug.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| 17 variadic `Database` pairs | `bind.set_function` everywhere | Phase 2 | M2 done; confirmed 86 = 71 + 15 |
| Lambda forwarders | member pointers (M3) | this phase | Debug-only text, above |
| `BinOp` enum + switch | transparent functors (M14) | this phase | Removes FIX-03's `apply_binop` item |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Projected per-file line counts after the phase | File-to-item map | Low. Measure with `wc -l` per commit; `csv.cpp` has the least margin |
| A2 | Accepting the extra Debug-only bad-argument text change (M3/M5) is within the spirit of CONTEXT's "Debug-only dot-call" allowance | Summary, Pitfall 1 | Medium. If the user rejects it, M5 must keep `sol::object` returns, which conflicts with DEDUP-03. Confirm at plan approval |
| A3 | Apple Clang accepts the same shapes as clang 22 and GCC 14 | Code Examples | Low. Only PR CI proves it (the same backstop as Phase 2) |

## Open Questions

1. **The wider Debug-only text list (M3 bad-argument signatures, M5 return-type signature).**
   - Known: no test pins it; Release has no such text; it is the same class as the accepted dot-call change.
   - Unclear: whether CONTEXT's "the only allowed text change" lock was meant literally.
   - Recommendation: proceed, and record the exact before/after strings (above) in the SUMMARY and PR. Flag it in the plan for the user's approval checkpoint.
2. **Linux/Apple builds.**
   - Known: GCC 14 syntax-checks every current TU in Docker (≈5 min wall time, bind-mount overhead).
   - Recommendation: run the Docker GCC syntax pass once per wave, and leave Apple Clang to PR CI.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSVC cl (env loaded in bash) | build | ✓ | 14.51.36231 | — |
| CMake / Ninja | build | ✓ | 4.3.1 / 1.13.2 | — |
| clang-format (pinned) | format gate | ✓ via `uvx --from clang-format==22.1.8` | 22.1.8 (PATH has 22.1.3) | — |
| run-clang-tidy / clang-tidy | tidy gate | ✓ (VS LLVM) | 22.1.3 | — |
| bun | sync test, surface diff | ✓ | 1.3.14 | — |
| uv | Python, clang-format | ✓ | 0.12.3 | — |
| Julia / Dart | six suites | ✓ | 1.11.9 / 3.13.4 | — |
| Docker `gcc:14` | Linux syntax check | ✓ | 29.6.2 | PR CI |
| `build/release/bin/quiver_cli.exe` | Release golden run | ✓ (rebuild needed) | — | — |

No missing dependencies.

## Validation Architecture

`workflow.nyquist_validation` is `false` in `.planning/config.json`, so the formal Nyquist section is skipped. Because DEDUP-06 *is* verification, the gate recipe follows.

### Per-commit gate (about 3 min; extends `build/split-check/checks.sh`)
```bash
cd /c/Development/Quiver/quiver1
cmake --build build --config Debug
./build/bin/quiver_tests.exe --gtest_filter='Lua*' --gtest_list_tests | grep -c '^  '          # 444
./build/bin/quiver_tests.exe --gtest_filter='Lua*' --gtest_brief=1 | tail -3
./build/bin/quiver_c_tests.exe --gtest_filter='LuaRunnerCApiTest.*' --gtest_brief=1 | tail -3   # 27
(cd bindings/js && bun test test/lua-api-sync.test.ts)                                           # 6 pass
bun run build/split-check/surface.ts src/lua_runner | diff build/split-check/before.txt - && echo SURFACE IDENTICAL
uvx --from clang-format==22.1.8 clang-format --dry-run --Werror src/lua_runner/*.cpp src/lua_runner/*.h
wc -l src/lua_runner/* | sort -n | tail -3                                                       # each <= ~450
grep -rnE 'csv\.hpp|internal/csv|csv_parser' src/lua_runner && echo "FAIL csv-parser include" || true
grep -c '\.set_function(' src/lua_runner/*.cpp | awk -F: '{s+=$2} END {print s}'                # 86
# Registered name == member name (catches swapped adapters/forwarders):
grep -ohE 'set_function\(\s*"[a-z_]+",\s*&([a-z_]+<&)?Database::[a-z_]+' src/lua_runner/*.cpp \
  | sed -E 's/.*"([a-z_]+)".*Database::([a-z_]+)/\1 \2/' | awk '$1 != $2 {print "MISMATCH", $0; bad=1} END {exit bad}'
bash build/dedupe-check/golden.sh                                                                 # diff vs baseline (below)
```
The name-equality grep is single-line, and it was mutation-tested on the probe file: 25 lines checked; a `get_scalar_metadata`→`get_set_metadata` swap prints `MISMATCH` and exits 1. Multiline `bind.set_function(\n "x",\n &…)` registrations (only `list_time_series_files_columns`) need `-z` or a manual check. M7's `read_groups_by_id<...>` wrappers are not registered this way, so review them by hand.

### Golden-output harness (Wave 0, gitignored `build/dedupe-check/`)
- **Why:** it covers behaviour no test pins, without changing the 444 count.
- **How:** `quiver_cli` prints the script's JSON return, as verified with `{"e":"...","ok":false,"v":0}`. Each script wraps every probe in `pcall` and returns a table, which the encoder emits with sorted keys, so the output is deterministic.
- **Baseline:** capture at the phase base commit with **both** `build/bin/quiver_cli.exe` (Debug) and `build/release/bin/quiver_cli.exe` (Release). Diff stdout only (Debug sol2 prints `[sol2] An exception occurred` to stderr). Use a fresh db path per run, at the same location.
- **Content:**
  - **M3:** every forwarder through `:` (return values).
  - **M3 / M11, Debug-only script:** dot calls on `describe`, `delete_element` and `w.write_row` (expected diff), plus `db:delete_element("C","x")` (expected signature diff).
  - **M1:** `transaction`/`dry_run` returning 0, 1 and 2 values; an error inside each, then the row count after.
  - **M4:** all 11 bulk readers on a nullable schema (`tests/schemas/valid/all_types.sql` / `nullable_time_series.sql`; read each schema for table names), emitting `math.type` per cell.
  - **M5:** `select('#', db:query_*(...))` for NULL and non-NULL; `read_scalars_by_id` key count with a NULL attribute.
  - **M7 / M8:** `read_vectors_by_id`/`read_sets_by_id`/`read_element_by_id`; every `get_*_metadata`/`list_*` table dumped.
  - **M9, options:** nil, `5`, `"x"`, `db` (userdata), `{[true]=1}`, `{"positional"}`, unknown key. `header_row` = `"2"`, `2.5`, `2.0`, `true`, `-1`, `0`, `1`. `header = 5`, `header = {name="a"}`. `quiver.metadata()`, `quiver.metadata(5)`, plus unknown key. `export_csv`/`import_csv` with `enum_labels` shape errors. `rename_agents(5)` and `{[1]="x"}`.
  - **M10 / M11, `write_row` cells:** hole, `true/false`, `math.maxinteger`, `math.mininteger`, `2.0`, `0.1`, `0/0`, `1/0`, `"0012"`, `{}`, `print`. Then a non-table row, a write after close, and too wide / too short with a header. Read the file back.
  - **M12:** `read_csv` / `read_csv_stream` header under `header_row = 0` and `1` (`type(header)`).
  - **M13:** reopen after close, same path while open, a global writer across two runs.
  - **M14:** for each of the 12 operators × {expr∘expr, expr∘2.0, 2.0∘expr}, save and read cell values; number∘number and string operand errors; NaN operand for each comparison.
  - **M15:** `quiver.metadata` with `dimensions={"stage","block","x"}` and `time_dimensions={"stage","block"}`, all accessors dumped.
  - **M16:** the full no-rows and length messages for vector, set and time-series writers.
- **Gate:** at every commit the diff must be empty, except the Debug-only lines the commit's item names (M3, M5, M11).

### Mutation checks (one per item, revert after)
These are run once by the executor to prove the gate can see the change; do not commit them.
- **M1:** swap `commit`/`rollback` → `TransactionBlockAutoCommit` / `TransactionBlockRollbackOnError` fail.
- **M9:** drop the table check in `option_entries` → `options must be a table` tests fail. Drop `is<int64_t>` → `HeaderRowAsFractionThrowsMustBeAnInteger` fails.
- **M10:** drop the non-finite check → the write_csv finite test fails.
- **M12:** return `{}` instead of nil → `test_lua_runner_read_csv.cpp:247,495` fail.
- **M13:** prune non-expired entries → `test_lua_runner_write_csv.cpp:1438` ("already open for writing") fails. Skip the add → `UnclosedWriterHeldInAGlobalIsAlsoFlushedWhenRunReturns` fails.
- **M16:** drop the moved no-rows check → `contain no rows` tests fail.
- **Unpinned items, shown by the golden harness only:** M14 `gte`→`greater`, and M15 `dimensions`↔`time_dimensions`.

### Per-wave and phase gates
- **Per wave:**
  - `cmake --build build/release` (or the `release` preset), then `Lua*`=444 and the C API = 27 in Release.
  - The golden Release diff.
  - Tidy on the folder: `uv run python "$(which run-clang-tidy)" -p build -quiet 'src[\\/]lua_runner[\\/]'`, compared with `build/split-check/tidy.txt`. The count must stay at or below 15; expect 14 after M1.
  - The Docker GCC syntax pass:
    ```bash
    MSYS_NO_PATHCONV=1 docker run --rm -v "C:/Development/Quiver/quiver1:/repo:ro" gcc:14 sh -c \
      'cd /repo && for f in src/lua_runner/*.cpp; do g++ -fsyntax-only -std=c++20 -Wall -Wextra \
      -DSOL_NO_NIL=1 -DSOL_SAFE_FUNCTION=1 -DSOL_SAFE_NUMERICS=1 -Iinclude -Isrc \
      -Ibuild/_deps/lua-src/src -Ibuild/_deps/lua-build/src -isystem build/_deps/sol2-src/include \
      -isystem build/_deps/spdlog-src/include -isystem build/_deps/tomlplusplus-src/include "$f"; done'
    ```
- **Phase gate:** six suites with a cleared Dart hook cache; PR CI covers Linux and Apple.

### Recommended plan / commit grouping

The plans run sequentially because all three touch `internal.h`. Each commit gets the per-commit gate.

**Plan 03-01: registration / adapters** (`internal.h`, `db_core.cpp`, `db_read.cpp`, `db_metadata.cpp`, `db_time_series.cpp`, `db_write.cpp`, `src/AGENTS.md`)
- **Task 0:** build the golden harness and capture the Debug and Release baselines at the base commit.
1. M3 (17 member pointers; confirm M2). Docs: rewrite `src/AGENTS.md:702` ("bound as plain lambdas").
2. M1 (`run_in_scope`).
3. M4 (two adapters in `internal.h`).
4. M5 + IN-01.
5. M7.
6. M8.
7. M16 (`db_write.cpp` + `db_time_series.cpp`; drop `join_column_names` from `internal.h`).

**Plan 03-02: options / CSV** (`internal.h`, `db_core.cpp`, `csv.cpp`, `binary.cpp` (metadata call site only), `src/csv/csv_write.cpp` comment, `src/AGENTS.md`)
1. M9 + M15 together. Both rewrite `build_metadata_from_lua`'s `option_entries` call. Docs: `src/AGENTS.md:271,698`.
2. M10 (+ `csv_write.cpp:26-31` comment, `src/AGENTS.md:713`).
3. M11 + IN-02 for `bind_csv` (`src/AGENTS.md:145`).
4. M12.

**Plan 03-03: binary / registry / docs** (`binary.cpp`, `lua_runner.cpp`, `internal.h`, `csv.cpp` one call site, `src/AGENTS.md`)
1. M13 (`add_writer`/`add_binary_file`, rename to `close_open_handles`; `src/AGENTS.md:819,829,833`).
2. M14 + IN-02 for `bind_binary`.
3. `src/AGENTS.md` "shared helpers" paragraph (success criterion 5):
   - the adapters (`bulk_read_lua`, `collection_read_lua`), `read_groups_by_id`, `metadata_to_lua` / `list_metadata_lua` / `get_metadata_lua`, `run_in_scope`;
   - `collect_entries` / `option_table` / `option_entries` (nil per caller);
   - `lua_to_value` as the one write-path dispatch, including CSV cells;
   - `CsvWriter` members, `header_object`, `RunHandles::add_*` pruning, `binop<Op>`, and `columns_to_cpp_rows` owning "contain no rows".
   - Final full gate plus the PR text: "reordered checks: none", the Debug-only text list, and the FIX-03 note.

### Docs to update (src/AGENTS.md, by commit)
| Line | Current text (verbatim fragment) | Item |
|------|----------------------------------|------|
| 145 | "lives entirely in the Lua-layer `CsvWriter` wrapper in `src/lua_runner/csv.cpp`" | M11 |
| 271 | "`csv_options_entries` and `collect_group_columns` check each key's Lua *type*" | M9 |
| 698 | "collect-then-validate walk (`csv_options_entries`)" | M9 |
| 702 | "`describe` / `describe_collection` / `summarize_collection` are bound as plain lambdas" | M3 |
| 713 | "`csv_cell_to_string` writes a boolean as the text `1`/`0`" | M10 |
| 819, 829, 833 | "`RunHandles::close_open_writers()`" / "`close_open_writers()`" | M13 |

## Security Domain

`security_enforcement` is on (ASVS L1). The phase changes no trust boundary; the risk is that a dedupe **drops a guard**.

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — |
| V3 Session Management | no | — |
| V4 Access Control | yes (filesystem sandbox) | `resolve_sandboxed_path` stays the single gate. No item touches `path_policy.cpp` or the sandbox → options → work order |
| V5 Input Validation | yes | Option decoders (M9), cell conversion (M10), key-width cap `csv_max_integer_key` (unchanged), group decoders (M16): all text and order preserved |
| V6 Cryptography | no | — |

| Threat | STRIDE | Mitigation |
|--------|--------|-----------|
| M9 fold drops the table check → a userdata `options` iterates as an empty table (Release UB / silent defaults) | Tampering | The table check moves *into* `option_entries`; pinned by 9 "options must be a table" tests |
| M10 drops the non-finite check → platform-specific `nan` text in CSV | Tampering | Kept in the `double` branch; pinned by "is not a finite number" |
| M13 prunes live entries → two writers on one path, data loss | Tampering / DoS | `expired()` only; pinned by "already open for writing" (write_csv:1438) |
| A key-width cap lost in a refactor | DoS | `csv_max_integer_key` is untouched; PIN-01 tests |

## Sources

### Primary (HIGH confidence)
- **Code read this session (current HEAD `71343da`):** `src/lua_runner/{internal.h, db_core.cpp, db_read.cpp, db_metadata.cpp, db_time_series.cpp, db_write.cpp, csv.cpp, binary.cpp, lua_runner.cpp}`, `include/quiver/{database.h, value.h, data_type.h}`, `src/utils/number.h`, `.clang-format`, `.clang-tidy`, `bindings/js/test/lua-api-sync.test.ts`, `build/split-check/{checks.sh, surface.ts}`.
- **Vendored sol2 v3.5.0:** `call.hpp:470-498` (member-pointer self check under `SOL_SAFE_USERTYPE`), `usertype.hpp:107-115` / `table_core.hpp:631-640` (`set_function`), `stack_check_unqualified.hpp` integral check (`lua_isinteger` under `SOL_NUMBER_PRECISION_CHECKS`), `version.hpp:340-406`.
- **Executed probes (scratchpad, not committed):**
  - `runtime.cpp`: Debug dot-call texts, `std::optional` push and absent key, nil-object assignment, `is<int64_t>` on `2.0` / `'12'` / `true`, `CsvWriter` member pointers.
  - `compile.cpp`: MSVC, clang and GCC 14.
  - clang-format 22.1.8 layout.
  - clang-tidy on the probe shapes.
  - `quiver_cli` at HEAD for the current Debug bad-argument texts.
- **Measured at HEAD:** `quiver_tests` `Lua*` = 444 (Debug and Release), C API = 27, surface = 71 `db` + 15 `quiver`, `.set_function(` = 86.

### Secondary
- `.planning/research/LUA-RUNNER-MAP.md` (M-items plus the critic pass), `.planning/research/PITFALLS.md` (Pitfalls 3, 8, 10, 12), and Phase 2 `02-VERIFICATION.md` / `02-REVIEW.md`.

## Metadata

**Confidence breakdown:**
- Target shapes: HIGH. Compiled on three compilers, formatted with the pinned clang-format.
- Behaviour equivalence: HIGH for M3/M5/M10/M12/M13 (probe-executed), and HIGH for M1/M7/M8/M9/M14/M15/M16 (structural: the same calls and messages in the same order).
- Pitfalls: HIGH. Every one is a measured gap (no `gte` test, identical dimension lists, Debug signature text quoted from a real run).

**Research date:** 2026-10-03
**Valid until:** 2026-11-02 (stable; invalidated by any change to `src/lua_runner/` or a sol2 `GIT_TAG` bump)
