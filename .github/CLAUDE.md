# CI & Release Pipeline (`.github/`)

Versioning rule (CMakeLists.txt is the source of truth, `scripts/assert_version.py` checks the
five manifests) lives in the root `CLAUDE.md`.

## Workflow Inventory

| Workflow | Trigger | Purpose |
|---|---|---|
| `ci.yml` | push/PR to master | Build matrix (ubuntu/windows/macos × Release/Debug) + ctest + artifact upload; four coverage jobs uploading to Codecov with flags `cpp`, `julia`, `dart`, `python`; plus `clang-format` check, `actionlint`, and a `bun-test` matrix (ubuntu+windows) |
| `publish.yml` | `workflow_dispatch` | Release orchestrator (see below) |
| `publish-s3.yml` | `workflow_dispatch` (usually from publish.yml) | Builds native libs for `linux-x86_64`, `macos-aarch64`, `windows-x86_64` (via `scripts/ci/native_s3.sh`) and stages them on S3 |
| `publish-julia.yml` | `workflow_dispatch` | Mirrors `bindings/julia` into psrenergy/Quiver.jl (see below) |
| `publish-python.yml` | push/PR to master + `workflow_dispatch` | cibuildwheel on a ubuntu+windows matrix (targets in `bindings/python/CLAUDE.md`); the PyPI publish job runs only on `workflow_dispatch` (trusted publishing, `skip-existing: true`, `environment: pypi`) |
| `publish-js.yml` | `workflow_dispatch` | npm publish with bundled native libs (see below) |

Composite actions in `.github/actions/`:
- `build-cpp` — configure/build the core + C API with a FetchContent source cache. The cache key
  includes a **toolchain fingerprint** (default CMake generator): FetchContent subbuilds pin the
  generator in their CMakeCache, so restoring a `_deps` cache built under a different default
  generator (e.g. windows-latest moving VS 17 → 18) fails configure.
- `detect-version` — reads the version from CMakeLists.txt via `scripts/assert_version.py`
  (which owns the version-file inventory) and checks the matching tag on origin.

## Release Pipeline

- `publish.yml` (`workflow_dispatch` with no inputs — every checkout pins `github.sha`, and there
  is no version input: the tag must equal what the source declares) orchestrates a full release: resolve version via
  `scripts/assert_version.py` + assert tag `v<version>` is absent or already at the release sha →
  dispatch `publish-s3.yml` and wait → create tag + GitHub release (`ncipollo/release-action`,
  `skipIfReleaseExists`) → dispatch `publish-julia.yml` / `publish-python.yml` / `publish-js.yml`
  **in parallel on the new tag** and wait for all three.
- Children are dispatched as top-level `workflow_dispatch` runs via `scripts/ci/dispatch_workflow.sh`
  (dispatch → correlate run id by workflow+ref+created-after → poll to completion), NOT as
  `workflow_call` reusable workflows: npm and PyPI trusted publishing validate the **top-level
  workflow filename** from the OIDC claims, so `publish-js.yml`/`publish-python.yml` must stay
  top-level. `gh workflow run` with `GITHUB_TOKEN` works (`workflow_dispatch` is an explicit
  exception to "GITHUB_TOKEN events don't trigger workflows"; needs `actions: write`).
- Dispatching the bindings on the tag pins them to the release commit and gives `publish-python`
  a per-release concurrency group (its `cancel-in-progress: true` can't be tripped by a master
  push mid-publish). The `npm`/`pypi` environments must keep deployment branch policy
  "No restriction" (or allow `v*` tags) or tag-dispatched publishes get rejected.
- A full re-run of a partially failed release is idempotent end-to-end: tag assert passes (same
  sha), S3 overwrites the same keys, release creation is skipped, the Julia PR branch is
  force-updated, PyPI uses `skip-existing: true`, npm hits the `npm view` guard.

## Julia Publishing (Quiver.jl mirror)

- `bindings/julia/` is the **canonical** Julia package and the *single source of truth*; the
  published `psrenergy/Quiver.jl` is a **full generated mirror** (never hand-edit it — develop
  only in `bindings/julia`). It is a plain S3-artifact package in General (no `Quiver_jll`, no
  Yggdrasil). The `publish-julia.yml` workflow downloads the native libs from S3 (staged by
  `publish-s3.yml`), runs `scripts/julia/generate_artifacts.jl` (tar → S3 upload →
  `Artifacts.toml`), then **wipes the mirror (keeping only its `.git/`) and copies the entire
  `bindings/julia` tree into it** — `src/`, `test/`, `.github/` (the mirror's `CI.yml`/`TagBot.yml`
  live here, dormant in the monorepo since nested workflows don't run), `README.md`, `.gitignore`,
  `.gitattributes`, `.JuliaFormatter.toml`, `LICENSE`, and the verbatim `Project.toml` (the
  binding shares Quiver.jl's UUID). It then copies the real test schemas from repo-root
  `tests/schemas/` into the mirror's `test/schemas/` (no schemas live in `bindings/julia`;
  `test/fixture.jl` resolves the schema dir at runtime), overlays the generated
  `Artifacts.toml`, and opens a PR to `Quiver.jl`. Only `Manifest.toml` is excluded (gitignored).
  Because it's a full mirror, the PR surfaces a **delete** for anything in Quiver.jl not present
  in `bindings/julia`, so any file the mirror needs (extra workflows, docs) must live in
  `bindings/julia`. The job runs on a `[self-hosted, linux]` runner whose ambient IAM role grants
  S3 write (no AWS key secrets); it requires the `QUIVER_JL_TOKEN` secret for the cross-repo PR.
- **Windows artifact gotcha:** `generate_artifacts.jl` `chmod 0o755`s the staged libs before
  tarring — Windows DLLs **must be executable in the artifact**, or Pkg's Windows extraction
  yields an NTFS ACL without execute and `LoadLibrary` fails with "Access is denied" (Linux `.so`
  load ignores the bit, so the symptom is Windows-only).
- **macOS artifact gotchas** (Apple-Silicon-only, `macos-aarch64`): (1) dyld resolves dependent
  dylibs **filesystem-first** — no Linux-style SONAME matching against already-loaded images — so
  the artifact ships libquiver ONLY under its install name `libquiver.0.dylib` (the `.0` tracks
  SOVERSION = major version); a second `libquiver.dylib` copy in the same dir could be loaded as
  a duplicate image (duplicated static state, e.g. the binary write registry). `libquiver_c.dylib`
  gets an `@loader_path` rpath via `install_name_tool` in `publish-s3.yml`. (2) `install_name_tool`
  invalidates the ad-hoc linker signature and arm64 macOS SIGKILLs `dlopen` of unsigned code, so
  the workflow re-signs with `codesign --force --sign -` — that step is load-bearing. (3) The
  mirror's `CI.yml` passes no `arch` to setup-julia (runner-native: x64 on ubuntu/windows,
  aarch64 on macos-latest); x64 Julia on an arm64 mac runs under Rosetta and would not match the
  `arch = "aarch64"` Artifacts.toml entry.

## npm Publishing (JS)

`publish-js.yml` downloads native libs from S3 into
`libs/{linux-x86_64,macos-aarch64,windows-x86_64}/`, asserts every lib is in a throwaway
`npm pack` tarball via `tar -tzf` (format-independent; npm roots entries under `package/`), then
publishes with **`npm publish --loglevel verbose` via `actions/setup-node@v6`** using **npm
Trusted Publishing (OIDC)** — `permissions: id-token: write`, no stored token; npm packs inline
so the published artifact carries the deterministic, asserted file set. setup-node uses
`package-manager-cache: false` (v6 caches by default; the Bun project has no
`package-lock.json`). Verbose logging is load-bearing: npm logs the OIDC exchange result only at
that level — a failed exchange silently falls back to token auth (setup-node's `NODE_AUTH_TOKEN`
placeholder) and dies with a misleading E404 on the PUT. Requires a trusted publisher configured
on npmjs.com (GitHub Actions · `psrenergy/quiver` · workflow `publish-js.yml` · environment
`npm`) and npm ≥ 11.5.1 (ensured via `npm install -g npm@latest` on Node 24). Bun can't do OIDC
trusted publishing yet (bun#24855→#15601), so the publish job uses npm; the binding + tests
remain 100% Bun. A standalone re-dispatch of an already-published version is skipped via an
`npm view` guard.
