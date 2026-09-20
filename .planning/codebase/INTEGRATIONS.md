# External Integrations

**Analysis Date:** 2026-09-17

## APIs & External Services

**None** - Quiver is a self-contained SQLite wrapper library with no external service dependencies.

## Data Storage

**Databases:**
- **SQLite** 3.50.2
  - Type: Embedded file-based relational database (vendored via CMake FetchContent)
  - Connection: File path (in-memory `:memory:` also supported)
  - Client: sqlite3 C API (wrapped by C++ core)
  - Schema: User-defined via SQL migrations (validated via `SchemaValidator`)

**File Storage:**
- **Local filesystem only** - All data persisted to `.db` file and `.qvr` binary files (no cloud storage integration)
  - `.db` files: SQLite database files
  - `.qvr` files: Binary columnar data format (binary subsystem)
  - `.toml` files: Binary metadata sidecars

**Caching:**
- None - SQLite provides its own page cache. No external caching service.

## Authentication & Identity

**Auth Provider:**
- None - Library does not require authentication. Bindings run in-process with filesystem access.

## Monitoring & Observability

**Error Tracking:**
- Codec: Custom error channel (`quiver_get_last_error` / `quiver_set_last_error`) in C API
- All errors surface as exceptions (C++) → error returns (C API) → language-native exceptions (bindings)
- No external error service integration

**Logs:**
- **Framework:** spdlog 1.17.0 (C++ logging library)
- **Output:** stderr (console_level configurable via `DatabaseOptions::console_level`)
- **No external logging service** - logs stay local

## CI/CD & Deployment

**Hosting:**
- **GitHub** - Repository and release artifacts
- **GitHub Actions** - CI/CD pipeline (test, build, coverage upload)
- **S3** (AWS S3 bucket owned by PSR) - Native library staging for Julia/JS releases
  - Accessed by: `publish-s3.yml` workflow → `scripts/ci/native_s3.sh`
  - Artifacts: `lib/` directory tarballs per platform (linux-x86_64, macos-aarch64, windows-x86_64)

**CI Pipeline:**
- **GitHub Actions workflows:**
  - `.github/workflows/ci.yml` - Build matrix (ubuntu/windows/macos × Release/Debug), ctest, artifact upload
  - `.github/workflows/publish-python.yml` - cibuildwheel (cp313 targets), PyPI publish (OIDC trusted publishing)
  - `.github/workflows/publish-s3.yml` - Native lib builds → S3 staging (Linux: manylinux2014 container, macOS/Windows: bare runner)
  - `.github/workflows/publish-julia.yml` - Mirror generation (full copy of `bindings/julia/` → psrenergy/Quiver.jl) + artifact download
  - `.github/workflows/publish-js.yml` - npm publish (OIDC trusted publishing, bundled natives)
  - `.github/workflows/bump-version.yml` - Version bump (all 5 manifests) + PR creation
  - `.github/workflows/publish.yml` - Release orchestrator (version check → S3 → tag → Julia/Python/JS dispatch)

**Composite Actions:**
- `.github/actions/build-cpp` - FetchContent configure/build with toolchain fingerprint caching (used by ci.yml and publish workflows)

## Package Registry Integrations

**Python (PyPI):**
- **Package:** `quiverdb` (v0.10.6)
- **Build:** scikit-build-core (CMake-driven wheel build)
- **Publish:** PyPI via `pypa/gh-action-pypi-publish@v1.14.2`
- **Auth:** OIDC trusted publishing (no stored tokens)
- **Targets:** cp313-manylinux_x86_64, cp313-win_amd64
- **Workflow:** `.github/workflows/publish-python.yml` (triggered on master push + workflow_dispatch)

**npm (npmjs.com):**
- **Package:** `quiverdb` (v0.10.6)
- **Format:** Bundled natives (`libs/{os}-{arch}/`) shipped in package
- **Publish:** npm via `actions/setup-node@v6` with OIDC trusted publishing
- **Auth:** OIDC (token-less; requires trusted publisher configured on npmjs.com)
- **Workflow:** `.github/workflows/publish-js.yml` (workflow_dispatch, dispatched by publish.yml)

**Dart Pub:**
- **Package:** `quiverdb` (v0.10.6)
- **Build hook:** `hook/build.dart` (native-assets, invokes CMake with QUIVER_UNVERSIONED_SHARED=ON)
- **Publish:** Manual (no automated CI workflow; handled by maintainer)
- **Targets:** Compiled via Dart's native-assets hook on developer machine before publish

**Julia General (Package Registry):**
- **Package:** `Quiver` (canonical: `bindings/julia/`, published mirror: `psrenergy/Quiver.jl`)
- **Distribution:** Plain S3-artifact package (no _jll suffix, no Yggdrasil)
- **Artifact Flow:** Native libs staged on S3 → `scripts/julia/generate_artifacts.jl` creates tarball + generates `Artifacts.toml` → uploaded to S3 → mirrored to General
- **Publish:** Automatic (General registry's automatic updates from GitHub tags)
- **Workflow:** `.github/workflows/publish-julia.yml` (workflow_dispatch, dispatched by publish.yml)

**S3 Native Artifacts:**
- **Purpose:** Central staging for pre-built native libraries (linux-x86_64, macos-aarch64, windows-x86_64)
- **Build:** `.github/workflows/publish-s3.yml`
  - Linux: manylinux2014 container (glibc 2.17, GCC 11) via `scripts/build_native_linux.sh`
  - macOS: bare runner (Apple Silicon) via `build-cpp` action
  - Windows: bare runner via `build-cpp` action
- **Upload:** IAM role (self-hosted runner) → no secret tokens
- **Consumption:** By Julia (`generate_artifacts.jl`), JS (`npm postinstall`), as fallback by dev builds

## Webhooks & Callbacks

**Incoming:**
- None - Library does not expose webhooks

**Outgoing:**
- None - Workflows use GitHub API (`gh`) for dispatch/polling, not webhooks

## Environment Configuration

**Required environment variables:**
- None at runtime (all configuration passed as method parameters)

**Optional environment variables:**
- `QUIVER_LIB_DIR` (Julia) - Override library search path
- `CODECOV_TOKEN` (GitHub Actions) - Codecov upload token
- `QUIVER_JL_TOKEN` (GitHub Actions) - Cross-repo PR token (publish-julia.yml)

**Secrets location:**
- GitHub Actions: Repository secrets (`Settings → Secrets and variables → Actions`)
  - `CODECOV_TOKEN` - Upload coverage to Codecov
  - `QUIVER_JL_TOKEN` - Open PR in psrenergy/Quiver.jl mirror

## Trusted Publishing (OIDC)

**PyPI:**
- **Configured on:** npmjs.com
- **Issuer:** github.com (GitHub Actions OIDC provider)
- **Workflow:** `.github/workflows/publish-python.yml`
- **Permissions:** `id-token: write` (OIDC token generation)
- **Auth Method:** `pypa/gh-action-pypi-publish@v1.14.2` (no stored PyPI token)

**npm:**
- **Configured on:** npmjs.com (trusted publisher: `psrenergy/quiver`, workflow `publish-js.yml`)
- **Issuer:** github.com
- **Workflow:** `.github/workflows/publish-js.yml`
- **Permissions:** `id-token: write`
- **Auth Method:** `actions/setup-node@v6` with npm >=11.5.1 (OIDC exchange logged at verbose level)

## Code Coverage Integration

**Service:** Codecov
- **Upload via:** `codecov/codecov-action@v7`
- **Coverage sources:**
  - `cpp` flag: lcov report from C++ tests (`tests/`)
  - `julia` flag: Coverage.jl report from Julia tests
  - `dart` flag: Dart coverage from `dart test` (uploaded via custom script)
  - `python` flag: pytest-cov from Python tests
- **Workflow:** `.github/workflows/ci.yml`
- **Merge behavior:** GitHub status checks (fail if coverage drops below threshold)

## Native Library Distribution

**Flow:**
1. **Build** (publish-s3.yml): Compile for linux-x86_64, macos-aarch64, windows-x86_64
2. **Stage on S3:** Upload to PSR-owned S3 bucket
3. **Julia:** Download during `generate_artifacts.jl` → tar into `Artifacts.toml` entry → upload S3 artifact
4. **npm:** Download during publish → bundle into `libs/{os}-{arch}/` → ship in npm package
5. **Dart:** Compile via hook at install-time (native-assets, no pre-built download)

**Platforms:**
- **Linux:** manylinux2014 (glibc 2.17, GCC 11, GLIBCXX 3.4.29)
- **macOS:** aarch64 (Apple Silicon; arm64 dylibs)
- **Windows:** x86_64 (MSVC 2022)

## Release Workflow

**Orchestration:** `.github/workflows/publish.yml` (manual dispatch)

**Steps:**
1. Resolve version from `CMakeLists.txt` via `scripts/assert_version.py`
2. Assert tag `v<version>` does not exist
3. Dispatch `publish-s3.yml` → wait (native lib staging)
4. Create GitHub release + tag
5. Dispatch `publish-julia.yml`, `publish-python.yml`, `publish-js.yml` in parallel on tag
6. Wait for all three to complete

**Version Bump:** `.github/workflows/bump-version.yml` (manual dispatch)
- Input: `part` (major/minor/patch)
- Action: Rewrite 5 manifests + open PR (does NOT dispatch publish.yml)

---

*Integration audit: 2026-09-17*
