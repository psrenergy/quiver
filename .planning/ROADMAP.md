# Roadmap: Quiver v0.5

## Phase 1: Dart Metadata Types

**Goal:** Replace inline record types with proper ScalarMetadata/GroupMetadata classes

**Requirements:** DART-01, DART-02, DART-03, DART-04, DART-05, DART-06, DART-07, DART-08

**Plans:** 1 plan

Plans:
- [ ] 01-01-PLAN.md — Create ScalarMetadata/GroupMetadata classes, replace all inline record types, add construction tests

**Success Criteria:**
1. ScalarMetadata and GroupMetadata classes exist in a new `metadata.dart` file
2. All inline record types in database.dart, database_metadata.dart, and database_read.dart replaced
3. quiverdb.dart exports both metadata types
4. All existing Dart tests pass unchanged (API compatible)
5. New tests verify metadata class construction and field access

## Phase 2: Binding Cleanup

**Goal:** Remove dead code across all language bindings (Julia, Dart, Python) to restore homogeneity

**Requirements:** JUL-01, JUL-05

**Plans:** 1 plan

Plans:
- [ ] 02-01-PLAN.md — Delete dead code (Julia quiver_database_sqlite_error, Python encode_string) and verify all binding tests pass

**Success Criteria:**
1. quiver_database_sqlite_error deleted from Julia exceptions.jl
2. Light audit of all bindings (Julia, Dart, Python) for additional dead code — findings cleaned up
3. helper_maps.jl and test_helper_maps.jl preserved (actively used)
4. All existing tests pass across all bindings

---
*Roadmap created: 2026-03-01*
*Phases: 2 | Requirements: 13 | Coverage: 100%*
