# Requirements: Quiver

**Defined:** 2026-02-23
**Core Value:** A single, clean C++ API that exposes full SQLite schema capabilities through uniform, mechanically-derived bindings.

## v1 Requirements

Requirements for milestone v1.0 Relations. Each maps to roadmap phases.

### Foundation

- [x] **FOUND-01**: Shared `resolve_fk_label` helper extracts the proven set FK pattern into a reusable method on `Database::Impl`

### Create Element

- [ ] **CRE-01**: User can pass a target label (string) for a scalar FK column in `create_element` and it resolves to the target's ID
- [ ] **CRE-02**: User can pass target labels for vector FK columns in `create_element` and they resolve to target IDs
- [x] **CRE-03**: Existing set FK resolution in `create_element` uses the shared helper (refactor, no behavior change)
- [ ] **CRE-04**: User can pass target labels for time series FK columns in `create_element` and they resolve to target IDs

### Update Element

- [ ] **UPD-01**: User can pass a target label for a scalar FK column in `update_element` and it resolves to the target's ID
- [ ] **UPD-02**: User can pass target labels for vector FK columns in `update_element` and they resolve to target IDs
- [ ] **UPD-03**: User can pass target labels for set FK columns in `update_element` and they resolve to target IDs
- [ ] **UPD-04**: User can pass target labels for time series FK columns in `update_element` and they resolve to target IDs

### Error Handling

- [x] **ERR-01**: Passing a label that doesn't exist in the target table throws a clear error: `"Failed to resolve label 'X' to ID in table 'Y'"`

### Cleanup

- [ ] **CLN-01**: Evaluate whether `update_scalar_relation` is redundant and decide keep/remove

## Future Requirements

None currently deferred.

## Out of Scope

| Feature | Reason |
|---------|--------|
| FK resolution cache | Premature optimization; SQLite page cache handles repeated lookups |
| Batch label resolution | More complex SQL, marginal perf gain, harder per-label error messages |
| FK resolution in typed update methods (`update_scalar_integer`, etc.) | These accept typed values; callers already have IDs |
| Reverse FK resolution (ID-to-label at write time) | `read_scalar_relation` handles the read direction |
| FK resolution in query methods | Queries are raw SQL; implicit resolution would be surprising |
| "Find or create" semantics for FK targets | Silent data creation is dangerous for a low-level wrapper |
| New binding methods or C API changes | Resolution happens entirely in C++ layer; bindings pass through unchanged |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| FOUND-01 | Phase 1 | Complete |
| CRE-01 | Phase 2 | Pending |
| CRE-02 | Phase 2 | Pending |
| CRE-03 | Phase 1 | Complete |
| CRE-04 | Phase 2 | Pending |
| UPD-01 | Phase 3 | Pending |
| UPD-02 | Phase 3 | Pending |
| UPD-03 | Phase 3 | Pending |
| UPD-04 | Phase 3 | Pending |
| ERR-01 | Phase 1 | Complete |
| CLN-01 | Phase 4 | Pending |

**Coverage:**
- v1 requirements: 11 total
- Mapped to phases: 11
- Unmapped: 0

---
*Requirements defined: 2026-02-23*
*Last updated: 2026-02-23 after roadmap creation*
