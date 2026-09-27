# 88 — Stale names in CI scripts and docs (`release.yml`, `setup-node@v6`)

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** comments in `scripts/ci/dispatch_workflow.sh`, `.github/workflows/*.yml`, `.github/CLAUDE.md`
**Depends on** none · **Overlaps with** 81, 83 (other edits in the same workflow files), 77, 80 (other `.github/CLAUDE.md` edits)

## Why

1. **`release.yml` never existed.** The orchestrator is `publish.yml`. Stale references:
   - `scripts/ci/dispatch_workflow.sh` ~L4: "# Used by release.yml to orchestrate the four publish workflows."
   - same file, ~L10: "... release.yml's ..."
   - `.github/workflows/publish-python.yml` ~L91: "# skip-existing makes a release.yml re-run idempotent ..."
2. **`setup-node@v6`.** `.github/CLAUDE.md` (~L142) says npm publishes "via `actions/setup-node@v6`",
   but `publish-js.yml` (~L29) uses `actions/setup-node@v7`. Pinned version numbers in prose go stale
   on every action bump.
3. **Misattached parenthetical.** `.github/CLAUDE.md` table row (~L13) for `publish-s3.yml`:
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
3. `.github/CLAUDE.md` ~L142: "via `actions/setup-node@v6`" becomes "via `actions/setup-node`". Check
   ~L144-147 for other version-pinned mentions and make them version-neutral too. Also check
   `publish-js.yml`'s own comments for a stale `@v6`: `grep -n "@v6" .github/workflows/publish-js.yml`.
4. `.github/CLAUDE.md` ~L13: move the parenthetical to "... `windows-x86_64` and stages them on S3
   (via `scripts/ci/native_s3.sh upload`)".

Afterwards run `grep -rn "release\.yml" scripts .github`. It must print nothing.

## Tests

None.

## Verification

- The grep above prints nothing, and `grep -n "setup-node@v" .github/CLAUDE.md` prints nothing.
- The YAML still parses: `uv run --with pyyaml python -c "import yaml; yaml.safe_load(open('.github/workflows/publish-python.yml'))"`.
- `dispatch_workflow.sh` is LF (a shell script). Keep it LF.

## Acceptance criteria

- [ ] No `release.yml` references remain, the setup-node prose is version-neutral, and the table row
      is accurate.

## Pitfalls

- None.

## Out of scope

- Workflow logic changes (plans 80, 81, 83).
