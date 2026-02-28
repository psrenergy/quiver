# Phase 3: Python DataType Constants - Context

**Gathered:** 2026-02-28
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace all magic integer literals (0, 1, 2, 3, 4) used for data type dispatch in the Python binding with a `DataType` IntEnum. This is a code quality improvement — no new functionality, no API changes beyond making `ScalarMetadata.data_type` return an enum instead of a plain int.

</domain>

<decisions>
## Implementation Decisions

### Module placement
- DataType IntEnum defined in `metadata.py` alongside ScalarMetadata (which uses it as its `data_type` field)
- `database.py` imports DataType directly: `from quiverdb.metadata import DataType`
- No new files — follows existing module structure

### Public API surface
- DataType exported in `__init__.py` so users can write `from quiverdb import DataType`
- Added to `__all__` list
- Query parameter building stays as-is (auto-detects type from Python values, no DataType hints)

### Scope of replacement
- Replace magic integers in ALL three contexts in `database.py`:
  1. ScalarMetadata.data_type field type (int -> DataType)
  2. Type dispatch in composite reads (read_scalars_by_id, read_vectors_by_id, read_sets_by_id, read_vector_group_by_id, read_set_group_by_id)
  3. Time series columnar reads and updates (ctype comparisons)
- Also replace magic integers in query param building (c_types[i] = DataType.NULL instead of 4, etc.)
- IntEnum is-a int, so all C interop works unchanged — values pass directly to C arrays

### Claude's Discretion
- Exact IntEnum class definition style (class body formatting)
- Whether to add a `__str__` override or rely on default IntEnum behavior
- Test structure for validating DataType values match C API constants

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-python-datatype-constants*
*Context gathered: 2026-02-28*
