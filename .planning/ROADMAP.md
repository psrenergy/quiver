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

## Phase 2: Julia Cleanup

**Goal:** Remove dead code and application-specific helpers to restore homogeneity

**Requirements:** JUL-01, JUL-02, JUL-03, JUL-04, JUL-05

**Success Criteria:**
1. quiver_database_sqlite_error deleted from exceptions.jl
2. helper_maps.jl removed from Quiver.jl includes and file deleted
3. test_helper_maps.jl deleted
4. All remaining Julia tests pass

---
*Roadmap created: 2026-03-01*
*Phases: 2 | Requirements: 13 | Coverage: 100%*
