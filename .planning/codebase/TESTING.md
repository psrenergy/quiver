# Testing Patterns

**Analysis Date:** 2026-02-26

## Test Framework

**C++ Core and C API Tests:**
- Runner: Google Test (GTest) + GMock, configured via `tests/CMakeLists.txt`
- Test discovery: `gtest_discover_tests()` — tests are auto-discovered at build time
- Config: `include(GoogleTest)` in `tests/CMakeLists.txt`

**Python Tests:**
- Runner: pytest >= 8.4.1
- Config: `bindings/python/pyproject.toml` (no `pytest.ini` — uses pyproject `[tool.pytest]` defaults)
- Linter: ruff (configured in `bindings/python/ruff.toml`)

**Julia Tests:**
- Runner: Julia `Test` stdlib with `@testset`
- Entry point: `bindings/julia/test/runtests.jl` — auto-discovers all `test_*.jl` files recursively

**Dart Tests:**
- Runner: `package:test` (Dart's standard test package)
- Entry point: per-file `main()` functions; run via `bindings/dart/test/test.bat`

**Run Commands:**
```bash
# C++ core library tests
./build/bin/quiver_tests.exe

# C API tests
./build/bin/quiver_c_tests.exe

# Julia tests
bindings/julia/test/test.bat

# Dart tests
bindings/dart/test/test.bat

# Python tests
bindings/python/test/test.bat

# All tests
scripts/test-all.bat
```

## Test File Organization

**C++ tests — all in `tests/` (flat, not co-located with source):**
- Naming: `test_{layer}_{topic}.cpp`
  - `test_database_{topic}.cpp` — C++ public API tests
  - `test_c_api_database_{topic}.cpp` — C API tests
  - `test_{entity}.cpp` — entity-level tests (`test_element.cpp`, `test_migrations.cpp`)
- Two compiled test executables:
  - `quiver_tests` — links `quiver` (C++ core)
  - `quiver_c_tests` — links `quiver_c` + `quiver` (C API)
- Non-test executables in `tests/`:
  - `tests/benchmark/benchmark.cpp` — transaction performance benchmark, never auto-run
  - `tests/sandbox/sandbox.cpp` — manual scratch executable

**Python tests — all in `bindings/python/tests/`:**
- Naming: `test_{topic}.py`
- Shared fixtures in `bindings/python/tests/conftest.py`

**Julia tests — all in `bindings/julia/test/`:**
- Naming: `test_{topic}.jl`
- Shared fixture in `bindings/julia/test/fixture.jl` (provides `tests_path()`)

**Dart tests — all in `bindings/dart/test/`:**
- Naming: `{topic}_test.dart`

**Shared SQL schemas — referenced across all test layers:**
- Valid schemas: `tests/schemas/valid/` (`basic.sql`, `collections.sql`, `relations.sql`, `csv_export.sql`, etc.)
- Invalid schemas: `tests/schemas/invalid/` (used in schema validator tests)
- Migration schemas: `tests/schemas/migrations/{version}/up.sql` and `down.sql`
- Issue-specific schemas: `tests/schemas/issues/issue{N}/`

## Test Structure

**C++ — fixture class or standalone TEST():**

Fixture pattern (for tests requiring setup/teardown):
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

Standalone pattern (for in-memory operations, no file cleanup needed):
```cpp
TEST(Database, CreateElementWithScalars) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element element;
    element.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});

    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);
}
```

**C++ section grouping within files:**
```cpp
// ============================================================================
// Vector/Set edge case tests
// ============================================================================
```

**Python — class-based grouping with pytest fixtures:**
```python
class TestCreateElement:
    def test_create_element_returns_id(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "cfg"))
        elem = Element().set("label", "Item1").set("some_integer", 42)
        result = collections_db.create_element("Collection", elem)
        assert isinstance(result, int)
        assert result > 0
```
Top-level functions for simple lifecycle tests, classes for grouped functional tests.

**Julia — nested `@testset`:**
```julia
module TestDatabaseCreate
using Quiver, Test

@testset "Create" begin
    @testset "Scalar Attributes" begin
        db = Quiver.from_schema(":memory:", path_schema)
        Quiver.create_element!(db, "Configuration"; label = "Test Config", integer_attribute = 42)
        Quiver.close!(db)
    end
end
end
```
Each test file is its own module. `runtests.jl` runs with `failfast = true` and `verbose = true`.

**Dart — `group()` + `test()` with try/finally:**
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

## Mocking

**No mocking framework used** anywhere in the test suite. All tests exercise real SQLite databases — either in-memory (`:memory:`) or temporary files.

**Why no mocks:** The library wraps SQLite directly. Tests validate actual database behavior, not interface contracts.

**What to mock:** Nothing in the current pattern. If external I/O is added (network, etc.), introduce mocking then.

## Fixtures and Factories

**C++ — `test_utils.h` macros (`tests/test_utils.h`):**
```cpp
// Path helper — resolves relative to test file location
#define SCHEMA_PATH(relative) quiver::test::path_from(__FILE__, relative)
#define VALID_SCHEMA(name)    SCHEMA_PATH("schemas/valid/" name)
#define INVALID_SCHEMA(name)  SCHEMA_PATH("schemas/invalid/" name)

// Quiet options helper
inline quiver_database_options_t quiet_options() {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    return options;
}
```

**Standard database setup in C++ tests:**
```cpp
// In-memory with schema, logging off — most common pattern
auto db = quiver::Database::from_schema(
    ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});
```

**Python — pytest fixtures in `bindings/python/tests/conftest.py`:**
```python
@pytest.fixture
def schemas_path() -> Path:
    return Path(__file__).resolve().parent.parent.parent.parent / "tests" / "schemas"

@pytest.fixture
def db(valid_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    database = Database.from_schema(str(tmp_path / "test.db"), str(valid_schema_path))
    yield database
    database.close()
```

Available fixtures: `schemas_path`, `valid_schema_path`, `migrations_path`, `collections_schema_path`, `relations_schema_path`, `db`, `collections_db`, `relations_db`, `csv_db`, `all_types_db`, `mixed_time_series_db`.

**Julia — `fixture.jl` helper:**
```julia
function tests_path()
    return joinpath(@__DIR__, "..", "..", "..", "tests")
end
```
Tests build schema paths inline: `joinpath(tests_path(), "schemas", "valid", "basic.sql")`.

## Coverage

**Requirements:** No coverage threshold enforced in CI or config files.

**Dart coverage scripts present:**
- `bindings/dart/test/coverage.bat` and `coverage.sh` — run tests with coverage collection (manual use)

**Python coverage:** Not configured in `pyproject.toml`. No `--cov` flag in test runner scripts.

**View coverage (Dart):**
```bash
bindings/dart/test/coverage.bat
```

## Test Types

**Unit Tests:**
- C++ tests operate at the public `Database` C++ API level (`quiver_tests`) or C API level (`quiver_c_tests`)
- All use real SQLite in-memory databases — fast, isolated, no disk I/O except `TempFileFixture` tests
- Each test creates its own database; no shared state between tests

**Integration Tests:**
- Round-trip tests: create → read back (e.g., `test_database_create.cpp`, `test_c_api_database_csv_import.cpp`)
- FK resolution: create parent → create child with label string → verify resolved integer ID
- CSV export/import: write data → export CSV → re-import → verify round-trip fidelity

**Regression Tests:**
- `tests/test_issues.cpp` — tests for specific reported bugs, named by issue number
- `tests/schemas/issues/` — SQL schemas for issue-specific regression tests

**No E2E or browser tests** — the project is a native library.

## Common Patterns

**Testing thrown exceptions in C++:**
```cpp
// Expect any std::runtime_error
EXPECT_THROW(db.create_element("NonexistentCollection", element), std::runtime_error);

// Expect and inspect the message
try {
    db.create_element("Child", child);
    FAIL() << "Expected std::runtime_error";
} catch (const std::runtime_error& e) {
    EXPECT_STREQ(e.what(), "Failed to resolve label 'Nonexistent Parent' to ID in table 'Parent'");
}

// Expect no throw
EXPECT_NO_THROW(db.describe());
```

**Testing error codes in C API:**
```cpp
// Null argument guard test
EXPECT_EQ(quiver_database_path(nullptr, &db_path), QUIVER_ERROR);

// Success check with ASSERT (stops test if fails)
ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
ASSERT_NE(db, nullptr);
```

**C API resource management in tests:**
```cpp
quiver_element_t* element = nullptr;
ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
quiver_element_set_string(element, "label", "Item 1");
int64_t id = 0;
EXPECT_EQ(quiver_database_create_element(db, "Collection", element, &id), QUIVER_OK);
EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
// ...
quiver_database_close(db);
```

**Testing Python exceptions:**
```python
with pytest.raises(QuiverError, match="closed"):
    db.path()

with pytest.raises(QuiverError):
    db.create_element("BadCollection", Element().set("label", "x"))
```

**Testing Julia exceptions:**
```julia
@test_throws Quiver.DatabaseException Quiver.create_element!(
    db, "Collection"; label = "Item 3", value_int = Int64[])
```

**Testing Dart exceptions:**
```dart
expect(
  () => db.createElement('Configuration', {'label': 'Test Config', 'integer_attribute': 'not an int'}),
  throwsA(isA<DatabaseException>()),
);
```

**Verifying write then read (round-trip pattern — used extensively):**
```cpp
// 1. Create
int64_t id = db.create_element("Configuration", element);
EXPECT_EQ(id, 1);

// 2. Read back
auto labels = db.read_scalar_strings("Configuration", "label");
EXPECT_EQ(labels.size(), 1);
EXPECT_EQ(labels[0], "Config 1");
```

**Testing sorted sets (order not guaranteed):**
```cpp
auto tags = sets[0];
std::sort(tags.begin(), tags.end());
EXPECT_EQ(tags, (std::vector<std::string>{"important", "review", "urgent"}));
```

**Testing time series values:**
```cpp
auto rows = db.read_time_series_group("Collection", "data", id);
EXPECT_EQ(rows.size(), 3);
EXPECT_EQ(std::get<std::string>(rows[0].at("date_time")), "2024-01-01T10:00:00");
EXPECT_DOUBLE_EQ(std::get<double>(rows[0].at("value")), 1.5);
```

**ASSERT vs EXPECT in C++ tests:**
- `ASSERT_*` — use when subsequent test steps depend on the assertion (e.g., `ASSERT_NE(db, nullptr)` before using `db`)
- `EXPECT_*` — use for independent checks that should all run even if one fails

---

*Testing analysis: 2026-02-26*
