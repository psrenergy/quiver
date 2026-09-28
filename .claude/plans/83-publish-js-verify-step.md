# 83 — `publish-js`: delete the redundant "Verify native libraries" step; check the tarball against what was downloaded

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** CI (`.github/workflows/publish-js.yml`), comments in root `CMakeLists.txt` and root AGENTS.md
**Depends on** none · **Overlaps with** 81 (edits `publish-js.yml`'s version resolution), 88 (stale comments in the same file)

## Why

The seven-file list of shipped natives is written out **three times**:
- `scripts/ci/native_s3.sh`: `files_for` (~L10-17), the source of truth for upload and download.
- `publish-js.yml`, step "Verify native libraries are present" (~L51-74): a `required=( ... )`
  array of the same seven paths, checked with `[ -f ]`.
- `publish-js.yml`, step "Pack and assert native libs are in the tarball" (~L76-95): the same
  seven paths again, in a `for f in ...` list checked against `tar -tzf`.

The verify step is redundant. `native_s3.sh download` exits non-zero when any file of `files_for`
is missing, so a missing native already fails the job at the download step. Confirm this by reading
`native_s3.sh`'s `download` branch (~L38-52): it must `set -e` or check each `aws s3 cp`. If it does
**not** fail on a missing file, keep the verify step and change only the tarball check (step 2
below).

The duplicated list is the real risk. A native added to `files_for` but not to the two workflow
lists would ship unchecked.

## Constraints and decisions

- Keep one list, `native_s3.sh`'s `files_for`. The tarball check asserts against **what was
  downloaded** (`find libs -type f`), so it needs no list of its own.
- Keep `publish-s3.yml`'s `lib_paths`. It names build-output paths, which `files_for` cannot express,
  so it is not a copy of the ship list.

## Changes

1. `.github/workflows/publish-js.yml`: delete the whole "Verify native libraries are present" step
   (~L51-74), unless the check above shows the download does not fail on a missing file.
2. In "Pack and assert native libs are in the tarball", replace the hardcoded list loop
   ```bash
          for f in \
            libs/linux-x86_64/libquiver.so libs/linux-x86_64/libquiver.so.0 libs/linux-x86_64/libquiver_c.so \
            libs/macos-aarch64/libquiver.0.dylib libs/macos-aarch64/libquiver_c.dylib \
            libs/windows-x86_64/libquiver.dll libs/windows-x86_64/libquiver_c.dll; do
   ```
   with
   ```bash
          for f in $(find libs -type f | sort); do
   ```
   Keep the loop body (`printf ... | grep -Fxq "$f" || { echo "::error::tarball $tarball is MISSING $f"; exit 1; }`)
   and the `done`. Add one guard before the loop, so an empty `libs/` cannot pass vacuously:
   `[ -n "$(find libs -type f)" ] || { echo "::error::no native libraries downloaded"; exit 1; }`.
3. Comments that say `publish-js.yml` ships the versioned names **by name** are no longer true.
   Remove `publish-js.yml` from them:
   - root `CMakeLists.txt` (~L21-22): "`scripts/ci/native_s3.sh`, `publish-s3.yml` and
     `publish-js.yml` ship libquiver.0.dylib / libquiver.so.0 by name" becomes "... `native_s3.sh` and
     `publish-s3.yml` ship ...".
   - root `AGENTS.md` Build System, `QUIVER_UNVERSIONED_SHARED` paragraph (~L382): same edit.

## Tests

None locally. The next npm publish run exercises the step.

## Verification

1. YAML parses: `uv run --with pyyaml python -c "import yaml; yaml.safe_load(open('.github/workflows/publish-js.yml'))"`.
2. `actionlint` if installed.
3. Dry-run the loop locally: create `bindings/js/libs/linux-x86_64/libquiver.so`, run `npm pack` in
   `bindings/js`, then run the new loop by hand and check it passes. Remove one file from the tarball
   check input and confirm it fails. Delete the scratch `libs/` afterwards.

## Acceptance criteria

- [ ] `publish-js.yml` no longer spells out the seven natives.
- [ ] The tarball check covers every downloaded file and fails on an empty download.
- [ ] Both comments are updated.

## Pitfalls

- `bindings/js/package.json` `files` includes `libs/**`, so every downloaded file must be in the
  tarball. That is what the new loop asserts.

## Out of scope

- The download or upload logic in `native_s3.sh`.
