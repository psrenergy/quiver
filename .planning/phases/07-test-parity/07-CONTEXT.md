# Phase 7: Test Parity - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Comprehensive test coverage audit and gap-fill across all language layers: C++, C API, Julia, Dart, Python, and Lua. Every functional area must have tests in every layer. No new features or API changes — purely test infrastructure.

</domain>

<decisions>
## Implementation Decisions

### Parity Baseline
- C++ is the reference layer — all other layers must cover what C++ covers
- Audit C++ itself first against the public API surface, filling any gaps before using it as baseline
- Bidirectional: if any binding layer tests an operation C++ doesn't, add it to C++ too
- Cascade order: C++ → C API → Julia → Dart → Python → Lua (each layer builds on the previous)
- New test schemas in `tests/schemas/` are allowed if needed for gap-fill

### Gap-Fill Depth
- Happy path only — one test per missing operation confirming it works with valid inputs
- Do not replicate error path tests from other layers; existence of a happy-path test is sufficient
- Use same test data (schemas, element values) as existing tests when possible for cross-layer consistency
- Add gap-fill tests to existing test files for that functional area; only create new files for completely untested areas

### Layer-Specific Handling
- Convenience methods (read_all_scalars_by_id, transaction blocks, datetime wrappers) are in scope — test in every layer that offers them
- C API gets a full mirror of C++ test file structure: one test file per functional area (lifecycle, create, read, update, delete, query, time series, transaction, metadata, CSV)
- Python is treated identically to Julia and Dart — no special focus
- Lua is included in the parity audit alongside the other 5 layers

### Audit Output Format
- Coverage matrix: rows = operations, columns = layers, cells = check/missing
- Matrix lives inside the PLAN.md files — audit results feed directly into task breakdown
- Audit checks test existence only (is there a test for this operation?), not test quality
- Final validation step: all 6 test suites (C++, C API, Julia, Dart, Python, Lua) must run with zero failures

### Claude's Discretion
- Whether to compare test scenarios vs operations (user deferred this choice)
- Exact grouping of operations in the coverage matrix
- Which convenience methods are equivalent across layers for comparison purposes

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

*Phase: 07-test-parity*
*Context gathered: 2026-02-24*
