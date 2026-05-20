---
id: 03-01-julia-add-time-series-row
phase: 03
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - bindings/julia/src/c_api.jl
  - bindings/julia/src/database_update.jl
  - bindings/julia/test/test_database_time_series.jl
requirements: [JULIA-11]
autonomous: true

must_haves:
  truths:
    - "Julia users can call Quiver.add_time_series_row!(db, collection, group, id; <dim>=…, <value>=…) after Phase 3"
    - "Calling Quiver.add_time_series_row! twice with the same key upserts (value columns are overwritten)"
    - "Multi-dimension PK schemas (date_time + block) round-trip through the Julia wrapper"
    - "Missing-dimension errors surface as a thrown exception carrying the canonical 'Cannot add_time_series_row: ...' message"
    - "bindings/julia/test/test.bat exits 0 with the new @testset 'Add Row' present"
  artifacts:
    - path: "bindings/julia/src/c_api.jl"
      provides: "Regenerated FFI binding containing the quiver_database_add_time_series_row ccall symbol"
      contains: "quiver_database_add_time_series_row"
    - path: "bindings/julia/src/database_update.jl"
      provides: "Julia idiomatic wrapper add_time_series_row!"
      contains: "function add_time_series_row!"
    - path: "bindings/julia/test/test_database_time_series.jl"
      provides: "Insert/upsert/multi-dim/missing-dim test coverage"
      contains: "@testset \"Add Row\""
  key_links:
    - from: "bindings/julia/src/database_update.jl"
      to: "C.quiver_database_add_time_series_row"
      via: "ccall via the regenerated c_api.jl module"
      pattern: "C\\.quiver_database_add_time_series_row"
    - from: "bindings/julia/src/database_update.jl"
      to: "get_time_series_metadata"
      via: "schema typing fetch for Int→Float auto-coercion"
      pattern: "get_time_series_metadata"
---

<objective>
Ship the Julia idiomatic wrapper `Quiver.add_time_series_row!(db, collection, group, id; kwargs...)` that mirrors the marshaling structure of the sibling `update_time_series_group!` (kwargs + metadata-driven typing + Int→Float auto-coercion + DateTime formatting + GC.@preserve keepalive), structurally simplified to a single-value-per-column shape with no `row_count` parameter. Add @testset "Add Row" coverage (insert + upsert + multi-dim + missing-dim error) and run `bindings/julia/test/test.bat` to verify the round-trip.

Purpose: Closes JULIA-11. Per D-01 / D-05 / D-06 / D-07 / D-08 / D-10 in `.planning/phases/03-language-bindings-documentation/03-CONTEXT.md`.
Output: Regenerated `c_api.jl` with the new ccall symbol, new `add_time_series_row!` function alongside `update_time_series_group!`, new tests under the existing `@testset "Time Series"` block.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md
@.planning/ROADMAP.md
@.planning/STATE.md
@.planning/phases/03-language-bindings-documentation/03-CONTEXT.md
@CLAUDE.md
@include/quiver/c/database.h
@bindings/julia/src/database_update.jl
@bindings/julia/src/database_metadata.jl
@bindings/julia/test/test_database_time_series.jl

<interfaces>
<!-- C API symbol the Julia wrapper marshals against, per CONTEXT.md canonical_refs -->

From include/quiver/c/database.h (lines 308-315):
```c
QUIVER_C_API quiver_error_t quiver_database_add_time_series_row(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* group,
                                                                int64_t id,
                                                                const char* const* column_names,
                                                                const int* column_types,
                                                                const void* const* column_data,
                                                                size_t column_count);
```

Sibling wrapper template (the exact structure to mirror), from bindings/julia/src/database_update.jl:17-121:
- Function signature: `update_time_series_group!(db::Database, collection::String, group::String, id::Int64; kwargs...)`
- Fetches `get_time_series_metadata(db, collection, group)` and builds `schema_types::Dict{String, C.quiver_data_type_t}` to drive Int→Float auto-coercion
- Builds `col_names_strs`, `col_types::Vector{Cint}`, `col_data_ptrs::Vector{Ptr{Cvoid}}`, `refs::Vector{Any}` (keepalive)
- Per-kwarg dispatch on `eltype(v)`: `DateTime` → `date_time_to_string` + `Cstring` array; `AbstractString` → `Cstring` array; `Integer` w/ FLOAT schema → coerce to `Float64`; `Integer` (other) → `Int64`; `Real` → `Float64`
- Final `GC.@preserve refs begin check(C.quiver_database_update_time_series_group(...)) end`

For `add_time_series_row!`, each `v` is a **scalar** (not a vector). Use the same dispatch chain but allocate a single-element typed array per kwarg (e.g. `Int64[Int64(v)]`, `Float64[Float64(v)]`, `Cstring` array of length 1). `column_count == length(kwargs)`. No `row_count` parameter — the C symbol has none.
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Regenerate Julia c_api.jl</name>
  <read_first>
    include/quiver/c/database.h (lines 295-326 — the add_time_series_row declaration + neighbours)
    bindings/julia/generator/generator.bat
    bindings/julia/src/c_api.jl (verify current state — should NOT yet contain the symbol)
  </read_first>
  <action>
    Run `bindings/julia/generator/generator.bat` from the repository root. The generator parses `include/quiver/c/database.h` and emits `bindings/julia/src/c_api.jl`. Do not hand-edit `c_api.jl`. Commit only the regenerated file. Do NOT delete `bindings/julia/Manifest.toml` unless the generator fails with a version conflict (per CLAUDE.md "Julia Notes").
  </action>
  <verify>
    <automated>findstr /C:"quiver_database_add_time_series_row" bindings\julia\src\c_api.jl</automated>
  </verify>
  <done>`bindings/julia/src/c_api.jl` contains the ccall declaration for `quiver_database_add_time_series_row` with the 8-parameter signature (db, collection, group, id, column_names, column_types, column_data, column_count) and no row_count.</done>
</task>

<task type="auto" tdd="true">
  <name>Task 2: Add Julia add_time_series_row! wrapper</name>
  <files>bindings/julia/src/database_update.jl</files>
  <read_first>
    bindings/julia/src/database_update.jl (lines 1-121 — the sibling `update_time_series_group!` template)
    bindings/julia/src/database_metadata.jl (the `get_time_series_metadata` signature)
    bindings/julia/src/c_api.jl (verify Task 1 regeneration committed the new ccall)
    include/quiver/c/database.h (lines 308-315)
    .planning/phases/03-language-bindings-documentation/03-CONTEXT.md (D-01)
  </read_first>
  <behavior>
    - Function `add_time_series_row!(db::Database, collection::String, group::String, id::Int64; kwargs...)` exists in `bindings/julia/src/database_update.jl`
    - Returns `nothing`; mutating semantics matches the `!` suffix convention
    - Each kwarg becomes a single-element typed array; `column_count == length(kwargs)`
    - Dispatch on `eltype(v)` mirrors `update_time_series_group!`: DateTime → string formatting; AbstractString → string; Integer with FLOAT schema → coerce to Float64; Integer (other) → Int64; Real → Float64
    - Uses `GC.@preserve refs begin check(C.quiver_database_add_time_series_row(db.ptr, collection, group, id, name_ptrs, col_types_arr, col_data_arr, Csize_t(column_count))) end`
    - No `row_count` parameter in the ccall — the C signature does not have it
    - No binding-layer error strings introduced (no new `throw(ErrorException(...))` carrying "Cannot add_time_series_row:" text — those come from C++ via `check()`)
  </behavior>
  <action>
    In `bindings/julia/src/database_update.jl`, alongside `update_time_series_group!` (insert immediately after the function ends — the natural sibling location per D-01), add a new `function add_time_series_row!(db::Database, collection::String, group::String, id::Int64; kwargs...)`. Mirror the marshaling structure of `update_time_series_group!` exactly, with these adaptations:
    - Drop the empty-kwargs clear-all branch (add_time_series_row has no clear semantics — the row IS the payload). If `isempty(kwargs)`, do NOT special-case; let the C API surface the canonical missing-dimension error (per D-10).
    - Drop the "validate all vectors have the same length" loop — values are scalars, not vectors.
    - Drop the per-element loop when building typed arrays — each kwarg produces a length-1 typed array.
    - In the `Integer`/`Real`/`AbstractString`/`DateTime` branches: build a 1-element array (e.g. `Int64[Int64(v)]`, `Float64[Float64(v)]`, a 1-element `Cstring` array). Keep using the same `schema_types` lookup driven by `get_time_series_metadata` for Int→Float coercion.
    - Final ccall is `C.quiver_database_add_time_series_row(db.ptr, collection, group, id, name_ptrs, col_types_arr, col_data_arr, Csize_t(column_count))` — eight arguments, no `row_count`.
    - Return `nothing`.
    No new helpers; no factoring with `update_time_series_group!`; "simple over abstract" per `<deferred>` in CONTEXT.md.
  </action>
  <verify>
    <automated>findstr /C:"function add_time_series_row!" bindings\julia\src\database_update.jl &amp;&amp; findstr /C:"C.quiver_database_add_time_series_row" bindings\julia\src\database_update.jl</automated>
  </verify>
  <done>`bindings/julia/src/database_update.jl` contains the new `function add_time_series_row!` definition with kwargs, schema-typed marshaling, GC.@preserve keepalive, and the 8-argument ccall to `C.quiver_database_add_time_series_row`. No `row_count` argument. No new error strings.</done>
</task>

<task type="auto" tdd="true">
  <name>Task 3: Add Julia @testset "Add Row" coverage</name>
  <files>bindings/julia/test/test_database_time_series.jl</files>
  <read_first>
    bindings/julia/test/test_database_time_series.jl (lines 88-117 — the existing `@testset "Update"` block as test-structure reference)
    bindings/julia/test/test_database_time_series.jl (lines 1-30 — fixtures and schema-loading patterns)
    tests/schemas/valid/collections.sql (primary single-dim schema)
    tests/schemas/valid/multi_dim_time_series.sql (composite-PK schema for multi-dim test)
    .planning/phases/03-language-bindings-documentation/03-CONTEXT.md (D-07 Julia scenario list)
  </read_first>
  <behavior>
    - A new `@testset "Add Row"` block exists inside the outer `@testset "Time Series"` (around line 88, ordered insert → upsert → multi-dim → missing-dim per D-07 + Claude's Discretion ordering)
    - Test 1 — insert: create one element, call `Quiver.add_time_series_row!(db, "Collection", "data", id; date_time = DateTime(2024, 1, 1), value = 10.0)`, then `read_time_series_group` and assert the row is present
    - Test 2 — upsert: insert the row, call `add_time_series_row!` again with the same `date_time` PK and a different `value` (e.g. 99.0), then assert `read_time_series_group` returns ONE row whose `value` is 99.0
    - Test 3 — multi-dim happy path: use `tests/schemas/valid/multi_dim_time_series.sql`; call `add_time_series_row!` with `date_time` AND `block` dimensions populated and a value column; assert round-trip
    - Test 4 — missing-dim error: call `add_time_series_row!` omitting the required `date_time` kwarg; `@test_throws` an exception whose message contains "Cannot add_time_series_row"
  </behavior>
  <action>
    In `bindings/julia/test/test_database_time_series.jl`, append a new `@testset "Add Row" begin ... end` block inside the outer `@testset "Time Series"` block (insert it immediately after the existing `@testset "Update Clear"` block at line ~140 so it groups with the other write tests). Use the same fixture-setup style as the existing `@testset "Update"` (build a temp DB from `collections.sql`, create the `Configuration` row, create one `Collection` element). For Test 3, load `tests/schemas/valid/multi_dim_time_series.sql` via the same `from_schema` pattern; sample dimension values are in that schema file. For Test 4, use `@test_throws` with an `occursin(...)` check on the message — assert the canonical "Cannot add_time_series_row" prefix is present (D-10 says this string comes from C++ and surfaces unchanged through `check()`).
  </action>
  <verify>
    <automated>findstr /C:"@testset \"Add Row\"" bindings\julia\test\test_database_time_series.jl</automated>
  </verify>
  <done>`bindings/julia/test/test_database_time_series.jl` contains `@testset "Add Row"` with four test cases (insert, upsert, multi-dim, missing-dim error). Existing testsets unmodified.</done>
</task>

<task type="auto">
  <name>Task 4: Run Julia test suite</name>
  <files>bindings/julia/test/test.bat</files>
  <read_first>
    bindings/julia/test/test.bat
    CLAUDE.md (section "Build & Test" — confirms `bindings/julia/test/test.bat` is the Julia test command)
  </read_first>
  <action>
    Run `bindings/julia/test/test.bat` from the repository root. All tests including the new `@testset "Add Row"` must pass. If a stale `Manifest.toml` blocks instantiation, delete `bindings/julia/Manifest.toml` and re-run instantiation per CLAUDE.md "Julia Notes" (one-time recovery only — do not commit Manifest.toml changes unless required by the test).
  </action>
  <verify>
    <automated>bindings\julia\test\test.bat</automated>
  </verify>
  <done>`bindings/julia/test/test.bat` exits with code 0. Test output shows the four new "Add Row" cases as passing alongside the existing time-series tests.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Julia kwargs → C ABI | Caller-supplied scalar values cross the FFI boundary as typed C arrays; the Julia layer is responsible for keepalive (`GC.@preserve`) and correct C-type marshaling. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-03-01 | Tampering | `add_time_series_row!` marshaling | mitigate | Single typed array per column; `GC.@preserve refs` covers the ccall window. Mirrors the locked `update_time_series_group!` pattern. |
| T-03-02 | Information Disclosure | Error messages | accept | Error strings come from C++/C API only (per CLAUDE.md). No PII / no sensitive paths leaked by binding layer. |
| T-03-03 | Denial of Service | Misuse via wrong eltype | mitigate | Existing `throw(ArgumentError("Unsupported column type:"))` fall-through in the sibling wrapper is replicated; bad input fails fast in Julia before crossing FFI. |
</threat_model>

<verification>
- `bindings/julia/src/c_api.jl` contains the new ccall (Task 1 acceptance)
- `bindings/julia/src/database_update.jl` exports `add_time_series_row!` at module scope (Julia module-level functions are reachable via `Quiver.add_time_series_row!` without an explicit `export` — matches sibling convention)
- All Julia tests pass via `bindings/julia/test/test.bat`
- No new "Cannot add_time_series_row:" string introduced inside `bindings/julia/` (grep for that prefix should match zero lines under `bindings/julia/`)
</verification>

<success_criteria>
- All four tasks' acceptance criteria pass
- `Quiver.add_time_series_row!` round-trips through Julia ↔ C ABI ↔ SQLite for insert, upsert, multi-dim, and missing-dim error paths
- JULIA-11 is satisfied: signature matches roadmap success criterion #1 verbatim
</success_criteria>

<output>
Create `.planning/phases/03-language-bindings-documentation/03-01-SUMMARY.md` when done.
</output>
