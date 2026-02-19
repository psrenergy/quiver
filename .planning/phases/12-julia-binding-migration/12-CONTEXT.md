# Phase 12: Julia Binding Migration - Context

**Gathered:** 2026-02-19
**Status:** Ready for planning

<domain>
## Phase Boundary

Migrate Julia's `update_time_series_group!` and `read_time_series_group` to use the new multi-column C API from Phase 11. Deliver idiomatic kwargs syntax for update and typed columnar results for read. Does not add new operations or change non-time-series bindings.

</domain>

<decisions>
## Implementation Decisions

### Update kwargs interface
- Pure kwargs syntax: `update_time_series_group!(db, col, grp, id; date_time=[...], temperature=[...], humidity=[...])`
- Each kwarg is a column name matching the schema, value is a typed vector
- Auto-coerce types: Int passed for REAL column converts to Float64, etc. Error on truly incompatible types (String for REAL)
- No Julia-side validation of column names — let C API validate and surface its error messages (CAPI-05)
- No kwargs = clear all rows: `update_time_series_group!(db, col, grp, id)` clears rows for that element

### Read return format
- Return `Dict{String, Vector}` — columnar format with column names as keys, typed vectors as values
- Runtime concrete types: `Vector{Int64}` for INTEGER, `Vector{Float64}` for REAL, `Vector{String}` for TEXT
- Empty results return empty `Dict{String, Vector}()` (no keys, no data)
- Regular Dict, no order guarantee on iteration (access by column name)

### Old API transition
- Replace in-place — no deprecation wrappers, no backwards compatibility
- Old row-dict interface (`Vector{Dict{String, Any}}` parameter) removed entirely
- Regenerate c_api.jl from current C headers using `bindings/julia/generator/generator.bat`
- Rewrite existing time series tests to use new kwargs/columnar interface
- Add multi-column mixed-type tests using Phase 11 schema (INTEGER + REAL + TEXT value columns)

### DateTime handling
- Full DateTime support on dimension column: accept Julia DateTime on update (convert to string), parse to DateTime on read
- Error on parse failure — if stored string can't parse to DateTime, throw error (forces data consistency)
- Reuse existing `date_time.jl` parsing logic (parse_date_time/format_date_time)
- Only dimension column (identified via metadata) gets DateTime treatment — other TEXT columns remain String
- Update accepts both String and DateTime for the dimension column

### Claude's Discretion
- Dict value type precision (abstract `Vector` vs `Union{Vector{Int64}, Vector{Float64}, Vector{String}}`)
- Internal marshaling strategy for kwargs -> C API columnar arrays
- GC.@preserve patterns for safe FFI calls
- Test file organization (new file vs extend existing)

</decisions>

<specifics>
## Specific Ideas

- Kwargs should match schema column names exactly — the call should read like the schema definition
- Type coercion should feel natural to Julia users (passing `[20]` for a REAL column just works)
- The C API already validates column names/types — binding stays thin per project philosophy

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 12-julia-binding-migration*
*Context gathered: 2026-02-19*
