---
phase: 02-loader-rewrite
plan: 01
subsystem: bindings
tags: [python, cffi, dll-loading, rpath, wheel-packaging]

# Dependency graph
requires:
  - phase: 01-build-system-migration
    provides: scikit-build-core integration with SKBUILD install targets for _libs/
provides:
  - Bundled-first library discovery in _loader.py with dev-mode fallback
  - Module-level _load_source attribute for load strategy introspection
  - Linux RPATH $ORIGIN on libquiver_c for SKBUILD wheel builds
affects: [02-loader-rewrite, 03-ci, 04-publish]

# Tech tracking
tech-stack:
  added: []
  patterns: [bundled-first-discovery, fail-fast-on-corrupt-bundled, dll-directory-handle-lifetime]

key-files:
  created: []
  modified:
    - bindings/python/src/quiverdb/_loader.py
    - CMakeLists.txt

key-decisions:
  - "PATH-only for dev mode fallback (no walk-up directory search)"
  - "Check specific library file existence inside _libs/, not just directory existence"
  - "Store os.add_dll_directory handle at module level to prevent garbage collection"

patterns-established:
  - "Bundled-first discovery: check _libs/_LIB_C_API.exists() then ffi.dlopen(absolute_path)"
  - "Fail-fast on corrupt bundled: OSError in bundled mode raises RuntimeError immediately, no dev fallback"
  - "Dev-mode bare names: Windows uses extension-free names, Linux uses .so extension for ffi.dlopen"

requirements-completed: [LOAD-01, LOAD-02, LOAD-03]

# Metrics
duration: 2min
completed: 2026-02-25
---

# Phase 2 Plan 1: Loader Rewrite Summary

**Bundled-first native library discovery with two-step fallback chain, os.add_dll_directory on Windows, and RPATH $ORIGIN for Linux wheel builds**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-25T21:29:19Z
- **Completed:** 2026-02-25T21:31:21Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Rewrote _loader.py with bundled _libs/ discovery (primary) and PATH-based dev-mode fallback
- Added os.add_dll_directory() call on Windows with module-level handle retention
- Added INSTALL_RPATH "$ORIGIN" for libquiver_c in SKBUILD builds (Linux)
- Exposed _load_source module attribute and diagnostic __main__ block
- Stripped all macOS/darwin code paths
- All 203 existing Python tests pass (verified twice during execution)

## Task Commits

Each task was committed atomically:

1. **Task 1: Rewrite _loader.py with bundled-first discovery and dev-mode fallback** - `9d3e5bf` (feat)
2. **Task 2: Add Linux RPATH $ORIGIN for libquiver_c in SKBUILD builds** - `79a407e` (feat)

## Files Created/Modified
- `bindings/python/src/quiverdb/_loader.py` - Rewritten with bundled-first discovery, dev fallback, _load_source attribute, diagnostic __main__
- `CMakeLists.txt` - Added INSTALL_RPATH "$ORIGIN" in SKBUILD block for Linux

## Decisions Made
- PATH-only for dev-mode fallback (no walk-up directory search) -- user confirmed dev workflow convenience not critical
- Check specific library file existence inside _libs/ (not just directory existence) -- prevents false-positive bundled detection on empty dirs
- Store os.add_dll_directory handle at module level to prevent garbage collection per Pitfall 5 from research

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Loader rewrite complete, ready for Plan 02 (local wheel validation)
- auditwheel behavior with pre-bundled CFFI libraries remains a concern for Phase 3 (CI)

## Self-Check: PASSED

All files verified present. All commit hashes verified in git log.

---
*Phase: 02-loader-rewrite*
*Completed: 2026-02-25*
