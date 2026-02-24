# Phase 6: CSV and Convenience Helpers - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

CSV export/import with struct marshaling to Python, plus pure-Python composite read helpers and datetime convenience methods. All intelligence is in the C++ layer or follows established Julia/Dart patterns — this phase wraps existing C API functions and composes existing Python binding methods.

</domain>

<decisions>
## Implementation Decisions

### CSV export API surface
- CSVExportOptions exposed as a Python dataclass (mirrors ScalarMetadata/GroupMetadata pattern)
- All fields optional with sensible defaults: `date_time_format=""`, `enum_labels={}`
- `enum_labels` type: `dict[str, dict[int, str]]` — nested dicts matching C++ `unordered_map` exactly
- `export_csv(collection, group, path, options)` signature matching C API (collection, group, path, options)
- `import_csv(table, path)` also bound in this phase — simple signature, no options struct

### Convenience helper return types
- `read_all_scalars_by_id(collection, id)` returns `dict[str, Any]` — attribute name to typed value
- `read_all_vectors_by_id(collection, id)` returns `dict[str, list]` — group name to typed list
- `read_all_sets_by_id(collection, id)` returns `dict[str, list]` — group name to typed list
- Multi-column groups: each column is a separate entry in the dict (not nested)
- DATE_TIME columns in read_all_* helpers are parsed to `datetime` objects (consistent with datetime helpers)

### DateTime helpers design
- Return Python `datetime.datetime` objects, not strings
- No timezone info in stored string → assume UTC (`tzinfo=datetime.timezone.utc`) — always timezone-aware
- Malformed datetime strings raise an exception (strict, not fallback to raw string)
- Helpers: `read_scalar_date_time_by_id`, `read_vector_date_time_by_id`, `read_set_date_time_by_id`, `query_date_time`

### Multi-column group readers
- `read_vector_group_by_id` and `read_set_group_by_id` return `list[dict[str, Any]]` — list of row dicts
- Matches `read_time_series_group` return format for consistency
- `vector_index` included in row dicts for vectors (preserves ordering info)
- DATE_TIME columns parsed to `datetime` objects (consistent with all other helpers)
- Single-column groups still return row dicts (consistent return type regardless of column count)

### Claude's Discretion
- id/label inclusion in `read_all_scalars_by_id` — Claude decides based on Julia/Dart precedent
- Internal marshaling approach for CSVExportOptions parallel arrays
- Error message text for malformed datetime strings

</decisions>

<specifics>
## Specific Ideas

- CSVExportOptions dataclass should follow the same pattern as ScalarMetadata/GroupMetadata already in the binding
- Julia pattern for convenience helpers: `list_scalar_attributes` → type dispatch → typed read per attribute
- All datetime parsing should handle both `YYYY-MM-DDTHH:MM:SS` and `YYYY-MM-DD HH:MM:SS` formats (matching C++ parser)

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 06-csv-and-convenience-helpers*
*Context gathered: 2026-02-24*
