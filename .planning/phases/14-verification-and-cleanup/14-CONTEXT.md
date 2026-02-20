# Phase 14: Verification and Cleanup - Context

**Gathered:** 2026-02-19
**Status:** Ready for planning

<domain>
## Phase Boundary

Remove deprecated single-column C API time series functions, add Lua multi-column time series test coverage matching Julia/Dart depth, perform a dead code clean sweep, update CLAUDE.md to reflect the new API, and gate on full test suite green via `scripts/build-all.bat`.

</domain>

<decisions>
## Implementation Decisions

### Lua test design
- Use the same multi-column mixed-type schema (INTEGER + REAL + TEXT value columns) used by Julia and Dart tests
- Match Julia/Dart coverage depth: update, read back with type checking, multi-row, error cases — full parity
- Add tests to the existing Lua test file, not a new dedicated file
- Also verify composite read helpers (`read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id`) alongside time series

### Old API removal boundary
- Remove old public C API functions AND all internal helpers, types, and converters that become unused
- Delete old single-column C API tests — Phase 11 already added multi-column coverage
- Remove old test schemas from `tests/schemas/valid/` that only served the single-column interface
- Regenerate both Julia and Dart FFI bindings after removing old C API functions to drop stale signatures

### Cleanup extras
- Perform a full dead code clean sweep beyond just time series — remove any confirmed unused functions, stale includes, etc.
- Leave untracked debug files (debug1.jl, debug1.sql, debug2.jl) as-is
- Update CMakeLists.txt, test configs, and any other build files to remove references to deleted files
- Final test gate is a single `scripts/build-all.bat` run — if it passes, we're green

### CLAUDE.md updates
- Remove all references to old single-column time series API patterns that no longer exist
- Add new documentation for the multi-column time series API patterns (columnar interface, C API signatures, binding usage)
- Add multi-column time series examples to the cross-layer naming table
- Remove specific test counts from documentation — they become stale quickly

### Claude's Discretion
- Order of operations (which cleanup step happens first)
- Exact dead code detection strategy
- How to structure new CLAUDE.md time series documentation sections

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

*Phase: 14-verification-and-cleanup*
*Context gathered: 2026-02-19*
