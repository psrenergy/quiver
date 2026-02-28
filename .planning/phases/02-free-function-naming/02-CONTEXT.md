# Phase 2: Free Function Naming - Context

**Gathered:** 2026-02-27
**Status:** Ready for planning

<domain>
## Phase Boundary

Rename `quiver_element_free_string` to `quiver_database_free_string` across all layers. Query/read string results must be freed through the correct entity-scoped function. This is a breaking change — no backwards compatibility shim.

</domain>

<decisions>
## Implementation Decisions

### Rename scope
- Add `quiver_database_free_string()` in `include/quiver/c/database.h`
- Remove `quiver_element_free_string()` from header, implementation, bindings, and tests — zero traces remain
- Breaking change is intentional (CLAUDE.md: "no backwards compatibility required")

### Binding updates
- Julia and Dart: re-run generators (`generator.bat`) to pick up the renamed function
- Python: manually update hand-written CFFI cdef in `_c_api.py` (generator output is reference-only)
- All bindings must call `quiver_database_free_string` for database-returned strings

### Test updates
- Update all test suites (C++, C API, Julia, Dart, Python) that reference the old function name
- No new test files needed — existing coverage validates the rename

### Claude's Discretion
- Implementation file placement (co-location with read allocations)
- Generator re-run sequencing
- Any intermediate refactoring needed for clean rename

</decisions>

<specifics>
## Specific Ideas

No specific requirements — the requirements (NAME-01, NAME-02) fully define the rename. Follow existing alloc/free co-location patterns from CLAUDE.md.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 02-free-function-naming*
*Context gathered: 2026-02-27*
