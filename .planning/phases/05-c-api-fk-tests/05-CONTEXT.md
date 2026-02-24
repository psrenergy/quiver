# Phase 5: C API FK Tests - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Mirror all 16 C++ FK resolution tests through the C API layer. Covers create and update paths for scalar, vector, set, and time series FK columns. Verifies FK label resolution, error handling, transactional safety, and non-FK regression through the C API.

</domain>

<decisions>
## Implementation Decisions

### Test file organization
- Split FK tests by operation: create FK tests appended to `test_c_api_database_create.cpp`, update FK tests appended to `test_c_api_database_update.cpp`
- Append at the end of each existing file
- No inline comments on time series columnar marshaling — keep tests clean like the rest

### Test structure & setup
- Each test creates its own db + parents inline — self-contained, no shared fixtures or helpers
- Reuse the existing `relations.sql` schema from `tests/schemas/valid/`
- Verification via same read-back pattern as C++ tests: create/update with FK labels, then read back via C API read functions and verify resolved integer IDs
- Error case tests: verify `QUIVER_ERROR` return code only, no error message substring checks

### Claude's Discretion
- Cleanup strategy in error-path tests (whether to free resources that were never allocated)

### Test mapping strategy
- Mirror C++ test names exactly (e.g., `CreateElementScalarFkLabel`, `UpdateElementVectorFkLabels`)
- Include both "all FK types in one call" combo tests for create and update
- Include `RejectStringForNonFkIntegerColumn` test — proves error propagates through C API layer
- Replicate transactional safety tests exactly: "no partial writes" on create failure, "failure preserves existing" on update failure

</decisions>

<specifics>
## Specific Ideas

- Test names should be 1:1 with C++ counterparts for easy cross-referencing between layers
- All 16 C++ FK tests should have a C API equivalent — no consolidation or omission

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 05-c-api-fk-tests*
*Context gathered: 2026-02-24*
