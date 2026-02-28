# Requirements: Quiver

**Defined:** 2026-02-28
**Core Value:** Reliable, type-safe database operations across all language bindings with consistent API surface

## v1 Requirements

Requirements for milestone v0.5. Each maps to roadmap phases.

### Bugs

- [ ] **BUG-01**: `update_element` validates array types for vector, set, and time series groups (matching `create_element` behavior)
- [ ] **BUG-02**: `describe()` prints each category header (Vectors, Sets, Time Series) exactly once per collection
- [ ] **BUG-03**: `import_csv` header parameter renamed from `table` to `collection` for consistency

### Code Quality

- [ ] **QUAL-01**: `current_version()` uses RAII `unique_ptr` with custom deleter instead of manual `sqlite3_finalize`
- [ ] **QUAL-02**: Scalar read methods use `read_grouped_values_by_id` template instead of manual loops
- [ ] **QUAL-03**: Group insertion logic extracted into shared helper used by both `create_element` and `update_element`
- [ ] **QUAL-04**: C API string copies use `strdup_safe` instead of inline allocation

## v2 Requirements

(None — v0.5 is a focused quality milestone)

## Out of Scope

| Feature | Reason |
|---------|--------|
| New public API methods | v0.5 is internal quality only |
| New language bindings | Not in scope for this milestone |
| Performance optimization | Not the focus unless incidental to refactoring |
| New features | Strictly internal improvements |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| BUG-01 | — | Pending |
| BUG-02 | — | Pending |
| BUG-03 | — | Pending |
| QUAL-01 | — | Pending |
| QUAL-02 | — | Pending |
| QUAL-03 | — | Pending |
| QUAL-04 | — | Pending |

**Coverage:**
- v1 requirements: 7 total
- Mapped to phases: 0
- Unmapped: 7 ⚠️

---
*Requirements defined: 2026-02-28*
*Last updated: 2026-02-28 after initial definition*
