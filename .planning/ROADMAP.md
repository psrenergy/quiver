# Roadmap: Quiver v1.0 Relations

## Overview

This milestone makes foreign key relations work naturally through `create_element` and `update_element` by resolving labels to IDs. The work starts with extracting a shared resolution helper from the existing set FK path, then applies it uniformly to all column types in create, then update, and finishes by evaluating whether the old `update_scalar_relation` method is still needed.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Foundation** - Extract shared FK resolution helper and prove it with the existing set FK refactor
- [ ] **Phase 2: Create Element FK Resolution** - Resolve FK labels for scalar, vector, and time series columns in create_element
- [ ] **Phase 3: Update Element FK Resolution** - Resolve FK labels for all column types in update_element
- [ ] **Phase 4: Cleanup** - Evaluate update_scalar_relation redundancy and finalize

## Phase Details

### Phase 1: Foundation
**Goal**: A single, tested FK label resolution helper exists and the existing set FK path uses it with no behavioral change
**Depends on**: Nothing (first phase)
**Requirements**: FOUND-01, CRE-03, ERR-01
**Success Criteria** (what must be TRUE):
  1. A `resolve_fk_label` method on `Database::Impl` resolves a string label to an integer ID for any FK column
  2. Passing a label that does not exist in the target table throws `"Failed to resolve label 'X' to ID in table 'Y'"`
  3. Existing set FK resolution in `create_element` uses the shared helper and all existing relation tests pass unchanged
  4. Passing a string for a non-FK INTEGER column produces a clear Quiver error (not a raw SQLite STRICT mode error)
**Plans**: TBD

Plans:
- [ ] 01-01: TBD

### Phase 2: Create Element FK Resolution
**Goal**: Users can pass target labels (strings) for scalar, vector, and time series FK columns in create_element and they resolve to target IDs automatically
**Depends on**: Phase 1
**Requirements**: CRE-01, CRE-02, CRE-04
**Success Criteria** (what must be TRUE):
  1. User can call `element.set("parent_id", "Parent Label")` for a scalar FK column and create_element stores the resolved integer ID
  2. User can pass string labels in a vector FK column and create_element resolves each label to its target ID
  3. User can pass string labels in a time series FK column and create_element resolves each label to its target ID
  4. All FK label resolution happens before any SQL writes (pre-resolve pass), so a resolution failure causes zero partial writes
**Plans**: TBD

Plans:
- [ ] 02-01: TBD

### Phase 3: Update Element FK Resolution
**Goal**: Users can pass target labels for scalar, vector, set, and time series FK columns in update_element and they resolve to target IDs automatically
**Depends on**: Phase 2
**Requirements**: UPD-01, UPD-02, UPD-03, UPD-04
**Success Criteria** (what must be TRUE):
  1. User can update a scalar FK column by passing a target label string and update_element resolves it to the target ID
  2. User can update vector FK columns by passing target label strings and update_element resolves each to its target ID
  3. User can update set FK columns by passing target label strings and update_element resolves each to its target ID
  4. User can update time series FK columns by passing target label strings and update_element resolves each to its target ID
  5. All FK label resolution in update_element happens before any SQL writes (pre-resolve pass)
**Plans**: TBD

Plans:
- [ ] 03-01: TBD

### Phase 4: Cleanup
**Goal**: The FK resolution feature is complete and the public API surface is evaluated for redundancy
**Depends on**: Phase 3
**Requirements**: CLN-01
**Success Criteria** (what must be TRUE):
  1. A clear decision is documented on whether `update_scalar_relation` is kept, removed, or kept with guidance noting the preferred alternative
  2. If kept: error messages in `update_scalar_relation` are consistent with the new FK resolution error format
  3. If removed: all call sites in tests and bindings are updated and no compilation errors exist
**Plans**: TBD

Plans:
- [ ] 04-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation | 0/0 | Not started | - |
| 2. Create Element FK Resolution | 0/0 | Not started | - |
| 3. Update Element FK Resolution | 0/0 | Not started | - |
| 4. Cleanup | 0/0 | Not started | - |
