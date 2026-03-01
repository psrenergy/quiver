# Requirements: Quiver

**Defined:** 2026-02-28
**Core Value:** Reliable, type-safe database operations across all language bindings with consistent API surface

## v1 Requirements

Requirements for milestone v0.5. Each maps to roadmap phases.

### Bugs

- [x] **BUG-01**: `update_element` validates array types for vector, set, and time series groups (matching `create_element` behavior)
- [x] **BUG-02**: `describe()` prints each category header (Vectors, Sets, Time Series) exactly once per collection
- [x] **BUG-03**: `import_csv` header parameter renamed from `table` to `collection` for consistency

### Code Quality

- [x] **QUAL-01**: `current_version()` uses RAII `unique_ptr` with custom deleter instead of manual `sqlite3_finalize`
- [x] **QUAL-02**: Scalar read methods use `read_column_values` template instead of manual loops
- [x] **QUAL-03**: Group insertion logic extracted into shared helper used by both `create_element` and `update_element`
- [ ] **QUAL-04**: C API string copies use `strdup_safe` instead of inline allocation

## v2 Requirements

(None -- v0.5 is a focused quality milestone)

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
| BUG-01 | Phase 1 | Complete |
| BUG-02 | Phase 1 | Complete |
| BUG-03 | Phase 1 | Complete |
| QUAL-01 | Phase 2 | Complete |
| QUAL-02 | Phase 2 | Complete |
| QUAL-03 | Phase 1 | Complete |
| QUAL-04 | Phase 3 | Pending |

**Coverage:**
- v1 requirements: 7 total
- Mapped to phases: 7
- Unmapped: 0

---
*Requirements defined: 2026-02-28*
*Last updated: 2026-03-01 after 02-01 completion*
