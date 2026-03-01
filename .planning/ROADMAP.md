# Roadmap: Quiver v0.5

## Overview

Milestone v0.5 addresses code quality: fixing bugs in element operations, describe output, and CSV import naming, then refactoring C++ internals for RAII and template consistency, and finally cleaning up C API string handling. All changes are internal -- no new public API surface. Every change requires passing tests across C++, C API, Julia, Dart, and Python.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Bug Fixes and Element Dedup** - Fix update_element validation, describe headers, import_csv naming, and extract shared group insertion helper
- [ ] **Phase 2: C++ Core Refactoring** - Apply RAII to current_version and template to scalar reads
- [ ] **Phase 3: C API String Consistency** - Replace inline string allocations with strdup_safe

## Phase Details

### Phase 1: Bug Fixes and Element Dedup
**Goal**: All known bugs are fixed and element creation/update share a single group insertion path
**Depends on**: Nothing (first phase)
**Requirements**: BUG-01, BUG-02, BUG-03, QUAL-03
**Success Criteria** (what must be TRUE):
  1. Calling `update_element` with a mismatched array type (e.g., passing strings to an integer vector) produces an error, matching `create_element` behavior
  2. `describe()` output for a collection with multiple vectors, sets, or time series groups shows each category header exactly once
  3. `import_csv` accepts `collection` (not `table`) as its parameter name across C++, C API, and all bindings
  4. `create_element` and `update_element` both call the same internal helper for inserting vector, set, and time series data -- no duplicated insertion logic remains
**Plans**: 2 plans

Plans:
- [x] 01-01-PLAN.md — Fix import_csv parameter rename (BUG-03) and restructure describe() output (BUG-02)
- [x] 01-02-PLAN.md — Extract shared group insertion helper (QUAL-03) fixing update_element type validation (BUG-01)

### Phase 2: C++ Core Refactoring
**Goal**: C++ internals use idiomatic RAII and template patterns, eliminating manual resource management and loop duplication in read paths
**Depends on**: Phase 1
**Requirements**: QUAL-01, QUAL-02
**Success Criteria** (what must be TRUE):
  1. `current_version()` manages its SQLite statement via `unique_ptr` with custom deleter -- no manual `sqlite3_finalize` call exists in the function
  2. All scalar read methods (`read_scalar_integers`, `read_scalar_floats`, `read_scalar_strings`) delegate to the `read_grouped_values_by_id` template -- no manual row-iteration loops remain in scalar read implementations
  3. All existing tests pass unchanged (behavior is identical, only implementation differs)
**Plans**: TBD

Plans:
- [ ] 02-01: TBD

### Phase 3: C API String Consistency
**Goal**: All C API string copies use the `strdup_safe` helper, eliminating inline `new char[]` + `memcpy` patterns
**Depends on**: Phase 2
**Requirements**: QUAL-04
**Success Criteria** (what must be TRUE):
  1. Every C API function that copies a C++ string to a returned `char*` uses `strdup_safe` -- no inline `new char[size + 1]` followed by `memcpy` exists in `src/c/` files
  2. All existing C API tests pass unchanged (behavior is identical, only implementation differs)
**Plans**: TBD

Plans:
- [ ] 03-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Bug Fixes and Element Dedup | 2/2 | Complete | 2026-03-01 |
| 2. C++ Core Refactoring | 0/? | Not started | - |
| 3. C API String Consistency | 0/? | Not started | - |
