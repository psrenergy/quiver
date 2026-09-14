# Testing Patterns

**Analysis Date:** 2026-09-14

## Test Framework

### Runner

**C++ core & C API:** Google Test (gtest) v1.17.0

**Bindings:**
- **Julia:** Custom test runner in `bindings/julia/test/runtests.jl`
- **Dart:** `dart test` (test discovery via `*_test.dart` files)
- **Python:** pytest (via `tests/test.bat`)
- **JavaScript:** Bun test runner (files named `*.test.ts`)

### Run Commands

```bash
# All tests (C++, C API, all bindings)
scripts/test-all.bat

# Build everything + run all tests
scripts/build-all.bat

# Individual suite
./build/bin/quiver_tests.exe              # C++ core
./build/bin/quiver_c_tests.exe            # C API
bindings/julia/test/test.bat              # Julia
bindings/dart/test/test.bat               # Dart
bindings/js/test/test.bat                 # JavaScript
bindings/python/tests/test.bat            # Python
```

### Assertion Library

**C++:** gtest with gmock

**Python/Julia/Dart/JS:** Language-native assertions

## Test File Organization

### File Naming and Location

**C++ core tests** (`tests/test_*.cpp`):
- One file per functional area: `test_database_lifecycle.cpp`, `test_database_create.cpp`, `test_database_read_scalar.cpp`, `test_database_update.cpp`, `test_database_delete.cpp`, `test_database_query.cpp`, `test_database_describe.cpp`, `test_database_time_series_group.cpp`, `test_database_time_series_row.cpp`, `test_database_time_series_files.cpp`, `test_database_time_series_metadata.cpp`, `test_database_transaction.cpp`, `test_database_csv_export.cpp`, `test_database_csv_import.cpp`, `test_database_errors.cpp`
- Supporting types: `test_element.cpp`, `test_row_result.cpp`, `test_migrations.cpp`, `test_schema_validator.cpp`, `test_issues.cpp`
- Lua integration: `test_lua_runner_*.cpp` (per-area split) — `_create`, `_read`, `_update`, `_delete`, `_query`, `_return`, `_time_series`, `_transaction`, `_errors`, `_csv_export`, `_csv_import`, `_all_types`, `_fk`, `_migrations`
- Binary subsystem: `test_binary_file.cpp`, `test_binary_metadata.cpp`, `test_binary_time_properties.cpp`, `test_csv_converter.cpp`, `test_iteration.cpp`
- Expression subsystem: `test_expression.cpp`
- Lua binary/expression bindings: `test_lua_binary.cpp`, `test_lua_expression.cpp`

**C API tests** (`tests/test_c_api_*.cpp`):
- Mirror same areas with `test_c_api_*` prefix
- Notable additions: `test_c_api_database_metadata.cpp`, `test_c_api_database_time_series_nulls.cpp` (per-cell NULL-mask round-trips)
- Notable omissions: no C API errors file (error handling tested through C++ core + binding integration)

**Binding tests:**
- **Julia:** `bindings/julia/test/test_*.jl` (file-per-area pattern)
- **Dart:** `bindings/dart/test/database_*_test.dart`
- **Python:** `bindings/python/tests/test_*.py`
- **JavaScript:** `bindings/js/test/*.test.ts` (files: `database-*.test.ts`, `lua-runner.test.ts`, `lua-api-sync.test.ts`, `composites.test.ts`, `introspection.test.ts`)

### Read/Write Split

Tests are organized by category, with consistent naming across suites:

**Read operations split by type:**
- `test_database_read_scalar.cpp` — scalar bulk reads, `_by_id` variants, element-level reads (`read_element_ids`, `read_element_by_id`, `number_of_elements`)
- `test_database_read_vector.cpp` — vector bulk reads, `_by_id` variants
- `test_database_read_set.cpp` — set bulk reads, `_by_id` variants
- **No `metadata` file for scalars** (metadata is grouped with the time-series subsystem)

**Time series split by operation:**
- `test_database_time_series_metadata.cpp` — `get_time_series_metadata`, schema structure
- `test_database_time_series_group.cpp` — `read_time_series_group`, `update_time_series_group`, validation
- `test_database_time_series_row.cpp` — `read_time_series_row`, `upsert_time_series_row`
- `test_database_time_series_files.cpp` — `has_time_series_files`, `read_time_series_files`, `update_time_series_files`
- **C API adds** `test_c_api_database_time_series_nulls.cpp` for per-cell NULL-mask round-trips (C++ core has no equivalent)

## Test Structure

### Per-Test Fixtures

**C++ core:**

`LuaRunnerTest` fixture (shared base in `test_lua_runner.h`):
```cpp
class LuaRunnerTest : public ::testing::Test {
protected:
    void SetUp() override { 
        collections_schema = VALID_SCHEMA("collections.sql"); 
    }
    std::string collections_schema;
};
```

`LuaSandboxTest` fixture (file-backed database in temp dir for sandboxed file I/O):
```cpp
class LuaSandboxTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create per-test temp directory using test suite + test name
        sandbox = std::filesystem::temp_directory_path() /
                  (std::string("quiver_lua_") + info->test_suite_name() + "_" + info->name());
        std::filesystem::remove_all(sandbox);
        std::filesystem::create_directories(sandbox);
    }
    void TearDown() override { std::filesystem::remove_all(sandbox); }
    
    std::string db_path() const { return (sandbox / "test.db").string(); }
    std::filesystem::path sandbox;
};
```

**Schema loading:**
```cpp
auto db = quiver::Database::from_schema(
    ":memory:", 
    VALID_SCHEMA("collections.sql"), 
    {.read_only = false, .console_level = quiver::LogLevel::Off}
);
```

### Test Patterns

**Basic test structure:**
```cpp
TEST(Database, CreateElementWithScalars) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"));
    
    quiver::Element element;
    element.set("label", std::string("Config 1"))
           .set("integer_attribute", int64_t{42});
    
    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);
    
    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels[0], "Config 1");
}
```

**Error testing with `expect_lua_error` helper:**
```cpp
void expect_lua_error(quiver::LuaRunner& lua, 
                      const std::string& script, 
                      const std::string& substring) {
    try {
        lua.run(script);
        FAIL() << "expected script to throw: " << script;
    } catch (const std::exception& e) {
        EXPECT_NE(std::string(e.what()).find(substring), std::string::npos) 
            << e.what();
    }
}

// Usage:
expect_lua_error(lua, "db:create_element()", "Cannot create_element");
```

**Transaction testing pattern:**
```cpp
TEST(Database, TransactionRollback) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"));
    db.begin_transaction();
    // operations...
    db.rollback();
    // verify state
}
```

**Dry run testing pattern:**
```cpp
TEST(Database, DryRun) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"));
    db.begin_dry_run();
    db.create_element("Configuration", element);
    db.end_dry_run();  // rolls back
    
    auto ids = db.read_element_ids("Configuration");
    EXPECT_TRUE(ids.empty());  // creation was rolled back
}
```

## Mocking

### Framework

**C++:** gtest gmock (`#include <gtest/gmock.h>`)

**Python/Julia/Dart/JS:** Language-native mocking or test doubles (simple stubs, not framework-heavy)

### What to Mock

- **External services:** Not applicable (Quiver is a database wrapper, not a network client)
- **File I/O:** Only in unit tests that don't touch real disk; integration tests use real temp files
- **SQLite:** Never mocked — tests run against real SQLite in `:memory:` databases

### What NOT to Mock

- **Database operations** — always test against real SQLite (`:memory:`)
- **Schema validation** — must test the actual validator against real schemas
- **Type conversion** — must test real value conversion, not mocked type checking
- **Bindings' FFI calls** — integration tests verify the real FFI boundary

## Fixtures and Factories

### Test Data

**Schema fixtures** (shared across all suites):
- Location: `tests/schemas/` — **centralized, never copied into a binding**
- Valid schemas: `tests/schemas/valid/` — all bindings reference these
  - `all_types.sql` — comprehensive type coverage (scalars, vectors, sets, time series)
  - `basic.sql` — simple Configuration + scalar attributes
  - `collections.sql` — Collection with vector/set/time-series groups
  - `multi_column_groups.sql` — vector/set with multiple value columns (tests row-count discovery)
  - `multi_time_series.sql` / `multi_dim_time_series.sql` — time-series variants
  - `nullable_time_series.sql` — NULL cell handling
  - `relations.sql` — foreign key and relation tables
- Invalid schemas: `tests/schemas/invalid/` — schema validation tests
  - `no_configuration.sql` — missing required Configuration table
  - `duplicate_attribute_*.sql` — ambiguous attribute names
  - `fk_*.sql` — foreign key violations
  - `label_*.sql` — label constraint violations
  - `set_no_unique.sql` / `vector_no_index.sql` — missing constraints
- Migrations: `tests/schemas/migrations/` — numbered `1/`, `2/`, `3/` with `up.sql`/`down.sql`
- Issue regressions: `tests/schemas/issues/` — per-issue migrations

**Element builder:**
```cpp
quiver::Element element;
element.set("label", std::string("Item 1"))
       .set("value", int64_t{42})
       .set("tags", std::vector<std::string>{"a", "b"});
```

**Schema loading macro:**
```cpp
#define VALID_SCHEMA(filename) tests/schemas/valid/##filename
auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"));
```

## Coverage

### Requirements

**Enforced:** None explicitly, but all public API paths are exercised

**Target:** >90% on core logic (manual assessment, no CI gate)

**Excluded from coverage:**
- `binary_sandbox` (intentional scratch directory)
- Deprecated code paths
- MSVC-only or platform-specific branches (tested on CI matrix)

### Generating Coverage Reports

C++ coverage not automated in CI; can be generated locally with gcov or llvm-cov.

## Test Types

### Unit Tests

**Scope:** Single function/method in isolation

**Approach:**
- In-memory database (`:memory:`)
- Minimal setup (one fixture, one schema)
- One assertion focus per test

**Examples:**
- `CreateElementWithScalars` — test element creation
- `UpdateElementThrowsOnMismatchedType` — test type validation
- `ReadScalarIntegersPreservesNulls` — test NULL handling

### Integration Tests

**Scope:** Multiple components working together

**Approach:**
- Real SQLite in `:memory:` or temp file
- Full workflows (create → read → update → delete)
- Cross-binding parity tests (same schema, all languages)

**Examples:**
- `TransactionCommitPersistsChanges` — integration of begin/commit/write/read
- `LuaRunnerExecutesQueryWithTransaction` — Lua + C++ integration
- `ExportCsvAndImportCsvRoundTrip` — CSV round-trip parity

### E2E Tests

**Scope:** Entire system end-to-end

**Approach:**
- CLI smoke test in `scripts/test-all.bat` (step 7)
  - Positive run: `quiver_cli --schema <path> --run <lua_file>` → exit 0
  - Negative run: `quiver_cli --schema <path> --migrations <path>` → exit 2 (invalid combination)
- No explicit E2E test files; CLI acts as the E2E harness

### Special Test Cases

**Boolean readers** (Dart, Python, Julia, JS only):
- `test_database_boolean.jl`, `database_boolean_test.dart`, `test_database_boolean.py`, `database-boolean.test.ts`
- Over `valid/all_types.sql` (`some_integer` scalar, `count_value` vector, `code` set)
- Tests strict 0/1 conversion, rejection of other integers
- **Release-sensitive tests** (three mixed-array tests in Lua):
  - `CreateElementMixedIntegerAndBooleanArray`
  - `CreateElementMixedFloatAndBooleanArray`
  - `CreateElementArrayCellTypeMismatchThrows`
  - These fail silently in Release (gtest runs in Debug with `SOL_SAFE_GETTER=ON`, but Release has it OFF)
  - **Must test Release build** with `--gtest_filter='LuaRunner*'` to verify boolean coercion

**DateTime readers** (Julia, Dart, Python only):
- Wrapping string readers with date parsing
- Test positional NULL preservation, empty reads, element-without-group-rows omission
- Validate against core's DATE_TIME grammar (`YYYY-MM-DD` or `YYYY-MM-DDTHH:MM:SS`)

**Lua sandbox** (file operations):
- Every file-touching Lua test uses `LuaSandboxTest`
- Verifies paths are rejected if they escape the database directory
- Tests `db:open_file`, `db:bin_to_csv`, `db:csv_to_bin`, `db:export_csv`, `db:import_csv`, `db:validate_migrations`, `expr:save`
- All use relative paths and temp directories

## Common Patterns

### Async Testing

Not applicable (Quiver is synchronous).

### Error Testing

**Pattern 1 — C++ with EXPECT_THROW:**
```cpp
EXPECT_THROW(
    db.update_element("Collection", 999, element),
    std::runtime_error
);
```

**Pattern 2 — C++ with exception message assertion:**
```cpp
try {
    db.create_element("Collection", element);
    FAIL() << "expected exception";
} catch (const std::runtime_error& e) {
    EXPECT_THAT(e.what(), 
                HasSubstr("Cannot create_element: element must have at least one scalar attribute"));
}
```

**Pattern 3 — Lua error testing:**
```cpp
expect_lua_error(lua, "db:create_element('Collection', {})", 
                 "Cannot create_element");
```

**Pattern 4 — Python error testing:**
```python
with self.assertRaises(QuiverError) as ctx:
    db.update_element("Collection", 999, value=42)
self.assertIn("Element not found", str(ctx.exception))
```

**Pattern 5 — Dart error testing:**
```dart
expect(
  () => db.updateElement("Collection", 999, Element()..set("value", 42)),
  throwsA(isA<QuiverError>()),
);
```

### Parametrized Tests

**C++ gtest:**
```cpp
class DatabaseReadScalarTest : public ::testing::TestWithParam<std::string> {
protected:
    std::string schema = VALID_SCHEMA(GetParam());
};

INSTANTIATE_TEST_SUITE_P(AllTypes, DatabaseReadScalarTest,
    ::testing::Values("basic.sql", "collections.sql", "all_types.sql"));

TEST_P(DatabaseReadScalarTest, ReadEmptyReturnsEmptyVector) {
    // tests run with each parameter
}
```

### Setup/Teardown

**C++ per-test:**
```cpp
TEST_F(LuaRunnerTest, ReadAfterCreate) {
    // SetUp() already ran, schema is loaded
    // TearDown() runs after
}
```

**C++ per-suite:**
```cpp
class DatabaseTransactionTest : public ::testing::Test {
    static void SetUpTestSuite() { /* called once before all tests */ }
    static void TearDownTestSuite() { /* called once after all tests */ }
};
```

**Temp directory cleanup:**
```cpp
void TearDown() override { 
    std::filesystem::remove_all(sandbox);  // cleans up after each test
}
```

## Release Build Sensitivity

### SOL_SAFE_GETTER Configuration

**C++ Lua binding setting** in `src/CMakeLists.txt`:
- `SOL_SAFE_NUMERICS=1` — turns on `SOL_NUMBER_PRECISION_CHECKS`, load-bearing for integer-vs-float type dispatch
- `SOL_SAFE_FUNCTION=1` — validates function calls
- `SOL_SAFE_GETTER` — **defaults ON in Debug, OFF in Release**

**Impact:**
- **Debug:** Unchecked Lua value reads default to safe behavior (throw on type mismatch)
- **Release:** Unchecked reads silently coerce (int → 0, float → 0.0, string → `""`)

**Load-bearing tests that only fail in Release:**
- `CreateElementMixedIntegerAndBooleanArray` (mixed {1, true} silently stores 0 in release)
- `CreateElementMixedFloatAndBooleanArray` (mixed {1.5, true} silently stores 0.0 in release)
- `CreateElementArrayCellTypeMismatchThrows` (wrong type silently coerces in release)

**How to test:**
```bash
# Build Release
cmake --build build --config Release

# Run Lua tests explicitly in Release
./build/bin/quiver_tests.exe --gtest_filter='LuaRunner*'
```

The fix uses `lua_table_to_vector<T>` which validates **every cell**, not just the dispatch cell, making it work in both Debug and Release.

## CI/CD Test Execution

`scripts/test-all.bat` runs in this order:

1. C++ core tests (`./build/bin/quiver_tests.exe`)
2. C API tests (`./build/bin/quiver_c_tests.exe`)
3. Julia tests (`bindings/julia/test/test.bat`)
4. Dart tests (`bindings/dart/test/test.bat`)
5. JavaScript tests (`bindings/js/test/test.bat`)
6. Python tests (`bindings/python/tests/test.bat`)
7. CLI smoke test (positive + negative runs)

All six language suites + CLI must pass for a successful test run. Individual suite failures are reported; CI exits with error code if any suite fails.

---

*Testing analysis: 2026-09-14*
