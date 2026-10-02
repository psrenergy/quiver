# Testing Patterns

**Analysis Date:** 2026-10-02

Authoritative: `tests/AGENTS.md`. Rule: tests at every layer (C++, C API, Julia, Dart, Python, JS, Lua); documented exceptions are binary/expression (Julia + Lua only), boolean/DateTime readers (bindings only), Lua-only CSV read/write.

## Test Framework

**Runners:**
- C++ / C API: GoogleTest v1.17.0 (FetchContent), targets `quiver_tests`, `quiver_c_tests` in `tests/CMakeLists.txt` (`gtest_discover_tests`).
- Julia: `Test` stdlib, entry `bindings/julia/test/runtests.jl` (`@testset verbose = true failfast = true`, auto-includes every `test_*.jl`).
- Dart: `package:test`.
- Python: pytest, fixtures in `bindings/python/tests/conftest.py`.
- JS: `bun:test`.

**Assertion Library:** gtest `EXPECT_*`/`ASSERT_*`; Julia `@test`/`@test_throws`; Dart `expect`/matchers; Python `assert`/`pytest.raises`; Bun `expect`.

**Run Commands:**
```bash
scripts/build-all.bat                 # configure + build + run all six suites
scripts/test-all.bat                  # run all six suites (already built)
./build/bin/quiver_tests.exe --gtest_filter='LuaRunner*'
./build/bin/quiver_c_tests.exe
bindings/julia/test/test.bat [file.jl]  # Pkg.test; ARGS[1] = single file
bindings/dart/test/test.bat           # dart test
bindings/js/test/test.bat             # bun test test (adds build/bin to PATH)
bindings/python/tests/test.bat        # uv run pytest tests/
```
Quoted filters break through `cmd //c`; call `pytest`/`dart test`/`bun test` directly for filtered runs.
Release-sensitive Lua tests: `cmake --preset release && cmake --build --preset release`, then `build/release/bin/quiver_tests.exe --gtest_filter='LuaRunner*'`.

## Test File Organization

**Location:** separate test dirs, not co-located: `tests/` (C++/C), `bindings/julia/test/`, `bindings/dart/test/`, `bindings/python/tests/`, `bindings/js/test/`.

**Naming (same area split in every layer):**
- C++: `tests/test_database_<area>.cpp`, `tests/test_lua_runner_<area>.cpp`, `tests/test_binary_*.cpp`, `tests/test_issues.cpp` (issue regressions).
- C API: `tests/test_c_api_database_<area>.cpp`, `tests/test_c_api_*.cpp`.
- Julia: `test_database_<area>.jl`; Dart: `database_<area>_test.dart`; Python: `test_database_<area>.py`; JS: `database-<area>.test.ts` (non-Database files bare: `composites.test.ts`, `lua-api-sync.test.ts`).
- Areas: `create`, `read_{scalar,vector,set}`, `update`, `delete`, `query`, `transaction`, `lifecycle`, `metadata`, `describe`, `csv_export`, `csv_import`, `boolean`, `time_series_{metadata,group,row,files,nulls}` (C++ core has no `nulls`; JS/Python no time-series `metadata`).

**Structure:**
```
tests/
├── test_utils.h          # path_from, quiet_options, SCHEMA_PATH/VALID_SCHEMA/INVALID_SCHEMA
├── test_lua_runner.h     # LuaRunnerTest, LuaSandboxTest, expect_lua_error
├── schemas/{valid,invalid,migrations,issues}/   # shared by ALL suites
├── fixtures/             # byte-exact CSVs (-text)
├── benchmark/            # quiver_benchmark (manual only)
└── sandbox/              # quiver_sandbox scratch target (do not delete)
```

## Test Structure

**C++ core (`tests/test_database_create.cpp`):**
```cpp
TEST(Database, CreateElementWithScalars) {
    auto db = quiver::Database::from_schema(
        ":memory:",
        VALID_SCHEMA("basic.sql"),
        {.read_only = false, .console_level = quiver::LogLevel::Off}
    );
    quiver::Element element;
    element.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);
    auto labels = db.read_scalar_strings("Configuration", "label");   // verify via public reads
    EXPECT_EQ(labels[0], "Config 1");
}
```
Suite names: `Database`, `DatabaseCApi`, `DatabaseDescribe`, `LuaRunnerTest`, `UiMetadataTest`; test names PascalCase describing behavior (`ReadVectorPreservesNullCellsAsNilHoles`).

**C API (`tests/test_c_api_database_create.cpp`):**
```cpp
auto options = quiver::test::quiet_options();
quiver_database_t* db = nullptr;
ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
...
EXPECT_EQ(quiver_element_destroy(element), QUIVER_OK);
quiver_database_close(db);
```
Free C strings with `quiver_database_free_string`, never `delete[]`. Inside an open-for-write binary span use `EXPECT`, not `ASSERT`, so the writer is closed.

**Julia (`bindings/julia/test/test_database_create.jl`):** each file is a `module Test...` with `include("fixture.jl")`, nested `@testset`s, `tests_path()` resolving `tests/schemas`, explicit `Quiver.close!(db)`.
```julia
@testset "Scalar Nothing And SubString" begin
    db = Quiver.from_schema(":memory:", joinpath(tests_path(), "schemas", "valid", "basic.sql"))
    Quiver.create_element!(db, "Configuration"; label = label, string_attribute = nothing)
    @test isnothing(Quiver.read_scalar_string_by_id(db, "Configuration", "string_attribute", 1))
    Quiver.close!(db)
end
```

**Dart (`bindings/dart/test/database_create_test.dart`):** `group(...)` + `test(...)`, `testsPath = path.join(path.current, '..', '..', 'tests')`, `try { ... } finally { db.close(); }`.

**Python (`bindings/python/tests/test_database_create.py`):** classes `TestCreateElement`, methods `test_<behavior>(self, collections_db: Database) -> None`, fixtures from `conftest.py` (`db`, `collections_db`, `relations_db`, `schemas_path`, `migrations_path`) that create file DBs in `tmp_path` and close on teardown.

**JS (`bindings/js/test/database-create.test.ts`):** `describe`/`test`, `SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql")`, `try { ... } finally { db.close(); }`.

**Patterns:**
- Setup: in-memory DB (`":memory:"`) from a shared schema per test; create `Configuration` first when the schema needs it.
- Teardown: explicit close (`try/finally` in Dart/JS, fixture `yield` in Python, `close!` in Julia; RAII in C++).
- Assert through the public read API, not raw SQL.

## Mocking

**Framework:** None. No mocks in any suite.

**Patterns:** real SQLite (in-memory or temp file), real native library; OS-level levers for failure cases (Windows exclusive `CreateFileW` lock / POSIX `chmod 000` in `tests/test_lua_runner_read_csv.cpp`).

**What to Mock:** nothing; use a purpose-built schema in `tests/schemas/valid/` or `invalid/` instead.

**What NOT to Mock:** SQLite, the C API, the FFI loader.

## Fixtures and Factories

**Test Data:**
```cpp
// tests/test_utils.h
#define VALID_SCHEMA(name) SCHEMA_PATH("schemas/valid/" name)
#define INVALID_SCHEMA(name) SCHEMA_PATH("schemas/invalid/" name)
inline quiver_database_options_t quiet_options();   // console_level = QUIVER_LOG_OFF
```
```cpp
// tests/test_lua_runner.h — file-touching Lua tests MUST use LuaSandboxTest
class LuaSandboxTest : public ::testing::Test {   // per-test temp dir, removed in TearDown
    std::string db_path() const { return (sandbox / "test.db").string(); }
};
```

**Location:**
- All `.sql` schemas in `tests/schemas/` — never copy into a binding. Each `invalid/` schema breaks exactly one rule; assert the message, not a bare throw.
- Migrations: `tests/schemas/migrations/{1,2,3}/{up,down}.sql`; issue regressions `tests/schemas/issues/issueNN/`.
- Committed CSV fixtures only in `tests/fixtures/` (byte-exact, BOM/CRLF matter).
- Nothing may be committed under `tests/schemas/ui/` (would affect every `from_migrations` call); build `ui/` trees at runtime via `UiTempTreeFixture` in `tests/test_database_ui_metadata.cpp`.

## Coverage

**Requirements:** none enforced numerically; `codecov.yml` uses `target: auto`, `threshold: 1%` for project and patch; ignores `tests/**`, `build/**`.

**CI (`.github/workflows/ci.yml`):** `cpp-coverage` (lcov -> codecov), `julia-coverage` (`julia-processcoverage`), `dart-coverage` (`package:coverage`, also exercises `QUIVER_UNVERSIONED_SHARED`). Local Dart: `bindings/dart/test/coverage.bat`.

## Test Types

**Unit Tests:** per-area C++ core tests on in-memory DBs; supporting types (`tests/test_element.cpp`, `tests/test_row_result.cpp`, `tests/test_schema_validator.cpp`).

**Integration Tests:** every binding suite is effectively integration (binding -> C API -> core -> SQLite). Lua tests drive `LuaRunner` scripts against a real DB. `bindings/js/test/lua-api-sync.test.ts` parses `src/lua_runner.cpp` to keep `bindings/js/src/lua-api.ts` in sync (needs no native lib).

**E2E Tests:** Not used. `quiver_benchmark` is manual, never in CI.

## Common Patterns

**Async Testing:** Not applicable (all APIs synchronous).

**Error Testing — assert the message, not just the throw:**
```cpp
// C++ (tests/test_database_delete.cpp)
try {
    db.delete_element_by_label("Collection", "No Such Item");
    FAIL() << "expected a throw";
} catch (const std::runtime_error& e) {
    EXPECT_STREQ(e.what(), "Element not found: label 'No Such Item' in collection 'Collection'");
}
```
```cpp
// Lua — EXPECT_THROW passes vacuously on "attempt to call a nil value"
expect_lua_error(lua, script, "Cannot update_element");
```
```julia
@test_throws Quiver.DatabaseException Quiver.delete_element!(db, "Configuration", Int64(999))
```
```dart
expect(() => db.deleteElement('Configuration', 999), throwsA(isA<DatabaseException>()));
```
```python
with pytest.raises(QuiverError, match="'collection'"):
    collections_db.create_element("Collection", label="Item1", collection="x")
```
```typescript
expect(() => db.updateElement("AllTypes", 999, { some_integer: 5 })).toThrow(QuiverError);
```
Unsupported-type tests use a function (`print`), not a boolean (booleans are accepted as INTEGER).

**Adding a feature:** add tests in the matching area file of each layer: `tests/test_database_<area>.cpp`, `tests/test_c_api_database_<area>.cpp`, `tests/test_lua_runner_<area>.cpp`, and the four binding files; add any new schema to `tests/schemas/valid/` and document it in `tests/AGENTS.md`.

---

*Testing analysis: 2026-10-02*
