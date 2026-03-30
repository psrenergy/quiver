# Testing

## Framework

**GoogleTest 1.17.0** for C++ and C API tests. Language-native test frameworks for bindings.

| Layer | Framework | Runner |
|-------|-----------|--------|
| C++ core | GoogleTest | `build/bin/quiver_tests.exe` |
| C API | GoogleTest | `build/bin/quiver_c_tests.exe` |
| Julia | Julia `Test` | `bindings/julia/test/test.bat` |
| Dart | `dart test` | `bindings/dart/test/test.bat` |
| Python | pytest | `bindings/python/test/test.bat` |

## Test Organization

Tests are organized by functional domain, mirroring source file structure.

### C++ Core Tests (37 files)

| File | Domain |
|------|--------|
| `test_database_lifecycle.cpp` | Open, close, move semantics, options |
| `test_database_create.cpp` | Create element operations |
| `test_database_read.cpp` | Read scalar/vector/set |
| `test_database_update.cpp` | Update scalar/vector/set |
| `test_database_delete.cpp` | Delete element |
| `test_database_query.cpp` | Parameterized and plain queries |
| `test_database_time_series.cpp` | Time series CRUD + metadata |
| `test_database_transaction.cpp` | Explicit transaction control |
| `test_database_csv_export.cpp` | CSV export (scalar, group, options) |
| `test_database_csv_import.cpp` | CSV import (round-trip, enum, errors) |
| `test_database_errors.cpp` | Error message patterns |
| `test_element.cpp` | Element builder |
| `test_binary_file.cpp` | Binary file read/write |
| `test_binary_metadata.cpp` | Metadata serialization |
| `test_binary_time_properties.cpp` | Time frequency/properties |
| `test_csv_converter.cpp` | Binary-CSV conversion |
| `test_lua_runner.cpp` | Lua scripting integration |
| `test_schema_validator.cpp` | Schema validation rules |
| `test_migrations.cpp` | Migration execution |
| `test_row_result.cpp` | Result/Row containers |
| `test_issues.cpp` | Regression tests for specific issues |

### C API Tests (17 files)

Mirror C++ tests with `test_c_api_` prefix: lifecycle, create, read, update, delete, metadata, query, time series, transaction, CSV export/import, element, lua runner, plus binary subsystem tests.

### Julia Tests (21 files)
Mirror C++ domain structure + binary subsystem tests.

### Dart Tests (18 files)
Mirror C++ domain structure.

### Python Tests (15 files)
Mirror C++ domain structure. No binary subsystem tests.

## Test Schemas

Shared SQL schemas in `tests/schemas/` used across all test layers.

### Valid Schemas (9)
`all_types.sql`, `basic.sql`, `collections.sql`, `composite_helpers.sql`, `csv_export.sql`, `describe_multi_group.sql`, `mixed_time_series.sql`, `multi_time_series.sql`, `relations.sql`

### Invalid Schemas (10)
Test schema validation error detection: `duplicate_attribute_vector.sql`, `fk_actions.sql`, `label_not_null.sql`, `no_configuration.sql`, etc.

## Test Patterns

### Database Test Setup
Each test creates a temporary database from a schema file:
```cpp
auto db = Database::from_schema(":memory:", "tests/schemas/valid/basic.sql");
```

### C API Test Pattern
```cpp
auto options = quiver_database_options_default();
quiver_database_t* db = nullptr;
ASSERT_EQ(quiver_database_from_schema(":memory:", schema_path, &options, &db), QUIVER_OK);
// ... test operations ...
quiver_database_close(db);
```

### Round-Trip Testing
CSV and binary subsystems use export→import→verify patterns.

## Running Tests

```bash
# All tests
scripts/test-all.bat

# Individual suites
./build/bin/quiver_tests.exe
./build/bin/quiver_c_tests.exe
bindings/julia/test/test.bat
bindings/dart/test/test.bat
bindings/python/test/test.bat

# Benchmark (manual only)
./build/bin/quiver_benchmark.exe
```

## Coverage Gaps

- Python has no binary subsystem tests
- No thread-safety tests for binary write registry
- No multi-process concurrency tests
- Benchmark is standalone, not part of CI
