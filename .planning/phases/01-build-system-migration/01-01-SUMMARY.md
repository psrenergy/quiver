---
phase: 01-build-system-migration
plan: 01
subsystem: infra
tags: [scikit-build-core, cmake, pyproject, wheel, python-packaging]

# Dependency graph
requires: []
provides:
  - scikit-build-core build backend configuration in pyproject.toml
  - SKBUILD-guarded CMake install targets for native library bundling
  - Wheel-compatible cmake.source-dir pointing to repo root
affects: [01-02, 02-loader-refactoring]

# Tech tracking
tech-stack:
  added: [scikit-build-core]
  patterns: [SKBUILD guard pattern for dual-purpose CMakeLists.txt]

key-files:
  created: []
  modified:
    - bindings/python/pyproject.toml
    - bindings/python/.gitignore
    - CMakeLists.txt
    - src/CMakeLists.txt

key-decisions:
  - "Guard package config/export install commands under NOT DEFINED SKBUILD to prevent INSTALL(EXPORT) error"

patterns-established:
  - "SKBUILD guard: if(DEFINED SKBUILD) for wheel-only paths, if(NOT DEFINED SKBUILD) for system install paths"
  - "scikit-build-core cmake.source-dir = '../..' pointing from bindings/python to repo root"

requirements-completed: [BUILD-01, BUILD-02]

# Metrics
duration: 22min
completed: 2026-02-25
---

# Phase 01 Plan 01: Build System Migration Summary

**scikit-build-core replaces hatchling as build backend with SKBUILD-guarded CMake install targets placing native libraries into quiverdb/_libs**

## Performance

- **Duration:** 22 min
- **Started:** 2026-02-25T14:57:27Z
- **Completed:** 2026-02-25T15:19:30Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Replaced hatchling with scikit-build-core as Python build backend
- Added SKBUILD guard to root CMakeLists.txt that forces C API on and disables tests during wheel builds
- Added wheel install targets placing both quiver and quiver_c DLLs into quiverdb/_libs
- Guarded all existing system install targets to prevent conflicts during SKBUILD builds
- Verified normal CMake Debug build is completely unaffected (121 targets build successfully)
- Verified all 203 Python binding tests pass with the new build backend

## Task Commits

Each task was committed atomically:

1. **Task 1: Replace hatchling with scikit-build-core in pyproject.toml and update .gitignore** - `68bf3f8` (chore)
2. **Task 2: Add SKBUILD guard to root CMakeLists.txt and guard existing installs in src/CMakeLists.txt** - `2ab5854` (feat)

## Files Created/Modified
- `bindings/python/pyproject.toml` - scikit-build-core build backend with cmake.source-dir, build-type, and wheel.packages
- `bindings/python/.gitignore` - Added _skbuild/ pattern for scikit-build-core working directory
- `CMakeLists.txt` - SKBUILD guard (force options + install targets) and guarded package config exports
- `src/CMakeLists.txt` - Wrapped existing quiver and quiver_c install targets with NOT DEFINED SKBUILD

## Decisions Made
- Guarded package config/export install commands (write_basic_package_version_file, configure_package_config_file, install EXPORT) under NOT DEFINED SKBUILD in root CMakeLists.txt. Without this guard, CMake fails during SKBUILD builds because the export target "quiverTargets" is never defined (the install(TARGETS ... EXPORT quiverTargets) in src/CMakeLists.txt is also guarded). This was discovered during verification when the Python test suite triggered a full scikit-build-core build.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Guarded package config install commands under NOT DEFINED SKBUILD**
- **Found during:** Task 2 (SKBUILD guard implementation)
- **Issue:** Root CMakeLists.txt has `install(EXPORT quiverTargets ...)` which requires the export to be defined by `install(TARGETS ... EXPORT quiverTargets)` in src/CMakeLists.txt. Since that was guarded with `NOT DEFINED SKBUILD`, the export was never created during SKBUILD builds, causing `CMake Error: INSTALL(EXPORT) given unknown export "quiverTargets"`.
- **Fix:** Wrapped the entire package config block (lines 48-74) in `if(NOT DEFINED SKBUILD)` -- these system install exports are not needed for wheel builds.
- **Files modified:** CMakeLists.txt
- **Verification:** Python test suite (203 tests) passes with clean venv using scikit-build-core build path.
- **Committed in:** 2ab5854 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Essential fix -- without it the SKBUILD build path was completely broken. No scope creep.

## Issues Encountered
None beyond the auto-fixed deviation above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Build backend swap is complete -- `pip wheel .` from bindings/python/ now invokes CMake and bundles native libraries
- Ready for Plan 02 (local wheel validation) to verify the wheel structure and runtime behavior

## Self-Check: PASSED

All files exist. All commits verified.

---
*Phase: 01-build-system-migration*
*Completed: 2026-02-25*
