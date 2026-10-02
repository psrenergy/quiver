# 88 — Stale names in CI scripts and docs (`release.yml`, `setup-node@v6`)

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** comments in `scripts/ci/dispatch_workflow.sh`, `.github/workflows/*.yml`, `.github/AGENTS.md`
**Depends on** none · **Overlaps with** 81, 83 (other edits in the same workflow files), 77, 80 (other `.github/AGENTS.md` edits)

## Why

1. **`release.yml` never existed.** The orchestrator is `publish.yml`. Stale references:
   - `scripts/ci/dispatch_workflow.sh` ~L4: "# Used by release.yml to orchestrate the four publish workflows."
   - same file, ~L10: "... release.yml's ..."
   - `.github/workflows/publish-python.yml` ~L91: "# skip-existing makes a release.yml re-run idempotent ..."
2. **`setup-node@v6`.** `.github/AGENTS.md` (~L142) says npm publishes "via `actions/setup-node@v6`",
   but `publish-js.yml` (~L29) uses `actions/setup-node@v7`. Pinned version numbers in prose go stale
   on every action bump.
3. **Misattached parenthetical.** `.github/AGENTS.md` table row (~L13) for `publish-s3.yml`:
   "Builds native libs for `linux-x86_64`, `macos-aarch64`, `windows-x86_64` (via
   `scripts/ci/native_s3.sh`) and stages them on S3". `native_s3.sh` stages the libraries; it does not
   build them.

## Constraints and decisions

- Comment and doc edits only.
- Make the setup-node references version-neutral, so the next bump cannot make them stale again.
- The table row does not need to repeat how the libs are built. The "glibc floor" paragraph below it
  (~L26-50) already documents that.

## Changes

1. `scripts/ci/dispatch_workflow.sh` ~L4 and ~L10: `release.yml` becomes `publish.yml`.
2. `.github/workflows/publish-python.yml` ~L91: `release.yml` becomes `publish.yml`.
3. `.github/AGENTS.md` ~L142: "via `actions/setup-node@v6`" becomes "via `actions/setup-node`". Check
   ~L144-147 for other version-pinned mentions and make them version-neutral too. Also check
   `publish-js.yml`'s own comments for a stale `@v6`: `grep -n "@v6" .github/workflows/publish-js.yml`.
4. `.github/AGENTS.md` ~L13: move the parenthetical to "... `windows-x86_64` and stages them on S3
   (via `scripts/ci/native_s3.sh upload`)".

Afterwards run `grep -rn "release\.yml" scripts .github`. It must print nothing.

## Tests

None.

## Verification

- The grep above prints nothing, and `grep -n "setup-node@v" .github/AGENTS.md` prints nothing.
- The YAML still parses: `uv run --with pyyaml python -c "import yaml; yaml.safe_load(open('.github/workflows/publish-python.yml'))"`.
- `dispatch_workflow.sh` is LF (a shell script). Keep it LF.

## Acceptance criteria

- [x] No `release.yml` references remain, the setup-node prose is version-neutral, and the table row
      is accurate.

## Pitfalls

- None.

## Out of scope

- Workflow logic changes (plans 80, 81, 83).

## Implementation notes

- **Done as planned:** `release.yml` → `publish.yml` in `dispatch_workflow.sh` (L4, L10) and
  `publish-python.yml` (L91). On L10 only the filename changed: `publish.yml`'s concurrency group
  really is named `release`. The `publish-s3.yml` table row now reads "... and stages them on S3
  (via `scripts/ci/native_s3.sh upload`)", and "via `actions/setup-node`" has no version.
- **Drift fixed:**
  - The table row also lists `linux-aarch64`, which the plan's quote omitted. Kept.
  - The npm paragraph is at ~L153-158, after plan 81 added two lines above it.
  - It held a **second** pin, "(v6 caches by default; ...)". That now reads "(it caches by default; ...)".
  - `publish-js.yml`'s setup-node comment said "— v6 caching would fail.". The plan's
    `grep "@v6"` misses it (there is no `@`). It now reads "— setup-node's default caching would fail.".
- **Verification:** `grep -rn "release\.yml" scripts .github`, `grep -n "setup-node@v" .github/AGENTS.md`
  and `grep -n "v6" .github/AGENTS.md .github/workflows/publish-js.yml` print nothing. Both edited
  workflows parse with PyYAML. `bash -n` passes on `dispatch_workflow.sh`.
- **Line endings:** this checkout has `core.autocrlf=true`, so the working tree is CRLF. `git ls-files --eol`
  shows `i/lf` for all four files, so the stored blobs stay LF.
- **`scripts/format.bat`:** biome "fixed" 43 JS files, but the only change was CRLF→LF in the working
  tree. There was no content diff against the index, so they were restored and not committed.
  Every other formatter reported no change.
- **For later plans (not fixed, out of scope):** `.github/AGENTS.md` ~L35 explains `docker run`
  over `container:` by "the runner's Node20 actions (`checkout`, `upload-artifact`)". Those
  actions are now `@v7` in `publish-s3.yml`, so the runtime name may be stale. The reasoning
  (no Node runtime inside a glibc-2.17 image) still holds.
