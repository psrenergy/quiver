# External Integrations

**Analysis Date:** 2026-03-08

## APIs & External Services

**None.**
Quiver is a self-contained library with no outbound HTTP calls, REST APIs, or third-party SaaS integrations. All external "integrations" are compile-time library dependencies resolved via CMake FetchContent from GitHub/GitLab.

## Data Storage

**Databases:**
- SQLite 3.50.2
  - Embedded — no separate server process
  - Fetched from: `https://github.com/sjinks/sqlite3-cmake.git` (tag `v3.50.2`)
  - Client: Direct `sqlite3_*` C API, accessed through C++20 `Database` class (`include/quiver/database.h`, `src/database.cpp`)
  - All schema creation uses `STRICT` tables
  - WAL mode not explicitly configured — SQLite defaults apply
  - Connection string: file path passed to `quiver_database_open()` or factory methods

**File Storage:**
- Local filesystem only
  - Binary blob files: `.qvr` format, written via `Blob` class (`include/quiver/blob/blob.h`)
  - TOML metadata sidecar files: `.toml` format alongside `.qvr` files
  - CSV export/import: local file paths via `export_csv()`/`import_csv()` (`src/database_csv_export.cpp`, `src/database_csv_import.cpp`)

**Caching:**
- None — no in-memory cache layer beyond SQLite's own page cache

## Authentication & Identity

**Auth Provider:**
- None — no authentication layer
- Database access control is filesystem-level only
- Read-only mode available via `DatabaseOptions.read_only` flag (`include/quiver/c/options.h`)

## Monitoring & Observability

**Error Tracking:**
- None — no external error tracking service (no Sentry, Datadog, etc.)

**Logs:**
- spdlog v1.17.0 (`cmake/Dependencies.cmake`)
- Console output only; log level configurable via `DatabaseOptions.console_level` (`quiver_log_level_t` enum in `include/quiver/c/options.h`)
- Levels: `QUIVER_LOG_DEBUG`, `QUIVER_LOG_INFO`, `QUIVER_LOG_WARN`, `QUIVER_LOG_ERROR`, `QUIVER_LOG_OFF`
- Log file: `quiver_database.log` present at project root (runtime output, not committed)

**Coverage:**
- Codecov (`codecov.yml`) — receives coverage reports from GitHub Actions CI
  - C++ coverage: lcov + `codecov/codecov-action@v5` with flag `cpp`
  - Julia coverage: `julia-actions/julia-processcoverage` with flag `julia`
  - Dart coverage: lcov with flag `dart`
  - Python coverage: pytest-cov with flag `python`
  - Token: `${{ secrets.CODECOV_TOKEN }}` (secret in GitHub Actions environment)

## CI/CD & Deployment

**Hosting:**
- PyPI - Python wheel distribution (published via `pypa/gh-action-pypi-publish@release/v1` on version tag push)
- GitHub Releases - Wheel artifacts attached on version tag push (via `softprops/action-gh-release@v2`)
- No other hosted artifact registry

**CI Pipeline:**
- GitHub Actions (`.github/workflows/`)
  - `ci.yml` - Main CI: build + test on ubuntu-latest, windows-latest, macos-latest in Debug and Release; coverage jobs for C++, Julia, Dart, Python; clang-format check
  - `build-wheels.yml` - Python wheel build on push/PR to master (ubuntu + windows, Python 3.13)
  - `publish.yml` - Triggered on `v*.*.*` tag push; builds wheels, validates version, creates GitHub Release, publishes to PyPI
- Dependabot: weekly updates for GitHub Actions only (`github-actions` ecosystem in `.github/dependabot.yml`)

**Secrets Required:**
- `CODECOV_TOKEN` - Used in `ci.yml` for coverage upload to Codecov
- PyPI OIDC (trusted publisher) - `id-token: write` permission in `publish.yml`; no stored secret needed for PyPI publish

## Webhooks & Callbacks

**Incoming:**
- None

**Outgoing:**
- None

## Language Binding FFI Toolchain

These are dev-time toolchain integrations, not runtime integrations:

**Julia FFI Generator:**
- `Clang.jl` 0.19.2 - Generates `bindings/julia/src/c_api.jl` from C headers
- Run: `bindings/julia/generator/generator.bat`
- Config: `bindings/julia/generator/generator.toml`

**Dart FFI Generator:**
- `ffigen` 11.0+ - Generates `bindings/dart/lib/src/ffi/bindings.dart` from C headers
- Run: `bindings/dart/generator/generator.bat`
- Config: `bindings/dart/ffigen.yaml` and `bindings/dart/pubspec.yaml` `ffigen:` section

**Python FFI:**
- CFFI 2.0.0 ABI-mode - Hand-written declarations in `bindings/python/src/quiverdb/_c_api.py`
- No compile-time binding generation; `_declarations.py` holds generator reference output
- Generator: `bindings/python/generator/generator.bat`

**Native Build Integration (Dart):**
- `native_toolchain_cmake` 0.2.2 - Invokes CMake from Dart's build hook system
- Hook: `bindings/dart/hook/`
- Dart `.dart_tool/hooks_runner/` caches build artifacts (clear on C API struct changes)

---

*Integration audit: 2026-03-08*
