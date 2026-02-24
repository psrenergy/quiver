# Requirements: Quiver

**Defined:** 2026-02-24
**Core Value:** A single, clean C++ API that exposes full SQLite schema capabilities through uniform, mechanically-derived bindings.

## v1 Requirements

Requirements for milestone v1.1 FK Test Coverage. Each maps to roadmap phases.

### C API

- [x] **CAPI-01**: C API FK resolution tests for create_element cover all 9 test cases (set FK, scalar FK, vector FK, time series FK, all types combined, no-FK regression, missing target error, non-FK integer error, zero partial writes)
- [x] **CAPI-02**: C API FK resolution tests for update_element cover all 7 test cases (scalar FK, vector FK, set FK, time series FK, all types combined, no-FK regression, failure preserves existing)

### Julia

- [ ] **JUL-01**: Julia FK resolution tests for create_element! cover all 9 test cases mirroring C++ create path
- [ ] **JUL-02**: Julia FK resolution tests for update_element! cover all 7 test cases mirroring C++ update path

### Dart

- [ ] **DART-01**: Dart FK resolution tests for createElement cover all 9 test cases mirroring C++ create path
- [ ] **DART-02**: Dart FK resolution tests for updateElement cover all 7 test cases mirroring C++ update path

### Lua

- [ ] **LUA-01**: Lua FK resolution tests for create_element cover all 9 test cases mirroring C++ create path
- [ ] **LUA-02**: Lua FK resolution tests for update_element cover all 7 test cases mirroring C++ update path

## Future Requirements

None currently deferred.

## Out of Scope

| Feature | Reason |
|---------|--------|
| New C++ FK tests | Already complete -- 16 tests in v1.0 |
| FK resolution code changes | v1.0 shipped the implementation; this is test-only |
| Binding convenience methods for FK | Bindings pass through to C API; no new methods needed |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| CAPI-01 | Phase 5 | Complete |
| CAPI-02 | Phase 5 | Complete |
| JUL-01 | Phase 6 | Pending |
| JUL-02 | Phase 6 | Pending |
| DART-01 | Phase 7 | Pending |
| DART-02 | Phase 7 | Pending |
| LUA-01 | Phase 8 | Pending |
| LUA-02 | Phase 8 | Pending |

**Coverage:**
- v1 requirements: 8 total
- Mapped to phases: 8
- Unmapped: 0

---
*Requirements defined: 2026-02-24*
*Last updated: 2026-02-24 after roadmap creation*
