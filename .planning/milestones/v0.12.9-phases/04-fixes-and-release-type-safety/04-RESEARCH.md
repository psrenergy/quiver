# Phase 4: Fixes and Release Type Safety - Research

**Researched:** 2026-10-03
**Domain:** sol2 v3.5.0 / Lua 5.4.8 argument checking in `src/lua_runner/`, plus the C4/C6/C7/C8 fixes and the sol2 safety flags
**Confidence:** HIGH. Every in-repo claim was read from the file this session. sol2/Lua claims come from the vendored sources under `build/_deps/`. Current behaviour was probed with the Debug and Release `quiver_cli` built at HEAD `b8f3246`.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

#### Type-error message shape (SAFE-01/02/03/05)
- **D-01:** Template: `Cannot <op>: <arg> must be a table, got <lua type>`, which extends the existing "options must be a table" style. Key and optional-argument checks follow the same shape: `Cannot <op>: <what> must be <expected>, got <lua type>`.
- **D-02:** `<lua type>` is Lua's own `type()` name (`number`, `string`, `boolean`, `userdata`, `function`, `nil`, `table`). It is the same in every build. A usertype prints as `userdata`.
- **D-03:** The 9 existing hand-written table checks go through `require_table` and gain `, got <type>`. Existing tests match by substring (`expect_lua_error`), so they stay green without editing their expectations.
- **D-04:** The existing "has unsupported Lua type" converter messages (`lua_to_value`, `lua_cell_as`) stay unchanged. They are value-converter errors, not SAFE-05's checks.

#### Text-only load (SAFE-07)
- **D-05:** The global `load` is replaced by a wrapper that always calls the original `load` with mode `"t"`, whatever mode the caller passed. It is installed in the `Impl` constructor next to the `dofile`/`loadfile` nil-out, keeping the constructor order (open_libraries → sandbox edits → `quiver` table → binders → `db`).
- **D-06:** The failure surface keeps `load`'s contract: a bytecode chunk makes `load` return `nil` plus Lua's own message, "attempt to load a binary chunk (mode is 't')". There is no new Pattern 1 text. Tests check both the binary rejection and that string-form `load` (with and without an explicit mode/env) still works.
- **D-07:** `string.dump` stays. Its output is just a string once `load` refuses binary chunks.
- The root AGENTS.md sandbox decision gains "`load` accepts text chunks only".

#### Safety flags and measurement (SAFE-06)
- **D-08:** Perf protocol: Release `quiver_cli` wall time, median of 5 runs, taken before and after the flag commit. Workloads: a 100k-element `read_scalar_floats` bulk read and a 1M-cell `file:read` loop. Run by hand and reported in the PR and SUMMARY, with no committed perf script. The budget is 5%. If it is exceeded, add `SOL_SAFE_GETTER=0` and `SOL_SAFE_STACK_CHECK=0` and re-measure. `SOL_SAFE_FUNCTION_CALLS` and `SOL_SAFE_USERTYPE` are never disabled.
- **D-09:** The stderr-silence test uses gtest `CaptureStderr` around two scripts: one whose error is caught by `pcall` inside the script, and one whose error propagates out of `run()`. Both must leave stderr empty with `SOL_PRINT_ERRORS=0`.
- **D-10:** A Release dot-call (`db.commit()`, which exits 139 today) is covered by the `SOL_ALL_SAFETIES_ON` backstop's raw sol2 text (`sol: received nil for 'self' argument…`), as the roadmap says. A test asserts that a dot-call throws, not crashes, in both builds. There is no explicit Pattern 1 self-check.
- The flags land as the phase's LAST commit, after every explicit check (SAFE-01..05, SAFE-07, FIX-01..03). The no-op `SOL_SAFE_FUNCTION=1` define and its AGENTS.md claim are deleted in the same commit.

#### Commit discipline and CHANGELOG
- **D-11:** Red-then-green: each fix is one commit holding both the new test(s) and the fix. "Red" is shown by running the new test against the pre-fix tree before applying the fix, with the failing output recorded in the plan SUMMARY. Every commit stays green, so the series bisects.
- **D-12:** The C7 BREAKING entry also warns that a `read_vectors_by_id` → `update_element` round trip of an empty or all-NULL column now clears that group instead of skipping it, alongside the clear-on-`{col = {}}` and the typo'd-empty-column throw.
- **D-13:** New tests go in the existing per-domain files (`tests/test_lua_runner_*.cpp`, `tests/test_lua_binary.cpp`, `tests/test_lua_expression.cpp`). No new test file.
- CHANGELOG (roadmap criterion 5): `## [0.13.0] — unreleased` plus the compare link `[0.13.0]: https://github.com/psrenergy/quiver/compare/v0.12.9...v0.13.0` in the existing link block. BREAKING entries (each saying what a script author must change): wrong-type arguments now throw (C1/C5), the empty-array change (C7, with D-12), text-only `load`, and the Release dot-call going from UB to an error with sol2's raw text. `### Fixed`: C2, C4, C6, C8. Behaviour wording only, no planning IDs.
- At the phase end, record the new `Lua*` and C API gtest counts as the baseline Phase 5 must reproduce.

### Claude's Discretion
- The exact helper signatures (`require_table(obj, operation, what)`, `lua_string_key(key, operation, what)`, `optional_from_lua<T>(obj, operation, what)` with `luaL_opt` semantics and `is<BinaryMetadata>()` for the usertype) and the argument names used in messages. Prefer the parameter names in `LUA_DB_API_REFERENCE`.
- Plan and wave grouping by file overlap. Phase 3's single-helper layout means each fix lands in one place.

### Deferred Ideas (OUT OF SCOPE)
- Adding the `got <type>` suffix to the "has unsupported Lua type" converter messages (D-04). It could be done later, editing both throw sites of each.
- An explicit Pattern 1 dot-call self-check in place of sol2's raw backstop text (D-10 alternative).

Also out of scope (CONTEXT `<domain>`): TEST-01 (`resolve_sandboxed_path` unit test), DOC-01 (planning-ID sweep), the DOC-04 "what the sandbox does not limit" sentence, the sparse-extent cap (C3), and M6.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SAFE-01 | Every table parameter is `sol::object` checked by `require_table` (`get_type()`), value-level sites included; Pattern 1 naming op and argument (C1) | Site inventory A (20 parameter conversions, 8 decoder check points, 5 rerouted checks, 2 value-level sites); placement rule; Debug/Release matrix |
| SAFE-02 | `lua_string_key` at the 4 map-key sites (C2) | Site inventory B, with current file:line |
| SAFE-03 | `optional_from_lua` at all 8 optional sites, `luaL_opt` semantics (C5) | Site inventory C, hoisting needed at 3 sites whose decode happens inside unordered function arguments |
| SAFE-04 | `db:transaction` / `db:dry_run` reject a non-function before any side effect (C6) | `run_in_scope` fix shape; observable order test (`transaction(5)` inside an open transaction) |
| SAFE-05 | Consistent "got <lua type>" suffix | `lua_type_name` helper over `lua_typename` (verified type names) |
| SAFE-06 | `SOL_ALL_SAFETIES_ON=1`, `SOL_PRINT_ERRORS=0`, delete `SOL_SAFE_FUNCTION=1`, hand-measured cost | sol2 macro analysis; Debug already has every safety on; perf protocol tried at HEAD; doc/comment sites to edit |
| SAFE-07 | `load` text-only | Verified 3-line Lua wrapper that preserves the none-vs-nil `env` rule |
| FIX-01 | COMMIT failure → best-effort rollback + rethrow (C4) | Reproduced with `PRAGMA defer_foreign_keys` in both builds; fix shape |
| FIX-02 | Empty array reaches the core (C7) | Core semantics read from `database_impl.h`/`database_update.cpp`; no existing test pins the skip; `lua-api.ts:308-309` text |
| FIX-03 | Expression helper errors name the operation; dead branches removed (C8, D1, `lua_data_type_name`) | Caller-threading design; `apply_binop` confirmed gone; `lua_data_type_name` can be derived from `data_type_to_string` |
</phase_requirements>

## Project Constraints (from AGENTS.md)

No `CLAUDE.md` and no `.claude/skills/` exist. The binding directives come from `AGENTS.md` (root) and `src/AGENTS.md`:

- **Error messages live in C++.** Pattern 1 is `Cannot {operation}: {reason}`, and `{operation}` is the public method the user called. No ad-hoc formats.
- **Clean code over defensive code.** Delete unused code; do not deprecate. Be critical, and do not invent changes.
- **Self-updating docs.** Keep the nearest AGENTS.md current (root, `src/`, `tests/`, `bindings/js/`).
- **Changelog.** Every user-visible change gets an entry under the current unreleased version. A breaking one is prefixed **BREAKING** and says what a caller must do. All five manifests are already 0.13.0, so there is no bump.
- **sol2 settings stay PRIVATE on `quiver`.** Write `sol::lua_nil`, never `sol::nil`. csv-parser headers are never included from `src/lua_runner/`. No new sol2 TUs without a reason (Phase 2 note: each sol2 TU costs about 20 s of CPU).
- **Do Not "Fix":** the agent-facing Lua reference stays in `bindings/js/src/lua-api.ts`, and lint debt in untouched JS files is left alone.
- **Tooling.** `clang-format` 22.1.8 (`uvx clang-format==22.1.8`). Python only through `uv run`. Call `quiver_tests.exe` directly for quoted filters, never through `cmd //c` (user memory note).
- **Files.** A `src/lua_runner/` file stays at about 450 lines or fewer (`csv.cpp` is at 420 today).
- **Sync test.** `bindings/js/test/lua-api-sync.test.ts:73` requires that every `.set_function(` in `src/lua_runner/` is a `bind.` or `ns.` call. A `lua.set_function("load", ...)` would fail it.

## Summary

The Phase 3 layout puts almost every fix in one shared decoder. `table_to_element`, `collect_group_columns`, `lua_table_to_value_map`, `lua_table_to_values` and `lua_table_to_dim_map` are the only places the 20 unchecked `sol::table` parameters get walked. A `require_table` call at the top of each decoder, plus switching the bound parameter types to `const sol::object&`, covers SAFE-01 with no per-lambda boilerplate. The decoders run first in every binding body, before any core call, which is exactly where sol2's Debug up-front check fires today. So the check order a script observes stays the same.

Debug builds already have every sol2 safety on: MSVC `_DEBUG` sets `SOL_DEBUG_BUILD`, which turns each safety to `SOL_DEFAULT_ON` (`version.hpp:331-336` and siblings). Debug also prints `[sol2] An exception occurred: …` to stderr for every C++ exception that crosses a binding, and I reproduced that this session. `SOL_ALL_SAFETIES_ON=1` therefore changes nothing in Debug beyond the stderr silence from `SOL_PRINT_ERRORS=0`. In Release it converges behaviour onto the Debug behaviour the whole suite already passes under. The risk of the flag commit is performance, not test breakage. On a trial at HEAD, the D-08 workloads take about 0.64 s (`read_scalar_floats`) and about 0.49 s (`file:read`) as Release medians.

C4 reproduces in both builds. `PRAGMA defer_foreign_keys = ON` plus an orphan `INSERT` inside `db:transaction` makes COMMIT fail with `Failed to commit transaction: FOREIGN KEY constraint failed`, and `db:in_transaction()` is still `true` afterwards. C5, C6, C7, C8 and SAFE-07 also reproduce in both builds with defined behaviour, so their red runs are safe anywhere. C1, C2-with-a-boolean-key and the dot-call are undefined behaviour in Release before their fixes. Show those red in Debug only, and run the dot-call red in Release with a single-test filter, expecting a crash.

**Primary recommendation:** add four helpers to `internal.h` (`lua_type_name`, `require_table`, `lua_string_key`, `optional_from_lua<T>`) sharing one `lua_type_error` message builder. Put the checks in the shared decoders. Land the fixes as 9 red-then-green commits in 4 sequential plans: helpers+C1/C2/C5 → C6/C4/C7 → C8/`load` → flags. Every plan touches `CHANGELOG.md` and `src/AGENTS.md`, so nothing can run in parallel.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Lua argument type checks (SAFE-01..05) | Lua binding layer (`src/lua_runner/internal.h` helpers + decoders) | sol2 (backstop, SAFE-06) | Only the binding layer knows the public operation and argument name, and sol2's own text cannot be Pattern 1 (FEATURES.md "global argument_handler" anti-feature) |
| Transaction sequencing (C4/C6) | Lua binding (`run_in_scope`, `db_core.cpp`) | C++ core (`Database::commit/rollback/end_dry_run`) | The core methods stay as they are. Only the sequencing changes, the same way Julia, Python and Dart already wrap it |
| Empty-array semantics (C7) | C++ core (`Impl::prepare_group_data`) | Lua binding (`table_to_element` stops filtering) | The core already defines "create skips, update routes for DELETE". Lua is the only binding that pre-filters |
| Text-only `load` (SAFE-07) | Lua runner ctor (`lua_runner.cpp` `Impl`) | — | This is sandbox policy, which lives next to the `dofile`/`loadfile` nil-out |
| Build safety flags (SAFE-06) | CMake (`src/CMakeLists.txt`, PRIVATE on `quiver`) | — | Every sol2 TU is in `quiver`, and no test or other target includes `<sol/sol.hpp>` |
| Agent-facing docs | `bindings/js/src/lua-api.ts` | sync test | Do-Not-Fix: the reference stays there |

## Standard Stack

No new dependencies. Everything uses the vendored, already-pinned stack.

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| sol2 | v3.5.0 (FetchContent, `cmake/Dependencies.cmake`) | Lua binding | Already the binding layer. All macro semantics below were read from `build/_deps/sol2-src/include/sol/version.hpp` [VERIFIED: vendored source] |
| Lua | 5.4.8, built as **C** (`LUA_LANGUAGE:STRING=C`, `build/CMakeCache.txt:486`), `LUA_USE_APICHECK=OFF` (`:503`; `build/release/CMakeCache.txt:480`) | Interpreter | `api_check` is a no-op in **both** builds, so `lua_next` on a non-table is UB in Debug as well, not only Release [VERIFIED: `lua-src/src/llimits.h:103-126`] |
| GoogleTest | v1.17.0 | Tests; `testing::internal::CaptureStderr` for D-09 | Already used |

**Installation:** none.

## Package Legitimacy Audit

No external packages are installed or added in this phase, so the gate does not apply.

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

## Architecture Patterns

### System Architecture Diagram

```
Lua script ──call db:x(args)──► sol2 trampoline
                                   │  (Debug today / Release after SAFE-06: up-front check of
                                   │   string/int/self params → raw sol2 text; sol::object params
                                   │   are never checked by sol2)
                                   ▼
                         binding body (db_*.cpp / binary.cpp / csv.cpp)
                                   │
              ┌────────────────────┼─────────────────────────────┐
              ▼                    ▼                             ▼
   optional_from_lua<T>     shared decoder                 resolve_sandboxed_path
   (nil/none → absent;      require_table(obj) FIRST       (path ops only; keeps running
    wrong type → Pattern 1)  → lua_string_key per key       BEFORE option/metadata decode)
              │              → lua_to_value / lua_cell_as
              │                (converter text, D-04)
              └──────────────┬─────────────────────────────┘
                             ▼
                    C++ core (Database / BinaryFile / Expression)
                             │ throws std::runtime_error (Pattern 1/2/3)
                             ▼
                    sol2 exception handler → Lua error string
                    (stderr print only if SOL_PRINT_ERRORS on — Debug today)
```

### Helper design (Claude's discretion: recommended signatures)

Put these in `src/lua_runner/internal.h`, next to `lua_cell_as` / `option_table`. All four share one message builder, so D-01's shape exists once.

```cpp
// Lua's own type() name (D-02). lua_typename maps light userdata to "userdata" too
// (ltm.c luaT_typenames_). A missing argument reads as nil, as in Lua itself.
inline std::string lua_type_name(const sol::object& o) {
    const auto t = o.get_type();
    return t == sol::type::none ? "nil" : sol::type_name(o.lua_state(), t);
}

inline std::runtime_error lua_type_error(const std::string& operation, const std::string& what,
                                         const char* expected, const sol::object& got) {
    return std::runtime_error("Cannot " + operation + ": " + what + " must be " + expected + ", got " +
                              lua_type_name(got));
}

// get_type(), never is<sol::table>(): sol2's table check is loose and accepts userdata.
inline sol::table require_table(const sol::object& o, const std::string& operation, const std::string& what,
                                const char* expected = "a table") {
    if (o.get_type() != sol::type::table) {
        throw lua_type_error(operation, what, expected, o);
    }
    return o.as<sol::table>();
}

inline std::string lua_string_key(const sol::object& key, const std::string& operation, const std::string& what) {
    if (key.get_type() != sol::type::string) {
        throw lua_type_error(operation, what, "a string", key);
    }
    return key.as<std::string>();
}

// luaL_opt semantics (lauxlib.h:152): none/nil → absent, any other type is checked.
template <typename T>
std::optional<T> optional_from_lua(const sol::object& o, const std::string& operation, const std::string& what,
                                   const char* expected) {
    if (!o.valid() || o.get_type() == sol::type::lua_nil) {
        return std::nullopt;
    }
    if constexpr (std::is_same_v<T, sol::table>) {
        return require_table(o, operation, what);
    } else {
        if (auto v = o.as<sol::optional<T>>()) {  // strict for bool (lua_isboolean), number, usertype
            return std::move(*v);
        }
        throw lua_type_error(operation, what, expected, o);
    }
}
```

`sol::type_name(lua_State*, type)` exists at `sol/types.hpp:913` [VERIFIED]. The Lua type-name table is `"no value", "nil", "boolean", udatatypename, "number", "string", "table", "function", udatatypename, "thread"` (`lua-src/src/ltm.c:30-35`) [VERIFIED]. sol2's `bool` check is `lua_isboolean(L_, index) == 1` (`stack_check_unqualified.hpp:108-110`) [VERIFIED], so `"false"` and `1` are rejected for a bool. A missing argument most likely arrives as nil, not none: sol2 builds the object through `lua_pushvalue` + `luaL_ref`, and a nil ref is `LUA_REFNIL` [ASSUMED]. The `none → "nil"` mapping covers both cases. Pin it with `quiver.metadata()` expecting `got nil`.

### Site inventory A: `require_table` (SAFE-01, SAFE-05)

**Placement rule (Pitfall 8):** every converted parameter's check goes in the shared decoder that first walks it. In each binding body that decoder runs before the first core call, which is where sol2's Debug up-front check fires today. None of the 20 sites has a sandbox path, so the "containment before options" pins are not involved. The `open_file` mode-before-path pin and the `write_row` type-before-closed pin are untouched.

**A1. Parameter types to change to `const sol::object&`** (bodies keep calling the same decoder):

| # | Site (current file:line) | Lua method(s) | Check lives in | `what` (recommended) |
|---|---|---|---|---|
| 1-3 | `db_write.cpp:159`, `:164`, `:169-174` | `create_element`, `update_element`, `update_element_by_label` | `table_to_element` (`db_write.cpp:51`) | `element_table` (lua-api.ts:268-270) |
| 4-7 | `db_write.cpp:243-286` | `update_vector_group`, `_by_label`, `update_set_group`, `_by_label` | `collect_group_columns` (`db_write.cpp:94`), via `group_rows_from_lua` (`:228`) | `columns` |
| 8-9 | `db_time_series.cpp:179-207` | `update_time_series_group`, `_by_label` | `collect_group_columns`, via `time_series_rows_from_lua` (`:112`), whose first statement it is, ahead of the metadata lookup | `columns` |
| 10-11 | `db_time_series.cpp:209-232` | `upsert_time_series_row`, `_by_label` | `lua_table_to_value_map` (`db_time_series.cpp:47`) | `row` |
| 12 | `db_time_series.cpp:250` | `update_time_series_files` | top of `update_time_series_files_lua` | `paths` |
| 13-15 | `db_core.cpp:105-119` (`sol::optional<sol::table>`) | `query_string/integer/float` | `optional_from_lua<sol::table>` (also C-inventory 6-8) | `params` (lua-api.ts:627-629) |
| 16 | `binary.cpp:240` | `file:read` `dims` | `lua_table_to_dim_map` (`binary.cpp:27`) | `dims` |
| 17-18 | `binary.cpp:246` | `file:write` `data`, `dims` | `data`: lambda (before `lua_table_to_vector<double>`); `dims`: `lua_table_to_dim_map` | `data`, `dims` (lua-api.ts:886) |
| 19 | `binary.cpp:265` | `quiver.metadata_from_element` | `table_to_element` | `element_table` (lua-api.ts:893 says `tbl`; see Open Q2) |
| 20 | `binary.cpp:293` | `expr:select_agents` | lambda, before `lua_table_to_vector<std::string>` | `labels` |

Decoder signatures change too: `table_to_element`, `collect_group_columns` (`internal.h:246-247`), `lua_table_to_value_map`, `lua_table_to_dim_map` and `lua_table_to_values` take `const sol::object&`. `group_rows_from_lua` and `time_series_rows_from_lua` pass it through. `table_to_element(caller, values)` already receives the public caller (`"metadata_from_element"` at `binary.cpp:266`).

**A2. Existing hand-written checks rerouted (D-03).** After Phase 3 the "9" are 5 throw sites. They keep their `what`, so every existing substring still matches:

| Throw site | Current text (verbatim) | After |
|---|---|---|
| `internal.h:201` (`option_entries`; read_csv, read_csv_stream, write_csv, export/import_csv, `quiver.metadata`) | `"Cannot " + operation + ": options must be a table"` | `require_table(options, operation, "options")` → `…: options must be a table, got number` |
| `internal.h:182` (`option_table`; `header`, `enum_labels` levels) | `"Cannot " + operation + ": option '" + what + "' must be a table"` | `require_table(value, operation, "option '" + what + "'")` |
| `binary.cpp:47-48` (`metadata_array`) | `"Cannot metadata: " + field + " must be a table"` | `require_table(*value, "metadata", field)` |
| `binary.cpp:301-302` (`rename_agents`) | `"Cannot rename_agents: mapping must be a table"` | `require_table(mapping, "rename_agents", "mapping")` |
| `csv.cpp:271-272` (`CsvWriter::write_row`) | `"Cannot write_row: row must be a table"` | `require_table(row, "write_row", "row")`. Keep it as the first statement (write_row pin: type before closed state) |

**A3. Value-level sites:**

| Site | Today | Fix |
|---|---|---|
| `db_write.cpp:58` `table_to_element`: `if (val.is<sol::table>())` | userdata is treated as an array. `require_dense_array` and `arr.size()` then run on userdata; the probe returned the garbage error `sol.quiver::Database*: 000001941D64DE08` | `if (val.get_type() == sol::type::table)`. A userdata then falls to `lua_to_value` → `attribute 'k' has unsupported Lua type` (D-04 text, **no** `got` suffix; see Open Q1) |
| `db_write.cpp:108-109` `collect_group_columns`: `if (!pair.second.is<sol::table>())` | userdata column → zero cells → misleading `columns [value_int] contain no rows` (probed) | `require_table(pair.second, caller, "column '" + name + "'", "an array of values")` → `…column 'x' must be an array of values, got userdata`. The existing tests (`test_lua_runner_time_series.cpp:239`, `test_lua_runner_update.cpp:432`) match `"must be an array of values"` and stay green |
| `db_write.cpp:113-114` (second "must be an array of values" throw: a non-integer or <1 **cell key**) | key-shape error | Leave unchanged. It is not a type error of the argument, and `got <type>` would name the key's type. STATE.md's "edit both sites" note does not fit this one (Open Q3) |

### Site inventory B: `lua_string_key` (SAFE-02)

| # | Site | Today | `what` (recommended) |
|---|---|---|---|
| 1 | `db_time_series.cpp:53` `lua_table_to_value_map` | `auto key = pair.first.as<std::string>();` | `column name` |
| 2 | `db_write.cpp:56` `table_to_element` | `auto k = key.as<std::string>();` | `attribute name` |
| 3 | `binary.cpp:30` `lua_table_to_dim_map` | `auto key = pair.first.as<std::string>();` | `dimension name` |
| 4 | `db_time_series.cpp:253` `update_time_series_files_lua` | `auto key = pair.first.as<std::string>();` | `column name` |

Message: `Cannot upsert_time_series_row: column name must be a string, got number`. The guarded key checks that already exist stay as they are: `string_key` (`db_core.cpp:19-24`, `keys of option '…' must be strings`, pinned verbatim at `test_lua_runner_csv_import.cpp:168`), `option_entries:210-211` (`option key must be a string`) and `collect_group_columns:100-106`. They are already Pattern 1, and rewording them would edit pinned text for no requirement. The range-for walks may throw mid-iteration: `basic_table_iterator`'s destructor pops its stack slots (`table_iterator.hpp:113-120`), as LUA-RUNNER-MAP critic #6 says. So no `collect_entries` is needed.

### Site inventory C: `optional_from_lua` (SAFE-03)

| # | Site | Today | T / `what` / expected | Placement |
|---|---|---|---|---|
| 1 | `binary.cpp:188` `open_file` metadata | `sol::optional<BinaryMetadata>` → wrong type = absent; a plain table then hits BinaryFile's "Metadata must be provided…" | `BinaryMetadata` / `metadata` / `a BinaryMetadata` | **After** `resolve_sandboxed_path` (`:193`), where `md` is built today (`:194`). Order stays mode → path → metadata. Add a pin: `db:open_file('../x','w',{})` reports containment |
| 2 | `binary.cpp:200` `bin_to_csv` aggregate | `sol::optional<bool>`; resolve and `value_or` are both **arguments of one call** (unspecified order) | `bool` / `aggregate` / `a boolean` | Hoist: `const auto resolved = resolve…; const bool aggregate = optional_from_lua<bool>(…).value_or(true);`. Containment first, the convention of every file op |
| 3 | `binary.cpp:240` `file:read` allow_nulls | `sol::optional<bool>`; dims decode and `value_or` are both arguments of `self.read(...)` | `bool` / `allow_nulls` / `a boolean` | Hoist: dims map first, then allow_nulls (argument order) |
| 4 | `binary.cpp:278` `aggregate` parameter | `sol::optional<double>`; `parse_aggregate_op` and the parameter ternary are both arguments of `self.aggregate(...)` | `double` / `parameter` / `a number` | Hoist: op first, then parameter |
| 5 | `binary.cpp:286` `aggregate_agents` parameter | same | same | same |
| 6-8 | `db_core.cpp:108/113/117` `query_*` params | `sol::optional<sol::table>`: `db:query_integer("SELECT 1", 5)` runs with no params (probed, both builds) | `sol::table` / `params` | Only one Lua-derived decode, so no ordering question |

`relation_target_from_lua` (`db_write.cpp:180-188`) is already an optional with `sol::object` semantics and keeps its D-04-style text `target_label has unsupported Lua type`. It is not one of the 8.

The three "hand-written ternaries" FEATURES.md mentions (`:771/:1040/:1047` pre-split) are `binary.cpp:194`, `:282`, `:289`. `optional_from_lua` returns `std::optional<T>` directly, so they disappear.

**Evaluation-order note (Pitfall 8):** today `file:write` (`binary.cpp:247`) decodes `data` and `dims` as two arguments of one call. Their order is unspecified (MSVC and GCC usually go right-to-left, Clang left-to-right), so "which bad cell wins" already differs by compiler. Hoist into locals in argument order (`data`, then `dims`), matching sol2's left-to-right up-front check order. List it in the SUMMARY as the one deliberate reorder ("was compiler-dependent").

### Pattern: `run_in_scope` (C4 + C6)

Current body (`db_core.cpp:70-92`): `begin`; `fn(self)`; on a Lua error `abort` (swallowed) and rethrow; then `finish()` **outside** any try (C4). The lambdas take `sol::protected_function fn` (`:131`, `:137`), so sol2 checks the type, with raw text in Debug and none in Release (C6).

```cpp
sol::object run_in_scope(Database& self, const char* operation, const sol::object& fn_arg,
                         void (Database::*begin)(), void (Database::*finish)(), void (Database::*abort)()) {
    // Before begin: a non-function must not open (or collide with) a transaction.
    if (fn_arg.get_type() != sol::type::function) {
        throw lua_type_error(operation, "fn", "a function", fn_arg);
    }
    const auto fn = fn_arg.as<sol::protected_function>();
    (self.*begin)();
    auto result = fn(std::ref(self));
    try {
        if (!result.valid()) {
            sol::error err = result;
            throw std::runtime_error(err.what());  // same text as today (golden tx_err / dry_err)
        }
        (self.*finish)();                          // a COMMIT / end_dry_run failure lands in the catch too
    } catch (...) {
        try { (self.*abort)(); } catch (...) {}
        throw;
    }
    if (result.return_count() > 0) {
        return result.get<sol::object>(0);
    }
    return sol::make_object(result.lua_state(), sol::lua_nil);
}
```

- **Matches Julia/Python/Dart for `transaction`.** `database_transaction.jl:22-35` and `database.py:370-382` both commit inside the try and roll back best-effort on any failure [VERIFIED: read this session].
- **`dry_run` difference (benign):** Julia and Python call `end_dry_run` on the success path *outside* the try, so a failure surfaces with no retry. In the uniform shape above, a failed `end_dry_run` gets one swallowed retry (`abort == finish == end_dry_run`). That is harmless: `end_dry_run` keeps the flag set on failure (`database.cpp:376-380`), and the original error is rethrown. Criterion 2 ("an `end_dry_run` failure still surfaces") holds.
- **Callable tables:** sol2's `protected_function` check accepted a table or userdata with `__call` (probed text: `must be a function or table or a userdata`). The `get_type()` check rejects them, the same rule as `read_csv_stream`'s `on_row` (`csv.cpp:354-356`). This is an edge narrowing; mention it in the C6 CHANGELOG line.
- `src/AGENTS.md:665-666` says the two lambdas stay "so their Debug bad-argument text is unchanged". Update it, since that text now changes on purpose.

### Pattern: text-only `load` (SAFE-07)

I verified this with the Debug CLI this session by installing it from a script:

```cpp
// lua_runner.cpp Impl ctor, right after the dofile/loadfile nil-out (D-05):
lua.safe_script(R"(
    local load = load
    _G.load = function(chunk, chunkname, _, ...) return load(chunk, chunkname, "t", ...) end
)");
```

- `luaB_load` (`lbaselib.c:387-404`) treats a nil `chunkname`/`mode` as absent (`luaL_optstring`) but tests `env` with `!lua_isnone(L, 4)`. The `...` keeps that distinction. With no 4th argument it passes none, so the global env is used. An explicit `nil` env passes nil. Probe results: `load("return x")()` → 5, `load(s,"c","t",{x=9})()` → 9, explicit nil env → `attempt to index a nil value (upvalue '_ENV')` (identical to stock), reader-function chunks work, and bytecode from a string, with mode `"b"`, or from a reader all give `nil, "attempt to load a binary chunk (mode is 't')"`.
- Do **not** use `lua.set_function("load", …)`. `lua-api-sync.test.ts:73` counts every `.set_function(` and requires it to be `bind.` or `ns.`, and the Phase 3 gate also checks `.set_function(` = 86.
- Known text change: an argument error raised inside the wrapped `load` (e.g. `load({})`) now names the function `'?'` instead of `'load'`, because a tail call loses the name (`ldebug.c:324-328`, `CIST_TAIL`). Nothing pins it. If it matters, use a non-tail call (`local r = table.pack(load(...)); return table.unpack(r, 1, r.n)`), which reports `upvalue 'load'`.
- `string.dump` stays (D-07). `load(string.dump(f))` returns a function today in both builds (probed), so the red is defined everywhere.

### Pattern: C7 (empty arrays reach the core)

- **Where the skip is:** `db_write.cpp:61` `if (arr.size() > 0) { … }`. An empty table falls through and is dropped.
- **Fix:** an empty array becomes `element.set(k, std::vector<Value>{})` (or `std::vector<int64_t>{}`, as Python and JS send). `require_dense_array` passes an empty table (0 entries == size 0).
- **Core semantics** (`database_impl.h:293-296`, verbatim comment): `// Empty array handling: create skips silently, update still routes (for DELETE)`. Code: `if (values.empty() && !delete_existing) { continue; }`.
  - `update_element`, empty array, column in exactly one group: that group is deleted and nothing is inserted, so it is cleared. The **whole** group is cleared, every value column, not just the named one.
  - `update_element`, typo'd empty name: `Cannot update_element: array 'typo' does not match any vector, set, or time series table in collection 'Collection'` (`database_impl.h:301-306`). An empty value on a **scalar** attribute name (`{some_integer = {}}`) gets the same error, because no group has that column.
  - `update_element`, empty column **plus a non-empty column of the same group** (a `read_vectors_by_id` result where `value_int` is all NULL, so `{}`, and `value_float = {1,2}`): `validate_group_columns` throws `Cannot update_element: vector columns in table 'Collection_vector_values' must have the same length` (`database_impl.h:229-239`). Today that round trip works, because Lua drops `value_int`. D-12's CHANGELOG text should name both outcomes: clears when it is the group's only column in the call, and a length error otherwise.
  - **Fan-out:** a shared FK column name clears **every** group that holds it (`relations.sql`: `parent_ref` in `Child_vector_refs` and `Child_set_parents`), and logs the existing fan-out warning. An empty `date_time = {}` routes to every time-series table of the collection that has `date_time`. This is the "C7 fan-out wording" STATE.md asks for.
  - `create_element` skips every empty array **before** the table lookup, so a typo'd empty column is still silently accepted on create (the same as C++/Python/JS). Pin it.
  - `{value_int = {}}` as the **only** key: `update_element` used to throw `element must have at least one attribute to update` (probed). After the fix the element has one (empty) array, so the call clears the group instead.
- **Tests that pin the skip today:** none. A search of `tests/test_lua*.cpp` and every binding test dir found no Lua `create_element`/`update_element` call with `{}`. No expectation changes.
- **Doc text to replace:** `bindings/js/src/lua-api.ts:308-309`, verbatim: `- **Empty arrays are skipped.** An attribute whose value is \`{}\` writes no vector/set (the element type can't be inferred from an empty array), so it is silently dropped.`

### Pattern: C8 + FIX-03

- `to_expression(o)` (`binary.cpp:108-116`) throws `"Cannot build expression: operand must be an expression or a binary file"`. Its callers are `binop<Op>` (`:123-134`, every arithmetic, logical and comparison operator) and the lambdas at `:166`, `:170`, `:315-322`.
- `binop<Op>` is registered by function pointer (`&binop<std::plus<>>`), so it has no runtime caller slot. The smallest change is to make `binop` a factory returning a capturing lambda, `binop<std::plus<>>("add")`, and give `to_expression(o, caller)` the name. sol2 stores capturing lambdas fine. `ns.set_function("gt", binop<std::greater<>>("gt"))` still matches the sync-test regex (first argument is a literal), and the `.set_function(` count is unchanged.
- **Operation names** (Open Q4): for `quiver.*` functions, the registered name (`gt`, `abs`, `ifelse`, `expression`, …). For metamethods, recommend Lua's own event names without underscores: `add`, `sub`, `mul`, `div`, `unm`, `band`, `bor`, `bnot` (the golden probe keys already use these).
- Optional: add `, got <type>` to the operand message for SAFE-05 consistency. It is cheap with `lua_type_error`.
- **D1:** `db_time_series.cpp:255-257` `if (val.is<sol::lua_nil_t>()) { cpp_paths[key] = std::nullopt; }` is unreachable, because `lua_next` never yields nil values. Delete it, and every value goes through `lua_cell_as<std::string>`.
- **`lua_data_type_name`** (`db_metadata.cpp:13-28` post-split, verbatim `default:` text `"Cannot lua_data_type_name: unknown data type "`). `enum class DataType { Integer, Real, Text, DateTime };` (`include/quiver/data_type.h:10`). The Lua names `integer/real/text/date_time` are exactly the lowercase of `data_type_to_string`'s `"INTEGER"/"REAL"/"TEXT"/"DATE_TIME"` (`data_type.h:27-42`). The smallest deletion is `lowercase(data_type_to_string(type))`, which removes the switch. Just dropping `default:` and keeping the switch would trigger MSVC C4715 (`/W4`, `cmake/CompilerOptions.cmake:6`) and GCC `-Wreturn-type` (`-Wall`, `:17`), since a function must still return after a switch over an enum. Use the explicit-ASCII rule from `src/AGENTS.md:212`, not `std::tolower(char)`.
- **`apply_binop`:** gone (no match in `src/lua_runner/`). Phase 3 commit `9c917d1` replaced it with `binop<Op>`.
- Sibling unreachable defaults remain in `db_read.cpp` (`read_scalars_by_id` `:30-34`, `read_groups_by_id` `:65-69`). They already name public operations, and FIX-03 does not list them. Leave them.

### Pattern: SAFE-06 flags

`src/CMakeLists.txt:71-76` today:
```
target_compile_definitions(quiver PRIVATE
    SOL_SAFE_NUMERICS=1
    SOL_SAFE_FUNCTION=1
    SOL_NO_NIL=1
)
```
Target:
```
target_compile_definitions(quiver PRIVATE
    SOL_ALL_SAFETIES_ON=1
    SOL_PRINT_ERRORS=0
    SOL_SAFE_NUMERICS=1
    SOL_NO_NIL=1
)
```
- **`SOL_SAFE_FUNCTION=1` is a no-op** [VERIFIED]. `grep "SOL_SAFE_FUNCTION\b\|SOL_SAFE_FUNCTION)"` over `sol2-src/include/sol/` finds nothing. sol2 reads `SOL_SAFE_FUNCTIONS`, `SOL_SAFE_FUNCTION_OBJECTS` and `SOL_SAFE_FUNCTION_CALLS` (`version.hpp:372-407`). Its claims to delete: `src/AGENTS.md:758-760` (`` `src/CMakeLists.txt` sets `SOL_SAFE_NUMERICS=1` and `SOL_SAFE_FUNCTION=1`, but `SOL_SAFE_GETTER` is left at sol2's default (on in debug, off in release) ``) and `src/lua_runner/internal.h:115-118`.
- **Keep `SOL_SAFE_NUMERICS=1` explicit.** `ALL_SAFETIES` implies it (`version.hpp:433-434`), but `src/AGENTS.md:777` and comments at `csv.cpp:84` and `:237` cite the define.
- **What changes in Debug:** only stderr. With MSVC `_DEBUG`, `SOL_DEBUG_BUILD` makes every safety `SOL_DEFAULT_ON` and `SOL_PRINT_ERRORS` `SOL_DEFAULT_ON` (`version.hpp:331-336`, `:347-352`, `:363-368`, `:385-390`, `:401-406`, `:463-468`, `:630-635`). `SOL_IS_ON` treats DEFAULT_ON as on (`version.hpp:44`, `:57`). So the Debug golden text should not change.
- **What changes in Release:** argument checks (`SOL_SAFE_FUNCTION_CALLS`), the `self` check (`SOL_SAFE_USERTYPE`, `call.hpp:485-492`), checked getters (`SOL_SAFE_GETTER`), reference checks, the stack check, and `sol::function` becoming `protected_function` (`forward.hpp:120-126`; unused here). Release converges on today's Debug behaviour, under which the whole suite passes. The Release golden baselines are already byte-identical to the Debug ones (`diff -r baseline/debug baseline/release` = identical), so expect no Release golden change.
- **stderr sources the D-09 test must silence:** `trampoline.hpp:50-54` (`[sol2] An exception occurred: `) and the `state_handling.hpp` panic/handler prints (`:63`, `:152`). The traceback print at `:88` is commented out. spdlog's console sink is **stderr** (`database.cpp:53` `stderr_color_sink_mt`) at default level Info (`options.h:22`), so the test must open the DB with `{.read_only = false, .console_level = quiver::LogLevel::Off}`, as `test_lua_runner_describe.cpp:9` does. Wrap only `lua.run(...)` in `CaptureStderr()` / `GetCapturedStderr()`. Use a script whose caught error is a C++ throw crossing a binding (e.g. `pcall(function() db:commit() end)`), plus one uncaught (`db:commit()` → `run()` throws). Pure Lua `error()` never printed.
- **Dot-call text (D-10):** a member-pointer method such as `db.commit()` gets `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` (`call.hpp:489-492`; "preceeded" is sol2's spelling). Probed at HEAD in Debug. A lambda-bound method (e.g. `db.create_element(...)`, `Database& self` as an ordinary argument) gets `stack index 1, expected userdata, received …` instead. Assert the substring `received nil for 'self' argument` for `db.commit()` and just "throws" for one lambda-bound method.
- **Comments and docs that become false after the flag:**
  - `internal.h:113-119`: lua_cell_as rationale "unchecked whenever SOL_SAFE_GETTER is off — which is every release build". The helper stays needed, because a checked getter failure is a raw `luaL_error` longjmp, not Pattern 1. Only the reason changes.
  - `internal.h:205-208` (option key comment), `db_write.cpp:97-99`.
  - `binary.cpp:55-56` and `:298-300` ("sol2 does not check a table parameter in Release"). These go in the C1 commit when the code moves anyway, or in the flag commit.
  - `src/AGENTS.md:272-277` and `:756-761`, `tests/AGENTS.md:143`.
  - The history comments in tests (`test_lua_runner_create.cpp:213`, `:293`; `test_lua_binary.cpp:325`) describe why a test exists. Leave them, or reword them to the past tense.
- **Harness:** `build/dedupe-check/wave_gate.sh` hard-codes `-DSOL_SAFE_FUNCTION=1` in the GCC 14 syntax pass, and `gate.sh`/`wave_gate.sh` hard-code `444` / `12` / `86`. Copy them to a Phase 4 harness directory with the counts as variables.

### Perf protocol (D-08), tried at HEAD this session

```bash
# Once, before building the flag commit: keep the "before" Release binaries (the exe loads libquiver.dll next to it).
B=build/perf-before; mkdir -p $B && cp build/release/bin/quiver_cli.exe build/release/bin/libquiver.dll $B/
# Setup (not timed): 100k elements + a 1000x100x10 binary file (1M cells), ~1.3 s
build/release/bin/quiver_cli.exe --schema tests/schemas/valid/collections.sql "$S/perf.db" "$S/setup.lua"
# Median of 5 (3rd of sorted) per workload and binary; Git Bash `date +%s%N` works
for w in read_floats file_read; do for i in 1 2 3 4 5; do s=$(date +%s%N); "$CLI" "$S/perf.db" "$S/$w.lua" >/dev/null; e=$(date +%s%N); echo $(( (e-s)/1000000 )); done | sort -n | sed -n 3p; done
```
Workload scripts, kept outside the repo (no committed perf script):
- `setup.lua`: `db:transaction` over `create_element("Collection", { label = "e"..i, some_float = i*0.5 })` for 100k elements. `quiver.metadata{… labels = 10 names, dimensions = {'row','col'}, dimension_sizes = {1000,100}}`. `db:open_file('perf_bin','w',md)` and write every `{row,col}`.
- `read_floats.lua`: `for _ = 1, 20 do n = n + #db:read_scalar_floats("Collection", "some_float") end`.
- `file_read.lua`: `db:open_file('perf_bin','r')`, then `r:read({row=row,col=col})` over 1000×100 (100k calls, 1M cells).

HEAD Release trial (sorted ms): read_floats `637 640 646 688 824`; file_read `461 466 490 507 710`. Run-to-run spread can exceed 5% (one outlier at +27%). Interleave before/after runs (before, after, before, …) and do one untimed warm-up of each binary, so OS file-cache effects do not show up as a regression. Run on the same AC/power state.

### Recommended plan and wave grouping

Every fix touches `CHANGELOG.md`, and most touch `src/AGENTS.md` or `internal.h`, so **all plans are sequential (one per wave)**. Each bullet is one red-then-green commit (D-11).

| Plan / wave | Commits (in order) | Files | New tests (est.) |
|---|---|---|---|
| **04-01** helpers + Release type checks | (1) open `## [0.13.0] — unreleased` + link (fold into commit 2 or its own docs commit). (2) **C1/SAFE-01+05**: helpers, 20 param conversions, decoder checks, 5 reroutes, 2 value-level sites, `file:write` hoist. (3) **C2/SAFE-02**: 4 key sites. (4) **C5/SAFE-03**: 8 optional sites + hoists | `internal.h`, `db_write.cpp`, `db_time_series.cpp`, `db_core.cpp`, `binary.cpp`, `csv.cpp`, tests (update, create, time_series, query, binary, expression), `CHANGELOG.md`, `src/AGENTS.md` | C1 ≈ 8, C2 ≈ 4, C5 ≈ 5 |
| **04-02** scoped blocks + empty arrays | (5) **C6/SAFE-04**. (6) **C4/FIX-01** (or 5+6 as one `run_in_scope` commit with two tests). (7) **C7/FIX-02** + `lua-api.ts:308-309` | `db_core.cpp`, `db_write.cpp`, `lua-api.ts` (also `:198` "rollback + rethrow if fn errors" → add "or commit fails"), tests (transaction, update, create), `CHANGELOG.md`, `src/AGENTS.md:665` | C6 2, C4 2 (red + "still surfaces" pins), C7 ≈ 4 |
| **04-03** expression names + text-only load | (8) **C8/FIX-03** incl. D1 and `lua_data_type_name`. (9) **SAFE-07** | `binary.cpp`, `db_time_series.cpp`, `db_metadata.cpp`, `lua_runner.cpp`, tests (expression, errors), `lua-api.ts:105`, root `AGENTS.md:86`, `src/AGENTS.md:704-705`, `CHANGELOG.md` | C8 ≈ 2, SAFE-07 ≈ 2 |
| **04-04** backstop flags (LAST) | (10) **SAFE-06**: CMake defines, stale comments/docs, stderr test, dot-call test; hand perf before/after; full six-suite gate; record counts | `src/CMakeLists.txt`, `internal.h`, `binary.cpp`, `db_write.cpp`, `src/AGENTS.md`, `tests/AGENTS.md`, `test_lua_runner_errors.cpp`, `CHANGELOG.md` | 2 |

Expected Lua gtest count ≈ 444 + ~31, about 475 (record the actual). Suites stay **12**: every new test uses an existing fixture (D-13). The C API `LuaRunnerCApiTest` stays at **27**.

**Per-commit gate (reuse `build/dedupe-check/`):** the Debug build, then `Lua*` (count = previous + new) and C API 27, the JS sync test, the `surface.ts` diff (names unchanged → identical), `.set_function(` = 86, clang-format, ≤450 lines, the planning-ID regex, and `golden.sh debug`. Per plan: the Release build, `golden.sh release`, tidy, and the GCC 14 syntax pass with the updated defines. Recapture a fresh baseline at the phase start (`b8f3246`) and confirm it equals the Phase 3 baseline.

**Golden probes that legitimately change (promote with `--capture` after review):**

| Commit | Probe file / keys | Change |
|---|---|---|
| C1 | `options.txt`: every `…_02_number/_03_string/_04_userdata/_09_false` "options must be a table", `enum_level1..3`, `header_number`, `metadata_none/number/labels_number`; `csv.txt`: `row_none/number/string/userdata`; `binary.txt`: `mapping_number`; `debug_text`: `control_writer_write_row` | `, got <type>` appended |
| C6 | `debug_text`: `control_transaction`, `control_dry_run` | sol2 raw text → `Cannot transaction: fn must be a function, got number` (run with `DEBUG_TEXT_CHANGE=1`) |
| C8 | `binary.txt`: `add/sub/mul/div/band/bor_{nn,string}`, `gt/lt/gte/lte/eq/neq_{nn,none,string}` | `Cannot build expression:` → `Cannot <op>:` |
| C2, C5, C4, C7, SAFE-07, SAFE-06 | none | must stay byte-identical. The probes use only valid keys and optionals, no `load`, no commit failure, and no empty element arrays. `core.lua` `tx_err`/`dry_err`/`nested` (traceback text) must stay identical through the `run_in_scope` change |

### Anti-Patterns to Avoid
- **`is<sol::table>()` or `sol::optional<sol::table>` as a guard.** Both use sol2's loose check, which accepts userdata (`stack_check_unqualified.hpp:41-52`, `:472-473`). Only `get_type() == sol::type::table` is a guard.
- **`require_table` at the top of a lambda that also resolves a path.** It would move the error ahead of containment (Pitfall 8). None of the 20 sites has a path, and `open_file` metadata goes after `resolve_sandboxed_path`.
- **Relying on the flag for messages.** `SOL_ALL_SAFETIES_ON` gives raw text and does not fix C5 (FEATURES.md §2). With the flag on, a parameter still typed `sol::table` is rejected by sol2 before the body, so the Pattern 1 check never runs. Convert the parameters first; the flag is the last commit.
- **`lua.set_function` for the `load` wrapper.** It breaks the sync test (see above).
- **An unguarded `.as<T>()` after the flag.** With `SOL_SAFE_GETTER` on, a mismatch is a `luaL_error` longjmp across C++ frames (Lua is C, `ldo.c` longjmp), which is UB for destructors. After this phase every `.as<>` in `src/lua_runner/` is preceded by a strict type check. I grepped every `.as<`/`.get<` site this session. The unguarded ones today are the four C2 key sites, plus two guarded only by the loose `is<sol::table>()`: `db_write.cpp:59` and `:111` (the A3 value-level sites).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Lua type names | A switch over `sol::type` | `sol::type_name(L, t)` (wraps `lua_typename`) | It already maps light userdata to `userdata`, and its names match Lua's `type()` (D-02) |
| Optional-argument semantics | Per-site `valid()`/`get_type()` ternaries | `optional_from_lua<T>` | `luaL_opt` semantics in one place (8 sites) |
| Text-only loading | A C++ reimplementation of `load` (reader functions, env upvalue) | Wrap the original `load` with a forced `"t"` | `luaB_load` already handles readers, chunkname defaults and the env rule. The wrapper only pins `mode` |
| Commit-failure trigger in tests | Mock DB, or locking a file from another process | `PRAGMA defer_foreign_keys = ON` + orphan `INSERT` via `query_string` inside the block | Deterministic, in-memory-capable, and the same in both builds (probed) |
| Stderr capture | Redirecting `std::cerr` rdbuf | `testing::internal::CaptureStderr()` | It captures fd 2, so both sol2's `std::cerr` and spdlog's stderr sink are covered |

## Runtime State Inventory

Not a rename or migration phase, so this section is omitted. (Data change only through C7 semantics, documented in the CHANGELOG. There is no stored-data migration.)

## Debug vs Release behaviour of each red test (Pitfall 1)

| Case | Debug today | Release today | Red run strategy |
|---|---|---|---|
| C1: number/string to a decoder-walked table param (`lua_next`) | sol2 raw `stack index N, expected table, received number: value is not a table or a userdata that can behave like one (bad argument into '…')` (probed) | **UB**: `lua_next` on a non-table; `api_check` compiled out | Red in **Debug only** (`--gtest_filter` on the new tests) |
| C1: number to `lua_len` sites (`file:write` data, `select_agents`) | sol2 raw text | Lua error from `lua_len` raised by `longjmp` through C++ frames (technically UB) | Debug only |
| C1: userdata payload to a group writer | **Silently clears the group** (probed: `ud_group = <no error>`, count 0 afterwards). Iterating a userdata through `lua_next` is UB in both builds, observed harmless | same UB | Debug red run, filtered to that test only, and record it. The committed test only ever runs post-fix |
| C1: userdata as an element attribute value | garbage error `sol.quiver::Database*: 0x…` (probed) | UB | Debug only |
| C2: **number** key | raw sol2 `stack index -1, expected string, received number` | defined: `column '1' not found …` (probed) | Both builds. Use number keys; a boolean key builds `std::string` from `nullptr` in Release |
| C5: wrong-type optional | silently absent (e.g. `query_integer("SELECT 1", 5)` → no error) | same | Both builds |
| C6: `db:transaction(5)` | raw sol2 `expected function` | `attempt to call a number value` + traceback; inside an open transaction: `Cannot begin_transaction: transaction already active` (probed) | Both builds. The in-transaction case is the observable "before any side effect" order test |
| C4: COMMIT failure | `in_transaction()` stays `true` (probed) | same | Both builds |
| C7 | skip (probed) | skip | Both builds |
| C8 | `Cannot build expression: …` | same | Both builds |
| SAFE-07 | bytecode loads (probed) | same | Both builds |
| SAFE-06 stderr | `[sol2] An exception occurred: …` printed (probed) | silent | Debug only |
| SAFE-06 dot-call | sol2 `self` text | **crash, exit 139** (03-REVIEW IN-06) | Release red: run the single test with a filter and record the exit code. The test lands only in the flag commit |

## Common Pitfalls

### Pitfall 1: A C1 test committed before its fix crashes Release CI
**What goes wrong:** a test passes a number to a still-unchecked `sol::table` parameter. **How to avoid:** D-11 puts test and fix in one commit. Run red runs only in Debug, with a filter. **Warning sign:** a red run that segfaults the whole `quiver_tests` process.

### Pitfall 2: The flag commit lands before a parameter is converted
**What goes wrong:** with `SOL_SAFE_FUNCTION_CALLS` on in Release, a parameter still typed `sol::table` gives sol2's raw text instead of the Pattern 1 text the test expects. Debug already behaves this way, so the C1 tests would fail in Debug first. **How to avoid:** the SAFE-06 commit is last (D-10 bullet). Before it, `grep -n "sol::table&\|sol::table [a-z]\|sol::optional<" src/lua_runner/` must list no **parameter** of a bound function.

### Pitfall 3: Function-argument evaluation order
**What goes wrong:** at `bin_to_csv`, `file:read`, `file:write` and `aggregate*`, decoding inside one call's argument list makes the reported error compiler-dependent. **How to avoid:** hoist into ordered locals (Inventory C).

### Pitfall 4: stderr test polluted by spdlog
**What goes wrong:** the default `console_level` is Info on stderr. **How to avoid:** use `LogLevel::Off`, and capture only around `run()`.

### Pitfall 5: Stale natives hide the flag change
**What goes wrong:** the Dart hook's Release build is cached under `.dart_tool/hooks_runner/` (`bindings/dart/AGENTS.md`). **How to avoid:** delete `bindings/dart/.dart_tool/hooks_runner/` and `.dart_tool/lib/` before `test-all.bat` (CONTEXT specifics). Close Julia/Python REPLs that hold `libquiver.dll`.

### Pitfall 6: The C7 change looks like a fix but deletes data
**What goes wrong:** scripts that pass `{}` for "no change" now clear groups, and multi-column round trips now throw a length error. **How to avoid:** a BREAKING entry with both outcomes and the fan-out (D-12). Update `lua-api.ts` in the same commit.

### Pitfall 7: Planning IDs in source or CHANGELOG
**How to avoid:** the gate regex `\b(D|LUA|…|SAFE|FIX|DOC)-[0-9]+\b|…|\b[CM][0-9]{1,2}\b` over `src/lua_runner`. CHANGELOG entries describe behaviour only.

## Code Examples

### Decoder with the check folded in (C1 + C2)
```cpp
// db_time_series.cpp, replacing lua_table_to_value_map (current :47-57)
std::map<std::string, Value> lua_table_to_value_map(const std::string& caller, const sol::object& row) {
    std::map<std::string, Value> result;
    for (auto& pair : require_table(row, caller, "row")) {
        const auto key = lua_string_key(pair.first, caller, "column name");
        result[key] = lua_to_value(pair.second, caller, "column '" + key + "'");
    }
    return result;
}
```

### C4 red test (both builds, relations.sql)
```cpp
// tests/test_lua_runner_transaction.cpp
quiver::LuaRunner lua(db);   // db from VALID_SCHEMA("relations.sql")
expect_lua_error(lua, R"(
    db:transaction(function(d)
        d:query_string("PRAGMA defer_foreign_keys = ON")
        d:query_string("INSERT INTO Child (label, parent_id) VALUES ('orphan', 999)")
    end)
)", "Failed to commit transaction: FOREIGN KEY constraint failed");
EXPECT_FALSE(db.in_transaction());   // today: true (probed in Debug and Release)
```
`Impl::commit` runs `execute_raw("COMMIT;", "commit transaction")` (`database_impl.h:382-385`). `execute_raw` builds `"Failed to " + what + ": " + error` (`database.cpp:229-236`).

### "still surfaces" pins (green before and after)
`db:transaction(function(d) d:commit() end)` → `Cannot commit: no active transaction`. `db:dry_run(function(d) d:end_dry_run() end)` → `Cannot end_dry_run: no active dry run` (`database.cpp:339`, `:370`).

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Release sol2 with no argument checks | `SOL_ALL_SAFETIES_ON` + explicit Pattern 1 checks | this phase | Release equals Debug semantics; UB becomes errors |
| `sol::optional<T>` for absent-means-default | `sol::object` + `luaL_opt` semantics | this phase (src/AGENTS.md:713 already states the rule) | A wrong type throws instead of being ignored |

**Deprecated/outdated:** `SOL_SAFE_FUNCTION=1`. It is not a sol2 macro, so delete it.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | A missing trailing argument reaches a `sol::object` parameter as nil, not none | Helper design | Low. `lua_type_name` maps none to `"nil"` either way. Pinned by a `quiver.metadata()` → `got nil` test |
| A2 | The flag's Release cost is under 5% on the D-08 workloads | Perf protocol | Medium. D-08 has a fallback (`SOL_SAFE_GETTER=0`, `SOL_SAFE_STACK_CHECK=0`). Measured only at HEAD so far, not after the change |
| A3 | No Release-only test relies on a currently unchecked Release path, so the flag breaks nothing | SAFE-06 | Low. Debug already runs with all safeties and the whole suite passes there. CI Release is the confirmation |
| A4 | Recommended argument names (`element_table`, `columns`, `row`, `paths`, `params`, `metadata`, `dims`, `data`, `labels`, `parameter`, `allow_nulls`, `aggregate`, `fn`) and operator names (`add`, `sub`, …) | Inventories, C8 | Text only. The user may prefer other spellings |
| A5 | Interleaved median-of-5 is stable enough to judge 5% on this machine | Perf | Medium. The trial showed one +27% outlier; the median absorbed it |

## Open Questions

1. **The roadmap's criterion 1 vs D-04 for `table_to_element` userdata cells.**
   - What we know: after the value-level fix, a userdata attribute value is rejected by `lua_to_value` with `attribute 'k' has unsupported Lua type`. That is Pattern 1 and names the op and argument, but D-04 freezes that text with no `got` suffix.
   - What's unclear: criterion 1 says these cases "end in 'got <lua type>'".
   - Recommendation: honour D-04 (a locked decision outranks roadmap wording). Note it in the plan's must-haves so the verifier does not flag it. The alternative is a dedicated userdata check in `table_to_element` before `lua_to_value`, which duplicates the converter's rejection.
2. **`metadata_from_element` argument name:** the reference says `tbl`. Recommend `element_table`, for consistency with `table_to_element`'s other callers.
3. **The second "must be an array of values" throw** (`db_write.cpp:113-114`, cell-key shape): recommend no suffix (it is not an argument type error). STATE.md assumed both sites change.
4. **Operator names in C8:** Lua event names (`add`, `unm`, `bnot`, …) or symbols (`+`)? Recommend event names.
5. **Golden-text rewording for the `options`/`row` checks** changes about 40 probe outputs. That is intended (D-03 + SAFE-05); confirm at review.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake + Ninja + MSVC | build | ✓ | cmake 4.3.1-msvc1, ninja 1.13.2 | — |
| Debug and Release builds | gates, perf | ✓ | `build/`, `build/release/` current with HEAD (libquiver.dll built after the last `src/` commit) | — |
| uvx clang-format | format gate | ✓ | 22.1.8 | — |
| bun | JS sync test, golden pretty-print | ✓ | 1.3.14 | — |
| Docker | Linux GCC/Clang check, GCC 14 syntax pass | ✓ | 29.6.2, daemon up | CI only |
| Julia / Dart / uv (Python) | six-suite gate | ✓ | 1.11.9 / 3.13.4 / uv 0.12.3 | — |
| Git Bash `date +%s%N` | perf timing | ✓ | used in the trial | — |

**Missing dependencies:** none.

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — |
| V3 Session Management | no | — |
| V4 Access Control | no (the sandbox path policy is unchanged) | `resolve_sandboxed_path` |
| V5 Input Validation | **yes**: scripts are untrusted input | `require_table` / `lua_string_key` / `optional_from_lua` at the boundary, plus the sol2 backstop |
| V6 Cryptography | no | — |
| V14 Configuration | yes | Build flags PRIVATE on `quiver`; `SOL_PRINT_ERRORS=0` |

### Known Threat Patterns for sol2/Lua hosts

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Type confusion (a non-table read as `Table*`) → memory corruption or crash | Tampering / DoS / EoP | Explicit `get_type()` checks + `SOL_ALL_SAFETIES_ON` (SAFE-01, SAFE-06) |
| Malicious bytecode (Lua 5.4 does not verify bytecode) → arbitrary memory access | EoP | `load` forced to mode `"t"` (SAFE-07); `string.dump` output becomes inert |
| Wrong-type payload silently clears data (userdata → empty group) | Tampering | `require_table` (C1); empty-array semantics documented (C7) |
| `self`-less dot-call → null dereference (exit 139) | DoS | `SOL_SAFE_USERTYPE` via the flag (D-10) |
| Error text leaked to the host's stderr from every caught error | Information disclosure | `SOL_PRINT_ERRORS=0` + CaptureStderr test (D-09) |
| Half-committed transaction after a COMMIT failure | Tampering | Rollback + rethrow (C4) |

## Sources

### Primary (HIGH confidence)
- `build/_deps/sol2-src/include/sol/`: `version.hpp` (safety macros, debug detection, print errors), `trampoline.hpp:30-70`, `state_handling.hpp:30-160`, `call.hpp:485-592`, `table_core.hpp:603-609` (`size()` = `lua_len`), `table_iterator.hpp:40-120`, `protected_function.hpp:78-178`, `forward.hpp:120-139`, `forward_detail.hpp:35-39`, `types.hpp:913`, `stack_check_unqualified.hpp:108-110`
- `build/_deps/lua-src/src/`: `lbaselib.c:323-404` (`luaB_load`, `load_aux`), `llimits.h:100-126`, `ltm.c:30-35`, `ldebug.c:324-328`
- Repo, read this session: `src/lua_runner/*.cpp`, `internal.h`, `src/CMakeLists.txt:71-84`, `src/database.cpp:229-388`, `src/database_impl.h:200-345,382-427`, `src/database_update.cpp`, `src/database_create.cpp`, `include/quiver/data_type.h`, `include/quiver/element.h`, `src/element.cpp`, `tests/test_lua_runner.h`, the test files cited, `bindings/js/src/lua-api.ts`, `bindings/js/test/lua-api-sync.test.ts`, `bindings/julia/src/database_transaction.jl`, `bindings/python/src/quiverdb/database.py:370-422`, `CHANGELOG.md`, `build/dedupe-check/{gate.sh,wave_gate.sh,golden.sh,debug_text.lua,scripts/,baseline/}`
- Probes run this session with `build/bin/quiver_cli.exe` and `build/release/bin/quiver_cli.exe` (scripts in the session scratchpad): C4 commit failure, `load` wrapper semantics, C1/C2/C5/C6/C7/C8 current behaviour, the perf trial

### Secondary (MEDIUM confidence)
- `.planning/research/{LUA-RUNNER-MAP,PITFALLS,FEATURES}.md` (pre-split line numbers, re-located by symbol here)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH. Nothing new; macros read from vendored source.
- Architecture (site inventories, fix shapes): HIGH. Every site was read at its current line, and the fix shapes were checked against the core semantics and probes.
- Pitfalls: HIGH for build behaviour (probed in both builds). MEDIUM for perf headroom (measured only before the change).

**Research date:** 2026-10-03
**Valid until:** until `src/lua_runner/` changes. The line numbers are for HEAD `b8f3246`.
