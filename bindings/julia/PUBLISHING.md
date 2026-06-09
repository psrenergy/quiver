# Publishing the Julia binding

The Julia binding is published as **[`Quiver.jl`](https://github.com/psrenergy/Quiver.jl)** —
a plain [Julia Artifacts](https://pkgdocs.julialang.org/v1/artifacts/) package (no `_jll`,
no Yggdrasil) registered in the **General** registry. Binaries are hosted on PSR's
`julia-artifacts` S3 bucket, the same pattern as `PSRIO.jl`.

## Source of truth (Model B)

`bindings/julia/` in **this** repo is the **canonical** Julia package — develop and test
here. `psrenergy/Quiver.jl` is a **generated mirror**: the release pipeline assembles it
from `bindings/julia/` and opens a PR. **Never hand-edit `Quiver.jl`.**

| Lives in `quiver` (canonical) | Pushed to `Quiver.jl` (generated) |
|---|---|
| `bindings/julia/src/` (incl. generated `c_api.jl`) | `src/` (verbatim mirror) |
| `bindings/julia/test/*.jl` (incl. portable `fixture.jl`) | `test/` (verbatim mirror) |
| repo-root `tests/schemas/` (shared SQL fixtures) | `test/schemas/` (vendored copy) |
| `scripts/julia/generate_artifacts.jl` → `Artifacts.toml` | `Artifacts.toml` |
| `bindings/julia/Project.toml` (UUID aligned to `cdbb3f72`) | `Project.toml` (verbatim copy) |
| `bindings/julia/LICENSE`, `.JuliaFormatter.toml` | same (verbatim) |

The full test suite runs in `Quiver.jl` CI against the **published artifact** on Ubuntu + Windows.
`fixture.jl`'s `tests_path()` resolves the repo-root `tests/` in the monorepo and the vendored
`test/schemas/` in the mirror, so the same test files work in both repos.

## Release flow (automated)

Triggered by the existing `Publish` workflow (`workflow_dispatch`). After the C++ libs,
wheels, JSR, tag, and GitHub release succeed, the **`publish-julia`** job:

1. Downloads the `native-linux-x86_64` / `native-windows-x86_64` artifacts from `build-native` (no recompile).
2. Runs `scripts/julia/generate_artifacts.jl`: tars each platform's libs into the loader
   layout (`lib/` on Linux, `bin/` on Windows), uploads to
   `s3://julia-artifacts/quiver/<version>/`, computes `git-tree-sha1` + `sha256`, and writes `Artifacts.toml`.
   The libs are `chmod 0o755` before tarring — **Windows DLLs must be executable in the artifact** or
   Pkg's extraction yields an ACL without execute and `LoadLibrary` fails with "Access is denied".
3. Checks out `Quiver.jl` and **faithfully mirrors the package**: `cp -r bindings/julia/{src,test}`,
   `cp bindings/julia/{Project.toml,LICENSE,.JuliaFormatter.toml}`, vendors `tests/schemas/` →
   `test/schemas/`, and drops in the new `Artifacts.toml`. Excludes `generator/`, `Manifest.toml`,
   `PUBLISHING.md`, `example2.jl`; preserves Quiver.jl's `.git`/`.github/`/`README`. `Project.toml`
   is copied verbatim (its UUID already matches the registered package).
4. Opens a PR to `Quiver.jl`.

Then a maintainer **reviews & merges** the PR, comments `@JuliaRegistrator register`, and
TagBot tags the release. Users `]add Quiver` from General; the S3 artifact downloads on install.

## Prerequisites (one-time setup — outside this repo)

**Runner & AWS credentials:** the `publish-julia` job runs on a **self-hosted `[self-hosted, linux]`
runner** whose IAM instance-profile role already grants write to the `julia-artifacts` bucket, so the
`aws` CLI picks up credentials from the default chain — **no AWS access-key secrets required**. Ensure
the `aws` CLI is installed on that runner. The job sets region `us-east-1`; change `AWS_DEFAULT_REGION`
in `publish.yml` if the bucket moves.

**Secret on the `quiver` repo:**
- `QUIVER_JL_TOKEN` — a fine-grained PAT or GitHub App token with **contents + pull-requests
  write on `psrenergy/Quiver.jl`** (the default `GITHUB_TOKEN` cannot push to another repo).

**S3 bucket:** objects under `quiver/<version>/` must be publicly readable (as PSRIO's are),
so General's AutoMerge can download the artifact during its install check.

## One-time `Quiver.jl` migration

The first `publish-julia` PR converts `Quiver.jl` from a `Quiver_jll` consumer into the
generated mirror. Review that first PR carefully; confirm it:
1. Replaces hand-written `src/` with the mirror, including the new `src/c_api.jl` loader.
2. `Project.toml`: drops `Quiver_jll` (deps + compat); adds `Artifacts` + `Libdl`; keeps
   `CEnum`, `Dates`; keeps `Quiver.jl`'s **UUID `cdbb3f72-2527-4dbd-9d0e-93533a5519ac`**;
   sets the synced version.
3. Adds `Artifacts.toml`.
4. Keeps `.github/workflows/TagBot.yml` and `@JuliaRegistrator` enabled.

Add an `AUTO-GENERATED` note to `Quiver.jl`'s README: *source of truth is
`psrenergy/quiver/bindings/julia`; do not hand-edit.*

## Local development

The loader (`src/c_api.jl`, generated from `generator/prologue.jl`) resolves `libquiver_c`
in three tiers:

1. `QUIVER_LIB_DIR` env var — explicit override.
2. an `Artifacts.toml` (present only in the published `Quiver.jl`) — the S3 artifact.
3. the in-tree `build/` directory — **monorepo local dev** (the default here).

So in this repo you just build the C++ core (`cmake --build build`) and run
`bindings/julia/test/test.bat`; no env var needed. Set `QUIVER_LIB_DIR` to point at a
different build if desired.

## Regenerating `c_api.jl`

When the C API changes, rebuild the C++ core and run `bindings/julia/generator/generator.bat`
(Windows). The generator emits `src/c_api.jl` including the loader from `prologue.jl`. Commit
the regenerated `c_api.jl`; the next release mirrors it into `Quiver.jl`.

> Note: there is no automatic check that `c_api.jl` is in sync with the headers — regenerate
> after C-API changes. (A future CI diff-check could enforce this.)
