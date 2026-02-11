# Requirements: Quiver Refactoring

**Defined:** 2026-02-09
**Core Value:** Every public C++ method is reachable from every binding through uniform, predictable patterns

## v1 Requirements

Requirements for this refactoring milestone. Each maps to roadmap phases.

### File Decomposition

- [x] **DECP-01**: C++ `database.cpp` (1934 lines) split into functional modules by operation type (lifecycle, create, read, update, delete, metadata, time series, query, relations, describe) ✓ (Phase 2)
- [x] **DECP-02**: Internal `database_impl.h` header extracted so split files share `Database::Impl` without exposing it publicly ✓ (Phase 1)
- [ ] **DECP-03**: C API `src/c/database.cpp` (1612 lines) split into functional modules mirroring C++ structure
- [ ] **DECP-04**: C API helper templates (marshaling, strdup_safe, metadata converters) extracted into shared internal header
- [x] **DECP-05**: All existing C++ tests pass after decomposition with zero public header changes ✓ (Phase 2)
- [ ] **DECP-06**: All existing C API tests pass after decomposition with zero public header changes

### Naming Consistency

- [ ] **NAME-01**: C++ public method names follow consistent convention across all Database methods
- [ ] **NAME-02**: C API function names follow consistent `quiver_{entity}_{operation}` convention
- [ ] **NAME-03**: Julia binding function names follow idiomatic Julia conventions while mapping predictably to C API
- [ ] **NAME-04**: Dart binding method names follow idiomatic Dart conventions while mapping predictably to C API
- [ ] **NAME-05**: Lua binding method names follow consistent convention matching C++ method names
- [ ] **NAME-06**: Naming convention documented in CLAUDE.md with cross-layer mapping examples

### Error Handling

- [ ] **ERRH-01**: All C++ methods use consistent exception patterns (same exception types, message format)
- [ ] **ERRH-02**: All C API functions use consistent try-catch-set_last_error pattern with QUIVER_REQUIRE macro
- [ ] **ERRH-03**: Julia bindings surface C API errors uniformly (no custom error messages)
- [ ] **ERRH-04**: Dart bindings surface C API errors uniformly (no custom error messages)
- [ ] **ERRH-05**: Lua bindings surface errors uniformly through pcall/error patterns

### Code Hygiene

- [x] **HYGN-01**: SQL string concatenation in schema queries replaced with parameterized equivalents or validated identifiers ✓ (Phase 9)
- [x] **HYGN-02**: `.clang-tidy` configuration added with `readability-identifier-naming`, `bugprone-*`, `modernize-*`, `performance-*` checks ✓ (Phase 9)
- [x] **HYGN-03**: Existing code passes clang-tidy checks (or suppressions documented for intentional exceptions) ✓ (Phase 9)

### Testing

- [ ] **TEST-01**: All C++ test suites pass after every refactoring phase
- [ ] **TEST-02**: All C API test suites pass after every refactoring phase
- [ ] **TEST-03**: Julia test suite passes after every refactoring phase
- [ ] **TEST-04**: Dart test suite passes after every refactoring phase
- [ ] **TEST-05**: `scripts/build-all.bat` succeeds as gate criterion for each phase

## v2 Requirements

Deferred to future milestone. Tracked but not in current roadmap.

### API Parity

- **PRTY-01**: Every C++ public method has a corresponding C API function
- **PRTY-02**: Every C API function has a Julia binding
- **PRTY-03**: Every C API function has a Dart binding
- **PRTY-04**: Every C API function has a Lua binding
- **PRTY-05**: Canonical API mapping table documenting all methods across all layers
- **PRTY-06**: Automated parity detection script that verifies binding completeness

### Blob Module

- **BLOB-01**: Blob module stubs resolved (either completed or removed)
- **BLOB-02**: If removed, SchemaValidator rejects BLOB columns explicitly

### Internal Headers

- **INTL-01**: Internal `database_impl.h` header extracted for C++ Pimpl sharing
- **INTL-02**: C API helper templates extracted into shared internal header

## Out of Scope

| Feature | Reason |
|---------|--------|
| New database operations | Refactoring only -- no new features beyond current API surface |
| Performance optimization | Focus is consistency, not speed |
| New language bindings (Python, etc.) | Stabilize existing bindings first |
| C++20 modules | Breaks FFI generator toolchain (Dart ffigen, Julia Clang.jl parse C headers) |
| SWIG-generated bindings | Produces non-idiomatic wrappers; current approach is superior |
| Async/thread-safe API | SQLite is inherently single-connection; adding threading adds complexity without benefit |
| Rich error codes | Current binary error codes + thread-local messages are correct for FFI |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| DECP-01 | Phase 2 | Complete ✓ |
| DECP-02 | Phase 1 | Complete ✓ |
| DECP-03 | Phase 4 | Pending |
| DECP-04 | Phase 4 | Pending |
| DECP-05 | Phase 2 | Complete ✓ |
| DECP-06 | Phase 4 | Pending |
| NAME-01 | Phase 3 | Pending |
| NAME-02 | Phase 5 | Pending |
| NAME-03 | Phase 6 | Pending |
| NAME-04 | Phase 7 | Pending |
| NAME-05 | Phase 8 | Pending |
| NAME-06 | Phase 10 | Pending |
| ERRH-01 | Phase 3 | Pending |
| ERRH-02 | Phase 5 | Pending |
| ERRH-03 | Phase 6 | Pending |
| ERRH-04 | Phase 7 | Pending |
| ERRH-05 | Phase 8 | Pending |
| HYGN-01 | Phase 9 | Pending |
| HYGN-02 | Phase 9 | Pending |
| HYGN-03 | Phase 9 | Pending |
| TEST-01 | Phase 10 | Pending |
| TEST-02 | Phase 10 | Pending |
| TEST-03 | Phase 10 | Pending |
| TEST-04 | Phase 10 | Pending |
| TEST-05 | Phase 10 | Pending |

**Coverage:**
- v1 requirements: 25 total
- Mapped to phases: 25
- Unmapped: 0

---
*Requirements defined: 2026-02-09*
*Last updated: 2026-02-09 after roadmap creation*
