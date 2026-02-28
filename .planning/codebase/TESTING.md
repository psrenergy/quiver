# Testing Patterns

**Analysis Date:** 2026-02-27

## Test Framework

**C++ / C API Runner:**
- GoogleTest (GTest) with GMock
- Config: `tests/CMakeLists.txt`
- Discovery: `gtest_discover_tests()` via CMake CTest integration

**Binding Test Runners:**
- Julia: `Test` stdlib + `@testset` with `recursive_include` in `bindings/julia/test/runtests.jl`
- Dart: `package:test` runner in `bindings/dart/test/`
- Python: `pytest` with fixtures in `bindings/python/tests/`

**Run Commands:**
```bash
# Build all and run all tests
scripts/build-all.bat

# Run individual test executables (after build)
./build/bin/quiver_tests.exe          # C++ core tests
./build/bin/quiver_c_tests.exe        # C API tests

# Binding tests
bindings/julia/test/test.bat          # Julia tests
bindings/dart/test/test.bat           # Dart tests
bindings/python/test/test.bat         # Python tests
```

**Coverage:** No coverage targets or thresholds enforced.

## Test File Organization

**Location:** Tests live in a top-level `tests/` directory (not co-located with source). Binding tests are in `bindings/<lang>/test/` or `bindings/<lang>/tests/`.

**Naming:**
- C++ core: `tests/test_<domain>.cpp` (e.g., `test_database_create.cpp`)
- C API: `tests/test_c_api_<domain>.cpp` (e.g., `test_c_api_database_create.cpp`)
- Julia: `bindings/julia/test/test_<domain>.jl`
- Dart: `bindings/dart/test/<domain>_test.dart`
- Python: `bindings/python/tests/test_<domain>.py`

**Directory layout:**
```
tests/
├── test_database_create.cpp
├── test_database_read.cpp
├── test_database_update.cpp
├── test_database_delete.cpp
├── test_database_lifecycle.cpp
├── test_database_transaction.cpp
├── test_database_query.cpp
├── test_database_csv_export.cpp
├── test_database_csv_import.cpp
├── test_database_time_series.cpp
├── test_database_errors.cpp
├── test_c_api_database_*.cpp          # C API variants
├── test_element.cpp
├── test_migrations.cpp
├── test_schema_validator.cpp
├── test_issues.cpp                    # Regression tests for specific issues
├── test_utils.h                       # Shared helpers and macros
├── benchmark/benchmark.cpp            # Benchmark (not in test suite)
├── sandbox/sandbox.cpp                # Scratch file (not in test suite)
└── schemas/                           # Shared SQL schemas (all tests reference here)
    ├── valid/
    │   ├── basic.sql
    │   ├── collections.sql
    │   ├── relations.sql
    │   ├── all_types.sql
    │   ├── csv_export.sql
    │   ├── composite_helpers.sql
    │   ├── mixed_time_series.sql
    │   └── multi_time_series.sql
    ├── invalid/                        # Invalid schemas for validation tests
    └── migrations/                     # Migration directories (1/, 2/, 3/ with up.sql/down.sql)
```

## Test Structure

**C++ — Flat TEST() for most tests:**
```cpp
TEST(Database, CreateElementWithScalars) {
    // Arrange: create in-memory database from schema, logging off
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Act: perform the operation
    quiver::Element element;
    element.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id = db.create_element("Configuration", element);

    // Assert
    EXPECT_EQ(id, 1);
    auto integers = db.read_scalar_integers("Configuration", "integer_attribute");
    EXPECT_EQ(integers[0], 42);
}
```

**C++ — TEST_F() fixture for lifecycle tests requiring temp files:**
```cpp
class TempFileFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "quiver_test.db").string(); }
    void TearDown() override {
        if (fs::exists(path))
            fs::remove(path);
    }
    std::string path;
};

TEST_F(TempFileFixture, OpenFileOnDisk) {
    quiver::Database db(path);
    EXPECT_TRUE(db.is_healthy());
}
```

**Test naming convention:** `TEST(Suite, DescriptiveActionOrScenario)` — e.g.:
- `TEST(Database, CreateElementWithVector)`
- `TEST(DatabaseErrors, ReadScalarIntegersNoSchema)`
- `TEST(DatabaseTransaction, BeginMultipleWritesRollback)`
- `TEST(DatabaseCApi, CreateElementNullDb)`

**Section headers** used in long test files to group related tests:
```cpp
// ============================================================================
// FK label resolution in create_element (all column types)
// ============================================================================
```

## Schema Access in Tests

**C++ tests** use macros from `tests/test_utils.h`:
```cpp
#define VALID_SCHEMA(name)   quiver::test::path_from(__FILE__, "schemas/valid/" name)
#define INVALID_SCHEMA(name) quiver::test::path_from(__FILE__, "schemas/invalid/" name)

// Usage:
auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), ...);
```

**Binding tests** compute the path to `tests/schemas/` relative to the test file location:
- Julia: `joinpath(@__DIR__, "..", "..", "..", "tests")` via `fixture.jl`
- Dart: `path.join(path.current, '..', '..', 'tests')`
- Python: `Path(__file__).resolve().parent.parent.parent.parent / "tests" / "schemas"` via `conftest.py`

All bindings reference the same central `tests/schemas/` directory.

## Assertion Patterns

**C++ assertions:**
```cpp
EXPECT_EQ(id, 1);                             // equality (non-fatal)
ASSERT_EQ(out_count, 2);                      // equality (fatal, aborts test on failure)
EXPECT_DOUBLE_EQ(values[0], 3.14);            // floating point equality
EXPECT_STREQ(label, "Item 1");                // C string equality
EXPECT_TRUE(result.has_value());              // boolean
EXPECT_THROW(db.create_element(...), std::runtime_error);  // exception thrown
EXPECT_NO_THROW(db.describe());               // no exception
FAIL() << "Expected std::runtime_error";      // explicit failure
```

**Use ASSERT_ when** subsequent assertions depend on the result (e.g., checking a count before indexing).

**C API tests** prefer `ASSERT_EQ(call(), QUIVER_OK)` for setup operations and `EXPECT_EQ(call(), QUIVER_OK)` for the operation under test.

**Error message testing** uses `try/catch` directly to check the exact message:
```cpp
try {
    db.begin_transaction();
    FAIL() << "Expected std::runtime_error";
} catch (const std::runtime_error& e) {
    EXPECT_STREQ(e.what(), "Cannot begin_transaction: transaction already active");
}
```

## Test Database Setup Pattern

Every test that needs data starts with an in-memory database, logging off:

**C++ pattern:**
```cpp
auto db = quiver::Database::from_schema(
    ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

// Schemas with FK dependencies need the Configuration element first:
quiver::Element config;
config.set("label", std::string("Test Config"));
db.create_element("Configuration", config);
```

**C API pattern:**
```cpp
auto options = quiver::test::quiet_options();  // helper in test_utils.h
quiver_database_t* db = nullptr;
ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
ASSERT_NE(db, nullptr);
// ... test body ...
quiver_database_close(db);
```

`quiver::test::quiet_options()` in `tests/test_utils.h` returns default options with `console_level = QUIVER_LOG_OFF`.

## Mocking

**No mocking framework used.** Tests exercise real operations against in-memory SQLite databases (`:memory:`). There are no mock objects or stubs.

**GMock** is linked to the C++ test binary (`GTest::gmock`) but used only for matcher imports (e.g., `#include <gmock/gmock.h>` in `test_database_read.cpp` for advanced matchers), not for creating mocks.

## Memory Management in C API Tests

C API tests manually free all allocated resources using the matching free functions:
```cpp
// Free arrays returned by read operations
quiver_database_free_integer_array(values);
quiver_database_free_float_array(values);
quiver_database_free_string_array(values, count);

// Free time series columnar data
quiver_database_free_time_series_data(col_names, col_types, col_data, col_count, row_count);

// Close database
quiver_database_close(db);

// Destroy element
quiver_element_destroy(element);
```

Pattern: allocate, assert result, use, free — explicit in every test.

## Python Test Patterns

**Fixtures** in `bindings/python/tests/conftest.py` provide databases via pytest yield fixtures:
```python
@pytest.fixture
def collections_db(collections_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    database = Database.from_schema(str(tmp_path / "collections.db"), str(collections_schema_path))
    yield database
    database.close()
```

**Test organization:** Mix of module-level functions and class-based grouping:
```python
# Flat function style (lifecycle tests):
def test_from_schema_creates_database(valid_schema_path: Path, tmp_path: Path) -> None:
    db = Database.from_schema(str(tmp_path / "test.db"), str(valid_schema_path))
    assert db is not None
    db.close()

# Class-based style (create tests):
class TestCreateElement:
    def test_create_element_returns_id(self, collections_db: Database) -> None:
        result = collections_db.create_element("Collection", label="Item1")
        assert isinstance(result, int)
        assert result > 0
```

**Exception testing:**
```python
with pytest.raises(QuiverError, match="closed"):
    db.path()
```

## Julia Test Patterns

Tests use `@testset` with nested `@testset` blocks for grouping:
```julia
@testset "Create" begin
    @testset "Scalar Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config", integer_attribute = 42)
        Quiver.close!(db)
    end
end
```

**Exception testing:**
```julia
@test_throws Quiver.DatabaseException Quiver.create_element!(db, "Collection"; label = "Bad", value_int = Int64[])
```

## Dart Test Patterns

Tests use `group`/`test` blocks with explicit `try/finally` for cleanup:
```dart
group('Create Scalar Attributes', () {
    test('creates Configuration with all scalar types', () {
        final db = Database.fromSchema(':memory:', path.join(testsPath, 'schemas', 'valid', 'basic.sql'));
        try {
            final id = db.createElement('Configuration', {'label': 'Test Config', 'integer_attribute': 42});
            expect(id, greaterThan(0));
        } finally {
            db.close();
        }
    });
});
```

**Exception testing:**
```dart
expect(
    () => db.createElement('Configuration', {'label': 'Test Config', 'integer_attribute': 'not an int'}),
    throwsA(isA<DatabaseException>()),
);
```

## Test Types

**Unit tests (dominant):**
- Single operation against an in-memory database
- Verify output via public read APIs — no internal state inspection

**Integration / round-trip tests:**
- CSV export then import (`test_database_csv_export.cpp`, `test_database_csv_import.cpp`)
- FK label resolution round-trips (`test_database_create.cpp`)
- Migration-then-read sequences (`test_database_lifecycle.cpp`)
- Transaction commit/rollback with mixed write types (`test_database_transaction.cpp`)

**Regression tests:**
- `tests/test_issues.cpp` — test cases for specific GitHub issues
- `tests/schemas/issues/` — schema fixtures for issue-specific tests

**No E2E or load tests** in the test suite. The benchmark (`tests/benchmark/benchmark.cpp`) is a standalone executable built but never run automatically.

## What Is Not Tested

- Internal Pimpl implementation details — tests only use the public API
- Thread safety — no concurrent access tests
- Cross-process database access
- Error recovery after partial writes (partially covered in `ScalarFkResolutionFailureCausesNoPartialWrites` tests)

---

*Testing analysis: 2026-02-27*
