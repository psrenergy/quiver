---
last_mapped_commit: cf9ca58aceab305fe6a08461f78da8556a6c95d0
last_mapped_at: 2026-10-07
---
# Coding Conventions

**Analysis Date:** 2026-10-07

## Naming Patterns

**Files:**

- Use snake_case `.h`/`.cpp` names and split Database implementation by operation: `include/quiver/database.h`, `src/database_create.cpp`, `src/database_read.cpp`, `src/database_csv_import.cpp`.
- Match C wrappers to their core area in `src/c/database_create.cpp`; place subsystem code in `src/binary/`, `src/expression/`, and Lua binders in `src/sandbox/` (`src/AGENTS.md`, `src/c/AGENTS.md`).
- Python and Julia use snake_case modules: `bindings/python/src/quiverdb/database_csv_import.py`, `bindings/julia/src/database_create.jl`. Dart uses snake_case files, e.g. `bindings/dart/lib/src/database_create.dart`; JS uses short category names and hyphenated compound names, e.g. `bindings/js/src/time-series.ts`.
- Test naming follows each language's runner: `tests/test_database_create.cpp`, `bindings/julia/test/test_database_create.jl`, `bindings/python/tests/test_database_create.py`, `bindings/dart/test/database_create_test.dart`, `bindings/js/test/database-create.test.ts`.

**Functions:**

- Use snake_case for C++ functions/methods; C functions prepend their entity, e.g. `create_element` and `quiver_database_create_element` (`src/database_create.cpp`, `src/c/database_create.cpp`).
- Python retains snake_case; Julia retains snake_case and appends `!` to mutators (`bindings/python/src/quiverdb/database.py`, `bindings/julia/src/database_create.jl`).
- Dart and JS use camelCase (`createElement`, `readTimeSeriesRow`); actual JS CSV methods use `exportCsv`/`importCsv` (`bindings/dart/lib/src/database_create.dart`, `bindings/js/src/csv.ts`).
- Lua uses snake_case and colon method syntax (`db:create_element`); pure metadata/expression functions live under `quiver.*` (`src/sandbox/database_create.cpp`, `src/sandbox/expression.cpp`).

**Variables:**

- Use lower_case locals, parameters and namespace names in C++; private members carry a trailing underscore (`.clang-tidy`, `include/quiver/element.h`). Existing legacy constants such as `kMaxDistributionCardinality` are exceptions (`src/database_describe.cpp`).
- Python/Julia locals use snake_case; Python internal helpers and attributes use `_` prefixes (`bindings/python/src/quiverdb/_helpers.py`, `bindings/julia/src/database.jl`).
- Dart/JS locals use camelCase; JS numeric API constants use uppercase snake_case (`bindings/js/src/types.ts`, `bindings/dart/lib/src/database.dart`).

**Types:**

- Use CamelCase C++ classes, structs, enums and enum values; use opaque `quiver_*_t` C handles and uppercase C enum constants (`.clang-tidy`, `include/quiver/c/common.h`, `src/c/internal.h`).
- Binding public classes use PascalCase: `Database`, `Sandbox`, `ScalarMetadata` (`bindings/python/src/quiverdb/__init__.py`, `bindings/js/src/index.ts`, `bindings/dart/lib/quiverdb.dart`, `bindings/julia/src/Quiver.jl`).
- Preserve the shared typing/NULL policy rather than inventing schema-dependent binding coercion (`AGENTS.md`, `src/type_validator.cpp`, `src/database_internal.h`).

## Code Style

**Formatting:**

- C++ uses LLVM-based clang-format **22.1.8**, C++20, 4 spaces, 120 columns, LF, left-aligned pointers, inserted braces, and expanded multiline argument lists (`.clang-format`, `.pre-commit-config.yaml`, `.github/workflows/ci.yml`).
- Run the CMake `format` target; `CMakeLists.txt` searches for `clang-format-22` before `clang-format`. Keep the formatter pin consistent with CI and pre-commit because the config contains version-22-only keys (`CMakeLists.txt`, `.clang-format`).
- Python uses Ruff: 120 columns, 4 spaces, double quotes, LF, Python 3.13 syntax (`bindings/python/ruff.toml`). Its `format.bat` runs `ruff check . --fix` and `ruff format .` through uv (`bindings/python/format.bat`).
- JS uses Biome: 2 spaces, 100 columns, double quotes and semicolons (`bindings/js/biome.json`, `bindings/js/package.json`).
- Dart uses `dart format .`, page width 120 and preserved trailing commas (`bindings/dart/format.bat`, `bindings/dart/analysis_options.yaml`).
- Julia's executable formatter is **Style.jl 0.1.0** through `Style.format(dirname(@__DIR__))`; follow that script rather than the root guide's JuliaFormatter label (`bindings/julia/format/format.jl`, `bindings/julia/format/Project.toml`, `AGENTS.md`).
- `scripts/format.bat` chains C++ and all four binding formatters. It expects a configured `build/` directory; preset builds reside under `build/dev/` and `build/release/` (`scripts/format.bat`, `CMakePresets.json`).
- Keep text LF via `.gitattributes`; byte-exact CSV/XLSX fixtures are excluded from normalization and applicable pre-commit rewriting (`.gitattributes`, `.pre-commit-config.yaml`, `tests/fixtures/`).

**Linting:**

- `.clang-tidy` enables bugprone, modernize, performance, identifier naming, redundant and simplification checks, with explicit exclusions. Warnings are not configured as errors there; do not silently widen the rule set.
- `scripts/tidy.bat` uses `run-clang-tidy`, strips a MinGW flag in the compilation database, and excludes dependency sources and `src/binary/`.
- C++ compiler options enable `/W4`, `/permissive-`, `/utf-8` on MSVC and `-Wall -Wextra -Wpedantic` elsewhere (`cmake/CompilerOptions.cmake`).
- Python lint selects only Ruff's import-sort rule family `I`; no Python static type-checking gate is configured (`bindings/python/ruff.toml`, `bindings/python/AGENTS.md`).
- Dart includes recommended lints and excludes generated FFI declarations (`bindings/dart/analysis_options.yaml`). JS enables Biome recommended rules (`bindings/js/biome.json`).
- Pre-commit runs cppcheck, clang-format, whitespace/LF cleanup, YAML/JSON validation, conflict-marker and large-file checks (`.pre-commit-config.yaml`).
- Workflow wiring is checked with actionlint 1.7.9; shellcheck integration is explicitly disabled (`.github/workflows/ci.yml`).

## Import Organization

**Order:**

1. C++ associated header first, then quoted project headers (`.clang-format`, `src/c/database_create.cpp`).
2. Third-party angle-bracket headers containing paths (`<gtest/gtest.h>`, `<quiver/database.h>`), then bare standard headers (`<string>`) (`.clang-format`, `tests/test_database_create.cpp`).
3. Python future annotations, standard library, third-party and package imports, separated by blank lines (`bindings/python/tests/conftest.py`, `bindings/python/src/quiverdb/_helpers.py`).
- Julia modules group `using` statements then `include` source files (`bindings/julia/test/test_database_create.jl`, `bindings/julia/src/Quiver.jl`).
- Dart imports packages and local files; implementation parts share the parent library (`bindings/dart/lib/src/database.dart`, `bindings/dart/lib/src/exceptions.dart`). JS uses explicit relative `.ts` imports and `import type` for type-only imports (`bindings/js/test/database-create.test.ts`).

**Path Aliases:**

- C++ installed headers use `quiver/...`; source implementation headers use relative quoted names (`src/database_create.cpp`, `tests/test_database_create.cpp`).
- Python uses `quiverdb.*`, Dart uses `package:quiverdb/...`, and Julia uses the `Quiver` module (`bindings/python/tests/conftest.py`, `bindings/dart/test/database_create_test.dart`, `bindings/julia/test/test_database_create.jl`).
- No JS path-alias configuration detected; use the existing relative `.ts` paths (`bindings/js/src/index.ts`, `bindings/js/test/package-entry.test.ts`).

## Error Handling

**Patterns:**

- C++ database errors use `std::runtime_error` and one of three messages: `Cannot {operation}: {reason}`, `{Entity} not found: {identifier}`, or `Failed to {operation}: {reason}` (`AGENTS.md`, `src/database_create.cpp`). Pass the public caller's operation name through validators (`src/type_validator.cpp`).
- Metadata validation in binary/expression has documented descriptive-message exceptions; new code follows the three patterns (`AGENTS.md`, `src/binary/binary_metadata.cpp`).
- Validate scalar and array inputs before the first write; use `Impl::TransactionGuard`, which becomes a no-op inside a caller-owned transaction. Preserve the documented SQLite-constraint limit and absence of SAVEPOINTs (`src/database_create.cpp`, `src/database_impl.h`, `src/AGENTS.md`).
- C entry points validate required pointers with `QUIVER_REQUIRE`, catch `std::exception`, store `e.what()` and return `QUIVER_ERROR`; successful calls return `QUIVER_OK` and values through out-parameters (`src/c/internal.h`, `src/c/database_create.cpp`).

```cpp
// src/c/database_create.cpp
QUIVER_REQUIRE(db, collection, element, out_id);
try {
    *out_id = db->db.create_element(collection, element->element);
    return QUIVER_OK;
} catch (const std::exception& e) {
    quiver_set_last_error(e.what());
    return QUIVER_ERROR;
}
```

- Read the single thread-local error channel only after failure; success leaves previous text intact (`src/c/common.cpp`, `src/c/AGENTS.md`).
- Binding `check` helpers surface core text as Python/JS `QuiverError` or Julia/Dart `DatabaseException` (`bindings/python/src/quiverdb/_helpers.py`, `bindings/js/src/errors.ts`, `bindings/julia/src/exceptions.jl`, `bindings/dart/lib/src/exceptions.dart`).
- Local errors are permitted for pre-FFI marshalling and binding-only boolean/datetime conversion; name the offending column/type where available (`AGENTS.md`, `bindings/python/src/quiverdb/_helpers.py`).
- Pair every native allocation with its entity-specific free; use `finally` for decode failures. Sandbox result strings require `quiver_sandbox_free_string` (`src/c/AGENTS.md`, `bindings/python/src/quiverdb/sandbox.py`, `bindings/dart/lib/src/sandbox.dart`).

## Logging

**Framework:** C++ per-database spdlog logger (`src/database.cpp`, `src/database_impl.h`).

**Patterns:**

- Use `impl_->logger->debug/info/warn/error`, never global spdlog functions. The logger owns stderr and file sinks and has a unique database name (`src/AGENTS.md`, `src/database.cpp`, `src/database_create.cpp`).
- Options control `console_level`; tests normally use `LogLevel::Off` or `quiver::test::quiet_options()` (`tests/test_database_create.cpp`, `tests/test_utils.h`).
- Bindings generally forward failures, with a warning/fallback for an empty C error string in Julia and Dart (`bindings/julia/src/exceptions.jl`, `bindings/dart/lib/src/exceptions.dart`).
- Lua errors propagate through Sandbox rather than writing diagnostics to stderr; regression assertions cover both caught and propagated errors (`tests/test_sandbox_errors.cpp`).

## Comments

**When to Comment:**

- Explain invariants, resource ownership and ordering that affect correctness. The comment before pre-validation in `src/database_create.cpp` explains why a caller-owned transaction makes that order necessary.
- Keep area guidance in the nearest `AGENTS.md` and update it when changing that area; user-visible changes belong in the unreleased `CHANGELOG.md` section (`AGENTS.md`).
- Generated FFI declarations have generation notices; regenerate the appropriate binding rather than hand-editing generated Dart/Julia files (`bindings/dart/pubspec.yaml`, `bindings/julia/src/c_api.jl`, `bindings/julia/AGENTS.md`). Python `_c_api.py` is deliberately hand-maintained (`bindings/python/AGENTS.md`).
- Avoid comments that resemble Lua usertype/open-library registration calls: the API synchronization parser counts these patterns (`src/AGENTS.md`, `bindings/js/test/sandbox-api-sync.test.ts`).

**JSDoc/TSDoc:**

- Python uses module/function docstrings and type annotations (`bindings/python/src/quiverdb/_helpers.py`, `bindings/python/tests/conftest.py`). Dart public helpers use `///` documentation (`bindings/dart/lib/src/exceptions.dart`). Julia public APIs use docstrings (`bindings/julia/src/database.jl`).
- The JS sandbox reference is a public text constant checked against the C++ binders; maintain it alongside binding changes (`bindings/js/src/sandbox-api.ts`, `bindings/js/test/sandbox-api-sync.test.ts`).

## Function Design

**Size:**

- Split concerns into existing area modules rather than generic forwarding frameworks. Lua binder translation units target roughly 450 lines or fewer (`src/AGENTS.md`, `src/sandbox/`).
- Preserve expanded per-method FFI call blocks. Shared data codecs are appropriate; closure-parameterized FFI abstraction is explicitly rejected (`AGENTS.md`, `bindings/python/AGENTS.md`, `bindings/dart/AGENTS.md`).

**Parameters:**

- Prefer `const` references for C++ inputs; options default to `{}` on C++ factories and NULL means default options in C (`include/quiver/database.h`, `src/c/AGENTS.md`).
- Python create/update/upsert methods accept `**kwargs` and declare leading parameters positional-only so attribute names cannot collide with parameter names (`bindings/python/src/quiverdb/database.py`, `bindings/python/AGENTS.md`).
- Keep group writes column-oriented in FFI bindings and row-oriented in C++; vector/set whole-group reads remain row-oriented by decision (`AGENTS.md`, `src/c/database_helpers.h`).
- Lua trust boundaries take `sol::object` and explicitly check table/function types before walking them; reuse helpers in `src/sandbox/internal.h` (`src/AGENTS.md`, `src/sandbox/csv.cpp`).

**Return Values:**

- C++ returns value types and `std::optional` for SQL NULL/missing values; C returns status plus values and presence masks through out-parameters (`include/quiver/database.h`, `include/quiver/c/database.h`).
- Preserve NULL positions and one bulk entry per element; never drop NULL cells during binding decoding (`AGENTS.md`, `src/database_read.cpp`, `src/c/database_read.cpp`).
- Python/Dart/JS have statically nullable collection elements; Julia uses schema nullability to select concrete versus optional arrays. Lua uses nil holes with a separate count authority (`AGENTS.md`, `bindings/julia/src/database_read.jl`, `src/sandbox/database_read.cpp`).
- `Sandbox::run` returns JSON text; bindings pass it through without parsing (`src/sandbox/return_json.cpp`, `AGENTS.md`).

## Module Design

**Exports:**

- C++ public headers stay in `include/quiver/`; internal implementation helpers stay in `src/`. Hide private library dependencies with Pimpl; plain value types use direct members and Rule of Zero (`src/AGENTS.md`, `include/quiver/element.h`).
- The core owns business logic; C ABI and language wrappers marshal values and resource lifetimes (`AGENTS.md`, `src/c/database_create.cpp`).
- C ABI allocators and matching frees are co-located by functional area (`src/c/database_read.cpp`, `src/c/database_metadata.cpp`, `src/c/database_time_series.cpp`, `src/c/AGENTS.md`).
- JS category modules augment the Database surface and are imported for their side effects by `bindings/js/src/index.ts`; wire new categories there (`bindings/js/src/index.ts`, `bindings/js/src/create.ts`).
- Dart Database operation files are `part` files, Python CSV support uses mixins, and Julia includes source into its module (`bindings/dart/lib/src/database.dart`, `bindings/python/src/quiverdb/database.py`, `bindings/julia/src/Quiver.jl`).

**Barrel Files:**

- C++ umbrella: `include/quiver/quiver.h`. Python public export list: `bindings/python/src/quiverdb/__init__.py`; do not export internal Element.
- Dart public barrel: `bindings/dart/lib/quiverdb.dart`. JS barrels: `bindings/js/mod.ts` and `bindings/js/src/index.ts`; `bindings/js/test/package-entry.test.ts` verifies package exports.
- Preserve intentional per-binding omissions: binary/expression are Julia/Lua only, JS dates stay strings, Lua lacks whole-group and boolean convenience readers, CSV/XLSX readers and CSV writing are Lua-only (`AGENTS.md`, `tests/AGENTS.md`).

---

*Convention analysis: 2026-10-07*
