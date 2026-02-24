# Phase 5: Time Series - Context

**Gathered:** 2026-02-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Bind Python to the existing C API for multi-column time series read/write operations and time series file reference operations. The C API signatures are fixed — this phase wraps them into Pythonic interfaces. No new C++ or C API work.

</domain>

<decisions>
## Implementation Decisions

### Row data format
- List of dicts for both read return and update input: `[{'date_time': '2024-01-01', 'value': 3.14}, ...]`
- Dict keys match exact SQL column names from the schema (no translation)
- Key insertion order preserved as returned by C API (dimension column first, then value columns in schema order)
- For update, all columns present in the schema are required in each row dict — error if a key is missing

### Type mapping
- INTEGER columns map to Python `int`
- FLOAT columns map to Python `float`
- STRING and DATE_TIME columns map to Python `str`
- Consistent with Phase 4 query return type conventions
- For update input, strict types only: `int` for INTEGER, `float` for FLOAT, `str` for STRING/DATE_TIME — error on wrong type, no coercion

### Clear/empty semantics
- `update_time_series_group(collection, id, group, [])` clears all rows — no separate clear method
- `read_time_series_group` returns `[]` for an element with no rows
- Both read and update raise an error if the element ID doesn't exist (distinguishes "no rows" from "no element")

### Files dict shape
- `has_time_series_files(collection)` returns plain `bool`
- `list_time_series_files_columns(collection)` returns `list[str]`
- `read_time_series_files(collection)` returns `dict[str, str|None]` (column name to path, None if unset)
- `update_time_series_files(collection, data)` accepts same dict shape as read returns — symmetric API

### Claude's Discretion
- DATE_TIME column handling: whether to return as plain `str` or auto-parse to `datetime.datetime` (decide based on Phase 4 patterns and other binding behavior)
- Internal CFFI marshaling approach for void** column dispatch
- Error message formatting for validation failures (type mismatch, missing keys)

</decisions>

<specifics>
## Specific Ideas

- Read/update API should feel symmetric: same dict shape goes in and comes out
- Empty list `[]` is the canonical way to clear rows, matching C API convention (column_count=0, row_count=0)
- Files API is also symmetric: dict in, dict out

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 05-time-series*
*Context gathered: 2026-02-23*
