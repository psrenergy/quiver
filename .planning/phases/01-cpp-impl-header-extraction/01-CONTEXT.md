# Phase 1: C++ Impl Header Extraction - Context

**Gathered:** 2026-02-09
**Status:** Ready for planning

<domain>
## Phase Boundary

Extract the `Database::Impl` struct definition from `src/database.cpp` into a private internal header (`src/database_impl.h`) so that future split files (Phase 2) can include it. No public header changes. No behavior changes. All tests must pass identically.

</domain>

<decisions>
## Implementation Decisions

### What moves to the header
- The `Database::Impl` struct definition moves to `src/database_impl.h`
- Anonymous namespace helper templates (read_grouped_values_all, get_row_value, etc.) stay local — they belong in their future split files, not in a shared header
- All decisions about TransactionGuard placement, with_transaction() approach, and helper organization are at Claude's discretion

### Claude's Discretion
- Whether TransactionGuard moves with Impl or stays in database.cpp (pick the cleanest approach based on actual dependencies)
- Whether with_transaction is an Impl method or standalone function (pick idiomatic C++)
- Header location and naming convention (src/database_impl.h is the default expectation)
- Include guard strategy (pragma once vs ifdef)
- Forward declarations vs direct includes in the internal header
- CMake include path approach (simplest correct method)

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches. The user trusts Claude to make the right structural decisions for this purely mechanical extraction.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 01-cpp-impl-header-extraction*
*Context gathered: 2026-02-09*
