# Codebase Structure

**Analysis Date:** 2026-10-02

## Directory Layout

```
quiver1/
├── include/quiver/          # Public C++ headers (database.h, element.h, value.h, lua_runner.h, ...)
│   ├── binary/              # BinaryFile, BinaryMetadata, CSVConverter, dimensions, time props
│   ├── expression/          # Expression + node classes
│   └── c/                   # C API headers (common.h, database.h, element.h, options.h, lua_runner.h)
│       ├── binary/
│       └── expression/
├── src/                     # C++ core implementation (+ src/AGENTS.md)
│   ├── database*.cpp        # Database methods split by verb
│   ├── database_impl.h      # Database::Impl (private)
│   ├── csv/                 # Internal CSV reader/writer
│   ├── binary/              # .qvr subsystem
│   ├── expression/          # Expression DAG nodes
│   ├── utils/               # string.h, datetime.h, number.h (header-only)
│   ├── cli/main.cpp         # quiver_cli
│   └── c/                   # C API implementation (+ src/c/AGENTS.md)
│       ├── binary/
│       └── expression/
├── bindings/
│   ├── julia/               # Quiver.jl: src/, test/, generator/ (Clang.jl)
│   ├── dart/                # quiverdb: lib/src/, lib/src/ffi/, hook/build.dart, test/, generator/
│   ├── python/              # quiverdb: src/quiverdb/, tests/, generator/
│   └── js/                  # quiverdb (Bun): src/, test/, mod.ts
├── tests/                   # GoogleTest suites (test_*.cpp, test_c_api_*.cpp)
│   ├── schemas/             # valid/, invalid/, issues/, migrations/ — shared by all bindings
│   ├── fixtures/            # Byte-exact CSV fixtures (-text in .gitattributes)
│   ├── benchmark/           # quiver_benchmark (manual)
│   └── sandbox/             # quiver_sandbox scratch target (intentional)
├── cmake/                   # CompilerOptions, Dependencies (FetchContent), Platform, config template
├── scripts/                 # build-all/test-all/clean-all/format/tidy/generator .bat, assert_version.py, ci/
├── docs/                    # User docs (introduction, rules, attributes, migrations, time_series)
├── .github/                 # workflows/ (ci, publish-*, bump-version) + composite actions
├── CMakeLists.txt           # Root build; project VERSION is the version source of truth
├── CMakePresets.json        # dev/release presets
└── AGENTS.md                # Root agent guide (nested AGENTS.md per area)
```

## Directory Purposes

**`include/quiver/`:** public C++ API. Key: `database.h`, `element.h`, `value.h`, `options.h`, `attribute_metadata.h`, `quiver.h` (umbrella).
**`src/`:** core implementation. Key: `database.cpp` (lifecycle, `Impl::execute`), `database_impl.h`, `schema_validator.cpp`, `type_validator.cpp`, `lua_runner.cpp`.
**`src/c/`:** C ABI. Key: `internal.h` (handle structs, `QUIVER_REQUIRE`), `database_helpers.h` (marshaling templates), `common.cpp` (last error).
**`bindings/julia/src/`:** `Quiver.jl` module root, `c_api.jl` (generated), `database_<verb>.jl` mirroring core split, `binary/`, `expression.jl`, `helper_maps.jl`.
**`bindings/dart/lib/src/`:** `database.dart` + `part`-style `database_<verb>.dart`, `ffi/bindings.dart` (ffigen), `hook/build.dart` builds native via CMake.
**`bindings/python/src/quiverdb/`:** `database.py` (single large class), `_c_api.py` (hand-maintained cdefs), `_loader.py`, `_helpers.py` (`check`).
**`bindings/js/src/`:** `loader.ts` (hand-written symbol table), feature modules (`read.ts`, `create.ts`, `time-series.ts`, ...), `lua-api.ts` (`LUA_DB_API_REFERENCE`).
**`tests/`:** C++ suites by feature; Lua tests `test_lua_runner_*.cpp`; C API tests `test_c_api_*.cpp`.

## Key File Locations

**Entry Points:**
- `src/cli/main.cpp`: CLI
- `include/quiver/c/database.h`: FFI entry
- `bindings/julia/src/Quiver.jl`, `bindings/dart/lib/quiverdb.dart`, `bindings/python/src/quiverdb/__init__.py`, `bindings/js/src/index.ts`

**Configuration:**
- `CMakeLists.txt`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `cmake/Dependencies.cmake`, `cmake/Platform.cmake`
- `.clang-format`, `.clang-tidy`, `.pre-commit-config.yaml`
- Binding manifests: `bindings/python/pyproject.toml`, `bindings/js/package.json`, `bindings/dart/pubspec.yaml`, `bindings/julia/Project.toml`

**Core Logic:**
- `src/database_*.cpp`, `src/database_impl.h`, `src/database_internal.h`, `src/schema*.cpp`

**Testing:**
- `tests/test_database_*.cpp`, `tests/test_c_api_*.cpp`, `tests/schemas/`
- `bindings/julia/test/`, `bindings/dart/test/`, `bindings/python/tests/`, `bindings/js/test/`

## Naming Conventions

**Files:**
- Core and C API split by verb: `database_<verb>.cpp` (`database_read.cpp`, `src/c/database_read.cpp`)
- Tests: `test_<area>.cpp`, `test_c_api_<area>.cpp`, `test_lua_runner_<area>.cpp`; Dart `*_test.dart`; Python `test_*.py`
- Schemas: snake_case `.sql` under `tests/schemas/{valid,invalid,issues}/`, migrations as numbered dirs under `tests/schemas/migrations/`

**Directories:**
- lowercase, one per subsystem (`binary/`, `expression/`, `csv/`, `utils/`)

**Symbols:** `verb_[category_]type[_by_id|_by_label]` in C++; derived names per layer (root `AGENTS.md`, "Cross-Layer Naming Conventions").

## Where to Add New Code

**New Database method:**
- Declaration: `include/quiver/database.h`; implementation in matching `src/database_<verb>.cpp`
- C API: `include/quiver/c/database.h` + `src/c/database_<verb>.cpp`
- Regenerate FFI: `scripts/generator.bat`; update `bindings/js/src/loader.ts` and `bindings/python/src/quiverdb/_c_api.py` by hand
- Bindings: `bindings/julia/src/database_<verb>.jl`, `bindings/dart/lib/src/database_<verb>.dart`, `bindings/python/src/quiverdb/database.py`, matching `bindings/js/src/*.ts`; Lua in `src/lua_runner.cpp` (+ `bindings/js/src/lua-api.ts` reference)
- Tests: `tests/test_database_<area>.cpp`, `tests/test_c_api_database_<area>.cpp`, each binding's test dir; schemas in `tests/schemas/valid/`
- Changelog: `CHANGELOG.md`

**Binary/expression feature:** `src/binary/` or `src/expression/expression_<node>.cpp`, headers under `include/quiver/{binary,expression}/`, C API in `src/c/{binary,expression}/`, Julia in `bindings/julia/src/binary/` or `expression.jl`, Lua in `src/lua_runner.cpp`.

**Utilities:**
- Header-only helpers: `src/utils/`; DB-internal helpers: `src/database_internal.h`; C marshaling: `src/c/database_helpers.h`

## Special Directories

**`build/`:** CMake output (`build/bin/`, `build/lib/`). Generated: Yes. Committed: No.
**`bindings/julia/src/c_api.jl`, `bindings/dart/lib/src/ffi/bindings.dart`:** Generated: Yes. Committed: Yes.
**`bindings/js/node_modules/`, `bindings/python/src/quiverdb/__pycache__/`:** Generated: Yes. Committed: No.
**`tests/sandbox/`:** intentional scratch target — do not delete.
**`tests/fixtures/`:** byte-exact CSVs, excluded from whitespace hooks.
**`.planning/`:** GSD planning artifacts.

---

*Structure analysis: 2026-10-02*
