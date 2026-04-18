# External Integrations

**Analysis Date:** 2026-04-18

Quiver is a library, not a networked service. "External integrations" here means embedded third-party libraries vendored via CMake FetchContent, plus the FFI bridge each language binding uses to talk to the native C API, plus the CI/publishing pipelines that ship the artifacts.

## APIs & External Services

**Runtime external services:** None. All work performed against local SQLite and local `.qvr` binary files.

**Build-time / CI services:**
- **GitHub** — FetchContent source for 7 of 8 C/C++ dependencies.
- **GitLab** — Lua via `gitlab.com/codelibre/lua/lua-cmake.git` (tag `lua-cmake/v5.4.8.0`).
- **Codecov** — via `codecov/codecov-action@v6` (4 jobs: cpp, julia, dart, python). Config: `codecov.yml`. Token: `CODECOV_TOKEN`.
- **PyPI** — via `pypa/gh-action-pypi-publish@release/v1`, trusted-publisher OIDC.
- **JSR** (`jsr.io`) — via `npx jsr publish --allow-slow-types`, OIDC.

## Data Storage

**SQLite 3.50.2 (embedded):**
- Vendored via FetchContent from `github.com/sjinks/sqlite3-cmake.git` (tag `v3.50.2`), declared in `cmake/Dependencies.cmake` lines 3-8. Linked PUBLIC as `SQLite::SQLite3` in `src/CMakeLists.txt`. Not linked against system libsqlite3.
- Direct C API calls in `src/database.cpp`, `src/schema.cpp`, `src/database_create.cpp`, `src/database_impl.h`. Uses `sqlite3_open_v2`, `sqlite3_prepare_v2`, `sqlite3_bind_*`, `sqlite3_step`, `sqlite3_column_*`, `sqlite3_get_autocommit`, `sqlite3_exec`, `sqlite3_close_v2`, `sqlite3_finalize`, `sqlite3_errmsg`, `sqlite3_initialize`.
- One-time init: `sqlite3_initialize()` guarded by `std::once_flag` (`src/database.cpp` lines 20-24).
- Open flags: `SQLITE_OPEN_READONLY` if read-only, else `SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE` (`src/database.cpp` line 102).
- RAII statement wrapper: `using StmtPtr = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;` (`src/database_impl.h` line 18).

**Pragmas used:**
- `PRAGMA foreign_keys = ON;` unconditionally on open (`src/database.cpp` line 116).
- `PRAGMA foreign_keys = OFF;` / `ON;` toggled around bulk CSV imports (9 sites in `src/database_csv_import.cpp` at lines 276, 279, 281, 370, 473, 476, 602, 676, 679).
- `PRAGMA user_version;` reads, `PRAGMA user_version = N;` writes (`src/database.cpp` lines 216, 264).
- `PRAGMA table_info(t)` (`src/schema.cpp` line 337).
- `PRAGMA foreign_key_list(t)` (`src/schema.cpp` line 375).
- `PRAGMA index_list(t)` + `PRAGMA index_info(idx)` (`src/schema.cpp` lines 407, 423).

**Schema constraints enforced by `src/schema_validator.cpp`:**
- `STRICT` tables used throughout test schemas (`tests/schemas/`).
- Mandatory `Configuration` table with `id INTEGER PRIMARY KEY, label TEXT UNIQUE NOT NULL`.
- Collection names may not contain underscores; reserved table suffixes `_vector_`, `_set_`, `_time_series_`, `_time_series_files`.
- Foreign keys must use `ON DELETE CASCADE ON UPDATE CASCADE`.

**Transactions:**
- Raw `BEGIN TRANSACTION;`, `COMMIT;`, `ROLLBACK;` via `sqlite3_exec` in `src/database_impl.h` lines 302-335.
- `TransactionGuard` (lines 337-365) — nest-aware RAII using `sqlite3_get_autocommit()` to detect outer explicit transaction.
- Public: `begin_transaction()`, `commit()`, `rollback()`, `in_transaction()`.

**Binary `.qvr` files:**
- `BinaryFile` (Pimpl) + `CSVConverter` (Pimpl) in `src/binary/`.
- TOML sidecar metadata parsed/serialized via `toml++`.
- In-process write registry (`static std::unordered_set<std::string>`) prevents concurrent read+write on same path. Not thread-safe or multi-process safe.

**File storage:** Local only (`std::filesystem`). No S3/GCS/Azure.

**Caching:** Schema metadata cached in `Database::Impl::schema` after first `load_schema_metadata()` (`src/database_impl.h` line 286).

## Authentication & Identity

None — library is local-only.

## Monitoring & Observability

**spdlog 1.17.0:**
- FetchContent tag `v1.17.0` (`cmake/Dependencies.cmake` lines 17-22).
- Per-database logger via `create_database_logger()` in `src/database.cpp` lines 43-88. Unique names `quiver_database_{N}` with atomic counter.
- Console sink: `stderr_color_sink_mt` at caller's level.
- File sink: `basic_file_sink_mt` writing `quiver_database.log` beside the `.db` file, truncated at each open. Skipped for `:memory:`.

**Error reporting:** Only `std::runtime_error` with three fixed patterns (see `CLAUDE.md` error handling section). C API surfaces via `quiver_set_last_error` / `quiver_get_last_error` (thread-local). No Sentry / external tracker.

**Metrics:** None.

## CI/CD & Deployment

**CI — `.github/workflows/ci.yml`** (triggers: push/PR to `master`):

| Job | Runner | Notes |
|-----|--------|-------|
| `build` | `{ubuntu,windows,macos}-latest` × `{Release, Debug}` | CMake + ctest `--output-on-failure`. Uploads `build/lib/*` + `build/bin/*` |
| `cpp-coverage` | ubuntu-latest | `--coverage` flags, lcov, Codecov `flags: cpp`, `fail_ci_if_error: true` |
| `julia-coverage` | ubuntu-latest × `[1.11, 1.12]` | `julia-actions/setup-julia@v2`, `julia-buildpkg@v1`, `julia-runtest@v1` with `coverage: true`, `julia-processcoverage@v1`. Uses `LD_LIBRARY_PATH=build/lib` |
| `dart-coverage` | ubuntu-latest | `dart-lang/setup-dart@v1` SDK `3.10.7`. Copies `.so` files into `bindings/dart/`. `dart test`, then `dart pub global run coverage:test_with_coverage`. Excludes generated FFI |
| `python-coverage` | ubuntu-latest | `astral-sh/setup-uv@v7`, `uv pip install pytest pytest-cov`, `uv pip install -e .`, `LD_LIBRARY_PATH=build/lib` |
| `clang-format` | ubuntu-latest | Installs LLVM 21 via `apt.llvm.org/llvm.sh`, runs `clang-format-21 --dry-run --Werror` on `include src tests` |
| `deno-test` | ubuntu-latest | `denoland/setup-deno@v2` pinned `2.7.12`, builds C++, `deno task test` |

Action versions: `actions/checkout@v6`, `actions/upload-artifact@v7`, `codecov/codecov-action@v6`.

**Wheel build — `.github/workflows/build-wheels.yml`** (push/PR to master):
- `pypa/cibuildwheel@v3.4.1` on `[ubuntu-latest, windows-latest]`, `package-dir: bindings/python`, `output-dir: wheelhouse`, 7-day retention.

**Publish — `.github/workflows/publish.yml`** (tags `v*.*.*`):
1. `build` — cibuildwheel 3.4.1 matrix.
2. `merge` — combines per-OS wheel artifacts.
3. `validate` — parses `pyproject.toml` via `tomllib`, checks tag matches version, asserts exactly 2 wheels.
4. `release` — `softprops/action-gh-release@v2` with `generate_release_notes: true`.
5. `publish` — `pypa/gh-action-pypi-publish@release/v1` in env `pypi`, OIDC (`id-token: write`).
6. `build-native` — matrix ubuntu-latest/windows-latest. Linux patches RPATH with `patchelf --set-rpath '$ORIGIN' build/lib/libquiver_c.so`.
7. `publish-jsr` — downloads native libs into `bindings/js/libs/{platform_key}/`, validates `deno.json` version, runs `npx jsr publish --allow-slow-types` in env `jsr`, OIDC.

**Dependabot:** `.github/dependabot.yml` — weekly updates for `github-actions` ecosystem only.

**Pre-commit hooks:**
- `pre-commit/pre-commit-hooks v6.0.0` — trailing-whitespace, end-of-file-fixer, check-yaml, check-json, check-merge-conflict, mixed-line-ending (`--fix=lf`), check-added-large-files (`--maxkb=1024`).
- `pocc/pre-commit-hooks v1.3.5` — clang-format, cppcheck on `^(include|src|tests)/.*\.(cpp|h)$`.
- `cheshirekow/cmake-format-precommit v0.6.13` — cmake-format on `CMakeLists.txt|\.cmake$`.

## FFI Surface (bindings → C API)

All non-C++ consumers talk to `libquiver_c`. On platforms where `libquiver_c` depends on `libquiver`, the binding loads the core first.

**Julia — `@ccall` against absolute path:**
- `bindings/julia/src/c_api.jl` — 125 `@ccall libquiver_c.*` sites. Auto-generated via `bindings/julia/generator/generator.jl` using Clang.jl 0.19.2 (pinned), with `generator.toml`, `prologue.jl`, `epilogue.jl`.
- Discovery: hard-coded relative path `joinpath(@__DIR__, "..", "..", "..", "build", library_dir(), library_name())` where `library_dir()` is `"bin"` on Windows, `"lib"` elsewhere.
- `CEnum.@cenum` for `quiver_error_t`, `quiver_log_level_t`, `quiver_data_structure_t`, `quiver_data_type_t`.

**Dart — `dart:ffi` + `package:ffi` + ffigen:**
- `bindings/dart/lib/src/ffi/library_loader.dart` + generated `bindings/dart/lib/src/ffi/bindings.dart`.
- Discovery: walk up to find `pubspec.yaml`, then search `.dart_tool/hooks_runner/shared/quiverdb/build/` recursively; fallback to system PATH. On Windows, preload `libquiver.dll` before `libquiver_c.dll`.
- Build hook `bindings/dart/hook/build.dart` drives CMake via `native_toolchain_cmake` with `QUIVER_BUILD_C_API=ON`, `QUIVER_BUILD_TESTS=OFF`, `QUIVER_BUILD_SHARED=ON`, pre-set `HAVE_GNU_STRERROR_R_EXITCODE` for cross-compile.
- Generator: `dart run ffigen` via `bindings/dart/generator/generator.bat`, config in `bindings/dart/ffigen.yaml` (and inline in `pubspec.yaml`). 5 entry points: `common.h`, `options.h`, `database.h`, `element.h`, `lua_runner.h`.

**Python — CFFI ABI-mode:**
- `bindings/python/src/quiverdb/_c_api.py` — hand-written `ffi.cdef(...)` matching C API headers. Alternative generator output in `_declarations.py` (reference only).
- `bindings/python/src/quiverdb/_loader.py` — two-tier discovery: (1) bundled `_libs/` (uses `os.add_dll_directory` on Windows and `ffi.dlopen(core_path)` to preload); (2) dev mode via system `libquiver_c.{dll,so}`.
- Generator: `bindings/python/generator/generator.py` reads `common.h`, `options.h`, `database.h`, `element.h` and rewrites `bool` → `_Bool`. Invoked via `uv run python generator/generator.py`.

**JS/Deno — `Deno.dlopen`:**
- `bindings/js/src/loader.ts` — 10 symbol groups (`lifecycleSymbols`, `elementSymbols`, `crudSymbols`, `readSymbols`, `querySymbols`, `transactionSymbols`, `metadataSymbols`, `timeSeriesSymbols`, `csvSymbols`, `freeSymbols`, `luaSymbols`).
- Type constants: `P="pointer"`, `BUF="buffer"`, `I32="i32"`, `I64="i64"`, `USIZE="usize"`, `F64="f64"`, `V="void"`. Struct shorthand `{ struct: [I32, I32] }` for `quiver_database_options_default`.
- Three-tier discovery: bundled `libs/{os}-{arch}/` → walk up 5 dirs for `build/bin/` → system PATH. On Windows, preload core with `Deno.dlopen(corePath, {})` first.
- Marshaling in `src/ffi-helpers.ts`: `TextEncoder`/`TextDecoder`, `Uint8Array`/`DataView`, `Deno.UnsafePointer`, `Deno.UnsafePointerView`, `BigInt64Array`, `Float64Array`.
- Std import: `jsr:@std/path@^1.1.4`.
- Previously used koffi + Node.js; migrated to Deno FFI per `bindings/js/MIGRATION.md`.

**Lua — in-process via sol2 (not a separate binding):**
- `src/lua_runner.cpp` creates `sol::state`, opens `sol::lib::base`, `sol::lib::string`, `sol::lib::table`, binds `Database` as usertype, exposes as global `db`.
- Compile defines: `SOL_SAFE_NUMERICS=1`, `SOL_SAFE_FUNCTION=1` (`src/CMakeLists.txt` lines 49-52).
- Build needs `/bigobj` on MSVC or `-Wa,-mbig-obj` on other Windows compilers due to template instantiation volume (`src/CMakeLists.txt` lines 56-60).

## Environment Configuration

**Runtime env vars:** None required.

**Test-time env vars:**
- `LD_LIBRARY_PATH` on Linux CI (Julia, Dart, Python, Deno jobs) points at `build/lib`.
- `PATH` on Windows prepended with `build/bin` via `test.bat` scripts.

**Secrets:**
- `CODECOV_TOKEN` (4 uses).
- Implicit `GITHUB_TOKEN` for release action.
- PyPI OIDC via `environment: pypi` + `id-token: write`.
- JSR OIDC via `environment: jsr` + `id-token: write`.
- **No `.env` files present at repo root.** `.gitignore` does not exclude `.env*` patterns.

## Webhooks & Callbacks

**Incoming:** None.
**Outgoing:** Only CI-initiated (Codecov uploads, PyPI publish, JSR publish, GitHub release). No runtime outgoing.

## Third-Party C/C++ Libraries (FetchContent)

All in `cmake/Dependencies.cmake`, pinned by tag (no commit SHAs).

| Library | Version | Source | Linkage | Used by |
|---------|---------|--------|---------|---------|
| sqlite3 (sjinks/sqlite3-cmake) | v3.50.2 | github.com/sjinks/sqlite3-cmake.git | PUBLIC `SQLite::SQLite3` | `src/database*.cpp`, `src/schema.cpp` |
| tomlplusplus | v3.4.0 | github.com/marzer/tomlplusplus.git | PRIVATE `tomlplusplus::tomlplusplus` | `src/binary/binary_metadata.cpp` |
| spdlog | v1.17.0 | github.com/gabime/spdlog.git | PRIVATE `spdlog::spdlog` | `src/database.cpp`, `src/database_impl.h`, `src/binary/csv_converter.cpp` |
| lua (codelibre/lua-cmake) | lua-cmake/v5.4.8.0 | gitlab.com/codelibre/lua/lua-cmake.git | PRIVATE `lua_library` (via sol2) | `src/lua_runner.cpp` |
| sol2 | v3.5.0 | github.com/ThePhD/sol2.git | PRIVATE `sol2` | `src/lua_runner.cpp` |
| rapidcsv | v8.92 | github.com/d99kris/rapidcsv.git | PRIVATE `rapidcsv` | `src/database_csv_export.cpp`, `src/database_csv_import.cpp` |
| argparse | v3.2 | github.com/p-ranav/argparse.git | PRIVATE `argparse` | `src/cli/main.cpp` only |
| googletest | v1.17.0 | github.com/google/googletest.git | PRIVATE `GTest::gtest_main`, `GTest::gmock` | `tests/*.cpp` (only when `QUIVER_BUILD_TESTS=ON`) |

**Absent:** `fast_float` (0 matches across repo), Catch2.

## Files of Interest

- `CMakeLists.txt` — project(version 0.7.0), options, SKBUILD handling
- `CMakePresets.json` — dev/release/windows-release/linux-release/all-bindings
- `cmake/Dependencies.cmake` — all FetchContent declarations
- `cmake/Platform.cmake` — OS/ext detection, RPATH, visibility
- `cmake/CompilerOptions.cmake` — compiler warnings
- `src/CMakeLists.txt` — targets `quiver`, `quiver_c`, `quiver_cli`
- `src/database.cpp`, `src/database_impl.h`, `src/schema.cpp` — SQLite integration
- `src/lua_runner.cpp` — sol2 / Lua binding
- `src/binary/binary_metadata.cpp` — toml++ usage
- `src/database_csv_export.cpp`, `src/database_csv_import.cpp` — rapidcsv
- `src/cli/main.cpp` — argparse
- `bindings/julia/Project.toml`, `bindings/julia/src/c_api.jl`, `bindings/julia/generator/generator.jl`
- `bindings/dart/pubspec.yaml`, `bindings/dart/ffigen.yaml`, `bindings/dart/hook/build.dart`, `bindings/dart/lib/src/ffi/library_loader.dart`
- `bindings/python/pyproject.toml`, `bindings/python/src/quiverdb/_c_api.py`, `bindings/python/src/quiverdb/_loader.py`, `bindings/python/generator/generator.py`
- `bindings/js/deno.json`, `bindings/js/biome.json`, `bindings/js/mod.ts`, `bindings/js/src/loader.ts`, `bindings/js/MIGRATION.md`
- `.github/workflows/ci.yml`, `.github/workflows/publish.yml`, `.github/workflows/build-wheels.yml`
- `.github/dependabot.yml`, `.pre-commit-config.yaml`, `codecov.yml`, `.clang-format`
