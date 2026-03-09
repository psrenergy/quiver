# Testing Patterns

**Analysis Date:** 2026-03-08

## Test Framework

### C++ Core and C API Tests

**Runner:**
- GoogleTest v1.17.0 (fetched via CMake FetchContent)
- Config: `tests/CMakeLists.txt`

**Assertion Library:**
- GoogleTest assertions (`EXPECT_EQ`, `ASSERT_EQ`, `EXPECT_THROW`, `EXPECT_NO_THROW`, etc.)
- GMock also linked but not used for mocking (no mock objects — all tests use real SQLite databases)

**Run Commands:**
```bash
cmake --build build --config Debug              # Build tests
./build/bin/quiver_tests.exe                   # C++ core tests
./build/bin/quiver_c_tests.exe                 # C API tests
ctest --build-dir build                        # Run via CTest
scripts/test-all.bat                           # All tests (C++, Julia, Dart, Python)
```

### Julia Tests

**Runner:** Julia's built-in `Test` stdlib
- Entry point: `bindings/julia/test/runtests.jl`
- Auto-discovers test files matching `test_*.jl` via `recursive_include()`

**Run Commands:**
```bash
bindings/julia/test/test.bat                   # Run all Julia tests
julia --project=bindings/julia bindings/julia/test/runtests.jl  test_database_create.jl  # Run single file
```

### Python Tests

**Runner:** pytest v8.4.1+
- Config: `bindings/python/pyproject.toml`
- Test discovery: standard pytest (`test_*.py`)
- Target: Python 3.13+

**Run Commands:**
```bash
bindings/python/test/test.bat                  # Run all Python tests
cd bindings/python && pytest tests/            # Direct pytest invocation
```

### Dart Tests

**Runner:** `package:test`
- Test files: `bindings/dart/test/*_test.dart`

**Run Commands:**
```bash
bindings/dart/test/test.bat                    # Run all Dart tests
bindings/dart/test/coverage.bat                # Run with coverage
```

## Test File Organization

### C++ Tests

**Location:** `tests/` — separate from source code (not co-located)

**Naming pattern:** `test_{area}.cpp` for C++ core, `test_c_api_{area}.cpp` for C API

```
tests/
├── test_utils.h                      # Shared helpers (VALID_SCHEMA macro, quiet_options())
├── test_database_lifecycle.cpp       # open, close, move semantics, options, migrations
├── test_database_create.cpp          # create_element operations
├── test_database_read.cpp            # read scalar/vector/set operations
├── test_database_update.cpp          # update operations
├── test_database_delete.cpp          # delete operations
├── test_database_query.cpp           # parameterized and plain queries
├── test_database_time_series.cpp     # time series read/update/metadata
├── test_database_transaction.cpp     # begin/commit/rollback
├── test_database_csv_export.cpp      # CSV export
├── test_database_csv_import.cpp      # CSV import
├── test_database_errors.cpp          # error paths: no schema, collection not found, etc.
├── test_c_api_database_*.cpp         # C API equivalents of the above
├── test_blob.cpp                     # Blob open/close/read/write
├── test_blob_csv.cpp                 # BlobCSV bin_to_csv, csv_to_bin
├── test_blob_metadata.cpp            # BlobMetadata factories, serialization
├── test_c_api_blob*.cpp              # C API blob equivalents
├── test_element.cpp                  # Element builder
├── test_lua_runner.cpp               # LuaRunner script execution
├── test_migrations.cpp               # Migration/Migrations class
├── test_issues.cpp                   # Regression tests for tracked issues
└── schemas/
    ├── valid/                        # Valid SQL schemas used in tests
    │   ├── basic.sql                 # Single-collection with scalar attributes
    │   ├── collections.sql           # Collections with vectors, sets, time series
    │   ├── relations.sql             # FK relationships (parent/child)
    │   ├── all_types.sql             # All supported data types
    │   ├── csv_export.sql            # CSV export testing schema
    │   └── ...
    ├── invalid/                      # Schemas expected to fail validation
    │   ├── no_configuration.sql
    │   ├── label_not_null.sql
    │   └── ...
    └── migrations/                   # Numbered migration directories (1/, 2/, 3/)
```

### Julia Tests

**Location:** `bindings/julia/test/` — separate test directory

**Naming:** `test_{area}.jl` — mirrors C++ test naming exactly

```
bindings/julia/test/
├── runtests.jl               # Entry point: auto-discovers test_*.jl
├── fixture.jl                # tests_path() helper
├── test_database_create.jl
├── test_database_lifecycle.jl
├── test_database_read.jl
├── test_database_update.jl
├── test_database_delete.jl
├── test_database_query.jl
├── test_database_time_series.jl
├── test_database_transaction.jl
├── test_database_csv_export.jl
├── test_database_csv_import.jl
├── test_database_metadata.jl
├── test_blob.jl
├── test_blob_csv.jl
├── test_blob_metadata.jl
├── test_element.jl
├── test_lua_runner.jl
├── test_schema_validator.jl
├── test_helper_maps.jl       # Julia-specific map helpers
└── test_aqua.jl              # Aqua.jl package quality checks
```

### Python Tests

**Location:** `bindings/python/tests/` — separate test directory

**Naming:** `test_{area}.py` — mirrors C++ naming

```
bindings/python/tests/
├── conftest.py               # All fixtures (db, collections_db, relations_db, etc.)
├── test_database_lifecycle.py
├── test_database_create.py
├── test_database_read_scalar.py
├── test_database_read_vector.py
├── test_database_read_set.py
├── test_database_update.py
├── test_database_delete.py
├── test_database_query.py
├── test_database_time_series.py
├── test_database_transaction.py
├── test_database_csv_export.py
├── test_database_csv_import.py
├── test_database_metadata.py
├── test_element.py
└── test_lua_runner.py
```

### Dart Tests

**Location:** `bindings/dart/test/`

**Naming:** `{area}_test.dart` (Dart convention: suffix not prefix)

```
bindings/dart/test/
├── database_create_test.dart
├── database_lifecycle_test.dart
├── database_read_test.dart
├── database_update_test.dart
├── database_delete_test.dart
├── database_query_test.dart
├── database_time_series_test.dart
├── database_transaction_test.dart
├── database_csv_export_test.dart
├── database_csv_import_test.dart
├── metadata_test.dart
├── element_test.dart
├── lua_runner_test.dart
├── schema_validation_test.dart
└── issues_test.dart
```

## Test Structure

### C++ — GTest Suite Pattern

Tests use free-standing `TEST()` macros (no class fixtures) for simple cases:
```cpp
TEST(Database, CreateElementWithScalars) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element element;
    element.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});

    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);
}
```

Class fixtures with `SetUp`/`TearDown` for tests requiring disk files:
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

**Section organization:** Long test files group related tests with banner comments:
```cpp
// ============================================================================
// Vector/Set edge case tests
// ============================================================================
```

### C++ — Error Testing Pattern

```cpp
// Simple throw check
EXPECT_THROW(db.create_element("NonexistentCollection", element), std::runtime_error);

// Specific message check (when message matters)
try {
    db.create_element("Child", child);
    FAIL() << "Expected std::runtime_error";
} catch (const std::runtime_error& e) {
    EXPECT_STREQ(e.what(), "Failed to resolve label 'Nonexistent Parent' to ID in table 'Parent'");
}
```

### Julia — @testset Pattern

Julia uses nested `@testset` blocks within a wrapping module:
```julia
module TestDatabaseCreate

using Quiver
using Test

include("fixture.jl")

@testset "Create" begin
    @testset "Scalar Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config", integer_attribute = 42)

        Quiver.close!(db)
    end

    @testset "Reject Invalid Element" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test_throws Quiver.DatabaseException Quiver.create_element!(db, "Configuration")

        Quiver.close!(db)
    end
end

end  # module
```

All test modules wrap content in `module Test{Name}` to avoid name conflicts.

### Python — Class-per-Feature Pattern

Tests group into classes by feature area within each file:
```python
class TestCreateElement:
    def test_create_element_returns_id(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        result = collections_db.create_element("Collection", label="Item1", some_integer=42)
        assert isinstance(result, int)
        assert result > 0

    def test_create_with_fk_label(self, relations_db: Database) -> None:
        """FK label resolution: string value for FK column resolves to parent ID."""
        ...

class TestFKResolutionCreate:
    """FK label resolution tests for create_element -- ported from Julia/Dart."""
    ...
```

Error testing in Python:
```python
with pytest.raises(QuiverError):
    relations_db.create_element("Child", label="Child 1", mentor_id=["Nonexistent Parent"])
```

### Dart — group/test Pattern

```dart
void main() {
  final testsPath = path.join(path.current, '..', '..', 'tests');

  group('Create Scalar Attributes', () {
    test('creates Configuration with all scalar types', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        final id = db.createElement('Configuration', {'label': 'Test Config'});
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('rejects wrong type for attribute', () {
      // ...
      expect(
        () => db.createElement('Configuration', {'label': 'Test', 'integer_attribute': 'not an int'}),
        throwsA(isA<DatabaseException>()),
      );
    });
  });
}
```

Dart tests always use `try/finally` for database cleanup — no fixture system.

## Mocking

**No mocking used anywhere.** All tests use real SQLite databases:
- C++ tests: `:memory:` databases (most) or `TempFileFixture` for disk I/O tests
- Julia tests: `:memory:` databases via `Quiver.from_schema(":memory:", path)`
- Python tests: `tmp_path` pytest fixture for disk databases
- Dart tests: `:memory:` databases (most)

GMock is linked in `quiver_tests` but no mock objects exist in the codebase.

## Fixtures and Factories

### C++ — `test_utils.h`

Located at `tests/test_utils.h`. Provides macros and helper functions:

```cpp
namespace quiver::test {

// Get path relative to the test file's directory
inline std::string path_from(const char* test_file, const std::string& relative);

// Default options with logging off for tests
inline quiver_database_options_t quiet_options() {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    return options;
}

}  // namespace quiver::test

// Convenience macros
#define VALID_SCHEMA(name)   SCHEMA_PATH("schemas/valid/" name)
#define INVALID_SCHEMA(name) SCHEMA_PATH("schemas/invalid/" name)
```

Usage: `VALID_SCHEMA("basic.sql")` expands to an absolute path at compile time.

### Julia — `fixture.jl`

Located at `bindings/julia/test/fixture.jl`:
```julia
function tests_path()
    return joinpath(@__DIR__, "..", "..", "..", "tests")
end
```
Schema paths built inline per test: `joinpath(tests_path(), "schemas", "valid", "basic.sql")`.

### Python — `conftest.py`

Located at `bindings/python/tests/conftest.py`. Provides schema-specific database fixtures:

```python
@pytest.fixture
def db(valid_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    database = Database.from_schema(str(tmp_path / "test.db"), str(valid_schema_path))
    yield database
    database.close()

@pytest.fixture
def collections_db(collections_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    database = Database.from_schema(str(tmp_path / "collections.db"), str(collections_schema_path))
    yield database
    database.close()

@pytest.fixture
def relations_db(relations_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    database = Database.from_schema(str(tmp_path / "relations.db"), str(relations_schema_path))
    yield database
    database.close()
```

Available fixtures: `db`, `collections_db`, `relations_db`, `csv_db`, `all_types_db`, `composite_helpers_db`, `mixed_time_series_db`

### Shared SQL Test Schemas

All test schemas located in `tests/schemas/` — shared across all language bindings:
- `tests/schemas/valid/` — schemas used to build test databases
- `tests/schemas/invalid/` — schemas expected to fail on load
- `tests/schemas/migrations/` — numbered migration directories (1/, 2/, 3/)

Schema access path from each binding:
- C++ tests: `VALID_SCHEMA("basic.sql")` → absolute path via `__FILE__`
- Julia tests: `joinpath(tests_path(), "schemas", "valid", "basic.sql")`
- Python tests: `schemas_path / "valid" / "basic.sql"` via `conftest.py` fixture
- Dart tests: `path.join(path.current, '..', '..', 'tests', 'schemas', 'valid', 'basic.sql')`

## Coverage

**C++:** No coverage target defined in CMakeLists; no enforced requirement.

**Julia:** Aqua.jl package quality tests in `bindings/julia/test/test_aqua.jl`.

**Dart:** Coverage via `bindings/dart/test/coverage.bat` using `dart pub global run coverage:test_with_coverage`.

**Python:** No coverage configuration found; no enforced requirement.

## Test Types

**Unit Tests:** All tests are integration-style — they exercise real database operations through the actual API surface. No isolated unit tests of internal methods.

**Integration Tests:** Every test creates a real SQLite database (`:memory:` or disk), performs operations, and asserts outcomes. This is the primary test style across all layers.

**Cross-Layer Parity:** Test cases are mirrored across C++, Julia, Python, and Dart. A feature added in C++ should have equivalent tests in all binding layers. Test names and scenarios match 1:1.

**Regression Tests:** `tests/test_issues.cpp` and `bindings/dart/test/issues_test.dart` — named issue-tracking tests.

**E2E Tests:** Not used.

## Common Patterns

### Test Database Setup (C++)

All tests that use a schema create an in-memory database:
```cpp
auto db = quiver::Database::from_schema(
    ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});
```

C API tests use `quiver_database_options_t` with logging off:
```cpp
auto options = quiver_database_options_default();
options.console_level = QUIVER_LOG_OFF;
quiver_database_t* db = nullptr;
ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
ASSERT_NE(db, nullptr);
// ... test body ...
quiver_database_close(db);
```

### Prerequisites in Tests

Many collections require a `Configuration` element before other creates can succeed:
```cpp
quiver::Element config;
config.set("label", std::string("Test Config"));
db.create_element("Configuration", config);

// Now create the element under test
quiver::Element element;
element.set("label", std::string("Item 1"));
int64_t id = db.create_element("Collection", element);
```

### ASSERT vs EXPECT

- `ASSERT_*` — used for prerequisites that make further testing pointless if they fail (e.g., database opened successfully)
- `EXPECT_*` — used for the actual property under test

```cpp
ASSERT_EQ(quiver_database_from_schema(..., &db), QUIVER_OK);  // fail fast if DB not open
ASSERT_NE(db, nullptr);
EXPECT_EQ(id, 1);                                              // actual assertion
```

### Julia — Explicit Close

Julia finalizers handle cleanup automatically, but tests call `Quiver.close!(db)` explicitly at the end of every test block — resource hygiene is explicit, not deferred.

### Python — Context Manager

Python provides both explicit `close()` and context manager:
```python
# Explicit close (used in fixtures via yield)
database = Database.from_schema(...)
yield database
database.close()

# Context manager
with Database.from_schema(str(tmp_path / "test.db"), str(valid_schema_path)) as db:
    assert db.is_healthy()
```

---

*Testing analysis: 2026-03-08*
