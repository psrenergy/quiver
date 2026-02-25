# Phase 8: Relations Cleanup - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Remove Python-only relation convenience methods (`read_scalar_relation`, `update_scalar_relation`) and replace them with the integrated FK resolution pattern used by Julia/Dart. Python's `create_element` and `update_element` should resolve FK labels to IDs automatically (matching the other bindings), and tests should cover the full FK resolution suite.

</domain>

<decisions>
## Implementation Decisions

### Test rewrite approach
- FK values set via `Element.set()` at creation time (not separate update calls)
- Relation tests move from `test_database_read_scalar.py` to `test_database_create.py` and `test_database_update.py` to match Julia/Dart organization
- Remove the `TestReadScalarRelation` class and its 3 test methods entirely from `test_database_read_scalar.py`
- Assertions verify FK integer IDs only (no label round-trip queries back to parent)
- Keep all 3 original scenarios (basic FK, empty collection, self-reference) as part of the new create-based tests

### FK label resolution (new capability)
- Python's `create_element` and `update_element` must support FK label resolution: passing a string label (e.g., `"Parent 1"`) for an FK integer column auto-resolves to the target element's ID
- This matches how Julia (`create_element!`, `update_element!`) and Dart (`createElement`, `updateElement`) handle FK columns
- Resolution happens in the Python binding layer (Element.set or pre-create/update hook), not in the C API

### Dead declaration cleanup
- Remove phantom C API declarations from `_declarations.py` (lines 127-138: `quiver_database_update_scalar_relation` and `quiver_database_read_scalar_relation` which don't exist in the C layer)
- Regenerate `_declarations.py` from C headers using `generator.bat` to ensure full sync
- Verify `__init__.py` and other exports have no stale relation references (confirmed clean)

### Test coverage — full Julia/Dart parity
- Port ALL FK resolution test categories from Julia/Dart:
  - **Create group**: scalar FK (label), scalar FK (integer), vector FK labels, set FK labels, time series FK labels, all FK types in one call, non-FK integer passthrough, missing target label error, non-FK string rejection error, resolution failure atomicity (no partial writes)
  - **Update group**: scalar FK label, scalar FK integer, vector FK labels, set FK labels, time series FK labels, all FK types in one call, non-FK passthrough, resolution failure preserves existing
- FK metadata tests: verify `get_scalar_metadata` returns `is_foreign_key` and `references_collection` for FK columns (matching Julia/Dart metadata test pattern)

### Claude's Discretion
- Where FK label resolution logic lives (Element class, Database class helper, or pre-operation hook)
- Internal implementation of label-to-ID resolution (query approach, caching, error message format)
- Exact test method naming conventions (should follow existing Python test patterns)

</decisions>

<specifics>
## Specific Ideas

- "Mimic other layers" — Python test structure and FK behavior should mirror Julia/Dart, not invent its own pattern
- Julia organizes FK tests under `@testset "Relations"` in create and `"Update Element FK Label"` groups in update
- Dart uses `group('FK Resolution - Create')` and `group('FK Resolution - Update')`
- Python should follow equivalent grouping (likely pytest classes: `TestFKResolutionCreate`, `TestFKResolutionUpdate`)

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 08-relations-cleanup*
*Context gathered: 2026-02-24*
