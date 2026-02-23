# Requirements: Quiver

**Defined:** 2026-02-23
**Core Value:** Consistent, type-safe database operations across multiple languages through a single C++ implementation

## v0.4 Requirements

Requirements for Python bindings and test parity. Each maps to roadmap phases.

### Infrastructure

- [ ] **INFRA-01**: Python package renamed from "margaux" to "quiver" with CFFI dependency, uv as package manager, and correct project metadata
- [ ] **INFRA-02**: CFFI ABI-mode declarations covering the full C API surface (hand-written from Julia c_api.jl reference)
- [ ] **INFRA-03**: DLL/SO loader that resolves libquiver and libquiver_c at runtime with Windows pre-load for dependency chain
- [ ] **INFRA-04**: Error handling via check() function that reads thread-local C API errors and raises Python exceptions
- [ ] **INFRA-05**: String encoding helpers for UTF-8 conversion at the FFI boundary (including nullable strings)
- [ ] **INFRA-06**: Test runner script (test.bat) matching Julia/Dart pattern with PATH setup

### Lifecycle

- [ ] **LIFE-01**: Database.from_schema() and Database.from_migrations() factory methods wrapping C API
- [ ] **LIFE-02**: Database.close() with pointer nullification and context manager support (with statement)
- [ ] **LIFE-03**: Database.path, Database.current_version, Database.is_healthy properties
- [ ] **LIFE-04**: Database.describe() for schema inspection
- [ ] **LIFE-05**: Element builder with set() fluent API for integer, float, string, null, and array types
- [ ] **LIFE-06**: quiver.version() returning library version string

### Read Operations

- [ ] **READ-01**: read_scalar_integers/floats/strings for bulk reads with proper try/finally free
- [ ] **READ-02**: read_scalar_integer/float/string_by_id for single-element reads
- [ ] **READ-03**: read_vector_integers/floats/strings for bulk vector reads with nested array marshaling
- [ ] **READ-04**: read_vector_integers/floats/strings_by_id for single-element vector reads
- [ ] **READ-05**: read_set_integers/floats/strings for bulk set reads
- [ ] **READ-06**: read_set_integers/floats/strings_by_id for single-element set reads
- [ ] **READ-07**: read_element_ids for collection element ID listing
- [ ] **READ-08**: read_scalar_relation for relation reads

### Write Operations

- [ ] **WRITE-01**: create_element wrapping C API with Element parameter and returned ID
- [ ] **WRITE-02**: update_element wrapping C API with Element parameter
- [ ] **WRITE-03**: delete_element wrapping C API
- [ ] **WRITE-04**: update_scalar_integer/float/string for individual scalar updates
- [ ] **WRITE-05**: update_vector_integers/floats/strings for vector replacement
- [ ] **WRITE-06**: update_set_integers/floats/strings for set replacement
- [ ] **WRITE-07**: update_scalar_relation for relation updates

### Transactions

- [ ] **TXN-01**: begin_transaction, commit, rollback, in_transaction methods
- [ ] **TXN-02**: transaction() context manager wrapping begin/commit/rollback

### Metadata

- [ ] **META-01**: get_scalar_metadata returning ScalarMetadata dataclass
- [ ] **META-02**: get_vector_metadata, get_set_metadata, get_time_series_metadata returning GroupMetadata dataclass
- [ ] **META-03**: list_scalar_attributes, list_vector_groups, list_set_groups, list_time_series_groups

### Queries

- [ ] **QUERY-01**: query_string, query_integer, query_float for simple SQL queries
- [ ] **QUERY-02**: query_string_params, query_integer_params, query_float_params with typed parameter marshaling and keepalive

### Time Series

- [ ] **TS-01**: read_time_series_group with void** column dispatch by type
- [ ] **TS-02**: update_time_series_group with columnar typed-array marshaling (including clear via empty arrays)
- [ ] **TS-03**: has_time_series_files, list_time_series_files_columns, read_time_series_files, update_time_series_files

### CSV

- [ ] **CSV-01**: export_csv with CSVExportOptions struct (date format, enum labels)
- [ ] **CSV-02**: import_csv for table data import

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
| (Populated during roadmap creation) | | |

**Coverage:**
- v0.4 requirements: 37 total
- Mapped to phases: 0
- Unmapped: 37

---
*Requirements defined: 2026-02-23*
*Last updated: 2026-02-23 after initial definition*
