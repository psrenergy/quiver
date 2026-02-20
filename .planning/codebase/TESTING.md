# Testing Patterns

**Analysis Date:** 2026-02-20

## Test Framework

**C++ / C API Runner:**
- GoogleTest (GTest) with GMock
- Version: fetched via CMake FetchContent (see `cmake/Dependencies.cmake`)
- Config: `tests/CMakeLists.txt`
- Test discovery: `gtest_discover_tests()` per executable

**Julia Runner:**
- Built-in `Test` stdlib with `@testset`
- Entry point: `bindings/julia/test/runtests.jl`
- Recursive auto-discovery of `test_*.jl` files

**Dart Runner:**
- `package:test` framework
- Entry point: `bindings/dart/test/`
- Files named `*_test.dart`

**Run Commands:**
```bash
# C++ core tests
./build/bin/quiver_tests.exe

# C API tests
./build/bin/quiver_c_tests.exe

# Julia tests
bindings/julia/test/test.bat

# Dart tests
bindings/dart/test/test.bat

# All tests + build
scripts/test-all.bat

# Build then test
scripts/build-all.bat
```

## Test File Organization

**C++ tests** — in `tests/`, separate from source:
- `test_{domain}.cpp` — C++ API tests (e.g., `test_database_create.cpp`)
- `test_c_api_{domain}.cpp` — C API tests (e.g., `test_c_api_database_create.cpp`)
- `test_utils.h` — shared utilities (path helpers, quiet options)

**Compiled into two separate executables:**
- `quiver_tests` — links against `quiver` (C++ library)
- `quiver_c_tests` — links against `quiver_c` and `quiver`

**Julia tests** — in `bindings/julia/test/`:
- `test_{domain}.jl` files auto-discovered by `runtests.jl`
- `fixture.jl` — shared helper functions

**Dart tests** — in `bindings/dart/test/`:
- `{domain}_test.dart` naming convention

**Test schemas** (shared across all test layers):
- Located in `tests/schemas/valid/` and `tests/schemas/invalid/`
- Referenced from all C++, Julia, and Dart tests via relative paths
- Valid: `basic.sql`, `collections.sql`, `relations.sql`, `multi_time_series.sql`, `mixed_time_series.sql`
- Invalid: one file per validation rule (e.g., `no_configuration.sql`, `label_not_null.sql`)
- Issues: regression schemas in `tests/schemas/issues/issue{N}/`

## Test Structure

**C++ flat tests (no fixture):** Used when each test sets up its own in-memory database.
```cpp
TEST(Database, CreateElementWithScalars) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element element;
    element.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});

    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);

    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels[0], "Config 1");
}
```

**C++ fixture-based tests:** Used when setup/teardown is shared (file-based DBs, migration paths).
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

**Section markers** divide logical groups within a test file:
```cpp
// ============================================================================
// Read scalar tests
// ============================================================================
```

**Julia testset pattern:**
```julia
module TestDatabaseCreate
using Test
using Quiver
include("fixture.jl")

@testset "Create" begin
    @testset "Scalar Attributes" begin
        db = Quiver.from_schema(":memory:", path_to_schema)
        Quiver.create_element!(db, "Configuration"; label = "Test", integer_attribute = 42)
        Quiver.close!(db)
    end
end
end  # module
```

**Dart test pattern:**
```dart
group('Create Scalar Attributes', () {
  test('creates Configuration with all scalar types', () {
    final db = Database.fromSchema(':memory:', schemaPath);
    try {
      final id = db.createElement('Configuration', {'label': 'Test Config'});
      expect(id, greaterThan(0));
    } finally {
      db.close();
    }
  });
});
```

## Database Setup in Tests

**In-memory databases** are the default — fast, no cleanup:
```cpp
auto db = quiver::Database::from_schema(
    ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});
```

**Logging always suppressed in tests:**
- C++: `{.read_only = 0, .console_level = QUIVER_LOG_OFF}`
- Via helper: `quiver::test::quiet_options()` defined in `tests/test_utils.h`
- Julia/Dart: logging off by default when no options specified

**Schema path macros** (defined in `tests/test_utils.h`):
```cpp
#define VALID_SCHEMA(name) SCHEMA_PATH("schemas/valid/" name)
#define INVALID_SCHEMA(name) SCHEMA_PATH("schemas/invalid/" name)
// Usage:
VALID_SCHEMA("basic.sql")       // → absolute path to tests/schemas/valid/basic.sql
INVALID_SCHEMA("no_configuration.sql")
```

All schema paths are computed at runtime relative to `__FILE__`, so tests work regardless of working directory.

## Assertion Patterns

**C++ value checks:**
```cpp
EXPECT_EQ(values[0], 42);              // integer equality
EXPECT_DOUBLE_EQ(values[0], 3.14);    // float equality (ULP tolerance)
EXPECT_EQ(labels[0], "Config 1");     // string equality
EXPECT_EQ(count, 2u);                 // size_t comparison (note 'u' suffix)
EXPECT_TRUE(result.has_value());      // optional has value
EXPECT_FALSE(result.has_value());     // optional is empty
EXPECT_TRUE(vec.empty());             // container is empty
EXPECT_STREQ(c_str, "expected");      // C string equality (C API tests)
```

**C++ throw checks:**
```cpp
EXPECT_THROW(db.create_element("Nonexistent", element), std::runtime_error);
EXPECT_THROW(bad_operation(), std::exception);  // broader catch when type uncertain
EXPECT_NO_THROW(db.from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts));
```

**ASSERT vs EXPECT:**
- `ASSERT_*` used to guard preconditions (stops test if fails): `ASSERT_EQ(err, QUIVER_OK)`, `ASSERT_NE(db, nullptr)`
- `EXPECT_*` used for actual behavioral assertions (continues on failure)

**C API result check pattern:**
```cpp
quiver_database_t* db = nullptr;
ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
ASSERT_NE(db, nullptr);
// ... use db ...
quiver_database_close(db);
```

**Time series multi-column check pattern (C API):**
```cpp
char** out_col_names = nullptr;
int* out_col_types = nullptr;
void** out_col_data = nullptr;
size_t out_col_count = 0, out_row_count = 0;
ASSERT_EQ(quiver_database_read_time_series_group(db, "Collection", "data", id,
    &out_col_names, &out_col_types, &out_col_data, &out_col_count, &out_row_count), QUIVER_OK);
EXPECT_EQ(out_row_count, 3);
auto** dates = static_cast<char**>(out_col_data[0]);
EXPECT_STREQ(dates[0], "2024-01-01T10:00:00");
quiver_database_free_time_series_data(out_col_names, out_col_types, out_col_data, out_col_count, out_row_count);
```

## Mocking

**Framework:** GMock (included in C++ tests via `<gmock/gmock.h>`)

**Actual usage:** GMock is linked but not actively used for mocks — all tests use real database instances. The matcher library is used for collection assertions:
```cpp
#include <gmock/gmock.h>
// Used implicitly for EXPECT_THAT matchers in read tests
```

No mock objects are defined. The architecture makes mocking unnecessary: tests use in-memory SQLite databases (`:memory:`) which are fast and isolated.

## Fixtures and Factories

**Test data approach:** Created inline per test via `Element` builder or C API element calls. No shared fixture data files.

**C++ element setup pattern:**
```cpp
quiver::Element element;
element.set("label", std::string("Config 1"))
       .set("integer_attribute", int64_t{42})
       .set("float_attribute", 3.14);
int64_t id = db.create_element("Configuration", element);
```

**Prerequisite data pattern:** Many tests require a `Configuration` row first (schema constraint). Always created explicitly:
```cpp
quiver::Element config;
config.set("label", std::string("Test Config"));
db.create_element("Configuration", config);
// Now create the actual test data in another collection
```

**Julia element setup:**
```julia
Quiver.create_element!(db, "Configuration"; label = "Test Config")
Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
```

**Dart element setup:**
```dart
db.createElement('Configuration', {'label': 'Test Config'});
db.createElement('Collection', {'label': 'Item 1', 'value_int': [1, 2, 3]});
```

## Coverage

**Requirements:** No enforced coverage threshold in CI config.

**Dart coverage script exists:**
```bash
bindings/dart/test/coverage.bat
bindings/dart/test/coverage.sh
```

**C++ coverage:** Not configured in CMake.

## Test Types

**Unit Tests:**
- `test_element.cpp` — tests `Element` class in isolation (no database)
- `test_row_result.cpp` — tests `Row` and `Result` types in isolation
- `tests/test_migrations.cpp` — tests `Migration`/`Migrations` classes in isolation

**Integration Tests (majority):**
- All `test_database_*.cpp` and `test_c_api_database_*.cpp` files
- Each test creates a real in-memory database and exercises the full stack

**Regression Tests:**
- `test_issues.cpp` — exercises specific bug-fix scenarios
- Schema files in `tests/schemas/issues/issue{N}/` document the regression

**Schema Validation Tests:**
- `test_schema_validator.cpp` — verifies that invalid schemas are rejected
- Each invalid schema file in `tests/schemas/invalid/` corresponds to a test case

**Binding Tests:**
- Julia: `bindings/julia/test/test_*.jl` — mirror C++ test coverage for Julia API
- Dart: `bindings/dart/test/*_test.dart` — mirror C++ test coverage for Dart API
- Bindings share `tests/schemas/` via relative paths

**E2E Tests:**
- `test_lua_runner.cpp` and `test_c_api_lua_runner.cpp` — Lua scripting integration
- Exercises Lua scripts that invoke database operations and verify results via C++ API

## Common Patterns

**Testing error conditions:**
```cpp
// Collection does not exist
EXPECT_THROW(db.create_element("NonexistentCollection", element), std::runtime_error);

// Null pointer (C API)
EXPECT_EQ(quiver_database_create_element(nullptr, "Plant", element, &id), QUIVER_ERROR);
EXPECT_EQ(quiver_database_create_element(db, nullptr, element, &id), QUIVER_ERROR);
```

**Testing optional results:**
```cpp
auto val = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id);
EXPECT_TRUE(val.has_value());
EXPECT_EQ(*val, 100);

auto missing = db.query_string("SELECT x FROM T WHERE 1=0");
EXPECT_FALSE(missing.has_value());
```

**Testing time series data (C++ API):**
```cpp
std::vector<std::map<std::string, quiver::Value>> rows = {
    {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.5}},
    {{"date_time", std::string("2024-01-01T11:00:00")}, {"value", 2.5}}};
db.update_time_series_group("Collection", "data", id, rows);
auto result = db.read_time_series_group("Collection", "data", id);
EXPECT_EQ(std::get<std::string>(result[0]["date_time"]), "2024-01-01T10:00:00");
EXPECT_DOUBLE_EQ(std::get<double>(result[0]["value"]), 1.5);
```

**Memory cleanup in C API tests:**
```cpp
// Every allocated output must be freed after use
quiver_database_free_integer_array(values);
quiver_database_free_string_array(out_values, out_count);
quiver_database_free_time_series_data(col_names, col_types, col_data, col_count, row_count);
quiver_database_close(db);
quiver_element_destroy(element);
```

**Testing set ordering (sorted before comparison):**
```cpp
auto sets = db.read_set_strings("Collection", "tag");
auto tags = sets[0];
std::sort(tags.begin(), tags.end());  // sets have no guaranteed order
EXPECT_EQ(tags, (std::vector<std::string>{"important", "review", "urgent"}));
```

---

*Testing analysis: 2026-02-20*
