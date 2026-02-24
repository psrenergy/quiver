# Phase 3: Update Element FK Resolution - Context

**Gathered:** 2026-02-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Extend `update_element` so users can pass target labels (strings) for scalar, vector, set, and time series FK columns and they resolve to target IDs automatically. The pre-resolve infrastructure from Phase 2 is the building block — this phase wires it into the update path.

</domain>

<decisions>
## Implementation Decisions

### Error behavior parity
- **Same fail-fast** — stop at the first unresolvable FK label and throw immediately, identical to create_element
- **Same bare error format** — `"Failed to resolve label 'X' to ID in table 'Y'"` with no method-specific context (matches ERR-01, same as create)
- **Same non-FK INTEGER guard** — string passed to a non-FK INTEGER column in update_element gets the same clear Quiver error, not raw SQLite STRICT mode error
- **Partial-update scope** — only resolve FK columns present in the Element being passed; columns not in the update are untouched

### Pre-resolve reuse
- **Same function, both paths** — update_element calls the exact same pre-resolve function that create_element uses. One function to maintain, guaranteed consistency
- **Same ResolvedElement type** — pre-resolve output is a ResolvedElement, same as create
- **Full schema scan** — pre-resolve scans all collection tables (same as create), even though update is partial. Cost is negligible and keeps the code simple
- **Minimal wiring** — implementation is: (1) add one call to pre_resolve at the top of update_element, (2) use resolved values downstream. Keep changes small

### Test model
- **Same relations.sql schema** — reuse the extended schema from Phase 2, test FK columns via update_element
- **Mirror Phase 2's test structure exactly** — per-type FK tests (scalar, vector, set, time series), combined all-types test, no-FK regression test, and error tests
- **Tests in existing file** — add to test_database_update.cpp, not a separate file

### Claude's Discretion
- Verification depth in tests (read-back resolved IDs vs no-throw sufficient)
- Any minor refactoring needed to consume ResolvedElement in update_element's downstream code

</decisions>

<specifics>
## Specific Ideas

- The implementation should be genuinely minimal — the pre-resolve infrastructure exists, update_element just needs to call it
- Phase 2's set FK resolution was unified into the pre-resolve pass, so update_element's set path gets FK resolution for free via the same function

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-update-element-fk-resolution*
*Context gathered: 2026-02-23*
