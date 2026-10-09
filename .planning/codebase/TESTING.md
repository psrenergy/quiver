---
last_mapped_commit: cf9ca58aceab305fe6a08461f78da8556a6c95d0
last_mapped_at: 2026-10-07
---
# Testing Patterns

**Analysis Date:** 2026-10-07

## Test Framework

**Runner:**

- C++ and C ABI: GoogleTest **1.17.0**, fetched only when `QUIVER_BUILD_TESTS` is enabled (`cmake/Dependencies.cmake`). CTest discovers both executables through `gtest_discover_tests` (`tests/CMakeLists.txt`).
- C++ executable: `quiver_tests`, including Lua Sandbox suites. C ABI executable: `quiver_c_tests`, conditional on `QUIVER_BUILD_C_API` (`tests/CMakeLists.txt`).
- Julia: standard-library `Test`, compatibility `1`, invoked through `Pkg.test`; recursive runner includes `test_*.jl` files or the single file passed in `ARGS[1]` (`bindings/julia/Project.toml`, `bindings/julia/test/runtests.jl`).
- Dart: `package:test` **^1.31.2** (`bindings/dart/pubspec.yaml`). Python: pytest **>=8.4.1** (`bindings/python/pyproject.toml`).
- JS: Bun's built-in `bun:test`; runner version is not pinned in `bindings/js/package.json`, and CI installs latest Bun (`.github/workflows/ci.yml`).

**Assertion Library:**

- GoogleTest `EXPECT_*`/`ASSERT_*`; GoogleMock matchers such as `ThrowsMessage`/`HasSubstr` (`tests/test_schema_validator.cpp`, `tests/test_database_read_scalar.cpp`).
- Julia `@test`, `@testset`, `@test_throws`; Dart `expect` plus matchers; pytest plain assertions and `pytest.raises`; Bun `expect` (`bindings/julia/test/test_database_create.jl`, `bindings/dart/test/database_create_test.dart`, `bindings/python/tests/test_database_create.py`, `bindings/js/test/database-create.test.ts`).

**Run Commands:**
Commands below are read from scripts, presets and CI; tests were not executed for this map (`CMakePresets.json`, `scripts/test-all.bat`, `.github/workflows/ci.yml`).

```powershell

# Repo root; configure/build prerequisites before running tests.

cmake --preset dev
cmake --build --preset dev
ctest --preset dev

cmake --preset release
cmake --build --preset release
ctest --preset release

# Existing Windows wrappers, called from repository root.

scripts/test-all.bat
bindings/julia/test/test.bat
bindings/dart/test/test.bat
bindings/python/tests/test.bat
bindings/js/test/test.bat
```

- Both presets use Ninja, tests and C API ON, with `build/dev/` or `build/release/` output (`CMakePresets.json`).
- `scripts/test-all.bat` assumes native executables in `build/bin/`; missing binaries are marked SKIP and do not themselves make the script fail. Use CTest presets to verify preset builds rather than interpreting that wrapper's exit status as proof every native suite ran (`scripts/test-all.bat`).
- Julia Windows wrapper pins `julia +1.12.5`; package compatibility is Julia 1.11 and CI exercises 1.11/1.12/1.13 (`bindings/julia/test/test.bat`, `bindings/julia/Project.toml`, `.github/workflows/ci.yml`).
- Python and JS Windows wrappers prepend `build/bin` to PATH. CI uses its platform-specific library path; preset output directories must be supplied when running a binding against preset-built libraries (`bindings/python/tests/test.bat`, `bindings/js/test/test.bat`, `.github/workflows/ci.yml`).
- Dart tests build native code through the native-assets hook; the Dart coverage CI job does not pre-build with the shared C++ action (`bindings/dart/hook/build.dart`, `.github/AGENTS.md`).

```bash

# Run inside bindings/python:

uv run pytest tests/
uv run pytest tests/test_database_create.py

# Run inside bindings/dart:

dart test
dart test test/database_create_test.dart

# Run inside bindings/js:

bun test test

# Run inside bindings/julia:

julia --project=. -e 'import Pkg; Pkg.test()'
```

- Wrapper scripts forward additional runner arguments; Julia `test.bat test_database_create.jl` selects a file through `test_args=ARGS` (`bindings/julia/test/test.bat`, `bindings/julia/test/runtests.jl`).
- No repository watch script is configured in `bindings/js/package.json`, `bindings/python/pyproject.toml`, or the binding test wrappers. The native suites can be filtered directly with `--gtest_filter` (`tests/AGENTS.md`).

## Test File Organization

**Location:**

- Keep native tests in root `tests/`; bindings have their own `test/` or `tests/` directories. Put shared SQL only in `tests/schemas/` (`tests/AGENTS.md`).
- Add native source files to the explicit lists in `tests/CMakeLists.txt`; Julia's runner discovers `test_*.jl` recursively (`tests/CMakeLists.txt`, `bindings/julia/test/runtests.jl`).

**Naming:**

- C++: `test_<area>.cpp`; ABI: `test_c_api_<area>.cpp`; Lua: `test_sandbox_<area>.cpp` (`tests/AGENTS.md`).
- Julia/Python: `test_database_<area>.jl/.py`; Dart: `database_<area>_test.dart`; JS: `database-<area>.test.ts` (`tests/AGENTS.md`).
- Split reads by scalar/vector/set and time-series by metadata/group/row/files/nulls when those concerns exist. Do not assume identical file sets: core has no separate time-series nulls file; Python/JS have no time-series metadata file (`tests/AGENTS.md`).

**Structure:**

```text
tests/
  CMakeLists.txt
  test_utils.h
  test_sandbox.h
  test_database_*.cpp
  test_c_api_*.cpp
  test_sandbox_*.cpp
  fixtures/               # byte-exact CSV and generated XLSX
  schemas/
    valid/
    invalid/
    migrations/<version>/up.sql, down.sql
    issues/<issue>/<version>/up.sql, down.sql
bindings/julia/test/       # runtests.jl, fixture.jl, test_*.jl
bindings/dart/test/        # *_test.dart, test/coverage wrappers
bindings/python/tests/    # conftest.py, test_*.py, test.bat
bindings/js/test/         # *.test.ts, test.bat
```

## Test Structure

**Suite Organization:**
Use the existing public create/read pattern from `tests/test_database_create.cpp`:

```cpp
TEST(Database, CreateElementWithScalars) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"),
        {.read_only = false, .console_level = quiver::LogLevel::Off}
    );
    quiver::Element element;
    element.set("label", std::string("Config 1"))
        .set("integer_attribute", int64_t{42}).set("float_attribute", 3.14);
    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);
    auto integers = db.read_scalar_integers("Configuration", "integer_attribute");
    EXPECT_EQ(integers[0], 42);
}
```

- Ordinary native tests use `TEST`; schema and Lua suites use `TEST_F` when setup is shared (`tests/test_database_create.cpp`, `tests/test_schema_validator.cpp`, `tests/test_sandbox.h`).
- ABI tests allocate opaque handles, assert `QUIVER_OK`, inspect outputs and explicitly destroy/free resources (`tests/test_c_api_database_create.cpp`).
- Julia files isolate definitions in `module Test...` and nest `@testset`; runner uses verbose/failfast mode (`bindings/julia/test/test_database_create.jl`, `bindings/julia/test/runtests.jl`).
- Python uses `Test...` classes and fixture arguments; Dart uses `group`/`test` inside `main`; JS uses `describe`/`test` (`bindings/python/tests/test_database_create.py`, `bindings/dart/test/database_create_test.dart`, `bindings/js/test/database-create.test.ts`).

**Patterns:**

- Set up a real in-memory schema for database-only behavior and file-backed databases for filesystem/lifecycle behavior (`tests/test_database_create.cpp`, `tests/test_database_lifecycle.cpp`).
- Use C++ RAII for core objects and `try/finally` cleanup in JS/Dart. Python fixtures yield then close; Julia tests call `close!` and also exercise scoped resources (`tests/test_database_create.cpp`, `bindings/js/test/database-create.test.ts`, `bindings/dart/test/database_create_test.dart`, `bindings/python/tests/conftest.py`, `bindings/julia/test/test_database_lifecycle.jl`).
- Match C ABI frees exactly, including masks and nested arrays; never `delete[]` a returned API string in the test (`tests/AGENTS.md`, `tests/test_c_api_database_read_vector.cpp`).
- Use `EXPECT` instead of fatal `ASSERT` inside an open-for-write binary span so cleanup remains reachable (`tests/AGENTS.md`, `tests/test_c_api_binary_file.cpp`).
- CSV writer tests round-trip through the reader; assertions on output substrings alone do not establish import/read correctness (`tests/test_sandbox_write_csv.cpp`, `tests/AGENTS.md`).

## Mocking

**Framework:**

- GoogleMock is linked to the C++ executable and used for exception/container matchers. No dedicated external-service or FFI mock layer was detected in the native or binding suites (`tests/CMakeLists.txt`, `tests/test_schema_validator.cpp`, `bindings/python/tests/`, `bindings/js/test/`).

**Patterns:**

- Test actual SQLite and the actual C ABI. GoogleMock here primarily expresses assertions rather than substituting a Database dependency (`tests/test_schema_validator.cpp`, `tests/test_database_read_scalar.cpp`).
- Standalone source-contract tests avoid loading the FFI entirely: `bindings/js/test/sandbox-api-sync.test.ts` imports the reference constant directly and parses C++ binder source.

**What to Mock:**

- No reusable mock setup detected; prefer the existing `:memory:` and temporary-filesystem fixtures (`tests/test_utils.h`, `tests/test_sandbox.h`, `bindings/python/tests/conftest.py`).

**What NOT to Mock:**

- Do not bypass native marshalling, presence-mask decoding, SQLite constraints or last-error propagation in cross-layer tests; those are the behavior under test (`tests/test_c_api_database_time_series_nulls.cpp`, `bindings/python/tests/test_database_time_series_nulls.py`, `bindings/js/test/database-time-series-nulls.test.ts`).

## Fixtures and Factories

**Test Data:**
Representative shared-schema and cleanup fixture from `bindings/python/tests/conftest.py`:

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

**Location:**

- C++ `VALID_SCHEMA`/`INVALID_SCHEMA` macros resolve relative to `__FILE__`; `quiet_options()` switches C logging off (`tests/test_utils.h`).
- Julia's `tests_path()` prefers repo-root schemas, falling back to mirror-vendored `test/schemas/` (`bindings/julia/test/fixture.jl`). Dart resolves root tests relative to binding cwd; JS resolves via `import.meta.dir` (`bindings/dart/test/database_create_test.dart`, `bindings/js/test/database-create.test.ts`).
- Shared schemas cover all types, FK relations, nullable groups, multi-column/multi-dimensional time series, CSV cascade cases and invalid schema contracts (`tests/schemas/valid/`, `tests/schemas/invalid/`, `tests/AGENTS.md`).
- Migration fixtures always include numbered `up.sql`/`down.sql`; issue regressions live in `tests/test_issues.cpp` and `tests/schemas/issues/` (`tests/AGENTS.md`).
- `SandboxTest` supports non-file Lua calls. Use `LuaSandboxTest` for every file-touching Lua operation: dedicated per-test temp directory, file-backed database, relative script paths and directory cleanup (`tests/test_sandbox.h`).
- UI metadata tests build sibling migrations/ui trees at runtime. Never commit `tests/schemas/ui/`, which would affect every shared migration consumer (`tests/test_database_ui_metadata.cpp`, `tests/AGENTS.md`).
- `tests/fixtures/ma_energia_residencial.csv` and `tests/fixtures/ma_gd_data.csv` preserve exact utility-file bytes, including BOM/CRLF distinctions. XLSX archives are generated independently with Python stdlib in `tests/fixtures/generate_xlsx.py`, committed, and copied into test sandboxes (`tests/AGENTS.md`, `.gitattributes`).

## Coverage

**Requirements:**

- No fixed percentage minimum is enforced locally. Codecov project and patch targets are `auto` with **1% threshold**; precision 2, round down; `tests/` and `build/` are ignored (`codecov.yml`).
- CI uploads flags `cpp`, `julia`, `dart`, `python`, `js`; uploads fail CI on error and disable report searching. The C++ upload additionally disables the gcov plugin to preserve the explicit lcov exclusions (`.github/workflows/ci.yml`, `.github/AGENTS.md`).
- Native coverage uses `--coverage` C/C++ instrumentation and lcov capture, removing system headers, fetched dependencies and native test sources (`.github/actions/build-cpp/action.yml`, `.github/workflows/ci.yml`).
- Julia processes coverage for `bindings/julia/src`; Dart removes generated `lib/src/ffi/bindings.dart`; Python produces package coverage XML (`.github/workflows/ci.yml`).
- Bun coverage explicitly skips test files via `coverageSkipTestFiles = true` (`bindings/js/bunfig.toml`).

**View Coverage:**

```bash

# Repo root after a CI-equivalent instrumented native build and ctest:

lcov --capture --directory build --output-file coverage.info --ignore-errors mismatch
lcov --list coverage.info --ignore-errors mismatch

# bindings/python; pytest-cov is installed separately by CI:

uv run pytest --cov=quiverdb --cov-report=xml

# bindings/dart; scripts activate package:coverage before this command:

dart pub global run coverage:test_with_coverage --out=coverage

# bindings/js:

bun test test --coverage --coverage-reporter=lcov
```

- Generated reports are `coverage.info`, Julia-processed `lcov.info`, `bindings/dart/coverage/lcov.info`, `bindings/python/coverage.xml`, and `bindings/js/coverage/lcov.info` (`.github/workflows/ci.yml`); they appear after coverage runs.

## Test Types

**Unit Tests:**

- Core values/algorithms: Element, Row, schema validation, binary metadata/time properties, expressions, iteration (`tests/test_element.cpp`, `tests/test_row_result.cpp`, `tests/test_schema_validator.cpp`, `tests/test_binary_metadata.cpp`, `tests/test_binary_time_properties.cpp`, `tests/test_expression.cpp`, `tests/test_iteration.cpp`).
- Path-policy unit tests compile the internal `src/sandbox/path_policy.cpp` into the test executable; keep that source to the one gate to avoid duplicate definitions in static builds (`tests/test_sandbox_path.cpp`, `tests/CMakeLists.txt`).
- Package/reference synchronization and loader checks: `bindings/js/test/sandbox-api-sync.test.ts`, `bindings/js/test/package-entry.test.ts`, `bindings/julia/test/test_artifact_loader.jl`.

**Integration Tests:**

- CRUD, transactions/dry runs, schema introspection, migrations, FK resolution, CSV round-trips, row/group time-series reads/writes and positional NULL preservation (`tests/test_database_*.cpp`, `tests/test_c_api_database_*.cpp`, `tests/AGENTS.md`).
- Lua is tested inside C++ through Sandbox; C ABI Sandbox wrappers and each host binding also exercise script execution (`tests/test_sandbox_*.cpp`, `tests/test_c_api_sandbox.cpp`, `bindings/julia/test/test_sandbox.jl`, `bindings/dart/test/sandbox_test.dart`, `bindings/python/tests/test_sandbox.py`, `bindings/js/test/sandbox.test.ts`).
- Preserve intentional coverage scope: binary/expression are core/C ABI/Julia/Lua; relation-map convenience is Julia-only; boolean readers exist in the four host bindings; standalone CSV/XLSX reader and CSV writer methods are Lua-only (`AGENTS.md`, `tests/AGENTS.md`).
- Match regression layers to the boundary affected: Boolean input needs Lua and all host wrappers, whereas there is no bool setter in C++/C ABI to test (`tests/AGENTS.md`, `bindings/python/tests/test_database_boolean.py`).
- Native CI builds Debug and Release on Ubuntu x64, Ubuntu ARM64, Windows and macOS. Bun has Ubuntu x64/ARM64 and Windows test legs (`.github/workflows/ci.yml`).
- Python wheel CI uses cibuildwheel and runs the shared pytest suite against built wheels; local validators live in `scripts/test-wheel.bat` and `scripts/test-wheel-install.bat` (`bindings/python/pyproject.toml`, `.github/workflows/publish-python.yml`).

**E2E Tests:**

- No browser E2E framework detected; this is a library. Native/FFI/file round-trips and installed-wheel tests provide its end-to-end checks (`tests/CMakeLists.txt`, `bindings/python/pyproject.toml`, `scripts/test-wheel-install.bat`).
- `quiver_benchmark` and `quiver_sandbox` are intentional standalone executables, not CTest suites (`tests/CMakeLists.txt`, `tests/benchmark/benchmark.cpp`, `tests/sandbox/sandbox.cpp`).

## Common Patterns

**Async Testing:**

- Database and FFI test paths are synchronous; no shared async test setup was detected in `bindings/js/test/`, `bindings/python/tests/`, `bindings/dart/test/`, or `bindings/julia/test/`.
- Streaming Lua readers invoke callbacks synchronously, assert equivalence with whole-file readers, and test early stop (`tests/test_sandbox_read_csv.cpp`, `tests/test_sandbox_read_xlsx.cpp`).

**Error Testing:**

- Assert both exception type and useful message text when the error source matters. Python example from `bindings/python/tests/test_database_create.py`:

```python
with pytest.raises(QuiverError, match="'collection'"):
    collections_db.create_element("Collection", label="Item1", collection="x")
```

- Lua uses `expect_sandbox_error`: catch the thrown exception and assert a substring, so a missing method cannot satisfy a negative test through an unrelated nil-call error (`tests/test_sandbox.h`).
- Core tests use `EXPECT_THROW` or `ThrowsMessage<std::runtime_error>(HasSubstr(...))`; C ABI tests assert `QUIVER_ERROR` and inspect `quiver_get_last_error` (`tests/test_schema_validator.cpp`, `tests/test_c_api_database_lifecycle.cpp`).
- Dart uses `throwsA(isA<DatabaseException>())`; JS uses `.toThrow(QuiverError)` and message assertions; Julia uses `@test_throws Quiver.DatabaseException` (`bindings/dart/test/database_create_test.dart`, `bindings/js/test/database-create.test.ts`, `bindings/julia/test/test_database_update.jl`).
- Test NULL-only rows separately from missing group rows and assert numeric masks/size at the ABI boundary; test empty/jagged/mixed-type columns before deletion (`tests/test_database_read_vector.cpp`, `tests/test_c_api_database_read_vector.cpp`, `tests/test_c_api_database_time_series_nulls.cpp`).
- Keep OS-sensitive file tests conditional: Windows exclusive-lock/device-path cases; POSIX unreadable-file permissions with skips for root, and symlink skips when unsupported (`tests/test_sandbox_read_csv.cpp`, `tests/test_sandbox_path.cpp`, `tests/AGENTS.md`).
- Run Lua regressions in both Debug and Release when changing table marshalling; getter and stack checks are deliberately disabled in both builds, while other sol2 safeguards remain enabled (`tests/AGENTS.md`, `src/CMakeLists.txt`).

---

*Testing analysis: 2026-10-07*
