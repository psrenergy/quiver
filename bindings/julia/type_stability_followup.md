# Julia type-stability / nullability follow-up

The bulk scalar readers (`read_scalar_integers` / `read_scalar_floats` / `read_scalar_strings`)
are nullability-aware: a `NOT NULL` column returns a concrete `Vector{T}`, a nullable column
returns `Vector{Optional{T}}` (`Optional{T} = Union{Nothing, T}`), decided from
`get_scalar_metadata(...).not_null`. This document tracks the readers that were deliberately left
out of that change and what (if anything) should happen to them.

## Guiding principle

Apply the concrete-vs-optional rule **only where the `Optional` would come solely from a NULL
cell** (i.e. nullability is the *only* reason a `nothing` can appear). Where a `nothing` can also
mean "no such row / no data / unknown", the optional is inherent and removing it would change the
method's contract — so those readers stay optional.

## Candidates

### `read_time_series_group` — convert (clean fit)

`bindings/julia/src/database_read.jl` (`read_time_series_group`, ~line 531). Value columns are
currently `Vector{Optional{T}}` **always**. Their `Optional` is NULL-cell-only, and
`get_time_series_metadata(...).value_columns[i].not_null` is already available per column. Make
each value column concrete `Vector{T}` when its `not_null` is true, `Vector{Optional{T}}`
otherwise. The dimension column stays a dense `Vector{DateTime}` (already concrete). Mirror the
mask-skipping pattern used in the scalar readers.

Note the cross-binding decision (root `CLAUDE.md`): time-series group data is column-oriented and
group reads currently return `Vector{Union{T,Nothing}}` always — update that design note if this
lands, and keep Python/Dart/JS on their static nullable surface.

### `read_time_series_row` — fix the real instability (different bug)

`read_time_series_row` (~line 601) is the genuinely unstable reader: it returns `Vector{Int64}`,
`Vector{Float64}`, `Vector{Optional{String}}`, or even `Vector{Any}` depending on the runtime
`data_type` and whether data is present. Its optional is **inherent** — semantics are "last
non-null value at or before `date_time`; `nothing` for elements with no matching data" — so it
can't become a concrete `Vector{T}`. The fix is *consistency*: always return a stable
`Vector{Optional{T}}` whose `T` is keyed on the group column's data type from metadata (never
`Vector{Any}`, never a bare concrete vector). This is about removing the eltype-by-data branch,
not about nullability.

### `read_scalar_*_by_id` — leave optional (contract)

`read_scalar_integer_by_id` / `_float_by_id` / `_string_by_id` / `_date_time_by_id` return
`nothing` for a **missing id** as well as for a NULL value. So even a `NOT NULL` column must stay
`Optional{T}`. Making them concrete would require splitting "id not found" (raise) from "value is
NULL" (`nothing`) — a contract change, explicitly out of scope. Leave as-is.

### `query_*` — leave optional (unknown nullability)

`query_string` / `query_integer` / `query_float` / `query_date_time` run arbitrary SQL. The result
column's nullability is generally not recoverable from schema metadata (expressions, aggregates,
joins), and a query with no rows legitimately yields `nothing`. Keep returning `Optional{T}`.

## When implementing any of the above

- Reuse `get_*_metadata(...).not_null` — do not re-query `PRAGMA table_info` by hand; the field is
  already plumbed C++ → C API → Julia.
- Add `isa` assertions in the matching `test/test_*.jl` keyed to each column's real nullability;
  do not use `@inferred` on the public reader (its inferred type is intentionally a 2-way union).
- Update the nearest `CLAUDE.md` (root design decision + `bindings/julia/CLAUDE.md`) per the
  self-updating rule.
