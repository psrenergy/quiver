---
last_mapped_commit: cf9ca58aceab305fe6a08461f78da8556a6c95d0
last_mapped_at: 2026-10-07
---
# External Integrations

**Analysis Date:** 2026-10-07

## APIs & External Services

**Runtime:**

- No hosted API client or application HTTP service detected in native linkage and binding runtime dependencies (`src/CMakeLists.txt`, `bindings/python/pyproject.toml`, `bindings/js/package.json`, `bindings/dart/pubspec.yaml`, `bindings/julia/Project.toml`). Quiver is an embedded library; remote integrations concern dependency acquisition and releases.

**Source dependency acquisition:**

- GitHub repositories supply SQLite (PSR-maintained fork), toml++, spdlog, sol2, csv-parser, OpenXLSX, argparse, and GoogleTest; GitLab supplies lua-cmake (`cmake/Dependencies.cmake`).
  - SDK/Client: CMake FetchContent and Git.
  - Auth: public source access; no dependency-fetch token specified in `cmake/Dependencies.cmake`.
- PSR Style.jl formatter is fetched from GitHub through Julia Pkg (`bindings/julia/format/Project.toml`).
  - SDK/Client: Julia Pkg.
  - Auth: no formatter token specified in `bindings/julia/format/Project.toml`.

**Native artifact distribution:**

- AWS S3 stages per-platform native files and Julia artifact tarballs (`scripts/ci/native_s3.sh`, `scripts/julia/generate_artifacts.jl`).
  - SDK/Client: AWS CLI for writes, curl for public native downloads, Julia Artifacts for published Julia installation (`bindings/julia/src/c_api.jl`).
  - Auth: ambient IAM role on self-hosted Linux release runners, as documented in `.github/AGENTS.md`; upload jobs in `.github/workflows/publish-s3.yml` and `.github/workflows/publish-julia.yml` do not inject AWS access-key secrets.
  - Location: bucket defaults to `julia-artifacts`, prefix defaults to `quiver`; override names are `QUIVER_S3_BUCKET` and `QUIVER_S3_PREFIX` (`scripts/ci/native_s3.sh`, `scripts/julia/generate_artifacts.jl`).
  - Object layout: `<prefix>/<version>/native/<platform>/<file>` for individual libraries; artifact tarballs use versioned S3 paths and SHA/tree hashes in generated `Artifacts.toml` (`scripts/ci/native_s3.sh`, `scripts/julia/generate_artifacts.jl`). Uploads set `public-read`.

**Package registries and repository mirroring:**

- PyPI distributes `quiverdb` wheels containing Python sources and native core/C API libraries (`bindings/python/pyproject.toml`, `.github/workflows/publish-python.yml`).
  - SDK/Client: scikit-build-core, cibuildwheel, twine validation, `pypa/gh-action-pypi-publish@v1.14.2`.
  - Auth: GitHub OIDC trusted publishing with `id-token: write`, GitHub environment `pypi`; no stored PyPI token is wired into `.github/workflows/publish-python.yml`.
- npm distributes Bun-oriented `quiverdb` TypeScript sources and all four platform-native bundles (`bindings/js/package.json`, `.github/workflows/publish-js.yml`).
  - SDK/Client: Node 24 and latest npm; `npm pack` verifies native inclusion before `npm publish --loglevel verbose`.
  - Auth: GitHub OIDC trusted publishing, GitHub environment `npm`; package provenance enabled in `bindings/js/package.json`. The publish workflow must remain a top-level dispatch for trusted-publisher claims (`.github/AGENTS.md`).
- GitHub repository `psrenergy/Quiver.jl` is a generated mirror of canonical `bindings/julia/`; a release workflow opens a mirror PR with generated `Artifacts.toml` and copied shared test schemas (`.github/workflows/publish-julia.yml`).
  - SDK/Client: checkout action, Julia packaging script, `peter-evans/create-pull-request@v8`.
  - Auth: GitHub Actions secret `QUIVER_JL_TOKEN` for cross-repository checkout/PR.
- Julia General/TagBot publication is documented for the mirror, whose package metadata and nested workflows live in `bindings/julia/Project.toml` and `bindings/julia/.github/workflows/` (`.github/AGENTS.md`).
  - Auth: mirror TagBot references `GITHUB_TOKEN` and `DOCUMENTER_KEY` secret names in `bindings/julia/.github/workflows/TagBot.yml`.
- Dart dependencies use pub.dev; `quiverdb` package manifest exists, but no automated Dart registry release appears in `.github/workflows/` (`bindings/dart/pubspec.yaml`, `bindings/dart/pubspec.lock`).

## Data Storage

**Databases:**

- SQLite 3.53.4 is fetched and linked as an in-process database engine (`cmake/Dependencies.cmake`, `src/CMakeLists.txt`).
  - Connection: caller-provided filesystem path or `:memory:` in `Database`; not an environment variable or network connection string (`include/quiver/database.h`, `src/database.cpp`).
  - Client: direct SQLite C API hidden by `Database::Impl`; no ORM (`src/database_impl.h`, `src/database.cpp`).
  - Schema: SQL file or numbered up/down migration directories; schema metadata loads lazily for ordinary open and eagerly for schema/migration factories (`include/quiver/database.h`, `src/database.cpp`, `src/migrations.cpp`, `AGENTS.md`).
  - Keep group/time-series schemas and domain validation in the native layer; every binding shares this engine (`src/schema_validator.cpp`, `src/type_validator.cpp`, `AGENTS.md`).

**File Storage:**

- Application data stays on the local filesystem: SQLite files, logs, CSVs, binary files/TOML metadata, XLSX inputs, migration SQL, and optional UI metadata (`src/database.cpp`, `src/database_csv_import.cpp`, `src/database_csv_export.cpp`, `src/binary/binary_file.cpp`, `src/xlsx/xlsx_read.cpp`, `src/ui_metadata.cpp`).
- Lua file operations resolve against the database directory and enforce canonical containment; `:memory:` rejects file operations (`src/sandbox/path_policy.cpp`, `src/sandbox/path_policy.h`). XLSX/standalone CSV helpers are internal Lua integrations, intentionally absent from host FFI surfaces (`src/sandbox/csv.cpp`, `src/sandbox/xlsx.cpp`, `AGENTS.md`).
- S3 stores release artifacts, not users' database contents (`scripts/ci/native_s3.sh`, `.github/workflows/publish-s3.yml`).

**Caching:**

- No external runtime cache detected; native schema metadata is held in `Database::Impl` (`src/database_impl.h`).
- Language loaders cache loaded libraries/bindings in process (`bindings/js/src/loader.ts`, `bindings/dart/lib/src/ffi/library_loader.dart`). Julia resolves native artifact storage through its depot (`bindings/julia/src/c_api.jl`).
- CI caches FetchContent sources and sccache objects with OS, architecture, generator/toolchain fingerprint, and build-mode keys (`.github/actions/build-cpp/action.yml`).

## Authentication & Identity

**Auth Provider:**

- Not applicable to the embedded database API: no user/account provider or server auth middleware detected (`include/quiver/database.h`, `src/database.cpp`).
  - Implementation: callers own database file access; read-only mode is a database option (`include/quiver/options.h`).
- Embedded Lua execution uses filesystem containment and restricted standard libraries, not identity-based permissions (`src/sandbox/sandbox.cpp`, `src/sandbox/path_policy.cpp`).
- Publishing identities are separate: Actions `GITHUB_TOKEN`, Julia mirror `QUIVER_JL_TOKEN`, registry OIDC, and ambient S3 IAM (`.github/AGENTS.md`, `.github/workflows/publish.yml`, `.github/workflows/publish-julia.yml`, `.github/workflows/publish-python.yml`, `.github/workflows/publish-js.yml`).

## Monitoring & Observability

**Error Tracking:**

- No runtime SaaS error tracking detected in `src/CMakeLists.txt` or binding manifests. C API operations expose the shared `quiver_get_last_error` channel (`include/quiver/c/common.h`, `src/c/common.cpp`); bindings surface native errors.
- Codecov receives CI coverage flags `cpp`, `julia`, `dart`, `python`, and `js` (`.github/workflows/ci.yml`, `codecov.yml`).
  - Client: `codecov/codecov-action@v7`, with CLI installed from PyPI and explicit-file uploads (`.github/workflows/ci.yml`).
  - Auth: Actions secret name `CODECOV_TOKEN` (`.github/workflows/ci.yml`).

**Logs:**

- Per-database spdlog logger with thread-safe stderr and basic file sinks; file path is `<database directory>/quiver_database.log`, file sink opens with truncation (`src/database.cpp`).
- Console severity comes from `DatabaseOptions.console_level`; file logging uses debug level. In-memory databases use stderr only; file-sink failure falls back to console with a warning (`include/quiver/options.h`, `src/database.cpp`).
- Dart native build hook uses a detached INFO logger for build output (`bindings/dart/hook/build.dart`); hosted CI/release logs remain in GitHub Actions (`.github/workflows/`).

## CI/CD & Deployment

**Hosting:**

- GitHub hosts source, release tags, release records, CI artifacts, and the Julia mirror (`.github/workflows/publish.yml`, `.github/workflows/publish-julia.yml`). There is no deployable application server or website target in `src/CMakeLists.txt`.
- S3, PyPI, npm, and Julia package/artifact mechanisms distribute libraries (`.github/workflows/publish-s3.yml`, `.github/workflows/publish-python.yml`, `.github/workflows/publish-js.yml`, `.github/workflows/publish-julia.yml`).

**CI Pipeline:**

- GitHub Actions push/PR CI on master: four OS/architecture runners × Debug/Release core builds, CTest, seven-day build artifacts, binding coverage, clang-format and actionlint checks (`.github/workflows/ci.yml`).
- Python wheel workflow also builds on master push/PR; only manual dispatch publishes to PyPI (`.github/workflows/publish-python.yml`).
- Version bump dispatch creates a five-manifest PR through `scripts/assert_version.py`; release is a separate dispatch (`.github/workflows/bump-version.yml`, `.github/workflows/publish.yml`).
- Release sequence: check version/tag, dispatch and await native S3 staging, create GitHub tag/release, then dispatch Julia/Python/npm publishes in parallel on the tag (`.github/workflows/publish.yml`, `scripts/ci/dispatch_workflow.sh`). No Dart child publish is wired here.
- Linux staging uses digest-pinned manylinux Docker images and portability gates; macOS/Windows use composite native builds. macOS dependency-path patching is followed by ad-hoc codesigning (`scripts/build_native_linux.sh`, `.github/workflows/publish-s3.yml`, `.github/actions/build-cpp/action.yml`).
- S3 uploads and Julia mirror generation require self-hosted Linux runners with ambient AWS permissions (`.github/workflows/publish-s3.yml`, `.github/workflows/publish-julia.yml`, `.github/AGENTS.md`).
- Release retries are designed to tolerate existing matching tags/releases, overwrite identical S3 keys, skip existing PyPI files and npm versions, and update the Julia PR branch (`.github/workflows/publish.yml`, `.github/workflows/publish-python.yml`, `.github/workflows/publish-js.yml`, `.github/AGENTS.md`).

## Environment Configuration

**Required env vars:**

- Ordinary database use requires no service credentials or environment variables; pass paths and options explicitly (`include/quiver/database.h`, `include/quiver/options.h`).
- Optional `QUIVER_LIB_DIR`: explicit Julia native-library directory; otherwise artifact depot then repository build directory (`bindings/julia/src/c_api.jl`).
- Optional `QUIVER_S3_BUCKET` / `QUIVER_S3_PREFIX`: release artifact location overrides (`scripts/ci/native_s3.sh`, `scripts/julia/generate_artifacts.jl`).
- CI dispatch uses GitHub CLI authentication supplied by Actions; release settings and secret names live in workflow wiring (`scripts/ci/dispatch_workflow.sh`, `.github/workflows/publish.yml`).
- Python looks first in packaged `_libs` then system library search; JS uses packaged platform folder, development paths, then system PATH; Dart uses hook output, macOS app framework, then system search (`bindings/python/src/quiverdb/_loader.py`, `bindings/js/src/loader.ts`, `bindings/dart/lib/src/ffi/library_loader.dart`).

**Secrets location:**

- GitHub Actions secret references: `CODECOV_TOKEN`, `QUIVER_JL_TOKEN`, and mirror `DOCUMENTER_KEY`; Actions provides `GITHUB_TOKEN` (`.github/workflows/ci.yml`, `.github/workflows/publish-julia.yml`, `bindings/julia/.github/workflows/TagBot.yml`). Values are not stored or inspected in this analysis.
- Registry auth uses configured npm/PyPI trusted publishers and environments; S3 write credentials are external runner IAM configuration (`.github/AGENTS.md`, `.github/workflows/publish-js.yml`, `.github/workflows/publish-python.yml`).

## Webhooks & Callbacks

**Incoming:**

- No application HTTP webhook endpoint detected in `src/CMakeLists.txt` or `include/quiver/`.
- GitHub push/PR and manual dispatch events invoke CI/releases; mirror TagBot listens to issue comments/manual dispatch (`.github/workflows/ci.yml`, `.github/workflows/publish.yml`, `bindings/julia/.github/workflows/TagBot.yml`).
- Lua stream readers accept in-process row callbacks for CSV/XLSX, not network webhooks (`src/sandbox/csv.cpp`, `src/sandbox/xlsx.cpp`).

**Outgoing:**

- Release automation dispatches GitHub workflows via gh, writes public S3 objects via AWS CLI, publishes registry artifacts, opens Julia mirror PRs, and uploads coverage (`scripts/ci/dispatch_workflow.sh`, `scripts/ci/native_s3.sh`, `.github/workflows/publish-js.yml`, `.github/workflows/publish-python.yml`, `.github/workflows/publish-julia.yml`, `.github/workflows/ci.yml`).
- No runtime outgoing application webhook integration detected in native linkage and host runtime manifests (`src/CMakeLists.txt`, `bindings/python/pyproject.toml`, `bindings/js/package.json`, `bindings/dart/pubspec.yaml`, `bindings/julia/Project.toml`).

---

*Integration audit: 2026-10-07*
