# Requirements: Quiver

**Defined:** 2026-02-23
**Core Value:** Consistent, type-safe database operations across multiple languages through a single C++ implementation

## v0.4 Requirements

Requirements for Python bindings and test parity. Each maps to roadmap phases.

### Infrastructure

- [x] **INFRA-01**: Python package renamed from "margaux" to "quiver" with CFFI dependency, uv as package manager, and correct project metadata
- [x] **INFRA-02**: CFFI ABI-mode declarations covering the full C API surface (hand-written from Julia c_api.jl reference)
- [x] **INFRA-03**: DLL/SO loader that resolves libquiver and libquiver_c at runtime with Windows pre-load for dependency chain
- [x] **INFRA-04**: Error handling via check() function that reads thread-local C API errors and raises Python exceptions
- [x] **INFRA-05**: String encoding helpers for UTF-8 conversion at the FFI boundary (including nullable strings)
- [x] **INFRA-06**: Test runner script (test.bat) matching Julia/Dart pattern with PATH setup

### Lifecycle

- [x] **LIFE-01**: Database.from_schema() and Database.from_migrations() factory methods wrapping C API
- [x] **LIFE-02**: Database.close() with pointer nullification and context manager support (with statement)
- [x] **LIFE-03**: Database.path, Database.current_version, Database.is_healthy properties
- [x] **LIFE-04**: Database.describe() for schema inspection
- [x] **LIFE-05**: Element builder with set() fluent API for integer, float, string, null, and array types
- [x] **LIFE-06**: quiver.version() returning library version string

### Read Operations

- [x] **READ-01**: read_scalar_integers/floats/strings for bulk reads with proper try/finally free
- [x] **READ-02**: read_scalar_integer/float/string_by_id for single-element reads
- [x] **READ-03**: read_vector_integers/floats/strings for bulk vector reads with nested array marshaling
- [x] **READ-04**: read_vector_integers/floats/strings_by_id for single-element vector reads
- [x] **READ-05**: read_set_integers/floats/strings for bulk set reads
- [x] **READ-06**: read_set_integers/floats/strings_by_id for single-element set reads
- [x] **READ-07**: read_element_ids for collection element ID listing
- [x] **READ-08**: read_scalar_relation for relation reads

### Write Operations

- [x] **WRITE-01**: create_element wrapping C API with Element parameter and returned ID
- [x] **WRITE-02**: update_element wrapping C API with Element parameter
- [x] **WRITE-03**: delete_element wrapping C API
- [x] **WRITE-04**: update_scalar_integer/float/string for individual scalar updates
- [x] **WRITE-05**: update_vector_integers/floats/strings for vector replacement
- [x] **WRITE-06**: update_set_integers/floats/strings for set replacement
- [x] **WRITE-07**: update_scalar_relation for relation updates

### Transactions

- [x] **TXN-01**: begin_transaction, commit, rollback, in_transaction methods
- [x] **TXN-02**: transaction() context manager wrapping begin/commit/rollback

### Metadata

- [x] **META-01**: get_scalar_metadata returning ScalarMetadata dataclass
- [x] **META-02**: get_vector_metadata, get_set_metadata, get_time_series_metadata returning GroupMetadata dataclass
- [x] **META-03**: list_scalar_attributes, list_vector_groups, list_set_groups, list_time_series_groups

### Queries

- [x] **QUERY-01**: query_string, query_integer, query_float for simple SQL queries
- [x] **QUERY-02**: query_string_params, query_integer_params, query_float_params with typed parameter marshaling and keepalive

### Time Series

- [x] **TS-01**: read_time_series_group with void** column dispatch by type
- [x] **TS-02**: update_time_series_group with columnar typed-array marshaling (including clear via empty arrays)
- [ ] **TS-03**: has_time_series_files, list_time_series_files_columns, read_time_series_files, update_time_series_files

### CSV

- [ ] **CSV-01**: export_csv with CSVExportOptions struct (date format, enum labels)
- [ ] **CSV-02**: import_csv for table data import *(Note: verify C++ implementation exists before binding — may be dropped)*

### Convenience

- [ ] **CONV-01**: read_all_scalars_by_id composing list_scalar_attributes + typed reads
- [ ] **CONV-02**: read_all_vectors_by_id composing list_vector_groups + typed reads
- [ ] **CONV-03**: read_all_sets_by_id composing list_set_groups + typed reads
- [ ] **CONV-04**: DateTime helpers (read_scalar_date_time_by_id, read_vector_date_time_by_id, read_set_date_time_by_id, query_date_time)
- [ ] **CONV-05**: read_vector_group_by_id and read_set_group_by_id multi-column group readers

### Test Parity

- [ ] **TEST-01**: Python test suite matching Julia/Dart test file structure (one file per functional area)
- [ ] **TEST-02**: Audit C++ test coverage and fill gaps where other layers have tests C++ lacks
- [ ] **TEST-03**: Audit C API test coverage and fill gaps (currently only CSV tests exist as dedicated C API tests)
- [ ] **TEST-04**: Audit Julia test coverage and fill gaps where other layers have tests Julia lacks
- [ ] **TEST-05**: Audit Dart test coverage and fill gaps where other layers have tests Dart lacks

## Future Requirements

### Python Packaging

- **PKG-01**: PyPI distribution with wheel builds
- **PKG-02**: Cross-platform binary wheels (Windows, Linux, macOS)

### Python Extensions

- **EXT-01**: LuaRunner Python binding
- **EXT-02**: Async API support for asyncio contexts

## Out of Scope

| Feature | Reason |
|---------|--------|
| LuaRunner in Python | Niche use case, Lua scripting is for embedded scenarios |
| Python <3.13 | Latest only, smallest maintenance surface |
| PyPI packaging | Build from source for now, separate concern |
| CFFI API mode | ABI mode sufficient, no C compiler at install time |
| Auto-generator for declarations | C API is bounded (~200 declarations), hand-writing is simpler |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| INFRA-01 | Phase 1 | Complete |
| INFRA-02 | Phase 1 | Complete |
| INFRA-03 | Phase 1 | Complete |
| INFRA-04 | Phase 1 | Complete |
| INFRA-05 | Phase 1 | Complete |
| INFRA-06 | Phase 1 | Complete |
| LIFE-01 | Phase 1 | Complete |
| LIFE-02 | Phase 1 | Complete |
| LIFE-03 | Phase 1 | Complete |
| LIFE-04 | Phase 1 | Complete |
| LIFE-05 | Phase 1 | Complete |
| LIFE-06 | Phase 1 | Complete |
| READ-01 | Phase 2 | Complete |
| READ-02 | Phase 2 | Complete |
| READ-03 | Phase 2 | Complete |
| READ-04 | Phase 2 | Complete |
| READ-05 | Phase 2 | Complete |
| READ-06 | Phase 2 | Complete |
| READ-07 | Phase 2 | Complete |
| READ-08 | Phase 2 | Complete |
| META-01 | Phase 2 | Complete |
| META-02 | Phase 2 | Complete |
| META-03 | Phase 2 | Complete |
| WRITE-01 | Phase 3 | Complete |
| WRITE-02 | Phase 3 | Complete |
| WRITE-03 | Phase 3 | Complete |
| WRITE-04 | Phase 3 | Complete |
| WRITE-05 | Phase 3 | Complete |
| WRITE-06 | Phase 3 | Complete |
| WRITE-07 | Phase 3 | Complete |
| TXN-01 | Phase 3 | Complete |
| TXN-02 | Phase 3 | Complete |
| QUERY-01 | Phase 4 | Complete |
| QUERY-02 | Phase 4 | Complete |
| TS-01 | Phase 5 | Complete |
| TS-02 | Phase 5 | Complete |
| TS-03 | Phase 5 | Pending |
| CSV-01 | Phase 6 | Pending |
| CSV-02 | Phase 6 | Pending |
| CONV-01 | Phase 6 | Pending |
| CONV-02 | Phase 6 | Pending |
| CONV-03 | Phase 6 | Pending |
| CONV-04 | Phase 6 | Pending |
| CONV-05 | Phase 6 | Pending |
| TEST-01 | Phase 7 | Pending |
| TEST-02 | Phase 7 | Pending |
| TEST-03 | Phase 7 | Pending |
| TEST-04 | Phase 7 | Pending |
| TEST-05 | Phase 7 | Pending |

**Coverage:**
- v0.4 requirements: 49 total
- Mapped to phases: 49
- Unmapped: 0

---
*Requirements defined: 2026-02-23*
*Last updated: 2026-02-23 after roadmap creation — traceability populated*
