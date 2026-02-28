# Requirements: Quiver

**Defined:** 2026-02-27
**Core Value:** A single C++ implementation powers every language binding identically

## v0.5 Requirements

Requirements for code quality milestone. Each maps to roadmap phases.

### Type Ownership

- [ ] **TYPES-01**: C++ defines `DatabaseOptions` with `enum class LogLevel` natively; C API converts at boundary
- [ ] **TYPES-02**: C++ defines `CSVOptions` natively; C API converts at boundary

### API Naming

- [ ] **NAME-01**: Add `quiver_database_free_string()` for database query results; update all bindings to use it
- [ ] **NAME-02**: Remove `quiver_element_free_string()` entirely (breaking change); update all bindings

### Python Binding

- [ ] **PY-01**: Python LuaRunner binding wraps `quiver_lua_runner_new/run/get_error/free`; stores Database reference to prevent GC lifetime hazard
- [ ] **PY-02**: Python `DataType` IntEnum replaces all hardcoded `0/1/2/3/4` magic numbers in `database.py`
- [ ] **PY-03**: Python tests cover `read_scalars_by_id`, `read_vectors_by_id`, `read_sets_by_id`, `read_element_by_id`, `read_vector_group_by_id`, `read_set_group_by_id`, `read_element_ids`

### Test Coverage

- [ ] **TEST-01**: Julia, Dart, and Python bindings test `is_healthy()` and `path()` methods

## Out of Scope

| Feature | Reason |
|---------|--------|
| Blob subsystem | Placeholder for future implementation |
| `quiver_database_path` dangling pointer fix | Tracked in CONCERNS.md; defer to future milestone |
| Python generator rework (`_c_api.py` vs `_declarations.py`) | Known concern; not blocking v0.5 |
| `_Bool` vs `int` normalization for boolean out-parameters | Cosmetic; defer |
| Performance optimizations (WAL, streaming cursors) | Acceptable for current scale |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| TYPES-01 | TBD | Pending |
| TYPES-02 | TBD | Pending |
| NAME-01 | TBD | Pending |
| NAME-02 | TBD | Pending |
| PY-01 | TBD | Pending |
| PY-02 | TBD | Pending |
| PY-03 | TBD | Pending |
| TEST-01 | TBD | Pending |

**Coverage:**
- v0.5 requirements: 8 total
- Mapped to phases: 0
- Unmapped: 8

---
*Requirements defined: 2026-02-27*
*Last updated: 2026-02-27 after initial definition*
