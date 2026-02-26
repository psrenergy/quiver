# Requirements: Quiver

**Defined:** 2026-02-26
**Core Value:** A human-centric database API: clean to read, intuitive to use, consistent across every language binding.

## v0.5 Requirements

Requirements for milestone v0.5. Each maps to roadmap phases.

### Read API

- [ ] **READ-01**: `read_element_by_id(collection, id)` in C++ returns an `ElementData` struct with all scalars, vectors, and sets for the element
- [ ] **READ-02**: C API exposes `quiver_database_read_element_by_id` with transparent struct types (`quiver_scalar_entry_t`, `quiver_array_entry_t`, `quiver_element_data_t`) and a matching `quiver_element_data_free`
- [ ] **READ-03**: Lua, Julia, Dart, and Python bindings expose `read_element_by_id` as a thin wrapper returning a native dict/map/table

### Cleanup

- [ ] **CLEAN-01**: Remove individual typed vector/set by-id reads from all bindings (Lua, Julia, Dart, Python)
- [ ] **CLEAN-02**: Remove composite `read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id` from all bindings (superseded by `read_element_by_id`)

### Testing

- [ ] **TEST-01**: Tests for `read_element_by_id` at all layers (C++, C API, Lua, Julia, Dart, Python)

## Future Requirements

(None — this is the first tracked milestone)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Changes to individual typed reads in C++/C API | Still useful for targeted single-value lookups |
| Time series in read_element_by_id | Handled separately by `read_time_series_group` |
| New data types or schema changes | Not part of this milestone |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| READ-01 | — | Pending |
| READ-02 | — | Pending |
| READ-03 | — | Pending |
| CLEAN-01 | — | Pending |
| CLEAN-02 | — | Pending |
| TEST-01 | — | Pending |

**Coverage:**
- v0.5 requirements: 6 total
- Mapped to phases: 0
- Unmapped: 6

---
*Requirements defined: 2026-02-26*
*Last updated: 2026-02-26 after initial definition*
