# Phase 3: Language bindings + documentation - Context

**Gathered:** 2026-05-19
**Status:** Ready for planning

<domain>
## Phase Boundary

Fan out the shipped C symbol `quiver_database_add_time_series_row` (locked in Phase 2) into idiomatic, thin wrappers across every Quiver binding — Julia, Dart, Python, Lua — plus per-binding tests and the targeted documentation sync that `docs/time_series.md` and `CLAUDE.md` need to reflect the actually-shipped surface.

Implementation deliverables:
1. **Julia** — `Quiver.add_time_series_row!(db, collection, group, id; <dim>=…, <value>=…)` wrapper + tests in `bindings/julia/test/test_database_time_series.jl`. Regenerate `bindings/julia/src/c_api.jl` via `bindings/julia/generator/generator.bat`.
2. **Dart** — `Database.addTimeSeriesRow(collection, group, id, row)` wrapper + tests in `bindings/dart/test/database_time_series_test.dart`. Regenerate `bindings/dart/lib/src/ffi/bindings.dart` via `bindings/dart/generator/generator.bat`.
3. **Python** — `Database.add_time_series_row(collection, group, id, **kwargs)` wrapper + tests in `bindings/python/tests/test_database_time_series.py`. Hand-edit `bindings/python/src/quiverdb/_c_api.py` to add the cdef block (per CLAUDE.md: `_c_api.py` is hand-written; `generator/generator.py` only prints to stdout as a diff aid).
4. **Lua** — `db:add_time_series_row(collection, group, id, row_table)` exposed in `src/lua_runner.cpp` + tests appended to `tests/test_lua_runner.cpp`.
5. **Docs** — Narrow update to `docs/time_series.md` to bring its `add_time_series_row!` example into sync with the shipped Julia signature; insertion into `CLAUDE.md` Core API list + Cross-Layer Naming table.

Explicitly NOT in this phase:
- C++ core or C API changes (locked by Phases 1–2)
- `add_time_series_rows` plural batched variant (CORE-15, deferred to v2)
- Multi-element add across multiple `id`s (out of scope per REQUIREMENTS.md)
- Broader cleanup of stale references in `docs/time_series.md` (e.g., `read_time_series_table`, `update_time_series_row!`, `delete_time_series!` — none of which exist in the shipped API). See `<deferred>`.
- Factoring out a shared `marshal_time_series_columns` helper between `add_time_series_row` and `update_time_series_group` in any binding — Phase 2 made the same call for the C API per the "simple over abstract" principle. See `<deferred>`.

</domain>

<decisions>
## Implementation Decisions

### Per-binding wrapper shape (D-01 through D-04)
Each wrapper mirrors its binding's existing `update_time_series_group` marshaling code exactly — same arena / `GC.@preserve` / `keepalive` mechanism, same auto-coercion / typing semantics — with the structural simplification that the row is a single map of column→scalar, not a vector of maps of column→vector. No new helpers are introduced.

- **D-01 (Julia)** — `Quiver.add_time_series_row!(db, collection, group, id; kwargs...)`. Fetch `get_time_series_metadata` for schema typing → same Int→Float auto-coercion when schema column is FLOAT → same `DateTime → date_time_to_string` formatting. Single typed value per column (`int64_t*`, `double*`, or `const char**`) — `column_count` equals `length(kwargs)`. No `row_count` parameter (matches C API signature). Place alongside `update_time_series_group!` in `bindings/julia/src/database_update.jl`.
- **D-02 (Dart)** — `Database.addTimeSeriesRow(String collection, String group, int id, Map<String, Object> row)`. Same runtime-type dispatch on `entry.value.runtimeType` (int/double/String/DateTime) used by `updateTimeSeriesGroup`. Allocate single-element typed arrays in the arena. Place alongside `updateTimeSeriesGroup` in `bindings/dart/lib/src/database_update.dart`.
- **D-03 (Python)** — `Database.add_time_series_row(self, collection: str, group: str, id: int, **kwargs)`. Mirror `_marshal_time_series_columns`'s **strict** typing (no Int→Float coercion — Python's update wrapper does not coerce; preserve homogeneity). Build single-element `ffi.new("int64_t[]", [v])` / `ffi.new("double[]", [v])` / `ffi.new("char*[]", [c_str])` arrays. Place the method in `bindings/python/src/quiverdb/database.py` alongside `update_time_series_group`. Dict unpacking (`db.add_time_series_row("Col", "grp", 1, **row_dict)`) falls out naturally from `**kwargs` — explicitly tested per PY-11.
- **D-04 (Lua)** — `db:add_time_series_row(collection, group, id, row_table)`. Hand-add `add_time_series_row_lua` static function in `src/lua_runner.cpp` that calls the existing `lua_table_to_value_map` helper (already handles `nil`/`int64`/`double`/`string`) and forwards to `db.add_time_series_row(collection, group, id, row)`. Bind via `bind.set_function("add_time_series_row", &add_time_series_row_lua)` alongside `update_time_series_group`.

### FFI regeneration strategy (D-05)
- **Julia / Dart** — Run `bindings/{julia,dart}/generator/generator.bat`. Generators parse `include/quiver/c/database.h` and emit the FFI binding automatically. The new symbol picks up mechanically.
- **Python** — Hand-edit `bindings/python/src/quiverdb/_c_api.py` to add the new `quiver_database_add_time_series_row` cdef in the `time_series` block. Run `python bindings/python/generator/generator.py` (stdout-only) once for cross-check; commit only the hand-edited `_c_api.py`.
- **Lua** — No generator; hand-edit `src/lua_runner.cpp`.

### Plan split (D-06)
Five atomic plans, one per deliverable. Each plan covers wrapper + tests for its binding; the docs plan is separate. Rationale: each binding is an independent codebase with its own toolchain, test harness, and failure modes — atomic per-binding commits give clean revert points and allow Phase 3 to be executed in parallel if desired without cross-contamination.

- **Plan 03-01** — Julia: regenerate `c_api.jl`; add `add_time_series_row!` to `database_update.jl`; export from `Quiver.jl`; add tests to `test_database_time_series.jl`; run `bindings/julia/test/test.bat`.
- **Plan 03-02** — Dart: regenerate `bindings.dart`; add `addTimeSeriesRow` to `database_update.dart` extension; add tests to `database_time_series_test.dart`; run `bindings/dart/test/test.bat` (with `.dart_tool/hooks_runner/` cleanup if DLL stale per CLAUDE.md).
- **Plan 03-03** — Python: hand-add cdef to `_c_api.py`; add `add_time_series_row` method to `database.py`; add tests to `test_database_time_series.py` including dict-unpacking case; run `bindings/python/test/test.bat`.
- **Plan 03-04** — Lua: hand-add `add_time_series_row_lua` + `bind.set_function` in `src/lua_runner.cpp`; rebuild via `cmake --build build --config Debug`; add `LuaRunnerTest.AddTimeSeriesRow*` tests to `tests/test_lua_runner.cpp`; run `./build/bin/quiver_tests.exe`.
- **Plan 03-05** — Docs: rewrite the stale `add_time_series_row!` example in `docs/time_series.md` (section "Inserting data") to match the shipped Julia signature exactly. Add `add_time_series_row` to `CLAUDE.md` Core API "Time series" bullet and the cross-layer naming table. No other doc edits in this plan.

### Test coverage breadth (D-07)
Per-binding tests stay at REQUIREMENTS minimums plus one happy-path multi-dimension PK case. Reason: Phase 1 (9 C++ scenarios) and Phase 2 (12 C API scenarios) already exhaustively cover semantics, marshaling, and error branches. Each binding test only needs to prove the language-level wrapper round-trips through the FFI correctly. Over-mirroring the error matrix at every binding is duplicate work that has caught no bugs historically in the parallel `update_time_series_group` wrappers.

Final scenario list per binding (run against `tests/schemas/valid/collections.sql` unless noted):
- **Julia (3):** `Quiver.add_time_series_row!` insert (single `date_time` + `value`); upsert (same key overwrites value); multi-dim happy path against `tests/schemas/valid/multi_dim_time_series.sql` (`date_time + block` PK); one error path — missing dimension column (verifies the canonical `"Cannot add_time_series_row: ..."` message surfaces through Julia's `check()` as a thrown exception).
- **Dart (3):** `addTimeSeriesRow` insert; upsert; multi-dim happy path. Error paths covered by the existing FFI error-relay tests in other Dart suites — no new error test needed unless Dart's `runtimeType` dispatch surfaces a unique failure mode.
- **Python (4):** `add_time_series_row` via kwargs insert; upsert; dict unpacking (`db.add_time_series_row("Col", "grp", 1, **row_dict)`); multi-dim happy path.
- **Lua (3):** `db:add_time_series_row` via Lua table insert; upsert; multi-dim happy path.

### Test file placement (D-08)
Match existing convention rather than the verbatim file names in REQUIREMENTS.md (which were approximate). The spec's value is "tests exist and pass", not "tests live in a specific filename".

- **Julia tests** → `bindings/julia/test/test_database_time_series.jl` (existing file; already contains `@testset "Update"` and other time-series tests). Add a new `@testset "Add Row"` block. REQUIREMENTS named `test_time_series.jl` — interpret as "the time-series test file"; the existing one matches.
- **Dart tests** → `bindings/dart/test/database_time_series_test.dart` (existing file). Add new `group('Add Time Series Row', () { ... })` block.
- **Python tests** → `bindings/python/tests/test_database_time_series.py` (existing file).
- **Lua tests** → `tests/test_lua_runner.cpp` (existing file; already has `LuaRunnerTest.UpdateTimeSeriesGroup` test as direct precedent). REQUIREMENTS named `tests/lua/` "or equivalent existing Lua test harness" — `test_lua_runner.cpp` is that harness.

### Documentation scope (D-09)
Narrow per DOC-11 verbatim. The whole `docs/time_series.md` file is heavily stale (references `read_time_series_table`, `update_time_series_row!`, `delete_time_series!` — none of which exist in the shipped API), but DOC-11 only requires fixing the `add_time_series_row!` example to match the shipped Julia signature. Broader doc cleanup is deferred (see `<deferred>`).

- **D-09a (`docs/time_series.md`)** — Rewrite ONLY the "Inserting data" subsection that demonstrates `Quiver.add_time_series_row!`. Current example uses the aspirational signature `add_time_series_row!(db, "Resource", "some_vector1", "Resource 1", 10.0; date_time = DateTime(2000))` (collection + attribute + label + value + date kwarg). Replace with the shipped signature: `add_time_series_row!(db, "Resource", "group1", id; date_time = DateTime(2000), some_vector1 = 10.0)` (collection + group + id + dimension and value kwargs). Keep the surrounding prose unchanged. Do not touch other sections.
- **D-09b (`CLAUDE.md`)** — Two surgical edits:
  1. Add `add_time_series_row()` to the "Time series" bullet in the "Core API" section (currently lists `read_time_series_group()`, `update_time_series_group()`).
  2. Add a row to the "Representative Cross-Layer Examples" table between "Time series read" and "Time series update":

     | Category | C++ | C API | Julia | Dart | Lua |
     |---|---|---|---|---|---|
     | Time series add row | `add_time_series_row()` | `quiver_database_add_time_series_row()` | `add_time_series_row!()` | `addTimeSeriesRow()` | `add_time_series_row()` |

### Error message surfacing (D-10)
All Pattern-1 errors (`"Cannot add_time_series_row: ..."`) originate in C++ (Phase 1). The C wrapper (Phase 2) relays them via `quiver_set_last_error`. Each binding's existing `check()` / error-conversion path picks them up unchanged. **No new error strings in any binding.** Per CLAUDE.md: "All error messages are defined in the C++/C API layer. Bindings retrieve and surface them — they never craft their own."

### Claude's Discretion
- Internal naming of static helper functions in `src/lua_runner.cpp` (e.g., reuse `lua_table_to_value_map` directly vs add a tiny `lua_table_to_value_map_single_row` — the former is a single-line forward, no helper needed).
- Whether to add a Python error-path test (missing dimension) — REQUIREMENTS doesn't require it for Python but adding one costs ~5 lines; default is to skip per the test-coverage decision above.
- Ordering of test cases within each binding's new testset/group/block — match the C++ test file's order (insert → upsert → multi-dim) for review consistency.
- Whether to run `bindings/dart/test/test.bat` with a fresh `.dart_tool/` cleanup or rely on incremental rebuild — start incremental; fall back to clean per the CLAUDE.md note if DLL drift is observed.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase 3 scope and requirements
- `.planning/ROADMAP.md` §"Phase 3: Language bindings + documentation" — phase goal, success criteria, dependency on Phase 2
- `.planning/REQUIREMENTS.md` §"Bindings" + §"Documentation" — JULIA-11, DART-11, PY-11, LUA-11, DOC-11 acceptance criteria
- `.planning/PROJECT.md` §"Current Milestone: v1.1 add_time_series_row" — milestone scope and constraints

### C++ + C API surface being wrapped
- `include/quiver/database.h:120-129` — `Database::add_time_series_row` C++ declaration (the semantics every binding must round-trip)
- `include/quiver/c/database.h:308-315` — `quiver_database_add_time_series_row` C API declaration (the FFI signature every binding marshals against)
- `src/database_time_series.cpp` — Pattern-1 error strings (`"Cannot add_time_series_row: ..."`) — originate here, surface unchanged through every binding
- `src/c/database_time_series.cpp` — C API implementation (marshaling reference; bindings mirror its column shape minus `row_count`)
- `tests/test_database_time_series.cpp` — nine C++ `AddTimeSeriesRow*` scenarios — defines the semantics each binding round-trips
- `tests/test_c_api_database_time_series.cpp` — twelve C API `AddTimeSeriesRow*` scenarios at the FFI boundary
- `tests/schemas/valid/collections.sql` — primary test schema (single-dim time series with `date_time` PK)
- `tests/schemas/valid/multi_dim_time_series.sql` — composite-PK schema (id + date_time + block) reused for multi-dim happy-path tests in every binding

### Per-binding update_time_series_group references (the marshaling templates to mirror)
- `bindings/julia/src/database_update.jl:17-121` — Julia `update_time_series_group!` (kwargs + metadata-driven typing + Int→Float coercion + DateTime formatting + `GC.@preserve` keepalive)
- `bindings/dart/lib/src/database_update.dart:47-149` — Dart `updateTimeSeriesGroup` (Map + runtimeType dispatch + arena allocation)
- `bindings/python/src/quiverdb/database.py:1156-1187` — Python `update_time_series_group` (rows + `_marshal_time_series_columns` helper at line 1482-1547)
- `src/lua_runner.cpp:795-806` — Lua `update_time_series_group_lua` (uses `lua_table_to_value_map` helper at line 215-231)

### FFI binding files (target outputs of generators / hand-edits)
- `bindings/julia/src/c_api.jl` (732 lines) — regenerated by `bindings/julia/generator/generator.bat`
- `bindings/dart/lib/src/ffi/bindings.dart` (2763 lines) — regenerated by `bindings/dart/generator/generator.bat`
- `bindings/python/src/quiverdb/_c_api.py` (338 lines) — **hand-edited**; `bindings/python/generator/generator.py` is a stdout diff aid only

### Existing test files to extend
- `bindings/julia/test/test_database_time_series.jl` — extend with `@testset "Add Row"`
- `bindings/dart/test/database_time_series_test.dart` — extend with `group('Add Time Series Row', ...)`
- `bindings/python/tests/test_database_time_series.py` — extend with new test functions
- `tests/test_lua_runner.cpp` — extend with `LuaRunnerTest.AddTimeSeriesRow*` cases (sibling pattern: `LuaRunnerTest.UpdateTimeSeriesGroup` at line 1189)

### Documentation targets
- `docs/time_series.md` §"Inserting data" (line 119-154) — the `add_time_series_row!` example to rewrite (DOC-11 scope)
- `CLAUDE.md` §"Core API" + §"Cross-Layer Naming Conventions" — the two surgical inserts described in D-09b

### Project conventions
- `CLAUDE.md` §"Cross-Layer Naming Conventions" — mechanical transformation rules from C++ to each binding
- `CLAUDE.md` §"Bindings" — generator commands per binding, Julia/Dart/Python-specific notes
- `CLAUDE.md` §"Error Handling" + "Error messages defined in C++/C API layer" — locks the no-binding-error-strings policy
- `CLAUDE.md` §"Multi-Column Time Series" — the columnar typed-arrays pattern documentation (note: post-Phase 2 it covers the `row_count`-less single-row variant)

### Phase 1 & 2 artifacts (informational)
- `.planning/phases/01-c-core-add-time-series-row/01-01-SUMMARY.md`, `01-02-SUMMARY.md` — C++ core + tests
- `.planning/phases/02-c-api-quiver-database-add-time-series-row/02-CONTEXT.md` — Phase 2 decisions (FFI signature shape, error-relay policy)
- `.planning/phases/02-c-api-quiver-database-add-time-series-row/02-01-SUMMARY.md`, `02-02-SUMMARY.md` — C API decl/impl + tests
- `.planning/phases/02-c-api-quiver-database-add-time-series-row/02-VERIFICATION.md` — proves the C symbol is shipped, callable, and tested at the FFI boundary

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **`update_time_series_group` wrappers per binding** — direct templates. Each binding's `add_time_series_row` is structurally a "single-row mode" of its sibling: same marshaling switch, same keepalive/arena lifetime, same error relay. Phase 3 changes are mechanical edits, not new design.
- **`lua_table_to_value_map` (src/lua_runner.cpp:215-231)** — already handles `nil` / `int64` / `double` / `string` conversion from a Lua table. The new `add_time_series_row_lua` is a one-line forward: `db.add_time_series_row(c, g, id, lua_table_to_value_map(row))`.
- **Julia `get_time_series_metadata` for schema typing** (bindings/julia/src/database_metadata.jl) — same metadata fetch used by `update_time_series_group!` for Int→Float coercion.
- **Python `_marshal_time_series_columns` (bindings/python/src/quiverdb/database.py:1482-1547)** — module-level helper. The new `add_time_series_row` does NOT need to reuse it (different row shape — single map, not list of maps); inline marshaling is shorter and clearer per the "simple over abstract" principle.
- **Dart `Arena` allocator** — already used by `updateTimeSeriesGroup`. Same pattern.

### Established Patterns
- **Three-message error convention** — locked at C++ layer. Bindings never craft messages.
- **`check()` / error-conversion helpers per binding** — Julia `check(C.quiver_database_add_time_series_row(...))`, Dart `check(bindings.quiver_database_add_time_series_row(...))`, Python `check(lib.quiver_database_add_time_series_row(...))`. Each binding already has its `check()`; nothing new.
- **Generator-driven FFI bindings (Julia, Dart)** — re-running the generator script is sufficient to pick up the new symbol from `include/quiver/c/database.h`. Python's `_c_api.py` is hand-maintained per CLAUDE.md.
- **Atomic-commit rhythm** — Phases 1 and 2 each committed every plan step atomically (decl, impl, tests, doc updates as separate commits). Phase 3's 5 plans should preserve this — one atomic commit per file touched within each plan.
- **`!` suffix for mutating Julia methods** — locked. `add_time_series_row!` (with bang) is the Julia name.
- **Naming transformation rules (CLAUDE.md)** — mechanical: C++ `add_time_series_row` → C API `quiver_database_add_time_series_row` → Julia `add_time_series_row!` → Dart `addTimeSeriesRow` → Python `add_time_series_row` → Lua `add_time_series_row`.

### Integration Points
- **`include/quiver/c/database.h`** — single source the Julia + Dart generators parse. The new declaration was added in Phase 2; this phase just consumes it.
- **Each binding's public module entry point** — `Quiver.jl` re-exports public symbols (verify `add_time_series_row!` is exported); `bindings/dart/lib/quiverdb.dart` exposes the `Database` class with the extension already; Python's `database.py` is its own module; Lua sees whatever `LuaRunner` binds.
- **Build artifacts** — Dart needs `libquiver_c.dll` + `libquiver.dll` on PATH (handled by `test.bat`). Python needs `build/bin/` on PATH (also handled by `test.bat`). Lua tests run inside the C++ GTest harness, which links statically.

</code_context>

<specifics>
## Specific Ideas

- **"Stick to the established pattern"** (carried forward from Phase 2 user directive) — interpreted at the binding layer as: for each binding, the new `add_time_series_row` wrapper copies the structure of that binding's existing `update_time_series_group` wrapper exactly. No new helper abstractions, no novel marshaling shape, no per-binding deviation from its sibling's conventions.
- **Docs file is broadly stale, but DOC-11 is narrow** — explicit user-facing decision to NOT fan out into a larger doc cleanup. Captured in `<deferred>`.

</specifics>

<deferred>
## Deferred Ideas

- **Broader `docs/time_series.md` cleanup** — The file references several functions that don't exist in the shipped API: `read_time_series_table`, `update_time_series_row!`, `delete_time_series!`. DOC-11 only mandates fixing the `add_time_series_row!` example. A dedicated docs-cleanup phase (or `/gsd-docs-update`) should sweep the rest. **Not in v1.1 scope.**
- **Shared `marshal_time_series_columns` helper between add and update per binding** — Phase 2 already declined this for the C API per the "simple over abstract" principle. Phase 3 makes the same call for each binding. If the marshaling duplication becomes awkward at review time across all four bindings simultaneously, a future cleanup phase can extract a helper per binding (or rethink at the C++ layer).
- **`add_time_series_rows` (plural batched variant)** — CORE-15, already deferred to v2 per REQUIREMENTS.md.
- **Multi-element add (rows for multiple `id`s in one call)** — Out of scope per REQUIREMENTS.md.
- **Streaming row append for very large datasets** — CORE-16, already deferred to v2.
- **High-level `read_time_series_row` wrappers in Dart and Python** — Julia already has `read_time_series_row` (bindings/julia/src/database_read.jl:582); Dart and Python only expose the raw C symbol in their FFI binding layer but no idiomatic wrapper. **Out of scope** for this phase — Phase 3 is about `add_time_series_row` symmetry, not retroactive `read_time_series_row` exposure. Worth a future single-issue phase.
- **Julia test file rename** (`test_database_time_series.jl` → `test_time_series.jl` to match the literal name in REQUIREMENTS.md) — not worth churning git history for a naming preference. Stay with the existing filename.

</deferred>

---

*Phase: 03-language-bindings-documentation*
*Context gathered: 2026-05-19*
