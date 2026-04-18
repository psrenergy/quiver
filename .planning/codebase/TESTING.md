# TESTING

**Analysis Date:** 2026-04-18

## Test Framework

**C++ / C API**
- Framework: GoogleTest (GTest) + GoogleMock.
- Configured in `tests/CMakeLists.txt` (line 1: `include(GoogleTest)`). Targets link `GTest::gtest_main` and `GTest::gmock` (lines 27-33, 69-75).
- Test discovery via `gtest_discover_tests(quiver_tests WORKING_DIRECTORY $<TARGET_FILE_DIR:quiver_tests>)` (lines 44-46, 88-90) — registers all tests with CTest.
- No SQLite/Lua mocking: tests run against real SQLite via `:memory:` or temp-path databases. GMock is linked but not actively used.

**Binding frameworks**

| Binding | Framework | Entry point |
|---------|-----------|-------------|
| Julia | `Test` stdlib (`@testset`, `@test`) | `bindings/julia/test/runtests.jl` |
| Dart | `package:test` (`group`, `test`, `expect`) | per-file `main()` in `bindings/dart/test/*_test.dart` |
| Python | `pytest` with fixtures | `bindings/python/tests/test_*.py` |
| JS / Deno | `Deno.test` + `jsr:@std/assert` | `bindings/js/test/*.test.ts` |

## Test File Organization

**All tests mirror source file names.** Test count in parent table: 322+ TEST/TEST_F invocations across 10+ files sampled.

C++ core tests (`tests/test_*.cpp`) mirror `src/database_*.cpp`: `test_database_lifecycle.cpp`, `test_database_create.cpp`, `test_database_read.cpp`, `test_database_update.cpp`, `test_database_delete.cpp`, `test_database_query.cpp`, `test_database_time_series.cpp`, `test_database_transaction.cpp`, `test_database_csv_export.cpp`, `test_database_csv_import.cpp`, `test_database_errors.cpp`, `test_element.cpp`, `test_migrations.cpp`, `test_row_result.cpp`, `test_schema_validator.cpp`, `test_lua_runner.cpp`, `test_issues.cpp`.

Binary subsystem tests: `test_binary_file.cpp`, `test_binary_metadata.cpp`, `test_binary_time_properties.cpp`, `test_csv_converter.cpp`.

C API tests mirror with `test_c_api_` prefix: `test_c_api_database_lifecycle.cpp`, `test_c_api_database_create.cpp`, `test_c_api_database_read.cpp`, `test_c_api_database_update.cpp`, `test_c_api_database_delete.cpp`, `test_c_api_database_query.cpp`, `test_c_api_database_time_series.cpp`, `test_c_api_database_transaction.cpp`, `test_c_api_database_metadata.cpp`, `test_c_api_database_csv_export.cpp`, `test_c_api_database_csv_import.cpp`, `test_c_api_element.cpp`, `test_c_api_lua_runner.cpp`, `test_c_api_binary_file.cpp`, `test_c_api_binary_metadata.cpp`, `test_c_api_csv_converter.cpp`. Full source list at `tests/CMakeLists.txt` lines 3-67.

Julia tests: `bindings/julia/test/` — `runtests.jl` (entry, recursively includes `test_*.jl`), `fixture.jl` (shared `tests_path()` helper), plus `test_database_lifecycle.jl`, `test_database_create.jl`, ... `test_binary_file.jl`, `test_binary_metadata.jl`, `test_csv_converter.jl`. Fail-fast: `@testset verbose = true failfast = true` (`runtests.jl` line 17).

Dart tests: `bindings/dart/test/` — one `main()` per file with top-level `group(...)`: `database_lifecycle_test.dart`, `database_create_test.dart`, `database_read_test.dart`, etc. + `metadata_test.dart`, `schema_validation_test.dart`, `issues_test.dart`.

Python tests: `bindings/python/tests/` — `conftest.py` supplies fixtures (`db`, `collections_db`, `relations_db`, `all_types_db`, `csv_db`, `mixed_time_series_db`, `composite_helpers_db`, `schemas_path`, etc.). Tests grouped in classes (`class TestCreateElement`, `class TestFKResolutionCreate`) with snake_case test methods. Files: `test_database_create.py`, `test_database_read_scalar.py`, `test_database_read_set.py`, `test_database_read_vector.py`, `test_database_update.py`, `test_database_time_series.py`, etc.

JS / Deno tests: `bindings/js/test/` — `database.test.ts`, `create.test.ts`, `read.test.ts`, `update.test.ts`, `csv.test.ts`, `query.test.ts`, `time-series.test.ts`, `transaction.test.ts`, `metadata.test.ts`, `introspection.test.ts`, `composites.test.ts`, `lua-runner.test.ts`. Pattern: `Deno.test({ name, sanitizeResources: false }, async (t) => { await t.step(...) })`.

## Shared Test Schemas

All bindings reference a single central `tests/schemas/` directory — never duplicate schemas per binding.
- `tests/schemas/valid/` — happy-path schemas: `basic.sql`, `collections.sql`, `relations.sql`, `all_types.sql`, `composite_helpers.sql`, `csv_export.sql`, `describe_multi_group.sql`, `mixed_time_series.sql`, `multi_time_series.sql`.
- `tests/schemas/invalid/` — negative-test schemas: `duplicate_attribute_time_series.sql`, `duplicate_attribute_vector.sql`, `fk_actions.sql`, `fk_not_null_set_null.sql`, `label_not_null.sql`, `label_not_unique.sql`, `label_wrong_type.sql`, `no_configuration.sql`, `set_no_unique.sql`, `vector_no_index.sql`.
- `tests/schemas/migrations/` — versioned migration fixtures (`1/up.sql`, `1/down.sql`, `2/...`, `3/...`).
- `tests/schemas/issues/` — reproducers for tracked issues.

Each binding resolves these paths relative to the repo root:
- C++: `SCHEMA_PATH(...)`, `VALID_SCHEMA("basic.sql")`, `INVALID_SCHEMA(...)` macros in `tests/test_utils.h` — use `path_from(__FILE__, ...)`.
- Julia: `tests_path()` in `bindings/julia/test/fixture.jl` — `joinpath(@__DIR__, "..", "..", "..", "tests")`.
- Dart: `final testsPath = path.join(path.current, '..', '..', 'tests');` at top of every `main()`.
- Python: `schemas_path` fixture in `conftest.py` lines 18-20 — `Path(__file__).resolve().parent.parent.parent.parent / "tests" / "schemas"`.
- JS: `join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "basic.sql")`.

## Test Structure Patterns

**C++ bare TEST** (`tests/test_database_create.cpp` lines 8-28):
```cpp
TEST(Database, CreateElementWithScalars) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"),
        {.read_only = false, .console_level = quiver::LogLevel::Off});
    quiver::Element element;
    element.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);
}
```

**C++ fixture** (`tests/test_database_lifecycle.cpp` lines 13-21):
```cpp
class TempFileFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "quiver_test.db").string(); }
    void TearDown() override { if (fs::exists(path)) fs::remove(path); }
    std::string path;
};
TEST_F(TempFileFixture, MoveConstructor) { ... }
```

**Binary subsystem fixture** (`tests/test_binary_file.cpp` lines 17-52) cleans up `.qvr`, `.toml`, `.csv` artifacts and supplies `make_simple_metadata()` / `make_time_metadata()` static factories.

Test naming convention: `TEST(Suite, TestName)` / `TEST_F(Fixture, TestName)` with CamelCase test names. Suite names: `Database`, `DatabaseCApi`, `DatabaseErrors`, `BinaryTempFileFixture`.

**C API test pattern** (`tests/test_c_api_database_create.cpp` lines 8-29): exercises C API directly to verify marshaling layer, using same GoogleTest structure.

**Test helpers** in `tests/test_utils.h`:
```cpp
namespace quiver::test {
    inline std::string path_from(const char* test_file, const std::string& relative);
    inline quiver_database_options_t quiet_options();  // returns options with QUIVER_LOG_OFF
}
#define SCHEMA_PATH(relative) quiver::test::path_from(__FILE__, relative)
#define VALID_SCHEMA(name)    SCHEMA_PATH("schemas/valid/" name)
#define INVALID_SCHEMA(name)  SCHEMA_PATH("schemas/invalid/" name)
```

**Julia pattern** (`bindings/julia/test/test_database_create.jl`): each file wrapped in its own `module TestXyz ... end` so `recursive_include` does not collide. Uses `Quiver.from_schema(":memory:", path_schema)`, `Quiver.create_element!(db, "Configuration"; kwargs...)`, `Quiver.close!(db)`.

**Dart pattern** (`bindings/dart/test/database_create_test.dart`): `try { ... } finally { db.close(); }` around each test body. Temp disk paths via `Directory.systemTemp.createTempSync('quiver_test_')`. Error expectations via `expect(() => ..., throwsA(isA<DatabaseException>()))`.

**Python pattern** (`bindings/python/tests/test_database_create.py`): test methods take `collections_db: Database` fixture by name; teardown via `yield` generator fixture in `conftest.py`.

**JS / Deno pattern** (`bindings/js/test/database.test.ts`): `sanitizeResources: false` required because FFI handles leak across Deno test steps; per-step `try { ... } finally { db.close(); }`.

## Mocking

**No mocking of SQLite.** Tests run against real SQLite. `:memory:` for speed; temp-path databases only when testing persistence. GMock is linked to `quiver_tests` at `tests/CMakeLists.txt` line 33 but no current uses — rationale is that SQLite's STRICT typing, FK cascade, triggers, and `sqlite3_get_autocommit()` semantics differ from in-memory mocks.

## Run Commands

**Everything (build + test):**
```bash
scripts/build-all.bat            # Debug (default)
scripts/build-all.bat --release  # Release
scripts/build-all.bat --debug    # Explicit debug
```

Steps from `scripts/build-all.bat`:
1. `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=<type> -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON`
2. `cmake --build build --config <type>`
3. Run `build/bin/quiver_tests.exe`
4. Run `build/bin/quiver_c_tests.exe`
5. `bindings/julia/test/test.bat`
6. `bindings/dart/test/test.bat`
7. `bindings/python/tests/test.bat`

**Test-only (assumes already built):**
```bash
scripts/test-all.bat  # runs all 6 suites: C++, C API, Julia, Dart, JS, Python
```
Tracks per-suite PASS/FAIL/SKIP; missing executables are reported as SKIP with a warning to run `build-all.bat` first (`scripts/test-all.bat`).

**Individual executables (after build):**
```bash
./build/bin/quiver_tests.exe       # C++ core (GoogleTest)
./build/bin/quiver_c_tests.exe     # C API (GoogleTest)
./build/bin/quiver_cli.exe         # CLI entry point (not a test)
./build/bin/quiver_benchmark.exe   # Transaction perf benchmark (manual only)
./build/bin/quiver_sandbox.exe     # Dev scratchpad (manual only)
```

Standard GoogleTest flags supported:
```bash
./build/bin/quiver_tests.exe --gtest_list_tests
./build/bin/quiver_tests.exe --gtest_filter=Database.Create*
./build/bin/quiver_tests.exe --gtest_repeat=10 --gtest_shuffle
```

**Per-binding scripts:**
- `bindings/julia/test/test.bat` — `julia +1.12.5 --project=<binding> -e "Pkg.test(test_args=ARGS)"`
- `bindings/dart/test/test.bat` — `pushd ..; dart test %*; popd`
- `bindings/python/tests/test.bat` — prepends `build/bin` to PATH, runs `uv run pytest tests/`
- `bindings/js/test/test.bat` — prepends `build/bin` to PATH, runs `deno test --allow-ffi --allow-read --allow-write --allow-env test/`

**CTest:**
```bash
cd build
ctest -C Debug --output-on-failure
```
CI uses this form.

## CI Workflows

All at `.github/workflows/`:
- `ci.yml` — main CI.
- `build-wheels.yml` — Python wheel build via `cibuildwheel`.
- `publish.yml` — release publishing.

`ci.yml` jobs (`.github/workflows/ci.yml`):
- **build** (lines 9-60) — matrix `ubuntu-latest` × `windows-latest` × `macos-latest` by `Release`/`Debug`. CMake configure + build + `ctest --output-on-failure`. Uploads `build/lib/*` + `build/bin/*`.
- **cpp-coverage** (lines 62-100) — Ubuntu; lcov with `--coverage` flags; excludes `/usr/*`, `build/_deps/*`, `tests/*`; uploads to Codecov with `flags: cpp`.
- **julia-coverage** (lines 102-147) — matrix Julia `1.11` and `1.12`. Builds C++ first (needed for shared lib), runs `julia-actions/julia-runtest` with coverage, `julia-actions/julia-processcoverage@v1` with `directories: bindings/julia/src`. Sets `LD_LIBRARY_PATH: ${{ github.workspace }}/build/lib`.
- **dart-coverage** (lines 149-202) — Dart 3.10.7; builds C++, copies `libquiver.so` + `libquiver_c.so` into `bindings/dart/`, runs `dart test --reporter expanded`, then `coverage:test_with_coverage`, filters `lib/src/ffi/bindings.dart` (generated).
- **python-coverage** (lines 204-241) — Ubuntu; `astral-sh/setup-uv@v7`; builds C++, `uv run pytest --cov=quiverdb --cov-report=xml`. Sets `LD_LIBRARY_PATH`.
- **clang-format** (lines 243-259) — installs `clang-format-21`; runs `find include src tests -name '*.cpp' -o -name '*.h' | xargs clang-format-21 --dry-run --Werror`.
- **deno-test** (lines 261-284) — Deno 2.7.12; builds C++, runs `deno task test` in `bindings/js/`. Sets `LD_LIBRARY_PATH`.

Codecov config file at `codecov.yml`. Token via `secrets.CODECOV_TOKEN`.

## Coverage Tooling

- **C++ / C API (Linux only):** lcov via `--coverage`. See `ci.yml` lines 62-100.
- **Julia:** `julia-actions/julia-processcoverage@v1` scoped to `bindings/julia/src`.
- **Dart:** `dart pub global run coverage:test_with_coverage --out=coverage`; filters generated FFI bindings. Local scripts: `bindings/dart/test/coverage.bat` and `coverage.sh`.
- **Python:** `pytest-cov` via `--cov=quiverdb --cov-report=xml`.
- **JS/Deno:** no coverage currently configured.

## Test Types

- **Unit tests:** single Database method or binding function per test (`test_database_read.cpp`, `test_database_create.cpp`).
- **Integration tests:** round-trips (CSV export→import in `test_database_csv_import.cpp`; bin→csv→bin in `test_csv_converter.cpp`), migrations (`test_migrations.cpp`), Lua scripting (`test_lua_runner.cpp`).
- **Error-path tests:** `test_database_errors.cpp`, `test_schema_validator.cpp` — missing schema, collection-not-found, wrong types, missing Configuration, FK action violations, duplicate attributes.
- **Regression tests:** `test_issues.cpp` + `tests/schemas/issues/` — reproducers for historical bugs.
- **Benchmark:** `tests/benchmark/benchmark.cpp` → `quiver_benchmark.exe`. **Not part of the automated test suite** — built by `build-all.bat` but never invoked automatically. Run manually for transaction perf.
- **Sandbox:** `tests/sandbox/sandbox.cpp` → `quiver_sandbox.exe`. Dev scratchpad, not run.
- **No E2E framework** beyond the above.

## Common Patterns

**Memory DB for speed, disk only when testing persistence:**
```cpp
auto db = quiver::Database::from_schema(
    ":memory:", VALID_SCHEMA("basic.sql"),
    {.read_only = false, .console_level = quiver::LogLevel::Off});
```

**Quiet console in tests:** C++ `console_level = quiver::LogLevel::Off` / C API `QUIVER_LOG_OFF` / helper `quiver::test::quiet_options()` in `tests/test_utils.h`.

**Error expectations:**
- C++: `EXPECT_THROW(db.create_element("Nonexistent", element), std::runtime_error);`
- C API: `EXPECT_EQ(quiver_database_create_element(nullptr, ...), QUIVER_ERROR);`
- Julia: `@test_throws DatabaseException Quiver.create_element!(...)`
- Dart: `expect(() => ..., throwsA(isA<DatabaseException>()))`
- Python: `with pytest.raises(QuiverError): ...`

**Verify via read APIs after mutations** (not raw SQL) — doubles as read-side regression check:
```cpp
int64_t id = db.create_element("Configuration", element);
auto labels = db.read_scalar_strings("Configuration", "label");
EXPECT_EQ(labels[0], "Config 1");
```

## Fragile Test Setup

Points that silently break on environment drift:

- **Dart DLL PATH chain (Windows):** `libquiver_c.dll` depends on `libquiver.dll`. Both must be on PATH for `dart test` to succeed. `bindings/dart/test/test.bat` **does not currently set PATH** (only runs `dart test %*`) — so DLLs must already be copied adjacent by the CMake POST_BUILD step (`tests/CMakeLists.txt` lines 36-42, 77-86). `bindings/dart/test/coverage.bat` line 8 **does** set PATH explicitly. CI copies them with `cp build/lib/libquiver.so bindings/dart/ && cp build/lib/libquiver_c.so bindings/dart/` (`ci.yml` lines 175-178).
- **Dart `hooks_runner` / `lib` cache:** when C API struct layout changes, Dart's native-assets cache holds a stale DLL — clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` to force a rebuild (documented in `CLAUDE.md`).
- **Python CFFI loader pre-loads `libquiver.dll` on Windows:** `bindings/python/src/quiverdb/_loader.py` calls `os.add_dll_directory(str(_LIBS_DIR))` **before** `ffi.dlopen(core_path)` and `ffi.dlopen(c_api_path)` so the dependency chain resolves. Dev mode (`test.bat`) prepends `build/bin/` to PATH.
- **Python / JS `test.bat` PATH requirement:** both files contain `set PATH=%~dp0..\..\..\build\bin;%PATH%`. Omitting this line causes cryptic `dlopen` errors.
- **Julia version pinned to `+1.12.5`** in `bindings/julia/test/test.bat` (requires juliaup with that exact channel). CI matrix is `1.11` and `1.12`.
- **CI on Linux sets `LD_LIBRARY_PATH=${{ github.workspace }}/build/lib`** for Julia, Python, Deno jobs. Same env required locally when running those bindings against a build tree.
- **Julia `Manifest.toml` conflicts:** delete it and run `julia --project=bindings/julia -e "using Pkg; Pkg.instantiate()"` (documented in `CLAUDE.md`).
- **`tests/schemas/` path assumption:** Julia/Dart/JS/Python all compute the schemas path as `../../../tests/` from their binding directory. Moving any binding directory breaks every test suite.

## Test Helpers / Fixtures Summary

| Binding | Helper | Location |
|---------|--------|----------|
| C++ | `path_from()`, `quiet_options()`, `SCHEMA_PATH`, `VALID_SCHEMA`, `INVALID_SCHEMA` | `tests/test_utils.h` |
| C++ | `TempFileFixture`, `MigrationFixture`, `BinaryTempFileFixture` | co-located in each test file |
| Julia | `tests_path()` | `bindings/julia/test/fixture.jl` |
| Julia | `recursive_include`, fail-fast `@testset` | `bindings/julia/test/runtests.jl` |
| Python | `tests_path`, `schemas_path`, `valid_schema_path`, `db`, `collections_db`, `relations_db`, `csv_db`, `all_types_db`, `mixed_time_series_db`, `composite_helpers_db` | `bindings/python/tests/conftest.py` |
| Dart | ad-hoc `testsPath` in each `main()` | every `*_test.dart` |
| JS | ad-hoc `SCHEMA_PATH`, `MIGRATIONS_PATH`, `existsSync()`, `makeTempDir()` | each `*.test.ts` |
