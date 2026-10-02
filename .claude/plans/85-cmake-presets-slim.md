# 85 — `CMakePresets.json`: two presets that mirror what the scripts do

**Batch** 7 · **Severity** low · **Breaking** no (no script or CI uses the presets) · **Size** S · **Layers** build config, root AGENTS.md, tests/AGENTS.md
**Depends on** none · **Overlaps with** 86 (root `CMakeLists.txt`), 44/46/48 (their Release-tree verification steps could use the new `release` preset)

## Why

No script, workflow or checked-in editor config reads `CMakePresets.json`
(`grep -rn "\-\-preset" scripts .github` finds nothing). As it stands the file:
- declares `cmakeMinimumRequired` 3.21 while the project requires 3.26 (`CMakeLists.txt`
  `cmake_minimum_required(VERSION 3.26.0)`);
- pins a `windows-release` preset to the "Visual Studio 17 2022" generator, which is not installed
  here (VS 18 is);
- has a `release` preset with `QUIVER_BUILD_TESTS=OFF`. tests/AGENTS.md (~L127-132) has to warn
  about that trap: "Do not use the plain `release` CMake preset for this ... it produces a Release
  tree with no test binary at all and would report success while testing nothing";
- keeps a host-conditioned `windows-release`/`linux-release` pair and a hidden `base` that sets
  `CMAKE_EXPORT_COMPILE_COMMANDS`, which the root CMakeLists already sets.

## Constraints and decisions

- **Maintainer decision (binding):** slim, don't delete. Keep `dev` and `release`, both Ninja, both
  with tests and the C API ON, with matching build and test presets. Set `cmakeMinimumRequired` to
  3.26. Update the root AGENTS.md Presets bullet and the tests/AGENTS.md trap note.
- Pin `"generator": "Ninja"` on both presets. Otherwise Windows falls back to the multi-config VS
  generator, where `CMAKE_BUILD_TYPE` is ignored.
- Presets build into `build/<presetName>/`, which is separate from the manual `build/` directory.

## Changes

### `CMakePresets.json` (whole file)

```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 26, "patch": 0 },
  "configurePresets": [
    {
      "name": "dev",
      "displayName": "Development (Debug, tests + C API)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "QUIVER_BUILD_TESTS": "ON",
        "QUIVER_BUILD_C_API": "ON"
      }
    },
    {
      "name": "release",
      "displayName": "Release (tests + C API)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "QUIVER_BUILD_TESTS": "ON",
        "QUIVER_BUILD_C_API": "ON"
      }
    }
  ],
  "buildPresets": [
    { "name": "dev", "configurePreset": "dev" },
    { "name": "release", "configurePreset": "release" }
  ],
  "testPresets": [
    { "name": "dev", "configurePreset": "dev", "output": { "outputOnFailure": true } },
    { "name": "release", "configurePreset": "release", "output": { "outputOnFailure": true } }
  ]
}
```
Check that the root `CMakeLists.txt` sets `CMAKE_EXPORT_COMPILE_COMMANDS` itself
(`grep -n "EXPORT_COMPILE_COMMANDS" CMakeLists.txt`). If it does not, keep that cache variable in
both presets.

### Root `AGENTS.md`, Build System → "Presets" bullet

New text:
> - **Presets** (`CMakePresets.json`): `dev` (Debug) and `release` (Release), both Ninja with tests
>   and the C API ON, each with a matching build and test preset. They build into
>   `build/<presetName>/`; the plain `build/` directory is the manual configure above. No script or
>   CI job uses them — they are for IDEs and ad-hoc Release test runs.

### `tests/AGENTS.md` (~L127-132)

Replace the "Do not use the plain `release` CMake preset ... Configure a separate tree explicitly:
`cmake -S . -B build-release ...`" text with:
> Build Release with tests via the preset: `cmake --preset release && cmake --build --preset release`,
> then run `build/release/bin/quiver_tests.exe --gtest_filter='LuaRunner*'`.

Check the binary location first. Outputs go to `build/<preset>/bin/` if the root CMakeLists sets
`CMAKE_RUNTIME_OUTPUT_DIRECTORY` relative to the binary dir (`grep -n "RUNTIME_OUTPUT" CMakeLists.txt`).

## Tests

Configure and build with both presets once.

## Verification

1. `cmake --preset dev && cmake --build --preset dev && ctest --preset dev`
2. `cmake --preset release && cmake --build --preset release && build/release/bin/quiver_tests.exe --gtest_filter=LuaRunner*`
3. `cmake --list-presets` shows exactly `dev` and `release`.

## Acceptance criteria

- [ ] Two configure presets, both Ninja with tests ON. `cmakeMinimumRequired` is 3.26.
- [ ] The root AGENTS.md and tests/AGENTS.md match.

## Pitfalls

- Ninja must be on PATH. It is via the VS CMake bundle. The presets do not bundle it.

## Out of scope

- Using presets in scripts or CI.

## Implementation notes

- **Done as planned.** `CMakePresets.json` is the plan's JSON verbatim. The root AGENTS.md
  Presets bullet is the plan's text. The tests/AGENTS.md trap note is replaced. Before editing, the
  branch integrated master at `a0a5432` (plans 79 and 81) as a fast-forward. Neither plan touches
  the presets or the two AGENTS.md hunks.
- **Drift fixed:**
  - `CMAKE_EXPORT_COMPILE_COMMANDS ON` is set in `cmake/CompilerOptions.cmake`, not the root
    `CMakeLists.txt` that the plan's grep names. CompilerOptions is included from the root file, so
    dropping the hidden `base` preset's cache variable is still safe. Both preset trees produce
    `compile_commands.json` without it.
  - The tests/AGENTS.md trap note had moved from ~L127-132 to ~L141-146 (plan 75 added lines).
    I found it by its text.
  - The plan's replacement sentence would have followed "Build Release and run
    `--gtest_filter='LuaRunner*'` when touching `lua_table_to_vector`." I merged the two into one
    sentence instead of stacking two "Build Release" sentences. The "Phase 2 ... verified this way
    (TEST-05)" sentence after it stays.
- **Verification (real runs, Ninja + MSVC 19.51 from the VS 18 bundle):**
  - `cmake --list-presets=all` lists exactly `dev` and `release` for configure, build and test.
  - `cmake --preset dev && cmake --build --preset dev && ctest --preset dev` gives
    `100% tests passed, 0 tests failed out of 1937` (462 s, serial).
  - `cmake --preset release && cmake --build --preset release &&
    build/release/bin/quiver_tests.exe --gtest_filter=LuaRunner*` gives `378 tests from 9 test
    suites ... PASSED 378`.
  - Both caches show `CMAKE_GENERATOR=Ninja`, the right `CMAKE_BUILD_TYPE`, and
    `QUIVER_BUILD_TESTS=ON` / `QUIVER_BUILD_C_API=ON`.
  - `scripts/format.bat` exited 0 with no content diff. Biome again rewrote 31 JS files from CRLF to
    LF only, and `git checkout -- bindings/js` restored them.
- **No CHANGELOG entry.** This is dev tooling, and the plan specifies none. For reference, `v0.12.8`
  is tagged and the manifests are at 0.12.9 with no `[0.12.9]` section yet.
- **Left alone:** `.gitignore`'s `build-release/` line. No doc recommends that tree anymore, but
  removing the line is outside this plan's scope and harmless to keep.
- **For later plans:** a Release test run can now use `build/release` through the preset, and both
  preset trees exist locally. Plan 86 edits only the root `CMakeLists.txt`, which no preset
  depends on.
