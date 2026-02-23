# Testing Patterns

**Analysis Date:** 2026-02-23

## Test Framework

**Runner:**
- Google Test (gtest) with gmock
- CMake integration via `gtest_discover_tests()` for automatic test discovery
- Config: `tests/CMakeLists.txt` enables via `include(GoogleTest)` and `enable_testing()`

**Assertion Library:**
- Google Test macros: `EXPECT_*`, `ASSERT_*` for condition testing
- `EXPECT_STREQ`, `EXPECT_EQ`, `EXPECT_NE`, `EXPECT_TRUE`, `EXPECT_FALSE`, `EXPECT_THROW` commonly used
- Value comparison with `EXPECT_EQ` for containers like vectors

**Run Commands:**
```bash
cmake --build build --config Debug                # Compile tests
./build/bin/quiver_tests.exe                      # Run C++ core tests
./build/bin/quiver_c_tests.exe                    # Run C API tests
scripts/build-all.bat                             # Build everything + run all tests (Debug)
scripts/build-all.bat --release                   # Build in Release mode + run all tests
scripts/test-all.bat                              # Run all tests (assumes already built)
```

**Standalone Benchmark:**
```bash
./build/bin/quiver_benchmark.exe                  # Transaction performance benchmark (manual)
```

## Test File Organization

**Location:**
- Co-located: `tests/test_{category}.cpp` for C++ tests alongside implementation
- Parallel structure: `tests/test_c_api_database_{category}.cpp` for C API wrappers
- Schemas: `tests/schemas/valid/` and `tests/schemas/invalid/` for SQL fixtures

**Naming:**
- C++ tests: `test_database_{category}.cpp` (e.g., `test_database_lifecycle.cpp`, `test_database_create.cpp`)
- C API tests: `test_c_api_database_{category}.cpp` (e.g., `test_c_api_database_read.cpp`)
- Unit tests (non-database): `test_{entity}.cpp` (e.g., `test_element.cpp`, `test_row_result.cpp`)

**Category Mapping:**
- `test_database_lifecycle.cpp` - open, close, move semantics, options
- `test_database_create.cpp` - create element operations
- `test_database_read.cpp` - read scalar/vector/set operations
- `test_database_update.cpp` - update scalar/vector/set operations
- `test_database_delete.cpp` - delete element operations
- `test_database_query.cpp` - parameterized and non-parameterized SQL queries
- `test_database_relations.cpp` - foreign key relation operations
- `test_database_time_series.cpp` - time series read/update/metadata operations
- `test_database_transaction.cpp` - explicit transaction control (begin/commit/rollback)
- `test_database_csv.cpp` - CSV export operations with options and formatting
- `test_database_errors.cpp` - error conditions and exception patterns
- `test_database_migrations.cpp` - schema migration lifecycle
- C API equivalents: `test_c_api_database_*.cpp` with same category organization

**Structure:**
```
tests/
├── test_database_lifecycle.cpp
├── test_database_create.cpp
├── test_database_read.cpp
├── test_c_api_database_lifecycle.cpp
├── test_c_api_database_create.cpp
├── test_element.cpp
├── test_utils.h                       # Shared fixtures and macros
├── benchmark/
│   └── benchmark.cpp
└── schemas/
    ├── valid/
    │   ├── basic.sql
    │   ├── collections.sql
    │   └── time_series.sql
    └── invalid/
        └── [negative test schemas]
```

## Test Structure

**Fixture Pattern:**
```cpp
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
```

**Test Cases:**
```cpp
// With fixture
TEST_F(TempFileFixture, OpenFileOnDisk) {
    quiver::Database db(path);
    EXPECT_TRUE(db.is_healthy());
    EXPECT_EQ(db.path(), path);
}

// Without fixture (simple unit tests)
TEST(Element, SetInt) {
    quiver::Element element;
    element.set("count", int64_t{42});
    EXPECT_EQ(std::get<int64_t>(element.scalars().at("count")), 42);
}
```

**Test Organization with Dividers:**
- Large test files use comment dividers to section related tests
```cpp
// ============================================================================
// Query string tests
// ============================================================================

TEST(DatabaseQuery, QueryStringReturnsValue) { ... }

// ============================================================================
// Query integer tests
// ============================================================================

TEST(DatabaseQuery, QueryIntegerReturnsValue) { ... }
```

**Naming Convention for Tests:**
- Format: `{SuiteOrFixture}_{OperationOrCondition}` or `{SuiteOrFixture}_{Operation}{Expected}`
- Examples:
  - `TempFileFixture::OpenFileOnDisk` (fixture-based)
  - `DatabaseQuery::QueryStringReturnsValue` (condition-based)
  - `DatabaseErrors::CreateElementNoSchema` (error scenario)
  - `Element::DefaultEmpty` (simple unit test)

## Mocking

**Framework:**
- Google Mock (gmock) included via gtest
- Rarely used in core tests; most tests use real database instances

**When Mocked:**
- Time-dependent operations use mock clocks (if needed)
- File I/O may be stubbed for negative tests
- Generally avoided in favor of real SQLite in-memory databases for integration-style tests

**What NOT to Mock:**
- Database operations (use real SQLite `:memory:` instead)
- File system (use real temp directories via `fs::temp_directory_path()`)
- Core library functionality

## Fixtures and Factories

**Test Data:**
- Schema fixtures in `tests/schemas/valid/` (SQL files)
- Schema validation in `tests/schemas/invalid/`
- Database factory methods in test setup:
```cpp
auto db = quiver::Database::from_schema(
    ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});
```

**Location:**
- Shared utilities in `tests/test_utils.h`:
```cpp
inline std::string path_from(const char* test_file, const std::string& relative) {
    namespace fs = std::filesystem;
    return (fs::path(test_file).parent_path() / relative).string();
}

inline quiver_database_options_t quiet_options() {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    return options;
}

#define VALID_SCHEMA(name) SCHEMA_PATH("schemas/valid/" name)
#define INVALID_SCHEMA(name) SCHEMA_PATH("schemas/invalid/" name)
```

**Usage:**
```cpp
auto db = quiver::Database::from_schema(
    ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});
```

**Element Construction (Builder Pattern):**
```cpp
quiver::Element element;
element.set("label", std::string("Item 1"))
    .set("value_int", std::vector<int64_t>{1, 2, 3})
    .set("value_float", std::vector<double>{1.5, 2.5, 3.5});

int64_t id = db.create_element("Collection", element);
```

## Coverage

**Requirements:** Not explicitly enforced; no coverage target configured in CMake

**View Coverage:**
- Not configured; would require adding lcov/gcov integration to CMakeLists.txt
- Manual coverage analysis possible by test file organization per category

**Coverage by Category:**
- Lifecycle (open, close, migrations): `test_database_lifecycle.cpp` (40+ tests)
- Create operations: `test_database_create.cpp` with scalars, vectors, sets, groups
- Read operations: `test_database_read.cpp` (all scalar/vector/set by ID and batch)
- Update operations: `test_database_update.cpp` (scalar, vector, set replacements)
- Delete operations: `test_database_delete.cpp`
- Error conditions: `test_database_errors.cpp` (null schema, missing attributes, etc.)
- Transactions: `test_database_transaction.cpp` (explicit begin/commit/rollback)
- Queries: `test_database_query.cpp` (parameterized and raw SQL)
- Time series: `test_database_time_series.cpp` (multi-column row operations)
- CSV export: `test_database_csv.cpp` (scalar/group export with formatting options)
- Relations: `test_database_relations.cpp` (foreign key updates and reads)
- Migrations: `test_database_migrations.cpp` (version tracking, apply, pending)

**C API Coverage:**
- Parallel test files for all C API functions
- Error handling via `quiver_get_last_error()` tested
- Output parameter validation tested

## Test Types

**Unit Tests:**
- Scope: Single class/component in isolation
- Approach: Create fixture, call one method, verify output
- Examples: `test_element.cpp` (Element builder), `test_row_result.cpp` (query result parsing)

**Integration Tests:**
- Scope: Full database operations with real SQLite
- Approach: Load schema, create elements, verify read/update/delete
- Examples: Most tests in `test_database_*.cpp` use real database
- Use in-memory `:memory:` SQLite for speed

**C API Tests:**
- Scope: Verify C wrapper layer correctness
- Approach: Call C functions directly, check error codes and output parameters
- Examples: `test_c_api_database_*.cpp` validate FFI marshaling

**E2E Tests:**
- Framework: Not used in core test suite
- Would be: Julia/Dart bindings language tests (separate from C++ suite)

## Common Patterns

**Async Testing:**
Not applicable (synchronous C++ library with no async operations).

**Error Testing:**

**Pattern 1: Exception testing in C++ API**
```cpp
TEST(DatabaseErrors, CreateElementNoSchema) {
    quiver::Database db(":memory:", {.read_only = 0, .console_level = QUIVER_LOG_OFF});
    quiver::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.create_element("Configuration", element), std::runtime_error);
}
```

**Pattern 2: Error code testing in C API**
```cpp
TEST_F(TempFileFixture, OpenNullPath) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_open(nullptr, &options, &db), QUIVER_ERROR);
}
```

**Pattern 3: Precondition error messages**
```cpp
TEST(DatabaseErrors, CreateElementEmptyElement) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});
    quiver::Element element;  // Empty element

    EXPECT_THROW(db.create_element("Configuration", element), std::runtime_error);
    // Error message: "Cannot create_element: element must have at least one scalar attribute"
}
```

**Database State Testing:**

**Pattern: Setup → Verify State → Modify → Re-verify**
```cpp
TEST(Database, CreateElementWithScalars) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Setup: Create element
    quiver::Element element;
    element.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);

    // Verify: Read back values
    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Config 1");
}
```

**Move Semantics Testing:**

**Pattern: Create → Move construct → Verify moved value**
```cpp
TEST_F(TempFileFixture, MoveConstructor) {
    quiver::Database db1(path);
    EXPECT_TRUE(db1.is_healthy());

    quiver::Database db2 = std::move(db1);
    EXPECT_TRUE(db2.is_healthy());
    EXPECT_EQ(db2.path(), path);
    // db1 no longer valid after move
}
```

**Multi-Column Data Testing:**

**Pattern: Set multi-column vectors → Read back → Verify structure**
```cpp
TEST(Database, CreateElementWithVectorGroup) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element element;
    element.set("label", std::string("Item 1"))
        .set("value_int", std::vector<int64_t>{10, 20, 30})
        .set("value_float", std::vector<double>{1.5, 2.5, 3.5});

    int64_t id = db.create_element("Collection", element);

    // Verify both vectors persisted correctly
    auto int_vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(int_vectors[0], (std::vector<int64_t>{10, 20, 30}));

    auto float_vectors = db.read_vector_floats("Collection", "value_float");
    EXPECT_EQ(float_vectors[0], (std::vector<double>{1.5, 2.5, 3.5}));
}
```

**Transaction Testing:**

**Pattern: Explicit transaction control**
```cpp
TEST(DatabaseTransaction, TransactionCommit) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    db.begin_transaction();
    // Perform writes
    quiver::Element e;
    e.set("label", std::string("Test"));
    db.create_element("Configuration", e);

    EXPECT_TRUE(db.in_transaction());
    db.commit();
    EXPECT_FALSE(db.in_transaction());

    // Verify committed
    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels.size(), 1);
}
```

**Time Series Testing:**

**Pattern: Multi-column row operations**
```cpp
// Set multi-column time series rows
std::vector<std::map<std::string, quiver::Value>> rows = {
    {{"date_time", "2024-01-01"}, {"value", 100.0}},
    {{"date_time", "2024-01-02"}, {"value", 110.5}}
};
db.update_time_series_group("Collection", "ts_data", id, rows);

// Read back
auto read_rows = db.read_time_series_group("Collection", "ts_data", id);
EXPECT_EQ(read_rows.size(), 2);
EXPECT_EQ(read_rows[0].at("value"), 100.0);
```

## Test Lifecycle

**SetUp:**
- Create temp database file path or use `:memory:`
- Initialize database with schema or migrations
- Create required initial data (e.g., Configuration entry)

**TearDown:**
- Delete temp database file
- No explicit database close needed (RAII handles cleanup)

**Example Fixture:**
```cpp
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
```

---

*Testing analysis: 2026-02-23*
