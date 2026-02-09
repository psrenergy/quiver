# Testing Patterns

**Analysis Date:** 2026-02-09

## Test Framework

**Runner:**
- Google Test (gtest) v1.x
- Config: `tests/CMakeLists.txt` with `gtest_discover_tests()`

**Assertion Library:**
- Google Test built-in assertions: `EXPECT_*`, `ASSERT_*`
- Mock library: Google Mock (gmock)

**Run Commands:**
```bash
cmake --build build --config Debug                # Build everything
./build/bin/quiver_tests.exe                      # Run C++ core tests
./build/bin/quiver_c_tests.exe                    # Run C API tests (if QUIVER_BUILD_C_API enabled)
scripts/build-all.bat                             # Build all + run all tests
scripts/test-all.bat                              # Run all tests (assumes built)
```

## Test File Organization

**Location:**
- C++ tests: `tests/test_*.cpp` - same directory, co-located with test utilities
- C API tests: `tests/test_c_api_*.cpp` - same directory
- Test schemas: `tests/schemas/valid/*.sql` and `tests/schemas/invalid/*.sql`
- Test utilities: `tests/test_utils.h` - shared header with helper macros

**Naming:**
- Functional tests: `test_<feature>.cpp` (e.g., `test_database_read.cpp`, `test_database_create.cpp`)
- Error tests: `test_<component>_errors.cpp` (e.g., `test_database_errors.cpp`)
- C API equivalents: `test_c_api_<same>.cpp` (e.g., `test_c_api_database_read.cpp`)
- Integration/issue tests: `test_issues.cpp`, `test_migrations.cpp`, `test_lua_runner.cpp`, `test_row_result.cpp`, `test_schema_validator.cpp`

**Structure:**
```
tests/
├── test_database_lifecycle.cpp        # Open, close, move semantics
├── test_database_create.cpp           # Create element operations
├── test_database_read.cpp             # Read scalar/vector/set operations
├── test_database_update.cpp           # Update operations
├── test_database_delete.cpp           # Delete operations
├── test_database_query.cpp            # SQL query operations
├── test_database_relations.cpp        # Relation operations
├── test_database_time_series.cpp      # Time series operations
├── test_database_errors.cpp           # Error handling for all operations
├── test_element.cpp                   # Element builder tests
├── test_lua_runner.cpp                # Lua scripting tests
├── test_migrations.cpp                # Migration lifecycle tests
├── test_row_result.cpp                # Row/Result container tests
├── test_schema_validator.cpp          # Schema validation tests
├── test_c_api_database_*.cpp          # C API equivalents
├── test_c_api_element.cpp
├── test_c_api_lua_runner.cpp
├── test_issues.cpp                    # Issue-specific regression tests
├── test_utils.h                       # Shared test utilities
└── schemas/
    ├── valid/*.sql                    # Valid test schemas (referenced via VALID_SCHEMA macro)
    ├── invalid/*.sql                  # Invalid schemas for error testing
    ├── migrations/                    # Migration test directories
    └── issues/                        # Issue-specific test data
```

## Test Structure

**Test Suite Organization:**

C++ tests use Google Test fixtures to manage setup/teardown:

```cpp
#include <gtest/gtest.h>
#include <quiver/database.h>
#include "test_utils.h"

namespace fs = std::filesystem;

class TempFileFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path = (fs::temp_directory_path() / "quiver_test.db").string();
    }
    void TearDown() override {
        if (fs::exists(path))
            fs::remove(path);
    }
    std::string path;
};

TEST_F(TempFileFixture, OpenFileOnDisk) {
    quiver::Database db(path);
    EXPECT_TRUE(db.is_healthy());
    EXPECT_EQ(db.path(), path);
}

TEST(Database, ReadScalarIntegers) {  // Ungrouped test - no fixture
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e1);

    auto values = db.read_scalar_integers("Configuration", "integer_attribute");
    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], 42);
}
```

**Patterns:**
- `SetUp()`: Create temporary files, initialize resources
- `TearDown()`: Clean up files with `fs::remove()`
- Test initialization: Database created fresh per test
- Logging: Always disable with `{.read_only = 0, .console_level = QUIVER_LOG_OFF}` to reduce noise
- Inline element construction with fluent API: `Element().set("name", value).set("other", value2)`

## Assertion Patterns

**Expectation vs Assertion:**
- `EXPECT_*`: Test continues on failure, collects all failures
- `ASSERT_*`: Test stops on failure, exits early

**Common patterns:**

```cpp
// Equality checks
EXPECT_EQ(actual, expected);          // For values, vectors
EXPECT_NE(a, b);                      // Not equal
EXPECT_STREQ(actual, expected);       // C strings

// Boolean checks
EXPECT_TRUE(condition);
EXPECT_FALSE(condition);

// Exception checks
EXPECT_THROW(expression, exception_type);    // Expect exception thrown
EXPECT_NO_THROW(expression);                 // Expect no exception

// Floating point comparisons
EXPECT_DOUBLE_EQ(actual, expected);   // Fuzzy equality for doubles
EXPECT_FLOAT_EQ(actual, expected);    // For floats

// Container checks
EXPECT_EQ(vector.size(), count);
EXPECT_TRUE(vector.empty());
EXPECT_EQ(vector[0], value);
EXPECT_EQ(vector, expected_vector);   // Full vector equality

// Null pointer checks
EXPECT_NE(ptr, nullptr);
EXPECT_EQ(ptr, nullptr);
```

## Mocking

**Framework:** Google Mock (gmock)

**Usage:** Not heavily used in current tests; tests favor real database instances

**Pattern (if needed):**
```cpp
#include <gmock/gmock.h>

class MockElement {
public:
    MOCK_METHOD(Element&, set, (const std::string&, int64_t));
};
```

**What to Mock:**
- External dependencies (file system, network) - currently no mocking used, real temp files created
- Heavy computation that would slow tests

**What NOT to Mock:**
- Core Database operations - use real in-memory SQLite instances
- Element builder - lightweight, no side effects
- Query results - real database results needed to verify correctness

## Test Fixtures and Factories

**Test Data:**
- Fixtures define schema paths: `VALID_SCHEMA("basic.sql")`, `VALID_SCHEMA("collections.sql")`
- Elements built inline per test with fluent API
- Collection setup: `db.create_element("Configuration", config)` followed by domain elements
- Configuration element required for all schemas (singleton pattern)

**Factories:**
- Element builder: `Element().set("label", "Item 1").set("value", 42)`
- Database factory: `Database::from_schema()`, `Database::from_migrations()`
- Test utilities: `test_utils.h` provides `VALID_SCHEMA()`, `INVALID_SCHEMA()` macros

**Location:**
- Schemas in `tests/schemas/valid/` and `tests/schemas/invalid/`
- Schema paths computed at runtime: `VALID_SCHEMA("basic.sql")` expands to `tests/schemas/valid/basic.sql`
- Macro implementation: `SCHEMA_PATH(relative) quiver::test::path_from(__FILE__, relative)`

## Coverage

**Requirements:** None enforced; coverage measured informally

**Test Counts by Category (as of analysis date):**
- Lifecycle (open, close, move): 35 tests
- Create operations: 12 tests
- Read operations (scalar/vector/set): 39 tests
- Update operations: 28 tests
- Delete operations: 5 tests
- Query operations: 17 tests
- Relations: 7 tests
- Time series: 19 tests
- Error handling: 42 tests
- Element/builder: 22 tests
- Migrations: 21 tests
- Lua runner: 83 tests
- Row/Result: 23 tests
- Schema validation: 31 tests
- Issues/regression: 1 test
- **Total: ~400+ tests across C++ and C API suites**

**View Coverage:**
No automated coverage tool configured; tests are designed for behavioral verification.

## Test Types

**Unit Tests:**
- Scope: Individual methods (e.g., `Element::set()`, `Row::get_integer()`)
- Approach: Direct instantiation, method calls, assertion on return values
- Example: `test_element.cpp` - 22 tests on Element builder fluent API

**Integration Tests:**
- Scope: Database + schema + operations end-to-end
- Approach: Load schema with `from_schema()`, create elements, read back, verify
- Example: `test_database_read.cpp` - read operations with real schema validation

**Query/SQL Tests:**
- Scope: Parameterized and non-parameterized SQL execution
- Approach: Execute raw SQL with parameters, verify results via `Result` container
- Location: `test_database_query.cpp` - 17 tests

**Error Tests:**
- Scope: Exception conditions and error paths
- Approach: `EXPECT_THROW()` for invalid operations
- Location: `test_database_errors.cpp` - 42 tests covering schema errors, validation, etc.

**E2E Tests:**
- Not a separate category; integration tests serve as E2E
- Lua scripting tests in `test_lua_runner.cpp` test full scenario execution

## Common Patterns

**Async Testing:**
Not applicable; SQLite operations are synchronous.

**Error Testing:**
```cpp
TEST(DatabaseErrors, CreateElementNoSchema) {
    quiver::Database db(":memory:", {.read_only = 0, .console_level = QUIVER_LOG_OFF});
    quiver::Element element;
    element.set("label", std::string("Test"));
    EXPECT_THROW(db.create_element("Configuration", element), std::runtime_error);
}

TEST_F(TempFileFixture, FromSchemaFileNotFound) {
    EXPECT_THROW(
        quiver::Database::from_schema(
            ":memory:", "nonexistent/path/schema.sql", {.read_only = 0, .console_level = QUIVER_LOG_OFF}),
        std::runtime_error);
}
```

**Boundary Value Tests:**
- Empty results: vectors/sets with 0 elements
- Null values: `set_null()` for scalar attributes
- Large values: tested via in-memory databases with multiple elements
- Example from `test_database_read.cpp`:
```cpp
TEST(Database, ReadScalarEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    auto integers = db.read_scalar_integers("Collection", "some_integer");
    auto floats = db.read_scalar_floats("Collection", "some_float");
    auto strings = db.read_scalar_strings("Collection", "label");

    EXPECT_TRUE(integers.empty());
    EXPECT_TRUE(floats.empty());
    EXPECT_TRUE(strings.empty());
}
```

**Parameterized Query Tests:**
- SQL with parameter placeholders: `?` for positional parameters
- Type arrays and value arrays passed in parallel
- Example from `test_database_query.cpp`:
```cpp
auto result = db.query_integer(
    "SELECT value FROM MyCollection WHERE id = ?",
    {QUIVER_DATA_TYPE_INTEGER},
    {&element_id});
```

**C API Testing Pattern:**
```cpp
auto options = quiver_database_options_default();
options.console_level = QUIVER_LOG_OFF;
quiver_database_t* db = nullptr;
ASSERT_EQ(quiver_database_open(path.c_str(), &options, &db), QUIVER_OK);

int healthy = 0;
EXPECT_EQ(quiver_database_is_healthy(db, &healthy), QUIVER_OK);
EXPECT_EQ(healthy, 1);

quiver_database_close(db);
```

**Time Series Tests:**
- Dimension column handling: `date_time`, `date_value`, etc.
- Metadata retrieval: `get_time_series_metadata()` includes `dimension_column`
- Files table tests: `has_time_series_files()`, `read_time_series_files()`
- Location: `test_database_time_series.cpp` - 19 tests

## Test Utilities and Helpers

**`test_utils.h` Macros:**
```cpp
// Convenience macros for schema paths
#define SCHEMA_PATH(relative) quiver::test::path_from(__FILE__, relative)
#define VALID_SCHEMA(name) SCHEMA_PATH("schemas/valid/" name)
#define INVALID_SCHEMA(name) SCHEMA_PATH("schemas/invalid/" name)

// Helper functions
inline std::string path_from(const char* test_file, const std::string& relative);
inline quiver_database_options_t quiet_options();
```

**Usage:**
```cpp
auto db = quiver::Database::from_schema(
    ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});
```

**Test Data Schemas:**
- `basic.sql`: Minimal Configuration table with scalar attributes (integer, float, string)
- `collections.sql`: Configuration + Collection with vectors and sets
- `migrations/1/up.sql`, `migrations/1/down.sql`: Test migrations
- `issues/issue52/`: Issue-specific regression test data

## Test Execution Details

**Test Discovery:**
- Google Test auto-discovers tests via `gtest_discover_tests(quiver_tests)`
- Tests run in arbitrary order (no ordering guarantees)
- Each test runs independently with fresh setup/teardown

**Isolation:**
- Temporary file cleanup: `TearDown()` removes database files
- In-memory databases: `:memory:` used for most tests, no cross-test pollution
- Fresh Database instances per test: no shared state

**Test Output:**
- CMake integration with `CTest`
- Verbose output shows test name, PASSED/FAILED, assertions
- Failed assertions include actual vs. expected values

## Special Test Cases

**Migration Tests (`test_migrations.cpp`):**
- Verify migration file reading (up.sql, down.sql)
- Test version comparison and ordering
- Location: `tests/schemas/migrations/` with numbered directories (1/, 2/, 3/)

**Lua Runner Tests (`test_lua_runner.cpp`):**
- Execute Lua scripts with database access
- 83 tests covering script execution, variable binding, error handling
- Lua integration tested via database operations within scripts

**Schema Validator Tests (`test_schema_validator.cpp`):**
- 31 tests validating Configuration table requirements
- Test foreign key constraints, composite keys, NULL/NOT NULL
- Invalid schemas in `tests/schemas/invalid/` used for negative tests

**Issue Regression Tests (`test_issues.cpp`):**
- 1 test for issue #52 (migration validation)
- Schemas stored in `tests/schemas/issues/<issue_number>/`

---

*Testing analysis: 2026-02-09*
