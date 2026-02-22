# Phase 7: Bindings - Context

**Gathered:** 2026-02-22
**Status:** Ready for planning

<domain>
## Phase Boundary

Julia, Dart, and Lua users can export CSV using idiomatic APIs with native map types for options. Each binding wraps the C API `quiver_database_export_csv` with language-native parameter styles. No new C++ or C API work — purely binding-layer implementation.

</domain>

<decisions>
## Implementation Decisions

### Options parameter style
- Julia: keyword arguments — `export_csv(db, collection, group, path; enum_labels=Dict(...), date_time_format="...")`
- Dart: named parameters — `exportCSV(collection, group, path, enumLabels: {...}, dateTimeFormat: '...')`
- Lua: optional table argument — `db:export_csv(collection, group, path, {enum_labels = {...}, date_time_format = '...'})`
- All options fully implicit when using defaults — `export_csv(db, coll, "", path)` works with no options argument

### Group parameter handling
- Empty string `""` for scalar export — matches C/C++ API exactly, group is always required
- No null/nothing/nil alternatives — consistent with upstream API
- No binding-layer validation — pass through to C API, let C++ surface errors

### Enum labels syntax
- Native nested map literals in each language:
  - Julia: `Dict("attr" => Dict(1 => "Low", 2 => "High"))`
  - Dart: `{"attr": {1: "Low", 2: "High"}}`
  - Lua: `{attr = {[1] = "Low", [2] = "High"}}`
- Integer keys only for enum values — strict, matches actual data type
- Unknown attribute names in enum_labels silently ignored — pass through to C++, unused keys are harmless

### Convenience overloads
- No shorthand for scalar export — always require all 4 positional args (collection, group, path + options)
- One function per binding: `export_csv` (Julia/Lua), `exportCSV` (Dart)
- Julia uses `build_csv_options(kwargs...)` pattern to construct C struct from keyword arguments (mirrors existing `build_options` pattern in `bindings/julia/src/database.jl`)

### Claude's Discretion
- Dart and Lua internal marshaling approach — whether to use a separate builder helper or marshal directly inside export function
- Internal struct conversion implementation details
- Test file organization within each binding's test suite

</decisions>

<specifics>
## Specific Ideas

- Julia already has a `build_options(kwargs...)` pattern for `DatabaseOptions` — apply the same pattern for CSV export options (`build_csv_options`)
- Bindings should marshal native nested maps to the C API's flat parallel-array `quiver_csv_export_options_t` struct internally

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 07-bindings*
*Context gathered: 2026-02-22*
