---
phase: 01-quiver-lua-cli-executable
plan: 01
subsystem: cli
tags: [argparse, lua, cli, spdlog, cmake]

# Dependency graph
requires: []
provides:
  - quiver_lua CLI executable (build/bin/quiver_lua.exe)
  - argparse v3.2 FetchContent dependency
  - spdlog stderr sink (stdout clean for script output)
affects: []

# Tech tracking
tech-stack:
  added: [argparse v3.2]
  patterns: [CLI thin-layer pass-through, spdlog stderr for CLI tools]

key-files:
  created: [src/cli/main.cpp]
  modified: [cmake/Dependencies.cmake, src/CMakeLists.txt, src/database.cpp, CLAUDE.md]

key-decisions:
  - "spdlog stderr_color_sink_mt lives in stdout_color_sinks.h (no separate header rename needed)"
  - "Stub main.cpp needed for CMake configure before full implementation"

patterns-established:
  - "CLI executables link quiver + quiver_compiler_options + argparse, unconditionally built"
  - "spdlog uses stderr for console output, keeping stdout clean for application output"

requirements-completed: [CLI-01, CLI-02, CLI-03, DB-01, DB-02, DB-03, DB-04, DB-05, SCRIPT-01, SCRIPT-02, SCRIPT-03, ERR-01, ERR-02, ERR-03]

# Metrics
duration: 5min
completed: 2026-02-27
---

# Phase 1 Plan 1: quiver_lua CLI Executable Summary

**Standalone quiver_lua CLI wrapping Database + LuaRunner with argparse, three database modes, UTF-8 BOM stripping, and proper exit codes**

## Performance

- **Duration:** 5 min
- **Started:** 2026-02-27T19:39:46Z
- **Completed:** 2026-02-27T19:45:15Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Built quiver_lua.exe: standalone CLI that opens a Quiver database and runs Lua scripts against it
- Three database construction modes: open existing, --schema (from schema file), --migrations (from migrations directory)
- All 14 requirements verified: --help, --version, --read-only, --log-level, exit codes, error messages, BOM stripping
- Existing test suite (532 C++ tests, 325 C API tests) passes with spdlog stderr change

## Task Commits

Each task was committed atomically:

1. **Task 1: Add argparse dependency, create quiver_lua CMake target, fix spdlog sink** - `7d17565` (feat)
2. **Task 2: Implement src/cli/main.cpp -- full CLI entry point** - `34e3a18` (feat)

**CLAUDE.md update:** `4dd9451` (chore: self-updating documentation)

## Files Created/Modified
- `src/cli/main.cpp` - Complete CLI entry point: argument parsing, database construction, Lua script execution (~120 lines)
- `cmake/Dependencies.cmake` - Added argparse v3.2 FetchContent block
- `src/CMakeLists.txt` - Added unconditional quiver_lua executable target
- `src/database.cpp` - Changed spdlog console sink from stdout to stderr
- `CLAUDE.md` - Added CLI executable to architecture and build sections

## Decisions Made
- spdlog's `stderr_color_sink_mt` is declared in `stdout_color_sinks.h` (not a separate header) -- kept the original include, changed only the sink type
- Created a stub `main.cpp` during Task 1 so CMake configure succeeds before the full implementation in Task 2

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] spdlog stderr header does not exist separately**
- **Found during:** Task 1 (spdlog sink change)
- **Issue:** Plan specified changing include from `stdout_color_sinks.h` to `stderr_color_sinks.h`, but spdlog v1.17.0 defines both stdout and stderr sinks in `stdout_color_sinks.h`
- **Fix:** Kept original include, changed only the sink construction from `stdout_color_sink_mt` to `stderr_color_sink_mt`
- **Files modified:** src/database.cpp
- **Verification:** Library builds and all 857 tests pass
- **Committed in:** 7d17565 (Task 1 commit)

**2. [Rule 3 - Blocking] CMake configure fails without source file**
- **Found during:** Task 1 (CMake target addition)
- **Issue:** Adding `add_executable(quiver_lua cli/main.cpp)` to CMakeLists.txt fails cmake configure when the file doesn't exist yet
- **Fix:** Created a stub main.cpp during Task 1, replaced with full implementation in Task 2
- **Files modified:** src/cli/main.cpp
- **Verification:** cmake configure and build succeed
- **Committed in:** 7d17565 (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (2 blocking)
**Impact on plan:** Both auto-fixes necessary for build system correctness. No scope creep.

## Issues Encountered
None beyond the auto-fixed deviations above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- quiver_lua.exe is fully functional and built unconditionally by the standard cmake workflow
- All 14 requirements verified through manual CLI testing
- Ready for any future CLI enhancements (REPL mode, additional flags)

## Self-Check: PASSED

All 5 files verified present. All 3 commits verified in git log.

---
*Phase: 01-quiver-lua-cli-executable*
*Completed: 2026-02-27*
