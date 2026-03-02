# Requirements: Quiver v0.5

**Defined:** 2026-03-01
**Core Value:** Clean, human-readable binding code with uniform API surface

## v0.5 Requirements

### Dart Metadata Types

- [x] **DART-01**: Define ScalarMetadata class with named fields matching Julia/Python (name, dataType, notNull, primaryKey, defaultValue, isForeignKey, referencesCollection, referencesColumn)
- [x] **DART-02**: Define GroupMetadata class with named fields matching Julia/Python (groupName, dimensionColumn, valueColumns)
- [x] **DART-03**: Replace all inline record types in database.dart with ScalarMetadata/GroupMetadata
- [x] **DART-04**: Replace all inline record types in database_metadata.dart with ScalarMetadata/GroupMetadata
- [x] **DART-05**: Replace all inline record types in database_read.dart that dispatch on dataType
- [x] **DART-06**: Export ScalarMetadata and GroupMetadata from quiverdb.dart
- [x] **DART-07**: All existing Dart tests pass with new metadata types
- [x] **DART-08**: Add Dart tests for ScalarMetadata and GroupMetadata construction and field access

### Julia Cleanup

- [ ] **JUL-01**: Delete quiver_database_sqlite_error from exceptions.jl
- [ ] **JUL-02**: Remove helper_maps.jl include from Quiver.jl
- [ ] **JUL-03**: Delete helper_maps.jl file
- [ ] **JUL-04**: Delete test_helper_maps.jl file
- [ ] **JUL-05**: All existing Julia tests pass after removal

## Out of Scope

| Feature | Reason |
|---------|--------|
| New C++ features | v0.5 is binding quality only |
| New C API functions | No API changes needed |
| Lua binding changes | No issues found |
| Python binding changes | No issues found (code is clean) |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| DART-01 | Phase 1 | Complete |
| DART-02 | Phase 1 | Complete |
| DART-03 | Phase 1 | Complete |
| DART-04 | Phase 1 | Complete |
| DART-05 | Phase 1 | Complete |
| DART-06 | Phase 1 | Complete |
| DART-07 | Phase 1 | Complete |
| DART-08 | Phase 1 | Complete |
| JUL-01 | Phase 2 | Pending |
| JUL-02 | Phase 2 | Pending |
| JUL-03 | Phase 2 | Pending |
| JUL-04 | Phase 2 | Pending |
| JUL-05 | Phase 2 | Pending |

**Coverage:**
- v0.5 requirements: 13 total
- Mapped to phases: 13
- Unmapped: 0 ✓

---
*Requirements defined: 2026-03-01*
