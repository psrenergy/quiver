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
  **Used by macOS/Windows only in `publish-s3.yml`** (`if: runner.os != 'Linux'`); `ci.yml` still
  uses it for all three OSes.

**glibc floor for the published Linux native libs (`publish-s3.yml`):** the `linux-x86_64` native
libs are NOT built via `build-cpp` on a bare `ubuntu-latest` runner — that binds `GLIBC_2.28`..`2.34`
symbols (libm math, `stat`, the pthread/dlopen libc merge) and fails on old systems like Amazon
Linux 1 = glibc 2.17 (Julia's own floor). Instead they are built inside the **manylinux2014** image
(CentOS 7 → **glibc 2.17**, **GCC 11** → `GLIBCXX_3.4.29`) — the same image family the Python wheels
build in — by **`scripts/build_native_linux.sh`**, a single script the CI job and a developer run
identically (`bash scripts/build_native_linux.sh`; it only needs Docker, so it also works on a Windows
dev box via Docker Desktop). It uses **`docker run`, not a job-level `container:`**: the runner's
Node20 actions (`checkout`, `upload-artifact`) and the `docker` CLI run on the host, and Node20 can't
start inside a glibc-2.17 image (`actions/checkout#1590`). The image is **pinned by digest** so the
toolchain/cmake/glibc floor can't drift. **GCC 11 (devtoolset-11) is required**: the core uses C++20
`<chrono>` calendar types (`year_month_day`/`hh_mm_ss`/`sys_days`) that GCC 10 lacks — and CentOS 7's
SCL caps at GCC 11, which keeps GLIBCXX ≤ the **3.4.30** ceiling Julia's bundled libstdc++ provides
(this is a plain S3 artifact — no `CompilerSupportLibraries_jll` at load time, so libstdc++ is whatever
Julia bundles; GCC 13 → `3.4.32` would fail to load). The script builds into `build/manylinux/`, runs `patchelf --set-rpath '$ORIGIN'`
on `libquiver_c.so` so it finds `libquiver.so.0` as a sibling in the flat ship layout, dereferences the
version symlinks into real files (matching the old `cp -L`), and runs the portability gate
in-container (fails unless glibc ≤ 2.17, GLIBCXX ≤ 3.4.30, both libs dynamically linked to
`libstdc++.so.6`, and `libquiver_c.so` carries an `$ORIGIN` rpath). libstdc++ stays **dynamic** — never
static-link (the C API catches C++-core exceptions by type across the `libquiver.so` →
`libquiver_c.so` boundary, and two static copies under `-fvisibility=hidden` would break that; this
also rules out zig/libc++ static toolchains). The three files land in `build/manylinux/lib/` exactly as
the downstream `upload-s3` job + `scripts/ci/native_s3.sh` expect. Feeds both the Julia and JS/npm
native libs (shared S3 staging). macOS/Windows still use `build-cpp` (gated `if: runner.os !=
'Linux'`) — only Linux needs the old-glibc image.

> **Why not the alternatives** (settled 2026-07-24): BinaryBuilder.jl also reaches 2.17 without Docker,
> but pulls the whole Julia + compiler-shard stack and can't run on a Windows dev box (local
> reproducibility was a requirement). A symbol-versioning `-include` CMake flag *cannot* reach 2.17 on
> current runners: `stat`@2.33 and `__libc_single_threaded`@2.32 have no older symbol version to
> redirect to, and the glibc-2.31 (ubuntu-20.04) hosted runners that used to dodge them are retired. A
> self-built crosstool-NG toolchain just reimplements the cross-toolchain with more maintenance.
> manylinux is the simplest robust option that also reproduces locally.


The publish workflows read the version by inlining `python3 scripts/assert_version.py` directly.

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
