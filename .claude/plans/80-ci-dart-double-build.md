# 80 — CI `dart-coverage`: drop the dead `build-cpp` and copy steps

**Batch** 7 · **Severity** medium (CI builds the C++ library twice, and three docs repeat a false premise) · **Breaking** no · **Size** S · **Layers** CI (`.github/workflows/ci.yml`), docs (`bindings/dart/AGENTS.md`, root AGENTS.md, optionally `bindings/dart/hook/build.dart`)
**Depends on** none · **Overlaps with** 81/83/88 (other `.github/` edits)

## Why

The `dart-coverage` job in `.github/workflows/ci.yml` (currently ~L125-150) does this:
```yaml
      - uses: ./.github/actions/build-cpp

      - name: Install Dart dependencies
        working-directory: bindings/dart
        run: dart pub get

      - name: Copy shared libraries
        run: |
          cp build/lib/libquiver.so bindings/dart/
          cp build/lib/libquiver_c.so bindings/dart/

      - name: Run tests with coverage
        working-directory: bindings/dart
        run: |
          dart pub global activate coverage
          dart pub global run coverage:test_with_coverage --out=coverage
```
`dart test` (and `coverage:test_with_coverage`, which wraps it) runs the package's native-assets
build hook (`bindings/dart/hook/build.dart`). The hook builds the C++ library itself, with
`QUIVER_UNVERSIONED_SHARED=ON`, and registers that output. So the `build-cpp` step and the copied
`.so` files are never loaded. The library is built twice and the job takes longer.

This also means three docs are wrong:
- `bindings/dart/AGENTS.md` (~L69): "Note **no CI job runs this hook on any OS** — Dart is built and
  published by hand."
- Root `AGENTS.md` Build System (~L384): "No CI job exercises the ON configuration."
- `bindings/dart/hook/build.dart` (~L74): "no CI job runs this hook". That comment is about the
  macOS `appleArgs`, and it is true for macOS only.

## Constraints and decisions

- Delete the `build-cpp` and "Copy shared libraries" steps. **Keep** "Install lcov": the coverage
  filtering step needs it.
- **Do not** also flip `build_tests` to OFF for other jobs (the Julia/Python/Bun ones), as the
  original finding proposed. The policy verifier noted that without adding `inputs.build_tests` to the
  sccache key in `.github/actions/build-cpp/action.yml`, a tests-OFF job could save the shared
  `sccache-<os>-Debug-false` cache entry that the `build` Debug jobs restore. That is out of scope.
- Before deleting, confirm that the hook really builds in CI. The next CI log for `dart-coverage`
  should show the CMake/native-toolchain output from `hook/build.dart` during the test step. If the
  job's test step fails after the change, the premise was wrong: restore the steps and record why in
  `bindings/dart/AGENTS.md`.

## Changes

1. `.github/workflows/ci.yml`, job `dart-coverage`: delete the step
   `- uses: ./.github/actions/build-cpp` and the step `- name: Copy shared libraries` (with its `run`
   block). Leave checkout, setup-dart, Install lcov, `dart pub get` and the coverage steps.
2. `bindings/dart/AGENTS.md` ~L69: replace "Note **no CI job runs this hook on any OS** — Dart is
   built and published by hand." with "The Linux **Dart Coverage** CI job runs this hook through
   `dart test` on every push; the macOS/Windows paths and the three macOS workarounds above remain
   unexercised in CI. Dart is published by hand."
3. Root `AGENTS.md` ~L384: "No CI job exercises the ON configuration." becomes "Only the Linux Dart
   Coverage CI job exercises the ON configuration (through the hook); no macOS or Windows job does."
4. Optional: `bindings/dart/hook/build.dart` ~L74, change "no CI job runs this hook" to "no macOS CI
   job runs this hook".

Also check `bindings/dart/test/coverage.sh` for an `export LD_LIBRARY_PATH=.../build/lib` line. It
is dead for the same reason, and nothing references the script (`grep -rn "coverage.sh" .github scripts bindings`).
If both hold, delete the line, or delete the script if it is entirely unused.

## Tests

The CI run is the test. Push to a branch or open a PR and confirm `dart-coverage` passes and
produces coverage.

## Verification

1. `uv run --with pyyaml python -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))"` parses.
2. If `actionlint` is installed, run `actionlint .github/workflows/ci.yml`.
3. The CI run on the PR shows `dart-coverage` green, with the hook's CMake output in the test step.

## Acceptance criteria

- [ ] `dart-coverage` has no `build-cpp` or copy step.
- [ ] The three doc statements match reality.
- [ ] CI is green.

## Pitfalls

- Do not remove `build-cpp` from the other jobs. They load `build/lib` or `build/bin` directly.

## Out of scope

- sccache key changes and `build_tests` defaults.
