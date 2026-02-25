# Requirements: Quiver

**Defined:** 2026-02-24
**Core Value:** Consistent, type-safe database operations across multiple languages through a single C++ implementation

## v0.4.1 Requirements

Catch-up milestone: align Python bindings with upstream C++ changes (relation removal, import_csv implementation, CSVOptions rename/restructure).

### Relations

- [ ] **REL-01**: Remove read_scalar_relation and update_scalar_relation methods from Python Database class
- [ ] **REL-02**: Update relation tests to use the new create/update/read pattern (relations via Element.set + read_scalar_integer_by_id)

### CSV

- [ ] **CSV-01**: Implement import_csv in Python by calling the real C API function (collection, group, path, options)
- [ ] **CSV-02**: Rename CSVExportOptions -> CSVOptions and fix enum_labels structure to match C++ (attribute -> locale -> label -> value, with full locale support)
- [ ] **CSV-03**: Add import_csv tests covering scalar and group import round-trips
- [ ] **CSV-04**: Update CLAUDE.md references from CSVExportOptions to CSVOptions

## Future Requirements

None -- this is a catch-up milestone.

## Out of Scope

| Feature | Reason |
|---------|--------|
| New C++ features | Catch-up only, no new API surface |
| Julia/Dart CSV changes | Already correct (matching C++ structure) |
| PyPI packaging | Separate concern, not for this milestone |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| REL-01 | Phase 8 | Pending |
| REL-02 | Phase 8 | Pending |
| CSV-01 | Phase 9 | Pending |
| CSV-02 | Phase 9 | Pending |
| CSV-03 | Phase 9 | Pending |
| CSV-04 | Phase 9 | Pending |

**Coverage:**
- v0.4.1 requirements: 6 total
- Mapped to phases: 6
- Unmapped: 0

---
*Requirements defined: 2026-02-24*
*Last updated: 2026-02-24 after roadmap creation*
