---
id: 03-05-docs-add-time-series-row
phase: 03
plan: 05
type: execute
wave: 1
depends_on: []
files_modified:
  - docs/time_series.md
  - CLAUDE.md
requirements: [DOC-11]
autonomous: true

must_haves:
  truths:
    - "docs/time_series.md 'Inserting data' example uses the shipped Julia signature add_time_series_row!(db, collection, group, id; <dim>=…, <value>=…)"
    - "The old aspirational signature (collection + attribute + label + positional value + date kwarg) is gone from docs/time_series.md"
    - "CLAUDE.md Core API 'Time series' bullet lists add_time_series_row()"
    - "CLAUDE.md Cross-Layer Examples table has a 'Time series add row' row between read and update rows"
  artifacts:
    - path: "docs/time_series.md"
      provides: "Updated 'Inserting data' subsection example matching the shipped signature"
      contains: "add_time_series_row!(db,"
    - path: "CLAUDE.md"
      provides: "Core API + Cross-Layer Examples coverage of add_time_series_row across all layers"
      contains: "Time series add row"
  key_links:
    - from: "docs/time_series.md 'Inserting data' subsection"
      to: "bindings/julia/src/database_update.jl add_time_series_row!"
      via: "The example demonstrates the exact Julia signature shipped by plan 03-01"
      pattern: "add_time_series_row!\\(db, \"Resource\", \"group1\""
    - from: "CLAUDE.md Cross-Layer Examples table 'Time series add row' row"
      to: "Every binding's add_time_series_row wrapper"
      via: "Mechanical naming-transformation table — single source of truth for the name in every layer"
      pattern: "\\| Time series add row \\|"
---

<objective>
Sync the project documentation with the shipped `add_time_series_row` API surface. Make two narrow, surgical edits — one in `docs/time_series.md` (rewrite the "Inserting data" example to use the shipped Julia signature) and two in `CLAUDE.md` (add a bullet to the Core API "Time series" list and insert a row in the Cross-Layer Examples table). Verification is grep-based source assertion only; no tests run.

Purpose: Closes DOC-11. Per D-09a / D-09b in `.planning/phases/03-language-bindings-documentation/03-CONTEXT.md`. Per `<deferred>` in CONTEXT.md, broader `docs/time_series.md` cleanup (stale references to `read_time_series_table`, `update_time_series_row!`, `delete_time_series!`) is explicitly out of scope.
Output: Updated `docs/time_series.md` "Inserting data" subsection + two surgical inserts in `CLAUDE.md`. No other file changes.
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
@docs/time_series.md
@CLAUDE.md

<interfaces>
<!-- Documentation targets per CONTEXT.md D-09a / D-09b -->

Current stale block in `docs/time_series.md` (lines 137-153) — TO REPLACE:
```julia
Quiver.add_time_series_row!(
    db,
    "Resource",
    "some_vector1",
    "Resource 1",
    10.0; # new value
    date_time = DateTime(2000)
)
```
This uses an aspirational signature (collection + attribute + label + positional value + date kwarg) that does NOT match what was shipped.

Shipped Julia signature (from plan 03-01) is:
```
add_time_series_row!(db::Database, collection::String, group::String, id::Int64; kwargs...)
```
Where `kwargs` includes both dimension columns AND value columns.

Replacement block must demonstrate this exact shape — e.g. `Quiver.add_time_series_row!(db, "Resource", "group1", id; date_time = DateTime(2000), some_vector1 = 10.0)` per D-09a verbatim.

Current state of CLAUDE.md Core API "Time series" bullet (line 442):
```
- Time series: `read_time_series_group()`, `update_time_series_group()` — both use multi-column `vector<map<string, Value>>` rows with N typed value columns per time series group
```

Current state of CLAUDE.md Cross-Layer Examples table (lines 569-570):
```
| Time series read | `read_time_series_group()` | `quiver_database_read_time_series_group()` | `read_time_series_group()` | `readTimeSeriesGroup()` | `read_time_series_group()` |
| Time series update | `update_time_series_group()` | `quiver_database_update_time_series_group()` | `update_time_series_group!()` | `updateTimeSeriesGroup()` | `update_time_series_group()` |
```

The new row goes between these two.
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Rewrite docs/time_series.md 'Inserting data' example</name>
  <files>docs/time_series.md</files>
  <read_first>
    docs/time_series.md (lines 119-154 — the "Inserting data" subsection — full current example block)
    .planning/phases/03-language-bindings-documentation/03-CONTEXT.md (D-09a — quotes both the current aspirational form and the replacement form)
    bindings/julia/src/database_update.jl (verify plan 03-01 shipped the signature `add_time_series_row!(db::Database, collection::String, group::String, id::Int64; kwargs...)`)
  </read_first>
  <action>
    In `docs/time_series.md`, rewrite the "Inserting data" subsection example (currently lines ~137-153). Replace BOTH existing `Quiver.add_time_series_row!(...)` blocks (the one at lines 137-144 and the one at lines 146-153) with versions matching the shipped Julia signature.
    
    The new shape per D-09a (collection + group + id + dimension and value kwargs, no positional value, no attribute-string parameter):
    - First call inserts `date_time = DateTime(2000)` paired with the value column (e.g. `some_vector1 = 10.0`)
    - Second call inserts `date_time = DateTime(2001)` with the next value (e.g. `some_vector1 = 11.0`)
    
    Use the literal group name `"group1"` and the literal `id` variable (introduced earlier in the example block — verify how the existing example introduces it; the example creates `"Resource 1"` via `create_element!` so the returned id should be captured into a variable). If the existing example does not capture the id, also adjust the `create_element!` line to assign its return value (`id = Quiver.create_element!(db, "Resource"; label = "Resource 1")`) — minimally invasive change to make the new signature compilable.
    
    Do NOT touch surrounding prose (the paragraph at line 119 explaining single-row insertion is fine as-is). Do NOT touch other sections (Reading data, etc.) — broader docs cleanup is deferred per `<deferred>` in CONTEXT.md.
  </action>
  <verify>
    <automated>findstr /C:"add_time_series_row!(db," docs\time_series.md &amp;&amp; findstr /C:"\"group1\"" docs\time_series.md &amp;&amp; (findstr /C:"\"some_vector1\"," docs\time_series.md ; if errorlevel 1 (echo old positional form is gone) else (echo FAIL: old positional form still present &amp; exit /b 1))</automated>
  </verify>
  <done>`docs/time_series.md` "Inserting data" subsection (lines ~119-154) shows two `Quiver.add_time_series_row!(...)` calls using the shipped signature shape (`db, collection, group, id; <dim>=…, <value>=…`). The old aspirational form (collection + `"some_vector1"` attribute string + `"Resource 1"` label + positional value before `;`) is gone. Surrounding prose and other sections are unchanged.</done>
</task>

<task type="auto">
  <name>Task 2: Add CLAUDE.md Core API bullet + Cross-Layer Examples table row</name>
  <files>CLAUDE.md</files>
  <read_first>
    CLAUDE.md (line 442 — the "Time series" bullet in the Core API → Database Class subsection)
    CLAUDE.md (lines 551-570 — the "Representative Cross-Layer Examples" table header through the Time series rows)
    .planning/phases/03-language-bindings-documentation/03-CONTEXT.md (D-09b — quotes both edits verbatim)
  </read_first>
  <action>
    Make two surgical edits to `CLAUDE.md`:
    
    1. **Core API "Time series" bullet (line 442)** — Modify the existing line to include `add_time_series_row()` alongside the existing methods. Replace:
       ```
       - Time series: `read_time_series_group()`, `update_time_series_group()` — both use multi-column `vector<map<string, Value>>` rows with N typed value columns per time series group
       ```
       with:
       ```
       - Time series: `read_time_series_group()`, `update_time_series_group()`, `add_time_series_row()` — both `read_time_series_group` and `update_time_series_group` use multi-column `vector<map<string, Value>>` rows with N typed value columns per time series group; `add_time_series_row` appends/upserts a single row using a `map<string, Value>` (one value per column)
       ```
       Wording: keep technically accurate — the new method uses a single-row map shape, not the vector-of-maps shape; reflect that distinction.
    
    2. **Cross-Layer Examples table row** — Insert a new table row between the existing "Time series read" row (line 569) and "Time series update" row (line 570). The new row, verbatim per D-09b:
       ```
       | Time series add row | `add_time_series_row()` | `quiver_database_add_time_series_row()` | `add_time_series_row!()` | `addTimeSeriesRow()` | `add_time_series_row()` |
       ```
       Preserve column alignment with the surrounding table (the table header is `| Category | C++ | C API | Julia | Dart | Lua |`).
    
    No other edits to CLAUDE.md.
  </action>
  <verify>
    <automated>findstr /C:"add_time_series_row()" CLAUDE.md &amp;&amp; findstr /C:"| Time series add row |" CLAUDE.md &amp;&amp; findstr /C:"`addTimeSeriesRow()`" CLAUDE.md &amp;&amp; findstr /C:"`add_time_series_row!()`" CLAUDE.md</automated>
  </verify>
  <done>`CLAUDE.md` Core API "Time series" bullet (line ~442) now lists `add_time_series_row()`. The Cross-Layer Examples table contains a new "Time series add row" row between the existing read and update rows, with `add_time_series_row()` / `quiver_database_add_time_series_row()` / `add_time_series_row!()` / `addTimeSeriesRow()` / `add_time_series_row()` across the five columns. No other CLAUDE.md edits.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Documentation → developer expectations | The docs are the contract users read first; a stale signature in `docs/time_series.md` would mislead users into writing non-compiling code. No runtime trust boundary. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-03-13 | Repudiation | Stale doc example | mitigate | The doc edit replaces the aspirational signature with the shipped signature verbatim; users who copy-paste will compile. |
| T-03-14 | Tampering | Broader doc cleanup creep | mitigate | Scope is locked to D-09a and D-09b; other stale references (`read_time_series_table`, `update_time_series_row!`, `delete_time_series!`) remain explicitly deferred per CONTEXT.md `<deferred>`. |
</threat_model>

<verification>
- `docs/time_series.md` "Inserting data" example uses the shipped Julia signature (Task 1 acceptance)
- `CLAUDE.md` Core API bullet + Cross-Layer Examples table both reference `add_time_series_row` (Task 2 acceptance)
- No edits to other doc sections (e.g. "Reading data", "Reading as a table") — `git diff docs/time_series.md` should be confined to the "Inserting data" subsection
- No edits to other CLAUDE.md sections beyond line 442 and the Cross-Layer Examples table insert — `git diff CLAUDE.md` should show only those two regions
</verification>

<success_criteria>
- Both tasks' acceptance criteria pass
- DOC-11 is satisfied: `docs/time_series.md` Julia example matches the shipped signature, and `CLAUDE.md` Core API + cross-layer naming table both include the new method
- The documentation is internally consistent: the example in `docs/time_series.md` uses the same signature shape as `bindings/julia/src/database_update.jl` (shipped by plan 03-01)
</success_criteria>

<output>
Create `.planning/phases/03-language-bindings-documentation/03-05-SUMMARY.md` when done.
</output>
