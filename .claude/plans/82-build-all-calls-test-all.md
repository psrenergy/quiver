# 82 — `build-all.bat` builds, then calls `test-all.bat`

**Batch** 7 · **Severity** medium · **Breaking** no (developer script; build-all now reports a summary instead of stopping at the first failing suite) · **Size** S · **Layers** tooling (`scripts/build-all.bat`), root AGENTS.md, tests/AGENTS.md
**Depends on** 65 (fixes `test-all.bat`'s CLI smoke test; without it, `build-all` would start failing on step 7) · **Overlaps with** 65 (both touch the test-script docs)

## Why

`scripts/build-all.bat` configures and builds (currently ~L52-64), then **re-implements**
`scripts/test-all.bat`'s six test steps (~L70-178) plus its own summary (~L180-197). The two copies
differ in failure handling: build-all stops at the first failing suite, test-all runs them all and
prints a summary. build-all also skips test-all's CLI smoke test. The docs record the drift instead
of removing it: root AGENTS.md (~L341-342) says "`test-all.bat` runs the six suites below plus a
`quiver_cli` smoke test; `build-all.bat` builds and then runs the six suites", and tests/AGENTS.md
(~L201-202) says "`scripts/build-all.bat` is also seven steps, but its step 1 is the build itself
followed by the six suites — it does not run the CLI smoke test."

Principle: delete duplication. One test runner.

## Constraints and decisions

- Keep build-all's argument parsing (`--release` etc., ~L20-38), its help text, and the configure +
  build lines (~L52-64).
- Call test-all with `%ROOT_DIR%`, **not** `%~dp0`. The `shift` in build-all's argument loop has
  already moved `%0`, so `%~dp0` is no longer the script's directory at that point.
- Check that `test-all.bat` tests the **same build type** build-all built. If test-all hardcodes
  `build\bin` (Debug layout) and build-all `--release` builds into the same `build\` dir, that is
  fine. If they differ, pass the build type through (`call ... %BUILD_TYPE%`) and teach test-all to
  accept it. Read test-all's header first.
- `.bat` files are CRLF. Keep them CRLF.

## Changes

### `scripts/build-all.bat`

1. Replace everything from the `REM Step 2: Run C++ Tests` banner (~L66-70) to the end of the file
   with:
   ```bat
   REM ============================================================
   REM Tests: the one runner (all six suites + the CLI smoke test, with a summary)
   REM ============================================================
   call "%ROOT_DIR%\scripts\test-all.bat"
   exit /b %errorlevel%
   ```
2. Relabel the build step's `echo [1/7] ...` lines to `echo [build] ...`, since test-all prints its
   own `[n/7]` numbering.
3. Update the header `REM` block (~L7-13): "Builds C++ library, C API, and runs all tests:" plus the
   list becomes "Configures and builds the C++ library and C API, then runs scripts\test-all.bat (all
   suites plus the CLI smoke test)."

### Docs

- Root `AGENTS.md` ~L341-342: "`test-all.bat` runs the six suites below plus a `quiver_cli` smoke
  test; `build-all.bat` builds and then runs the six suites (breakdown in `tests/AGENTS.md`)." becomes
  "`test-all.bat` runs the six suites below plus a `quiver_cli` smoke test; `build-all.bat` builds and
  then calls `test-all.bat` (breakdown in `tests/AGENTS.md`)."
- `tests/AGENTS.md` ~L201-202: replace the "`scripts/build-all.bat` is also seven steps ..." sentence
  with "`scripts/build-all.bat` configures and builds, then calls `test-all.bat`."
- Keep tests/AGENTS.md ~L186 (the benchmark is built by build-all). It stays true.

## Tests

Run the script. That run is the test.

## Verification

1. `scripts\build-all.bat`. It builds, then prints test-all's `[1/7]`...`[7/7]` and its summary.
   The exit code is 0 when everything passes.
2. `scripts\build-all.bat --release`. Check that the release build is what gets tested (see the
   build-type constraint above).
3. `git diff --stat scripts/build-all.bat` shows a large deletion and a few added lines, not an
   every-line diff (that would mean CRLF was lost).

## Acceptance criteria

- [ ] build-all contains no copy of the suite commands.
- [ ] build-all's exit code is test-all's.
- [ ] Both AGENTS.md sentences are updated.

## Pitfalls

- `setlocal enabledelayedexpansion` in build-all does not leak into the `call`ed test-all, which has
  its own `setlocal`. Fine either way.
- If test-all uses `exit` without `/b`, it would close the caller's shell. Check that it uses
  `exit /b`.

## Out of scope

- Making test-all accept a build type, unless the build-type check above shows it is needed.
