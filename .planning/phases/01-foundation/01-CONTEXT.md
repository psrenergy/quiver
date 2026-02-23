# Phase 1: Foundation - Context

**Gathered:** 2026-02-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Extract a shared `resolve_fk_label` helper method on `Database::Impl` that resolves a string label to an integer ID for any FK column. Refactor the existing set FK path in `create_element` to use this helper with zero behavioral change. Detect and reject strings passed to non-FK INTEGER columns with a clear Quiver error.

</domain>

<decisions>
## Implementation Decisions

### Non-FK error behavior
- Error fires during a **pre-resolve pass**, before any SQL writes -- consistent with the pre-resolve approach that Phase 2 will use
- Only catch strings in **non-FK INTEGER columns** -- REAL columns are out of scope
- Error message: **just the error**, no hints about what to do instead -- matches existing terse error style

### Error message consistency
- ERR-01 format (`"Failed to resolve label 'X' to ID in table 'Y'"`) is locked from requirements
- Non-FK INTEGER error message format: **Claude's discretion** to pick the best fit among the 3 established patterns (Cannot/Not found/Failed to)
- Whether to include source collection context or FK column path in errors: **Claude's discretion**
- **No defensive validation** of the target table's label column -- assume schema convention (TEXT UNIQUE label) holds
- Labels are UNIQUE by schema construction -- the helper does not need to handle ambiguity

### Refactor boundary
- Helper should be **general from the start** -- designed to work for any FK column type (scalar, vector, set, time series), not just what sets need
- Phase 2 and 3 should be able to just call the existing helper without extending its interface
- FK detection (determining which columns are foreign keys) vs resolution (looking up the ID): **Claude's discretion** on whether these are separate methods or combined

### Testing
- **New dedicated tests** for the resolve_fk_label helper in addition to verifying existing tests pass unchanged
- Tests go in **existing `test_database_relations.cpp`** -- FK resolution is conceptually part of relations
- Test cases should cover: valid label resolution, missing label error, non-FK INTEGER column error

### Claude's Discretion
- Exact error message wording for non-FK INTEGER columns (within the 3 established patterns)
- Whether to include collection context or FK column path in error messages
- FK detection as a separate method vs built into the resolve helper
- Internal implementation details of schema introspection for FK detection

</decisions>

<specifics>
## Specific Ideas

- Labels are UNIQUE by schema construction -- if that invariant somehow breaks, validation should catch it (not the resolve helper)
- Helper should be reusable by Phase 2 (create) and Phase 3 (update) without modification

</specifics>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 01-foundation*
*Context gathered: 2026-02-23*
