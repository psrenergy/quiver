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

- [x] build-all contains no copy of the suite commands.
- [x] build-all's exit code is test-all's.
- [x] Both AGENTS.md sentences are updated.

## Pitfalls

- `setlocal enabledelayedexpansion` in build-all does not leak into the `call`ed test-all, which has
  its own `setlocal`. Fine either way.
- If test-all uses `exit` without `/b`, it would close the caller's shell. Check that it uses
  `exit /b`.

## Out of scope

- Making test-all accept a build type, unless the build-type check above shows it is needed.

## Implementation notes

- **"Depends on 65" did not apply.** Plan 65 landed (`69d939c`) as a docs-only change. `01e78d7`
  had already removed the CLI smoke step from `test-all.bat`, so there was nothing left to fix.
  Several details in this plan were stale as a result, and I corrected them:
  - test-all has **six** steps (`[n/6]`) and no smoke test. I left "smoke test" out of the new REM
    text: the header reads "(all six test suites, with a summary)" and the test banner reads "the
    one runner (all six suites, with a summary)".
  - Both "before" doc sentences were the plan-65 versions, not the ones quoted above. The
    replacement texts are as specified. Root AGENTS.md now says "...`build-all.bat` builds and then
    calls `test-all.bat` (breakdown in `tests/AGENTS.md`)". tests/AGENTS.md now says
    "`scripts/build-all.bat` configures and builds, then calls `test-all.bat`."
  - Small extra fix: the build banner `REM Step 1: Build C++ Library and C API` became
    `REM Build C++ Library and C API`, since no step 2 follows any more.
- **Build-type check: no pass-through needed.** build-all configures Ninja, which is single-config,
  so Debug and Release both write to `build\bin`. test-all reads `build\bin\*.exe`, the JS and
  Python `test.bat` files put `build\bin` on PATH, and Julia's in-tree fallback is `build/`. Dart
  builds its own native library through its hook, as it did before. Out-of-scope item: unchanged.
- **Behaviour change (intended):** build-all used to stop at the first failing suite. It now runs
  every suite and prints test-all's PASS/FAIL/SKIP summary. Its exit code is test-all's: I checked
  1 on failure and 0 on success with scratch stubs that go through `shift` and `call` +
  `exit /b %errorlevel%`.
- No CHANGELOG entry: the plan specifies none, and this is developer tooling, as in plan 65.
- Verification:
  1. `scripts\build-all.bat` (Debug): `[build]`, then test-all's `[1/6]`...`[6/6]`, every suite
     PASS, exit 0. Counts: C++ 1394, C API 543, Julia 1574, Dart 444, JS 241, Python 350.
  2. `scripts\build-all.bat --release`: **incomplete.** Claude Code killed the run partway through
     the compile (151/208) because the machine was low on memory, with parallel sessions building.
     The `FAILED` line in its log is the killed `cl.exe`, not a compile error. Before the kill it
     had configured the same `build\` as Release (`Build type: Release`,
     `CMakeCache CMAKE_BUILD_TYPE=Release`), which settles the build-type question. Afterwards I
     put `build\` back on Debug (configure only). Re-run it when memory allows.
  3. `git diff --stat scripts/build-all.bat`: about +8/−136, and `file` still reports CRLF.
- `scripts\format.bat` passes, but on this autocrlf checkout biome rewrites every JS file from
  CRLF to LF in the working tree. The content is identical (`git diff --ignore-cr-at-eol` is
  empty), so I restored those files with `git checkout`. Pre-existing and not caused by this
  plan; worth knowing for plan 87.
- **For later plans:** test-all runs under `setlocal enabledelayedexpansion`, so its final
  `echo All tests PASSED!` / `Some tests FAILED!` prints without the `!`. Pre-existing and
  cosmetic; left alone.
- Acceptance criteria:
  - [x] build-all has no copy of the suite commands (`grep` for `quiver_tests`, `quiver_c_tests`,
    `test.bat` and `[n/7]` finds nothing).
  - [x] build-all's exit code is test-all's (`call` + `exit /b %errorlevel%`; 0 on the Debug run,
    1 and 0 in the stub check).
  - [x] Both AGENTS.md sentences are updated.
