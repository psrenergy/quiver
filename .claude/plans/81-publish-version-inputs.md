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

- [x] No child workflow declares or reads a `version` input.
- [x] `publish.yml` passes no `version=` argument, and every `ref` input is kept.
- [x] `.github/AGENTS.md` is updated.

## Pitfalls

- `publish-python.yml` may have no `version` input. Check it and leave it unchanged if so.
- Keep the dispatch argument order `workflow ref key=value...` that `dispatch_workflow.sh` expects.

## Out of scope

- Other publish-workflow changes (plan 83).

## Implementation notes

- **Done as planned, no drift.** Every quoted excerpt and line anchor matched exactly: s3 L6-9 and
  L34-37, julia L6-9 and L30-33, js L6-9 and L42-45, publish.yml L66/L101/L139. Each child lost its
  4-line `version:` input and its 4-line override block, and now reads
  `v="$(python3 scripts/assert_version.py)"` followed by the unchanged
  `echo "version=$v" >> "$GITHUB_OUTPUT"`.
- **Why the assignment is kept on its own line.** Under Actions' default `bash -e` shell, a bare
  `v="$(...)"` assignment carries the command's exit status, so a manifest mismatch still fails the
  step. Inlining the command into `echo "version=$(...)"` would lose that status and publish an
  empty version.
- **`publish.yml`.** I dropped `"version=$VERSION"` from all three dispatch lines and removed the
  now-unused `VERSION:` from the publish-s3 step's `env:`. `"ref=$SHA"` stays. The julia, python
  and js steps keep `VERSION` for the `"v$VERSION"` tag ref.
- **`publish-python.yml` is unchanged.** It only ever declared `ref` (plan pitfall confirmed).
  `git grep` found no other reader of `inputs.version` and no other doc describing a child
  `version` input.
- **`.github/AGENTS.md`.** The plan's sentence is appended to the paragraph that describes how the
  publish workflows read the version (~L70-75).
- **Master:** `origin/master` (`629db2b`) was already in the branch, so `git merge origin/master`
  reported "Already up to date".
- **Verification:**
  - Before the change, the plan's grep printed 6 hits. After it, the grep prints nothing (exit 1).
  - All 7 workflows load under `yaml.safe_load`.
  - `actionlint -shellcheck=` (the `ci.yml` flags, run via `uv run --with actionlint-py`, which
    installed v1.7.12) exits 0 both before and after the change.
  - The edited files keep their CRLF working-tree endings.
  - `scripts/format.bat` exited 0. clang-format, Julia, Dart and ruff changed nothing. Biome again
    rewrote 43 JS files from CRLF to LF, and the whitespace-insensitive diff was empty, so
    `git checkout -- bindings/js` restored them.
  - No test suites were run, because no code changed.
- **No CHANGELOG entry.** This is release tooling, not a library change. `CHANGELOG.md` also has
  no `[0.12.9] — unreleased` section yet.
- **Merge timing.** `publish.yml` dispatches `publish-s3` on the branch name (`github.ref_name`),
  not on the SHA. A run of the *old* orchestrator still in flight when this merges would send
  `version=` to the *new* child, and the API would reject it with a 422. Do not merge while a
  `publish.yml` run is in progress. Julia and JS are dispatched on the release tag, which is
  created at the orchestrator's own SHA, so they always read matching files.
- **For 83/88:** 83 edits other steps of `publish-js.yml`. The `Resolve version` step (`id: ver`)
  is now 3 lines shorter, and its `steps.ver.outputs.version` consumers are unchanged. 88 renames
  stale names in the same workflows and in `dispatch_workflow.sh`, which this plan did not touch.
