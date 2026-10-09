---
last_mapped_commit: cf9ca58aceab305fe6a08461f78da8556a6c95d0
last_mapped_at: 2026-10-07
---
# Codebase Structure

**Analysis Date:** 2026-10-07

## Directory Layout

```text
quiver1/
├── AGENTS.md                  # Cross-cutting rules and settled API decisions
├── CMakeLists.txt             # Project version, targets, options, install/wheel packaging
├── CMakePresets.json          # Build presets
├── CHANGELOG.md               # User-visible changes
├── include/quiver/            # Installed public C++ headers
│   ├── binary/                # Binary files, metadata, dimensions and iteration
│   ├── expression/            # AbstractExpression, Expression and node API
│   └── c/                     # Installed C ABI headers
│       ├── binary/            # Binary C ABI
│       └── expression/        # Expression C ABI
├── src/                       # Core implementation and private headers
│   ├── AGENTS.md              # Core and internal-subsystem instructions
│   ├── database_*.cpp         # Database operation areas
│   ├── binary/                # Binary files, metadata, calendar traversal and CSV conversion
│   ├── expression/            # Expression node implementations and save engine
│   ├── c/                     # C ABI adapters; its own AGENTS.md
│   │   ├── binary/            # Binary ABI implementations
│   │   └── expression/        # Expression ABI implementation
│   ├── sandbox/               # Direct C++/sol2 Lua adapters, path policy, JSON encoding
│   ├── csv/                   # Internal shared CSV reader/writer
│   ├── xlsx/                  # Internal OpenXLSX reader
│   ├── utils/                 # Header-only string/date/number helpers
│   └── cli/                   # CLI entry point
├── bindings/                  # Canonical host-language source packages
│   ├── julia/                 # Quiver.jl; src/, generator/, test/, format/, revise/
│   ├── dart/                  # quiverdb; lib/, generator/, hook/, test/
│   ├── python/                # quiverdb; src/quiverdb/, generator/, tests/
│   └── js/                    # Bun quiverdb; mod.ts, src/, test/
├── tests/                     # C++ and C ABI test suites
│   ├── schemas/               # Shared SQL fixtures for every language layer
│   │   ├── valid/             # Valid schemas
│   │   ├── invalid/           # Single-rule invalid schemas
│   │   ├── migrations/        # Numbered up.sql/down.sql examples
│   │   └── issues/            # Issue-specific migration regressions
│   ├── fixtures/              # Byte-sensitive CSV and deterministic XLSX fixtures
│   ├── benchmark/             # Transaction benchmark target
│   └── sandbox/               # Intentional scratch executable
├── cmake/                     # Dependencies, compiler options, platform and package export
├── scripts/                   # Build/test/format/generate/version/packaging helpers
│   ├── ci/                    # Workflow dispatch and native S3 helper scripts
│   └── julia/                 # Artifact manifest generation
├── .github/                   # CI and package/native publishing workflows
│   ├── actions/build-cpp/     # Composite native-build action
│   └── workflows/             # CI, publish, S3 and version-bump workflows
├── docs/                      # User-facing schema/API conceptual documentation
├── assets/                    # Logo
├── .planning/codebase/        # Architecture and codebase maps
└── build/                     # Ignored generated native output/dependencies
```

## Directory Purposes

**`include/quiver/`:**

- Purpose: Public installed C++ API; keep private SQLite/parser/sol2 implementation types out of these headers.
- Contains: Database/value/result/metadata/options/migration headers and `binary/` / `expression/` APIs.
- Key files: `include/quiver/database.h`, `include/quiver/quiver.h`, `include/quiver/element.h`, `include/quiver/value.h`, `include/quiver/attribute_metadata.h`, `include/quiver/sandbox.h`.

**`src/`:**

- Purpose: Core behavior and internal implementation shared by every binding.
- Contains: Database facade implementation split by operation, schema/type validation, UI metadata and value implementations.
- Key files: `src/database.cpp`, `src/database_impl.h`, `src/database_internal.h`, `src/schema.cpp`, `src/schema_validator.cpp`, `src/type_validator.cpp`, `src/CMakeLists.txt`, `src/AGENTS.md`.

**`include/quiver/c/` and `src/c/`:**

- Purpose: Public FFI ABI and its native adapter implementation.
- Contains: Opaque handles, status/enum types, output-pointer factories, allocations/frees and presence-mask conversions.
- Key files: `include/quiver/c/database.h`, `include/quiver/c/common.h`, `src/c/internal.h`, `src/c/common.cpp`, `src/c/database_helpers.h`, `src/c/AGENTS.md`.

**`src/sandbox/`:**

- Purpose: Embedded Lua bindings and script-access policy; register Database once and extend it with area binders.
- Contains: `database_*.cpp` bindings, binary/expression binding modules, CSV/XLSX binders, shared converters, run cleanup, JSON encoder and path policy.
- Key files: `src/sandbox/sandbox.cpp`, `src/sandbox/internal.h`, `src/sandbox/path_policy.cpp`, `src/sandbox/return_json.cpp`, `src/sandbox/csv.cpp`, `src/sandbox/xlsx.cpp`.

**`src/binary/` and `src/expression/`:**

- Purpose: Binary dataset storage and lazy file-backed computations, independent of SQLite database operations.
- Contains: File streams, metadata/TOML serialization, calendar-coordinate traversal, CSV conversion, expression nodes and row-wise evaluation.
- Key files: `src/binary/binary_file.cpp`, `src/binary/binary_metadata.cpp`, `src/binary/binary_utils.h`, `src/binary/iteration.cpp`, `src/expression/expression.cpp`, `src/expression/expression_helpers.h`.

**`src/csv/` and `src/xlsx/`:**

- Purpose: Internal format helpers grouped by format rather than consumer; these have no public installed headers or FFI surface.
- Contains: Pimpl readers and a plain CSV writer/shared record emitter.
- Key files: `src/csv/csv_read.h`, `src/csv/csv_read.cpp`, `src/csv/csv_write.h`, `src/csv/csv_write.cpp`, `src/xlsx/xlsx_read.h`, `src/xlsx/xlsx_read.cpp`.

**`bindings/julia/`:**

- Purpose: Canonical Quiver.jl source; the published Julia repository is a generated mirror.
- Contains: Hand-written operation wrappers, generated C module, binary/expression wrappers and test/generator/formatter environments.
- Key files: `bindings/julia/src/Quiver.jl`, `bindings/julia/src/c_api.jl`, `bindings/julia/src/database.jl`, `bindings/julia/src/binary/Binary.jl`, `bindings/julia/src/expression.jl`, `bindings/julia/src/helper_maps.jl`, `bindings/julia/AGENTS.md`.

**`bindings/dart/`:**

- Purpose: Dart package with hand-written high-level wrappers and native-assets build hook.
- Contains: Database part/extension files, generated ffigen declarations, hand-written loader and tests.
- Key files: `bindings/dart/lib/quiverdb.dart`, `bindings/dart/lib/src/database.dart`, `bindings/dart/lib/src/ffi/bindings.dart`, `bindings/dart/lib/src/ffi/library_loader.dart`, `bindings/dart/hook/build.dart`, `bindings/dart/pubspec.yaml`, `bindings/dart/AGENTS.md`.

**`bindings/python/`:**

- Purpose: CFFI ABI-mode package and native-wheel packaging.
- Contains: Public Database/Sandbox/metadata, internal Element, hand-maintained C declarations, native loader and CSV mixins.
- Key files: `bindings/python/src/quiverdb/__init__.py`, `bindings/python/src/quiverdb/database.py`, `bindings/python/src/quiverdb/_c_api.py`, `bindings/python/src/quiverdb/_loader.py`, `bindings/python/pyproject.toml`, `bindings/python/AGENTS.md`.

**`bindings/js/`:**

- Purpose: Bun-specific TypeScript package using `bun:ffi`.
- Contains: Lifecycle class, operation modules attached to its prototype, manual FFI table, column marshalling and shipped Lua reference string.
- Key files: `bindings/js/mod.ts`, `bindings/js/src/index.ts`, `bindings/js/src/database.ts`, `bindings/js/src/loader.ts`, `bindings/js/src/group-columns.ts`, `bindings/js/src/sandbox-api.ts`, `bindings/js/AGENTS.md`.

**`tests/`:**

- Purpose: Native C++/C ABI behavioral coverage and shared fixture authority.
- Contains: Area-specific `test_*.cpp`, `test_c_api_*.cpp` and `test_sandbox_*.cpp`, reusable fixtures, shared SQL and file fixtures.
- Key files: `tests/CMakeLists.txt`, `tests/test_utils.h`, `tests/test_sandbox.h`, `tests/schemas/valid/collections.sql`, `tests/fixtures/generate_xlsx.py`, `tests/AGENTS.md`.

**`cmake/`, `scripts/` and `.github/`:**

- Purpose: Native build configuration, local automation and release orchestration.
- Contains: FetchContent dependencies/compiler policy/platform setup, batch/shell/Python/Julia scripts and hosted workflow definitions.
- Key files: `cmake/Dependencies.cmake`, `cmake/CompilerOptions.cmake`, `cmake/Platform.cmake`, `scripts/build-all.bat`, `scripts/test-all.bat`, `scripts/generator.bat`, `scripts/assert_version.py`, `.github/workflows/ci.yml`, `.github/workflows/publish.yml`.

## Key File Locations

**Entry Points:**

- `include/quiver/quiver.h`: C++ umbrella header.
- `include/quiver/database.h`: Database facade declaration.
- `include/quiver/c/database.h`: Main C ABI database surface.
- `src/cli/main.cpp`: Native Lua-script CLI.
- `src/sandbox/sandbox.cpp`: Lua state construction and script execution.
- `bindings/julia/src/Quiver.jl`: Julia module assembly.
- `bindings/dart/lib/quiverdb.dart`: Dart public exports.
- `bindings/python/src/quiverdb/__init__.py`: Python public exports.
- `bindings/js/mod.ts`: JS package entry, re-exporting `bindings/js/src/index.ts`.

**Configuration:**

- `AGENTS.md`: Cross-layer rules, ownership, API omissions and version/changelog requirements.
- `CMakeLists.txt`, `CMakePresets.json`, `src/CMakeLists.txt`: Native project/build/target configuration.
- `.clang-format`, `.clang-tidy`, `.clangd`: C++ formatting, analysis and editor configuration.
- `.pre-commit-config.yaml`, `.gitattributes`, `codecov.yml`: Repository checks, byte normalization and coverage configuration.
- `bindings/julia/Project.toml`, `bindings/dart/pubspec.yaml`, `bindings/python/pyproject.toml`, `bindings/js/package.json`: Binding package manifests; versions follow native project version.
- `bindings/dart/pubspec.yaml`: Active inline ffigen configuration; `bindings/dart/ffigen.yaml` is an unused duplicate unless explicitly selected.
- `bindings/js/biome.json`, `bindings/python/ruff.toml`, `bindings/dart/analysis_options.yaml`: Binding-specific formatting/static analysis.

**Core Logic:**

- `src/database_impl.h`: Lazy schema publication, lookup helpers, prepared group writes and TransactionGuard.
- `src/database_internal.h`: Nullable read templates, typing helpers, metadata and time-series dimension discovery.
- `src/database_create.cpp`, `src/database_read.cpp`, `src/database_update.cpp`, `src/database_delete.cpp`: CRUD.
- `src/database_metadata.cpp`, `src/database_time_series.cpp`, `src/database_query.cpp`, `src/database_describe.cpp`: Metadata, time-series, SQL queries and text summaries.
- `src/database_csv_import.cpp`, `src/database_csv_export.cpp`: Database CSV semantics through shared format helpers.
- `src/schema.cpp`, `src/schema_validator.cpp`, `src/type_validator.cpp`: Schema introspection and validation.
- `src/migration.cpp`, `src/migrations.cpp`: Migration file access and ordered version discovery.
- `src/ui_metadata.cpp`: Optional sidecar loader; `src/database_describe.cpp` owns rendering.

**Testing:**

- `tests/test_database_*.cpp`: C++ Database suites.
- `tests/test_c_api_*.cpp`: C ABI suites and ownership/error transport checks.
- `tests/test_sandbox_*.cpp`: Direct Lua behavior, file policy and format tests.
- `tests/test_binary_*.cpp`, `tests/test_expression.cpp`, `tests/test_csv_converter.cpp`, `tests/test_iteration.cpp`: Binary/expression behavior.
- `bindings/julia/test/runtests.jl`, `bindings/dart/test/`, `bindings/python/tests/`, `bindings/js/test/`: Host-specific suites.
- `tests/schemas/`, `tests/fixtures/`: Shared data authority; do not duplicate SQL schemas under bindings.

## Naming Conventions

**Files:**

- C++ source/header pairs use snake_case and `.cpp` / `.h`: `src/database_time_series.cpp`, `include/quiver/database.h`.
- Database operation areas share `database_<area>` across core, C adapters, Lua adapters and Julia/Dart wrappers: `src/database_create.cpp`, `src/c/database_create.cpp`, `src/sandbox/database_create.cpp`, `bindings/julia/src/database_create.jl`, `bindings/dart/lib/src/database_create.dart`.
- Native tests use `test_<area>.cpp`; C ABI tests insert `c_api`; Lua tests insert `sandbox`: `tests/test_database_create.cpp`, `tests/test_c_api_database_create.cpp`, `tests/test_sandbox_create.cpp`.
- Julia/Python tests use `test_*.jl` / `test_*.py`; Dart uses `*_test.dart`; Bun uses `<area>.test.ts` with hyphenated area names (`bindings/julia/test/`, `bindings/python/tests/`, `bindings/dart/test/`, `bindings/js/test/`).
- Analysis documents use uppercase names under `.planning/codebase/`; area instructions use `AGENTS.md`.

**Directories:**

- Public API mirrors subsystem boundaries in implementation: `include/quiver/binary/` ↔ `src/binary/`, `include/quiver/c/` ↔ `src/c/`.
- Internal format helpers live under format-named directories, not under the Lua consumer: `src/csv/`, `src/xlsx/`.
- Migration directories are positive numeric versions, each containing `up.sql` and `down.sql`: `tests/schemas/migrations/1/`, `tests/schemas/migrations/2/`.

## Where to Add New Code

**New Database Feature:**

- Primary API: Add the public declaration to `include/quiver/database.h`; place behavior in the matching `src/database_<area>.cpp`.
- Shared state/helper: Use `src/database_impl.h` for instance-dependent internals; use `src/database_internal.h` for existing internal free/template patterns.
- ABI: Add declarations in `include/quiver/c/database.h`, implementations in the matching `src/c/database_<area>.cpp`; colocate new alloc/free APIs as required by `src/c/AGENTS.md`.
- Lua: Add to the matching `src/sandbox/database_<area>.cpp`; extend the single registered usertype instead of registering another (`src/sandbox/sandbox.cpp`).
- Host wrappers: Extend matching Julia/Dart operation files, Python `database.py` or established CSV mixins, and JS operation modules; synchronize generated/manual FFI interfaces (`bindings/julia/src/`, `bindings/dart/lib/src/`, `bindings/python/src/quiverdb/`, `bindings/js/src/`).
- Tests: Mirror behavior in relevant `tests/test_database_*.cpp`, `tests/test_c_api_*.cpp`, `tests/test_sandbox_*.cpp` and host suites, observing the documented subsystem omissions (`AGENTS.md`).

**New Component/Module:**

- Public C++ API: `include/quiver/<subsystem>/`; implementation: `src/<subsystem>/`; explicitly register new source files in `src/CMakeLists.txt`.
- Private format utility: Add a sibling format directory under `src/`, following `src/csv/` / `src/xlsx/`; do not add installed headers merely because Lua consumes it (`src/AGENTS.md`).
- New expression node: Declare alongside node classes in `include/quiver/expression/expression_node.h`, implement in `src/expression/expression_<kind>.cpp`, reuse `src/expression/expression_helpers.h`.
- Public binary/expression binding: Use C ABI plus Julia and direct Lua only, following the explicit coverage exception (`src/c/binary/`, `src/c/expression/`, `bindings/julia/src/binary/`, `bindings/julia/src/expression.jl`, `src/sandbox/binary.cpp`, `src/sandbox/expression.cpp`).

**Utilities:**

- Shared core text/date/number helpers: `src/utils/string.h`, `src/utils/datetime.h`, `src/utils/number.h`.
- C ABI conversions: `src/c/database_helpers.h`; opaque handle definitions and null-pointer macros: `src/c/internal.h`.
- Lua conversions and binder declarations: `src/sandbox/internal.h`; path checks: `src/sandbox/path_policy.cpp`.
- Host marshalling: `bindings/python/src/quiverdb/_helpers.py`, `bindings/js/src/ffi-helpers.ts`, `bindings/js/src/group-columns.ts`; retain language-local helpers rather than moving domain policy out of C++.

**Schemas and Fixtures:**

- Valid/invalid SQL: `tests/schemas/valid/`, `tests/schemas/invalid/`; every binding references these files (`tests/AGENTS.md`).
- Migration examples/regressions: `tests/schemas/migrations/`, `tests/schemas/issues/`.
- Byte-sensitive CSV/XLSX: `tests/fixtures/`; XLSX fixture source is `tests/fixtures/generate_xlsx.py` and binary fixture normalization is controlled by `.gitattributes`.

**Documentation and Package Changes:**

- User docs: `docs/introduction.md`, `docs/rules.md`, `docs/attributes.md`, `docs/migrations.md`, `docs/time_series.md` and binding READMEs.
- Lua prompt/API reference: `bindings/js/src/sandbox-api.ts`; its surface is checked by `bindings/js/test/sandbox-api-sync.test.ts`.
- Update nearest existing `AGENTS.md` when implementation guidance changes; user-visible behavior requires `CHANGELOG.md`; synchronized version policy is implemented by `scripts/assert_version.py` (`AGENTS.md`).

## Special Directories

**`build/`, `build-release/`, `out/`, `cmake-build-*/`:**

- Purpose: Native build output, CMake metadata and fetched dependencies; `build/` exists in this checkout.
- Generated: Yes.
- Committed: No; excluded by `.gitignore`.

**`bindings/julia/src/c_api.jl` and `bindings/dart/lib/src/ffi/bindings.dart`:**

- Purpose: Low-level generated FFI declarations.
- Generated: Yes; Julia generator uses `bindings/julia/generator/`, Dart generation uses the ffigen block in `bindings/dart/pubspec.yaml`.
- Committed: Yes; regenerate rather than edit manually (`bindings/julia/AGENTS.md`, `bindings/dart/AGENTS.md`).

**`bindings/python/src/quiverdb/_c_api.py` and `bindings/js/src/loader.ts`:**

- Purpose: Low-level FFI declarations and symbol signatures.
- Generated: No; Python's generator prints declarations as a diff aid, and JS is hand-written.
- Committed: Yes; manually synchronize with `include/quiver/c/` (`bindings/python/AGENTS.md`, `bindings/js/AGENTS.md`).

**`tests/schemas/` and `tests/fixtures/`:**

- Purpose: Shared SQL and exact-byte file fixtures.
- Generated: SQL/CSV are authored or committed source data; XLSX ZIP fixtures are reproducibly generated from `tests/fixtures/generate_xlsx.py`.
- Committed: Yes (`tests/AGENTS.md`, `.gitattributes`).

**`tests/sandbox/`:**

- Purpose: Intentional ad-hoc experiment target, not the implementation of the Lua Sandbox.
- Generated: No.
- Committed: Yes; preserve the target (`tests/CMakeLists.txt`, `tests/AGENTS.md`).

**`.planning/codebase/`:**

- Purpose: Reference maps consumed by planning and execution workflows.
- Generated: Yes, by repository analysis.
- Committed: Intended workflow artifacts; commit handling belongs to the mapping orchestrator.

---

*Structure analysis: 2026-10-07*
