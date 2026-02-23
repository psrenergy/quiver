# Phase 2: Reads and Metadata - Context

**Gathered:** 2026-02-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Python bindings for all scalar/vector/set read operations (bulk and by-id), element ID listing, relation reads, and metadata queries returning typed dataclasses. All operations wrap existing C API functions with correct memory management. No write operations, no transactions, no queries.

</domain>

<decisions>
## Implementation Decisions

### Return type choices
- Set reads (`read_set_*`) return `list`, not Python `set` — consistent with vector reads and other bindings
- Bulk scalar reads return flat `list[int]`, `list[float]`, `list[str | None]` — no ID keying
- Bulk vector/set reads return `list[list[int]]` etc. — list of lists, matching Julia/Dart pattern
- `read_element_ids()` returns `list[int]`
- `read_scalar_relation()` returns `int | None` — nullable foreign keys handled with None
- Bulk string reads with NULLs: `None` in list preserving positional alignment (`list[str | None]`)

### Metadata dataclass design
- Field names match C struct exactly: `attribute_name`, `data_type`, `is_nullable`, `is_foreign_key`, `references_collection`, `references_column`
- GroupMetadata.value_columns is `list[ScalarMetadata]` — nested composition, not flat dicts
- Frozen/mutable and package-level export decisions: Claude's discretion

### Null/optional semantics
- `_by_id` reads: return `None` for NULL attribute values (not raise)
- Missing element IDs: pass through C API behavior (don't add Python-side logic)
- Empty vector/set for existing element: return empty `list[]` (not None)
- Empty string `""` is distinct from NULL — `""` for empty, `None` for NULL

### Claude's Discretion
- `list_scalar_attributes()` / `list_vector_groups()` return type (names-only `list[str]` or metadata objects)
- ScalarMetadata/GroupMetadata frozen vs mutable
- Whether ScalarMetadata/GroupMetadata are exported from `quiver.__init__`

</decisions>

<specifics>
## Specific Ideas

- Consistency with Julia/Dart bindings is the guiding principle — when in doubt, match what other bindings do
- Phase 1 patterns (database.py, _helpers.py, check() function) serve as the foundation for all read methods

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

### Test Organization
- Read tests split by data type: `test_database_read_scalar.py`, `test_database_read_vector.py`, `test_database_read_set.py`
- Metadata tests in one combined file: `test_database_metadata.py`
- Element ID reads and relation reads co-located in the relevant read test files (not separate files)
- Test schemas reused from `tests/schemas/valid/` (shared with C++/Julia/Dart, established in Phase 1)

---

*Phase: 02-reads-and-metadata*
*Context gathered: 2026-02-23*
