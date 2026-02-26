# Roadmap: Quiver v0.5 (read_element_by_id)

## Overview

This milestone adds a composite `read_element_by_id` function that returns all scalars, vectors, and sets for an element in a single call. Work flows top-down through the library's layers: C++ core and C API first, then thin binding wrappers in all four languages, then cleanup of the redundant typed reads that the new function supersedes. Each phase delivers a verifiable capability before the next begins.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Core Implementation** - C++ `read_element_by_id` and C API with transparent structs, plus C++/C tests
- [ ] **Phase 2: Binding Wrappers** - Thin wrappers in Lua, Julia, Dart, and Python with tests at all layers
- [ ] **Phase 3: Binding Cleanup** - Remove redundant typed vector/set by-id reads and composite helpers from all bindings

## Phase Details

### Phase 1: Core Implementation
**Goal**: Callers in all four bindings can retrieve all scalars, vectors, and sets for an element in a single call via binding-level composition of existing C API reads
**Depends on**: Nothing (first phase)
**Requirements**: READ-01, READ-02
**Success Criteria** (what must be TRUE):
  1. Lua caller can call `db:read_element_by_id(collection, id)` and receive a flat Lua table with all scalar, vector column, and set column values as top-level keys
  2. Julia caller can call `read_element_by_id(db, collection, id)` and receive a flat Dict{String,Any} with the same structure
  3. Dart caller can call `db.readElementById(collection, id)` and receive a flat Map<String, Object?> with the same structure
  4. Python caller can call `db.read_element_by_id(collection, id)` and receive a flat dict with the same structure
  5. All four bindings return empty table/dict/map for nonexistent element IDs
  6. Tests pass in all four bindings covering: element with scalars/vectors/sets, scalars only, nonexistent ID
**Plans**: 2 plans

Plans:
- [x] 01-01-PLAN.md -- Implement read_element_by_id in Lua and Julia bindings with tests
- [x] 01-02-PLAN.md -- Implement read_element_by_id in Dart and Python bindings with tests

### Phase 2: Binding Wrappers
**Goal**: Users in any supported language can call `read_element_by_id` and get back a native data structure
**Depends on**: Phase 1
**Requirements**: READ-03, TEST-01
**Success Criteria** (what must be TRUE):
  1. Lua caller can call `db:read_element_by_id(collection, id)` and receive a Lua table with scalar, vector, and set data
  2. Julia caller can call `read_element_by_id(db, collection, id)` and receive a native Dict
  3. Dart caller can call `db.readElementById(collection, id)` and receive a Dart Map
  4. Python caller can call `db.read_element_by_id(collection, id)` and receive a Python dict
  5. Tests pass at all six layers (C++, C API, Lua, Julia, Dart, Python) covering `read_element_by_id`
**Plans**: TBD

Plans:
- [ ] 02-01: TBD
- [ ] 02-02: TBD

### Phase 3: Binding Cleanup
**Goal**: Redundant typed by-id reads and composite helpers are removed from all bindings, leaving `read_element_by_id` as the single composite read path
**Depends on**: Phase 2
**Requirements**: CLEAN-01, CLEAN-02
**Success Criteria** (what must be TRUE):
  1. Individual typed vector/set by-id reads (e.g., `read_vector_integers_by_id`, `read_set_strings_by_id`) no longer exist in any binding (Lua, Julia, Dart, Python)
  2. Composite helpers (`read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id`) no longer exist in any binding
  3. All existing tests still pass after removal (no regressions from cleanup)
  4. Binding test suites that previously tested removed functions are updated or removed
**Plans**: TBD

Plans:
- [ ] 03-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Core Implementation | 2/2 | Complete | 2026-02-26 |
| 2. Binding Wrappers | 0/0 | Not started | - |
| 3. Binding Cleanup | 0/0 | Not started | - |
