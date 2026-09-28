# 86 — Root `CMakeLists.txt`: a recursive format glob, and remove the no-op lines

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** build config, bindings/python/pyproject.toml, bindings/python/AGENTS.md
**Depends on** 79 (deletes the CMake `tidy` target and edits the "format and tidy targets" comment right above this glob — if 79 landed, the comment already reads "format target") · **Overlaps with** 79, 85

## Why

1. **The format glob looks like a whitelist.** Root `CMakeLists.txt` (~L92-107) lists 13 patterns:
   ```cmake
   # Source files for format and tidy targets
   file(GLOB_RECURSE ALL_SOURCE_FILES
       ${CMAKE_SOURCE_DIR}/include/quiver/*.h
       ${CMAKE_SOURCE_DIR}/include/quiver/c/*.h
       ${CMAKE_SOURCE_DIR}/src/*.cpp
       ...
       ${CMAKE_SOURCE_DIR}/tests/benchmark/*.cpp
   )
   ```
   `GLOB_RECURSE` already recurses from each root, so `src/*.cpp` matches `src/binary/*.cpp`,
   `src/c/*.cpp`, `src/csv/*.cpp` and `src/expression/*.cpp` alike. The per-subdirectory entries are
   redundant, and their presence suggests that unlisted directories (`src/csv/`, `src/expression/`)
   are excluded, which they are not. CI's clang-format check uses a recursive `find` over the same
   five roots.
2. **Two testing calls** (~L57-58): `include(CTest)` and `enable_testing()`. `include(CTest)`
   already calls `enable_testing()`, and it adds the dashboard targets and a `BUILD_TESTING` option,
   which duplicates `QUIVER_BUILD_TESTS`. Nothing uses either
   (`grep -rn "BUILD_TESTING" CMakeLists.txt cmake src tests` finds nothing). `gtest_discover_tests`
   and CI's plain `ctest` need only `enable_testing()`.
3. **Redundant pyproject arg**: `bindings/python/pyproject.toml` ~L14
   `cmake.args = ["-DQUIVER_BUILD_TESTS=OFF"]`. The root CMakeLists already detects `SKBUILD` and
   forces tests OFF and the C API ON (root AGENTS.md "Build System").
4. **`OUTPUT_NAME quiver`** in `src/CMakeLists.txt` (~L94) restates the target name `quiver`.

Principle: delete what has no effect. A no-op line reads as if it mattered.

## Constraints and decisions

- The glob must still cover everything CI checks, and nothing more. Five recursive roots:
  `include/*.h`, `src/*.cpp`, `src/*.h`, `tests/*.cpp`, `tests/*.h`.
- Keep `enable_testing()` and delete `include(CTest)` (policy verifier), after confirming nothing
  uses `BUILD_TESTING` or CTest dashboard targets.
- Keep `PREFIX "lib"` in `src/CMakeLists.txt`. Only `OUTPUT_NAME` goes.

## Changes

1. Root `CMakeLists.txt`, the glob:
   ```cmake
   # Source files for the format target (recursive, matching CI's clang-format check)
   file(GLOB_RECURSE ALL_SOURCE_FILES
       ${CMAKE_SOURCE_DIR}/include/*.h
       ${CMAKE_SOURCE_DIR}/src/*.cpp
       ${CMAKE_SOURCE_DIR}/src/*.h
       ${CMAKE_SOURCE_DIR}/tests/*.cpp
       ${CMAKE_SOURCE_DIR}/tests/*.h
   )
   ```
2. Root `CMakeLists.txt` ~L57: delete `include(CTest)` and keep `enable_testing()`.
3. `bindings/python/pyproject.toml` ~L14: delete `cmake.args = ["-DQUIVER_BUILD_TESTS=OFF"]`. Check
   that the `SKBUILD` block in the root CMakeLists really forces `QUIVER_BUILD_TESTS OFF`
   (`grep -n "SKBUILD" -A6 CMakeLists.txt`). If it does not, keep the pyproject line and skip this
   step.
4. `src/CMakeLists.txt` ~L94: delete the `OUTPUT_NAME quiver` line from `set_target_properties`. If
   that leaves `set_target_properties(quiver PROPERTIES PREFIX "lib")`, keep it.
5. `bindings/python/AGENTS.md` ~L102-104: "(`cmake.source-dir = ../..`, Release,
   `-DQUIVER_BUILD_TESTS=OFF`; the root CMakeLists detects `SKBUILD` and forces the C API ON)"
   becomes "(`cmake.source-dir = ../..`, Release; the root CMakeLists detects `SKBUILD` and forces
   the C API ON and tests OFF)".

## Tests

Rebuild and re-run formatting.

## Verification

1. `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON`
2. `cmake --build build --config Debug && ctest --test-dir build` (tests are still discovered)
3. `cmake --build build --target format`, then `git status` shows no changes. If the recursive
   glob now covers `src/csv` or `src/expression` files the old glob skipped, and clang-format
   reformats them, include that formatting diff in this change. CI already checks them.
4. Build the wheel once: `cd bindings/python && uv build --wheel`. Check that the wheel builds
   without tests (it should take no longer than before).
5. Check the built library is still named `libquiver.dll` / `libquiver.so`
   (`ls build/bin build/lib`).

## Acceptance criteria

- [ ] The glob has five recursive roots.
- [ ] One testing call (`enable_testing()`).
- [ ] No redundant pyproject `cmake.args`, and no `OUTPUT_NAME quiver`.
- [ ] Build, tests, format target and wheel are all fine. bindings/python/AGENTS.md is updated.

## Pitfalls

- `include/*.h` recursive covers `include/quiver/binary`, `include/quiver/expression` and
  `include/quiver/c/*`. That is intended.

## Out of scope

- The tidy target (plan 79). The presets (plan 85).
