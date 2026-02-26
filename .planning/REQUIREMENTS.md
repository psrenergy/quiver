# Requirements: Quiver

**Defined:** 2026-02-26
**Core Value:** A human-centric database API: clean to read, intuitive to use, consistent across every language binding.

## v0.5 Requirements

Requirements for milestone v0.5. Each maps to roadmap phases.

### Read API

- [x] **READ-01**: Lua and Julia bindings implement `read_element_by_id` as a binding-level composition function returning a flat table/Dict with all scalars, vector columns, and set columns as top-level keys
- [x] **READ-02**: Dart and Python bindings implement `read_element_by_id` as a binding-level composition function returning a flat Map/dict with all scalars, vector columns, and set columns as top-level keys
- [ ] **READ-03**: All bindings return empty table/dict/map for nonexistent IDs, exclude `id` from result, include `label` as a regular scalar, and use native DateTime types where available

### Cleanup

- [ ] **CLEAN-01**: Remove individual typed vector/set by-id reads from all bindings (Lua, Julia, Dart, Python)
- [ ] **CLEAN-02**: Remove composite `read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id` from all bindings (superseded by `read_element_by_id`)

### Testing

- [ ] **TEST-01**: Tests for `read_element_by_id` in all bindings (Lua, Julia, Dart, Python)

## Future Requirements

(None -- this is the first tracked milestone)

## Out of Scope

| Feature | Reason |
|---------|--------|
| C++/C API `read_element_by_id` function | Per CONTEXT.md: binding-level composition, no new C++/C types |
| Changes to individual typed reads in C++/C API | Still useful for targeted single-value lookups |
| Time series in read_element_by_id | Handled separately by `read_time_series_group` |
| New data types or schema changes | Not part of this milestone |

## Traceability

| Requirement | Phase | Plan | Status |
|-------------|-------|------|--------|
| READ-01 | Phase 1 | 01-01 | Complete |
| READ-02 | Phase 1 | 01-02 | Complete |
| READ-03 | Phase 1 | 01-01, 01-02 | Pending |
| CLEAN-01 | Phase 3 | — | Pending |
| CLEAN-02 | Phase 3 | — | Pending |
| TEST-01 | Phase 1 | 01-01, 01-02 | Pending |

**Coverage:**
- v0.5 requirements: 6 total
- Mapped to phases: 6
- Unmapped: 0

---
*Requirements defined: 2026-02-26*
*Last updated: 2026-02-26 after Phase 1 revision (aligned with CONTEXT.md decisions)*
