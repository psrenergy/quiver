# Phase 1: Core Implementation - Context

**Gathered:** 2026-02-26
**Status:** Ready for planning

<domain>
## Phase Boundary

Implement `read_element_by_id(collection, id)` as a binding-level composition function in all supported languages (Lua, Julia, Dart, Python). No new C++ or C API types are needed — each binding composes existing C API calls to build a flat dict/map/table of all scalars, vectors, and sets for a given element.

**Structural impact:** This decision eliminates the need for a separate C++/C API phase. Phase 1 merges into Phase 2 — the binding wrappers phase becomes the first phase of work. The roadmap should be restructured accordingly.

</domain>

<decisions>
## Implementation Decisions

### Architecture
- Binding-level composition, not C++/C API function
- Each binding implements `read_element_by_id` independently by composing existing C API calls
- Same pattern as existing composite helpers (`read_all_scalars_by_id`, etc.)
- No new `ElementData` struct in C++, no new transparent C structs
- Fresh metadata query each call (no caching)
- Always read all categories (no optional filtering parameter)

### Return structure
- Flat dict/map/table with attribute names as keys, values as values
- Every column is its own top-level key — group names do not appear
- Multi-column vectors: each column is a separate key (e.g., `{"value1": [1,2,3], "value2": [4,5,6]}`)
- Include all schema groups even if empty for this element (empty arrays for groups with no data)
- `label` is included as a regular scalar attribute
- `id` is excluded (caller already knows it)
- Time series data is excluded (handled separately by `read_time_series_group`)

### Type handling
- Enum FK columns (INTEGER referencing another table): return raw integer FK, not resolved label
- All FK columns (including inter-collection references): return raw integers
- DATE_TIME columns: use existing `read_scalar_date_time_by_id` to return native date types
- NULL scalar values: include key with null/nil/None value (all schema attributes always present)

### Edge cases
- Nonexistent element ID: return empty dict (no validation query, no error)
- Vectors: sorted by `vector_index` (existing behavior preserved)
- Sets: returned in dict, order does not matter

### Claude's Discretion
- Internal implementation details within each binding
- How to detect DATE_TIME columns from metadata
- Error handling for unexpected metadata states

</decisions>

<specifics>
## Specific Ideas

- User explicitly wants every column as its own top-level key — no nesting by group name
- Follow the same implementation pattern as existing `read_all_scalars_by_id` / `read_all_vectors_by_id` / `read_all_sets_by_id` helpers
- The function supersedes those three composite helpers (Phase 3 cleanup removes them)

</specifics>

<deferred>
## Deferred Ideas

- Phase 1 and Phase 2 should be merged in the roadmap (Phase 1 has no C++/C work)
- REQUIREMENTS.md needs updating: READ-01 and READ-02 are obsolete as written

</deferred>

---

*Phase: 01-core-implementation*
*Context gathered: 2026-02-26*
