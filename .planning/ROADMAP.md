# Roadmap: Quiver

## Milestones

- ✅ **v1.0 Relations** -- Phases 1-4 (shipped 2026-02-24)
- **v1.1 FK Test Coverage** -- Phases 5-8 (in progress)

## Phases

<details>
<summary>v1.0 Relations (Phases 1-4) -- SHIPPED 2026-02-24</summary>

- [x] Phase 1: Foundation (1/1 plan) -- completed 2026-02-23
- [x] Phase 2: Create Element FK Resolution (2/2 plans) -- completed 2026-02-23
- [x] Phase 3: Update Element FK Resolution (1/1 plan) -- completed 2026-02-24
- [x] Phase 4: Cleanup (2/2 plans) -- completed 2026-02-24

</details>

### v1.1 FK Test Coverage

**Milestone Goal:** Mirror all 16 C++ FK resolution tests across C API, Julia, Dart, and Lua to ensure FK label resolution is tested end-to-end through every binding layer.

- [ ] **Phase 5: C API FK Tests** - 16 FK resolution tests covering create and update paths through the C API
- [ ] **Phase 6: Julia FK Tests** - 16 FK resolution tests covering create! and update paths through Julia bindings
- [ ] **Phase 7: Dart FK Tests** - 16 FK resolution tests covering createElement and updateElement through Dart bindings
- [ ] **Phase 8: Lua FK Tests** - 16 FK resolution tests covering create_element and update_element through Lua scripting

## Phase Details

### Phase 5: C API FK Tests
**Goal**: C API callers can verify that FK label resolution works correctly for all column types through create and update element functions
**Depends on**: Phase 4 (v1.0 shipped FK resolution implementation)
**Requirements**: CAPI-01, CAPI-02
**Success Criteria** (what must be TRUE):
  1. C API create_element resolves set FK labels to IDs, scalar FK labels to IDs, vector FK labels to IDs, and time series FK labels to IDs -- verified by reading back resolved integer IDs after creation
  2. C API create_element returns QUIVER_ERROR with descriptive last_error when a FK target label does not exist, and writes zero rows on resolution failure
  3. C API update paths (scalar, vector, set, time series) resolve FK labels to IDs -- verified by updating an existing element and reading back the resolved values
  4. C API update failure preserves the element's existing data -- verified by attempting an update with a bad FK label and confirming original values remain
  5. Non-FK integer columns pass through unchanged in both create and update paths -- verified by a no-FK regression test that creates and updates elements with plain integer attributes
**Plans**: 2 plans

Plans:
- [ ] 05-01-PLAN.md — 9 FK resolution create tests (set, scalar, vector, time series, combined, no-FK, errors, partial writes)
- [ ] 05-02-PLAN.md — 7 FK resolution update tests (scalar, vector, set, time series, combined, no-FK, failure preserves existing)

### Phase 6: Julia FK Tests
**Goal**: Julia callers can verify that FK label resolution works correctly for all column types through create_element! and update paths
**Depends on**: Phase 5
**Requirements**: JUL-01, JUL-02
**Success Criteria** (what must be TRUE):
  1. Julia create_element! resolves set FK labels to IDs, scalar FK labels to IDs, vector FK labels to IDs, and time series FK labels to IDs -- verified by reading back resolved integer IDs after creation
  2. Julia create_element! throws an error when a FK target label does not exist, and writes zero rows on resolution failure
  3. Julia update paths (scalar, vector, set, time series) resolve FK labels to IDs -- verified by updating an existing element and reading back the resolved values
  4. Julia update failure preserves the element's existing data -- verified by attempting an update with a bad FK label and confirming original values remain
  5. Non-FK integer columns pass through unchanged in both create and update paths -- verified by a no-FK regression test
**Plans**: TBD

Plans:
- [ ] 06-01: TBD
- [ ] 06-02: TBD

### Phase 7: Dart FK Tests
**Goal**: Dart callers can verify that FK label resolution works correctly for all column types through createElement and updateElement
**Depends on**: Phase 5
**Requirements**: DART-01, DART-02
**Success Criteria** (what must be TRUE):
  1. Dart createElement resolves set FK labels to IDs, scalar FK labels to IDs, vector FK labels to IDs, and time series FK labels to IDs -- verified by reading back resolved integer IDs after creation
  2. Dart createElement throws an exception when a FK target label does not exist, and writes zero rows on resolution failure
  3. Dart update paths (scalar, vector, set, time series) resolve FK labels to IDs -- verified by updating an existing element and reading back the resolved values
  4. Dart update failure preserves the element's existing data -- verified by attempting an update with a bad FK label and confirming original values remain
  5. Non-FK integer columns pass through unchanged in both create and update paths -- verified by a no-FK regression test
**Plans**: TBD

Plans:
- [ ] 07-01: TBD
- [ ] 07-02: TBD

### Phase 8: Lua FK Tests
**Goal**: Lua callers can verify that FK label resolution works correctly for all column types through create_element and update_element
**Depends on**: Phase 5
**Requirements**: LUA-01, LUA-02
**Success Criteria** (what must be TRUE):
  1. Lua create_element resolves set FK labels to IDs, scalar FK labels to IDs, vector FK labels to IDs, and time series FK labels to IDs -- verified by reading back resolved integer IDs after creation
  2. Lua create_element raises an error when a FK target label does not exist, and writes zero rows on resolution failure
  3. Lua update paths (scalar, vector, set, time series) resolve FK labels to IDs -- verified by updating an existing element and reading back the resolved values
  4. Lua update failure preserves the element's existing data -- verified by attempting an update with a bad FK label and confirming original values remain
  5. Non-FK integer columns pass through unchanged in both create and update paths -- verified by a no-FK regression test
**Plans**: TBD

Plans:
- [ ] 08-01: TBD
- [ ] 08-02: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 5 -> 6 -> 7 -> 8

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Foundation | v1.0 | 1/1 | Complete | 2026-02-23 |
| 2. Create Element FK Resolution | v1.0 | 2/2 | Complete | 2026-02-23 |
| 3. Update Element FK Resolution | v1.0 | 1/1 | Complete | 2026-02-24 |
| 4. Cleanup | v1.0 | 2/2 | Complete | 2026-02-24 |
| 5. C API FK Tests | v1.1 | 0/2 | Not started | - |
| 6. Julia FK Tests | v1.1 | 0/? | Not started | - |
| 7. Dart FK Tests | v1.1 | 0/? | Not started | - |
| 8. Lua FK Tests | v1.1 | 0/? | Not started | - |
