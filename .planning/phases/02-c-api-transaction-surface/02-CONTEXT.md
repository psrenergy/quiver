# Phase 2: C API Transaction Surface - Context

**Gathered:** 2026-02-21
**Status:** Ready for planning

<domain>
## Phase Boundary

Expose C++ transaction control (begin_transaction, commit, rollback, in_transaction) as flat C API functions returning `quiver_error_t`. Follows established C API patterns (try-catch, QUIVER_REQUIRE, binary return codes). This is the FFI surface that language bindings (Phase 3) will call.

</domain>

<decisions>
## Implementation Decisions

### API Surface (Scope)
- Include `in_transaction` in Phase 2 — the C++ method exists, it's a trivial one-liner wrapper, no reason to defer
- TQRY-01 requirement moves from "Future Requirements" into CAPI-01 / Phase 2 scope
- Update REQUIREMENTS.md traceability to reflect this
- Update ROADMAP.md Phase 2 success criteria to add a 4th criterion for `in_transaction`
- `in_transaction` should also be added to Phase 3 scope for Julia/Dart/Lua bindings

### `in_transaction` Signature
- Out-param pattern: `quiver_error_t quiver_database_in_transaction(quiver_database_t* db, bool* out_active)`
- Consistent with all other C API functions (no special-casing as a direct-return utility)
- Out-param named `out_active` (descriptive, not generic `out_result`)
- NULL db handle returns `QUIVER_ERROR` via `QUIVER_REQUIRE(db)` — uniform with all other functions

### Test Coverage
- Lean tests that mirror success criteria — C++ tests already cover transaction logic
- New test file: `test_c_api_database_transaction.cpp` (follows existing split pattern)
- Reuse the same schema from `tests/schemas/` that C++ transaction tests use
- Batched-writes test must verify data integrity: commit, then read back values through C API to confirm persistence

### Claude's Discretion
- Source file placement for C API implementation (new `database_transaction.cpp` or existing file)
- Header declaration location (existing `include/quiver/c/database.h` or new header)
- Exact test case names and organization within the test file

</decisions>

<specifics>
## Specific Ideas

No specific requirements — standard mechanical wrapping following established C API patterns (QUIVER_REQUIRE, try-catch, binary error codes).

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 02-c-api-transaction-surface*
*Context gathered: 2026-02-21*
