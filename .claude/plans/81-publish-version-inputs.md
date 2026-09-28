# 81 — Publish workflows: remove the redundant `version` inputs

**Batch** 7 · **Severity** medium (a manual dispatch can publish artifacts under the wrong version) · **Breaking** no (release tooling) · **Size** S · **Layers** CI (`.github/workflows/publish-{s3,julia,js}.yml`, `publish.yml`), `.github/AGENTS.md`
**Depends on** none · **Overlaps with** 83 (edits `publish-js.yml` elsewhere), 88 (stale names in the same workflows), 77/80 (other `.github/` docs)

## Why

Each child publish workflow takes a `workflow_dispatch` `version` input and prefers it over the
version in the checked-out manifests, e.g. `publish-s3.yml` (~L5-11, ~L34-37):
```yaml
    inputs:
      version:
        description: "Override the version"
      ref:
        description: "Override ref or sha"
...
          v="${{ inputs.version }}"
          ...
            v="$(python3 scripts/assert_version.py)"
```
`publish-julia.yml` (~L6-9, ~L30-33) and `publish-js.yml` (~L6-9, ~L42-45) do the same. The
orchestrator `publish.yml` passes the version it already derived (~L66, ~L101, ~L139):
```yaml
        run: bash scripts/ci/dispatch_workflow.sh publish-s3.yml "$DISPATCH_REF" "version=$VERSION" "ref=$SHA"
        run: bash scripts/ci/dispatch_workflow.sh publish-julia.yml "v$VERSION" "version=$VERSION"
        run: bash scripts/ci/dispatch_workflow.sh publish-js.yml "v$VERSION" "version=$VERSION"
```
In the orchestrated flow the input is redundant, because the checkout at that ref has the same
version. In a manual dispatch it is harmful: typing a version that differs from the checked-out
manifests publishes those manifests' artifacts labelled with the typed version. The single source
of truth is `CMakeLists.txt`, checked by `scripts/assert_version.py` (root AGENTS.md "Versioning").

## Constraints and decisions

- Delete the three `version` inputs, and resolve the version from `assert_version.py` at checkout,
  unconditionally.
- **The same commit** must drop the `"version=$VERSION"` arguments in `publish.yml`.
  `scripts/ci/dispatch_workflow.sh` sends inputs to the GitHub API, which rejects an undeclared
  input with HTTP 422, so splitting the change breaks the next release.
- **Keep every `ref` input.** `publish-s3` needs its `ref` because the tag does not exist yet when S3
  runs. On julia/js/python it allows running the current workflow file against an older tag or SHA,
  which the Actions UI ref selector cannot do.

## Changes

1. `.github/workflows/publish-s3.yml`, `publish-julia.yml`, `publish-js.yml`:
   - Delete the `version:` input block (its `description`, `required`, `type` lines) under
     `workflow_dispatch.inputs`. Keep `ref:`.
   - Replace the version-resolution block (the `v="${{ inputs.version }}"` / fallback `if`) with
     `v="$(python3 scripts/assert_version.py)"`. Keep whatever the block does with `v` afterwards,
     such as writing to `$GITHUB_OUTPUT`/`$GITHUB_ENV`. Read the full block in each file first.
2. `.github/workflows/publish.yml`: at ~L66, ~L101 and ~L139, delete the `"version=$VERSION"`
   argument. Keep `"ref=$SHA"` at ~L66. After this, `VERSION` is used only for the `"v$VERSION"`
   tag ref on the julia/js dispatch lines. If the publish-s3 dispatch step no longer references
   `VERSION`, drop it from that step's `env:`.
3. `.github/AGENTS.md`: add one line where the child publish workflows are described: "The child
   publish workflows take no `version` input: each resolves it from its checkout via
   `scripts/assert_version.py`; `ref` is their only override."

## Tests

No automated test. Validate the YAML and do a dry read.

## Verification

1. `uv run --with pyyaml python -c "import yaml,glob; [yaml.safe_load(open(f)) for f in glob.glob('.github/workflows/*.yml')]"`
2. `actionlint` if installed.
3. `grep -n "inputs.version\|version=\$VERSION" .github/workflows/*.yml` prints nothing.
4. The next real release, via the Bump Version PR and then `publish.yml`, must dispatch all
   children without a 422. Flag this in the PR description for the releaser.

## Acceptance criteria

- [ ] No child workflow declares or reads a `version` input.
- [ ] `publish.yml` passes no `version=` argument, and every `ref` input is kept.
- [ ] `.github/AGENTS.md` is updated.

## Pitfalls

- `publish-python.yml` may have no `version` input. Check it and leave it unchanged if so.
- Keep the dispatch argument order `workflow ref key=value...` that `dispatch_workflow.sh` expects.

## Out of scope

- Other publish-workflow changes (plan 83).
