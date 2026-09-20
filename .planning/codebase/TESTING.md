# Testing Patterns

**Analysis Date:** 2026-09-17

## Test Framework

### Six Test Suites

The project contains six independent test suites, one per layer:

| Suite | Framework | Run Command | Output |
|-------|-----------|-------------|--------|
| **C++ Core** | GoogleTest | `./build/bin/quiver_tests.exe` | `build/bin/quiver_tests.exe` |
| **C API** | GoogleTest | `./build/bin/quiver_c_tests.exe` | `build/bin/quiver_c_tests.exe` |
| **Julia** | Test.jl | `bindings/julia/test/test.bat` | `bindings/julia/test/` |
| **Dart** | package:test | `bindings/dart/test/test.bat` | `bindings/dart/test/` |
| **Python** | pytest | `bindings/python/tests/test.bat` | `bindings/python/tests/` |
| **JavaScript (Bun)** | bun:test | `bindings/js/test/test.bat` | `bindings/js/test/` |

### Batch Runners

**`scripts/build-all.bat`**
Build everything + run all six test suites (Debug mode):
```bash
scripts/build-all.bat            # Debug
scripts/build-all.bat --release  # Release
```

**`scripts/test-all.bat`**
Run all six test suites (assumes already built):
```bash
scripts/test-all.bat
```

This also includes a CLI smoke test (positive: `--schema` + Lua script → exit 0; negative: `--schema` and `--migrations` together → exit 2).

## C++ Core Tests (`tests/test_*.cpp`)

**Location:** `tests/test_*.cpp`, one file per functional area

Files organized by concern:

### Database Operations
- `test_database_lifecycle.cpp` — open/close/move/options, factory methods
- `test_database_create.cpp` — `create_element`, validation, type checking
- `test_database_read_scalar.cpp` — scalar read operations, element-level reads (`read_element_ids`, `read_element_by_id`, `number_of_elements`)
- `test_database_read_vector.cpp` — vector group reads
- `test_database_read_set.cpp` — set group reads
- `test_database_update.cpp` — `update_element`, `update_relation`, group writers
- `test_database_delete.cpp` — `delete_element`
- `test_database_describe.cpp` — `describe()`, `describe_collection()`, `summarize_collection()`
- `test_database_query.cpp` — parameterized SQL queries
- `test_database_transaction.cpp` — transaction control, dry runs, nesting
- `test_database_time_series_metadata.cpp` — time series metadata
- `test_database_time_series_group.cpp` — time series read/update, validation
- `test_database_time_series_row.cpp` — `upsert_time_series_row`, `read_time_series_row`
- `test_database_time_series_files.cpp` — time series file table operations
- `test_database_csv_export.cpp` — CSV export, foreign key round-tripping
- `test_database_csv_import.cpp` — CSV import, custom date formats
- `test_database_errors.cpp` — error message patterns, precondition failures

### Supporting Types
- `test_element.cpp` — Element builder, fluent API
- `test_row_result.cpp` — Row and Result query-result types
- `test_migrations.cpp` — Migration discovery, up/down validation
- `test_schema_validator.cpp` — Schema convention validation

### Binary Subsystem
- `test_binary_file.cpp` — Binary file I/O, Pimpl implementation
- `test_binary_metadata.cpp` — Metadata serialization, validation
- `test_binary_time_properties.cpp` — TimeFrequency, time dimension properties
- `test_csv_converter.cpp` — Binary ↔ CSV conversion
- `test_iteration.cpp` — `first_dimensions`, `next_dimensions`, dimension traversal

### Expression Subsystem
- `test_expression.cpp` — Expression DAG, operators, aggregation, save

### Lua Bindings
Files mirroring database suite areas, with shared fixtures `LuaRunnerTest`, `LuaSandboxTest`:
- `test_lua_runner_create.cpp` — `db:create_element`, boolean write tests
- `test_lua_runner_read.cpp` — scalar/vector/set reads
- `test_lua_runner_update.cpp` — `db:update_element`, group writers
- `test_lua_runner_delete.cpp` — `db:delete_element`
- `test_lua_runner_query.cpp` — parameterized queries
- `test_lua_runner_return.cpp` — JSON encoding of script return values, table holes → objects
- `test_lua_runner_time_series.cpp` — time series read/write, `nil` holes
- `test_lua_runner_transaction.cpp` — `db:dry_run` (core dry runs live in `test_database_transaction.cpp`)
- `test_lua_runner_csv_export.cpp` — `db:export_csv`
- `test_lua_runner_csv_import.cpp` — `db:import_csv`
- `test_lua_runner_all_types.cpp` — type coverage, fixture `LuaRunnerAllTypesTest`
- `test_lua_runner_fk.cpp` — foreign key resolution, fixture `LuaRunnerFkTest`
- `test_lua_runner_migrations.cpp` — sandboxed `db:validate_migrations`
- `test_lua_runner_errors.cpp` — error messages, unsupported types (functions), boolean rejection in `update_relation`
- `test_lua_binary.cpp` — binary subsystem bindings (file I/O sandboxed to db directory)
- `test_lua_expression.cpp` — expression subsystem bindings (`quiver.*` operators and functions)

### Regression Tests
- `test_issues.cpp` — Issue-numbered regression tests

### Test Helper Infrastructure
- `test_utils.h` — Shared helpers, constants (VALID_SCHEMA, INVALID_SCHEMA paths), fixtures
- `test_lua_runner.h` — Shared `LuaRunnerTest`, `LuaSandboxTest` fixtures, `expect_lua_error` helper

**Example test structure:**
```cpp
TEST(Database, CreateElementWithScalars) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), 
        {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element element;
    element.set("label", std::string("Config 1"))
        .set("integer_attribute", int64_t{42})
        .set("float_attribute", 3.14);

    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);

    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels[0], "Config 1");
}
```

## C API Tests (`tests/test_c_api_*.cpp`)

Mirror the C++ suite with FFI marshaling details:

- `test_c_api_database_*.cpp` — Per-area C API tests (lifecycle, create, read scalar/vector/set, update, delete, transaction, etc.)
- `test_c_api_database_metadata.cpp` — Metadata getters (C API only, no C++ equivalent)
- `test_c_api_database_time_series_nulls.cpp` — Per-cell NULL mask round-trips (C API only, C++ core lacks this file)
- `test_c_api_element.cpp` — Element builder C API
- `test_c_api_lua_runner.cpp` — LuaRunner C API (run returns JSON via `char** out_result`)
- `test_c_api_expression.cpp` — Expression C API
- `test_c_api_binary_*.cpp` — Binary subsystem C API (file, metadata, CSV converter)

**Return code pattern:**
```cpp
quiver_error_t err = quiver_database_create_element(db, "Collection", element, &out_id);
if (err != QUIVER_OK) {
    const char* msg = quiver_get_last_error();
    // handle error
}
```

## Binding Test Suites

All bindings reference the shared schema set (`tests/schemas/`); **never copy schemas into a binding**.

### Julia Tests (`bindings/julia/test/`)

Test framework: **Test.jl** (`runtests.jl` runner)

Files organized per area:
- `test_database_lifecycle.jl` — open/close/move/options, factory methods
- `test_database_create.jl` — `create_element!`, kwargs API
- `test_database_read_scalar.jl` — scalar readers (concrete/nullable), `read_scalar_date_times`, `read_scalar_booleans`
- `test_database_read_vector.jl` — vector group reads, composite helper `readVectorsById`
- `test_database_read_set.jl` — set group reads, composite helper `readSetsById`
- `test_database_update.jl` — `update_element!`, group writers, `_by_label!` forms
- `test_database_delete.jl` — `delete_element!`
- `test_database_query.jl` — parameterized queries
- `test_database_transaction.jl` — transaction blocks, dry runs
- `test_database_time_series_*.jl` — per-concern time series tests (metadata, group, row, files)
- `test_database_csv_*.jl` — CSV export/import
- `test_database_boolean.jl` — boolean convenience readers (`readScalarBooleans`)
- `test_database_date_time.jl` — datetime convenience readers
- `test_migrations.jl` — migration validation
- `test_helper_maps.jl` — relation-map helpers (Julia-only convenience)
- `test_binary_*.jl` — binary subsystem (open_file, metadata, CSV converter)
- `test_expression.jl` — expression subsystem

**Example test:**
```julia
@testset "CreateElement" begin
    @testset "creates element with label" begin
        db = Database.open(schema_path)
        id = create_element!(db, "Configuration", label="cfg")
        @test id > 0
        close!(db)
    end
end
```

### Dart Tests (`bindings/dart/test/`)

Test framework: **package:test**

Files: `*_test.dart` per area
- `database_lifecycle_test.dart`
- `database_create_test.dart`
- `database_read_scalar_test.dart`
- `database_read_vector_test.dart`
- `database_read_set_test.dart`
- `database_update_test.dart`
- `database_delete_test.dart`
- `database_query_test.dart`
- `database_transaction_test.dart`
- `database_time_series_*.test.dart` — per-concern time series tests
- `database_csv_export_test.dart`
- `database_csv_import_test.dart`
- `database_boolean_test.dart` — boolean readers
- `date_time_test.dart` — datetime readers and grammar validation
- `composites_test.dart` — composite helpers (`readVectorGroupById`, `readElementById`)
- `introspection_test.dart` — `describe`, `summarize`, metadata
- `binary_*.dart` — binary subsystem
- `expression_test.dart` — expression subsystem

**Example test:**
```dart
void main() {
  group('Create Scalar Attributes', () {
    test('creates Configuration with all scalar types', () {
      final db = Database.fromSchema(':memory:', schema_path);
      try {
        final id = db.createElement('Configuration', {
          'label': 'Test Config',
          'integer_attribute': 42,
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });
  });
}
```

### Python Tests (`bindings/python/tests/`)

Test framework: **pytest**

Files: `test_*.py` per area
- `test_database_lifecycle.py`
- `test_database_create.py`
- `test_database_read_scalar.py`
- `test_database_read_vector.py`
- `test_database_read_set.py`
- `test_database_update.py`
- `test_database_delete.py`
- `test_database_query.py`
- `test_database_transaction.py`
- `test_database_time_series_*.py` — per-concern time series tests
- `test_database_csv_export.py`
- `test_database_csv_import.py`
- `test_database_boolean.py` — boolean readers
- `test_date_time.py` — datetime readers
- `test_composites.py` — composite helpers
- `test_introspection.py` — describe, summarize
- `test_binary_*.py` — binary subsystem
- `test_expression.py` — expression subsystem
- `conftest.py` — pytest fixtures

**Example test:**
```python
class TestCreateElement:
    def test_create_element_returns_id(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", label="cfg")
        result = collections_db.create_element(
            "Collection", label="Item1", some_integer=42
        )
        assert isinstance(result, int)
        assert result > 0
```

**Fixture pattern (conftest.py):**
```python
@pytest.fixture
def collections_db() -> Generator[Database, None, None]:
    schema_path = SCHEMAS_PATH / "valid" / "collections.sql"
    db = Database.from_schema(":memory:", str(schema_path))
    yield db
    db.close()
```

### JavaScript/Bun Tests (`bindings/js/test/`)

Test framework: **bun:test**

Files: `*.test.ts` with `database-` prefix for Database operations:
- `database-lifecycle.test.ts`
- `database-create.test.ts`
- `database-read-scalar.test.ts`
- `database-read-vector.test.ts`
- `database-read-set.test.ts`
- `database-update.test.ts`
- `database-delete.test.ts`
- `database-query.test.ts`
- `database-transaction.test.ts`
- `database-time-series-*.test.ts` — per-concern time series tests
- `database-csv-export.test.ts`
- `database-csv-import.test.ts`
- `database-boolean.test.ts` — boolean readers
- `composites.test.ts` — composite helpers (`readVectorGroupById`)
- `introspection.test.ts` — describe, summarize, metadata
- `lua-runner.test.ts` — `LuaRunner` operations
- `lua-api-sync.test.ts` — **Sync test** (only non-database file): parses `src/lua_runner.cpp` and verifies `bindings/js/src/lua-api.ts` documents every bound `db:`/`quiver.*` name and the exact `open_libraries` list. This test **requires no native library** and passes on a checkout with no `build/` (uses direct imports to avoid FFI loader).
- `binary-*.ts` — binary subsystem
- `expression.test.ts` — expression subsystem

**Example test:**
```typescript
describe("createElement", () => {
  test("creates element with integer scalar and returns numeric ID", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Item1", some_integer: 42 });
      expect(id).toBeGreaterThan(0);
    } finally {
      db.close();
    }
  });
});
```

## Shared Test Schemas (`tests/schemas/`)

**All suites reference these schemas; never copy them into a binding.**

### Valid Schemas (`valid/`)
- `all_types.sql` — all scalar/vector/set/time-series types
- `basic.sql` — Configuration only
- `collections.sql` — Configuration + Collection with vectors/sets
- `composite_helpers.sql` — Tests for composite read helpers
- `csv_export.sql` — CSV export/import round-tripping
- `describe_multi_group.sql` — Multiple groups per collection
- `mixed_time_series.sql` — Time series with mixed types
- `multi_column_groups.sql` — Vector/set groups with 2+ value columns (ordered so first is not only one — exposes row-count bug)
- `multi_dim_time_series.sql` — Time series with multiple dimension columns
- `multi_time_series.sql` — Multiple time series groups per collection
- `nullable_time_series.sql` — Time series with nullable columns
- `relations.sql` — Foreign key relationships, test data for deliberate column-name fan-out

### Invalid Schemas (`invalid/`)
- `duplicate_attribute_time_series.sql`
- `duplicate_attribute_vector.sql`
- `fk_actions.sql` — Foreign key action validation
- `fk_not_null_set_null.sql`
- `label_not_null.sql`
- `label_not_unique.sql`
- `label_wrong_type.sql`
- `no_configuration.sql`
- `set_no_unique.sql`
- `vector_no_index.sql`

### Migration Schemas (`migrations/`)
- `1/up.sql` + `1/down.sql`
- `2/up.sql` + `2/down.sql`
- `3/up.sql` + `3/down.sql`

### Issue Regressions (`issues/`)
- `issue52/` — Issue-specific migrations
- `issue70/` — Issue-specific migrations

## Special Directories

### `tests/sandbox`

**Intentional scratch target for ad-hoc C++ experiments.** Do NOT delete or "clean up." Only links against the C++ core. Used for exploratory testing and benchmark runs. Built by `build-all.bat` but never run automatically.

## Tests-at-Every-Layer Rule

All public C++ methods are tested in all layers: C API, Julia, Dart, Python, JS, and Lua.

**Documented exceptions (deliberate omissions, not gaps):**

1. **Binary and expression subsystems** — Tested in Julia and Lua only
   - Dart, Python, JS deliberately do not expose them (no FFI consumer)
   - `tests/CLAUDE.md` and root `CLAUDE.md` document these decisions

2. **Lua whole-group readers** — Not bound
   - `read_vector_group_by_id` / `read_set_group_by_id` not exposed
   - Lua reads each column independently through per-column readers
   - Documented in root design decisions and agent-facing Lua reference

3. **Boolean convenience readers** — Julia, Dart, Python, JS only
   - No Lua equivalent (Lua has native booleans; no write-side gate)
   - No C++/C API counterpart (readers are binding-only convenience)
   - Each binding has `test_database_boolean.py` / `database_boolean_test.dart` / etc.

4. **JavaScript datetime wrappers** — JS keeps string-based surface
   - No DateTime class (other bindings have `read_scalar_date_times`)
   - Datetimes remain ISO 8601 strings throughout

## Coverage and CI

**Codecov configuration:** `codecov.yml` defines CI coverage tracking

**CI test jobs:** `.github/workflows/` runs the six suites per platform (Windows, Linux, macOS)

**Release flow:** `scripts/build-all.bat`, `scripts/test-all.bat`, then publish workflows per binding (see `.github/CLAUDE.md`)

---

*Testing analysis: 2026-09-17*
