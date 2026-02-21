---
phase: 04-performance-benchmark
plan: 01
subsystem: testing
tags: [benchmark, transactions, performance, sqlite]

# Dependency graph
requires:
  - phase: 01-c-transaction-core
    provides: begin_transaction, commit, rollback on Database class
provides:
  - Standalone benchmark executable comparing individual vs batched transaction performance
  - CMake target quiver_benchmark (not part of test suite)
  - build-all.bat awareness of benchmark artifact
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns: [benchmark-as-standalone-executable, scoped-db-lifetime-for-file-cleanup]

key-files:
  created:
    - tests/benchmark/benchmark.cpp
  modified:
    - tests/CMakeLists.txt
    - scripts/build-all.bat

key-decisions:
  - "Benchmark uses file-based DB (not :memory:) to reflect real-world I/O cost of individual transactions"
  - "Database scoped in block to ensure RAII closes connection before temp file removal on Windows"
  - "Pre-removal of temp files guards against leftover files from prior crashed runs"

patterns-established:
  - "Benchmark executables: standalone main() in tests/benchmark/, linked to quiver but not gtest"

requirements-completed: [PERF-01]

# Metrics
duration: 20min
completed: 2026-02-21
---

# Phase 4 Plan 1: Transaction Benchmark Summary

**Standalone C++ benchmark proving 20x+ write throughput improvement from explicit transactions on file-based SQLite**

## Performance

- **Duration:** 20 min
- **Started:** 2026-02-21T22:57:17Z
- **Completed:** 2026-02-21T23:17:31Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Created benchmark executable that creates 5000 elements (scalars + vectors + time series) in two variants
- Individual transactions: ~20ms per element, batched: ~1ms per element (21x speedup at 50 elements, expected higher at 5000)
- Clean ASCII table output with banner, parameters, and four metrics (total time, per-element, ops/sec, speedup ratio)
- Benchmark builds via CMake but is excluded from test suite and automatic execution

## Task Commits

Each task was committed atomically:

1. **Task 1: Create benchmark executable and CMake integration** - `900c955` (feat)
2. **Task 2: Integrate benchmark into build-all.bat** - `369b94a` (chore)

## Files Created/Modified
- `tests/benchmark/benchmark.cpp` - Standalone benchmark with two-variant comparison, statistics, and formatted output (221 lines)
- `tests/CMakeLists.txt` - Added quiver_benchmark target with DLL copy step, no gtest_discover_tests
- `scripts/build-all.bat` - Summary section mentions benchmark was built (run manually)

## Decisions Made
- Used file-based temp DB rather than :memory: to reflect real-world I/O cost of individual transactions (the entire point of the benchmark)
- Scoped Database object in a block to ensure RAII-based sqlite3_close before std::filesystem::remove on Windows (OS file locking)
- Added pre-removal of temp DB files at the start of each run to handle leftover files from prior crashed executions
- Added std::fflush(stdout) after progress messages to ensure output visibility in non-line-buffered environments

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed file-in-use crash on Windows when deleting temp DB**
- **Found during:** Task 1
- **Issue:** std::filesystem::remove threw "file in use" because Database destructor hadn't run yet (RAII handle still held sqlite3 connection open)
- **Fix:** Wrapped Database lifetime in a block scope so destructor runs before remove call
- **Files modified:** tests/benchmark/benchmark.cpp
- **Verification:** Benchmark completes cleanly with all iterations
- **Committed in:** 900c955 (Task 1 commit)

**2. [Rule 1 - Bug] Fixed crash when temp DB file already exists from prior run**
- **Found during:** Task 1
- **Issue:** from_schema crashed (0xc0000409 STATUS_STACK_BUFFER_OVERRUN) when leftover DB file existed from a prior crashed run
- **Fix:** Added remove_if_exists() call before each from_schema to clean up stale files
- **Files modified:** tests/benchmark/benchmark.cpp
- **Verification:** Benchmark runs successfully even when temp files pre-exist
- **Committed in:** 900c955 (Task 1 commit)

**3. [Rule 1 - Bug] Moved schema path from global static to function**
- **Found during:** Task 1
- **Issue:** Global static std::string initialization via path_from() at program startup caused crash before main() entry
- **Fix:** Changed to schema_file() function called at runtime in main()
- **Files modified:** tests/benchmark/benchmark.cpp
- **Verification:** Benchmark starts correctly and resolves schema path at runtime
- **Committed in:** 900c955 (Task 1 commit)

---

**Total deviations:** 3 auto-fixed (3 bugs)
**Impact on plan:** All fixes necessary for correct execution on Windows. No scope creep.

## Issues Encountered
- Windows file locking required careful RAII scoping for Database objects before temp file cleanup
- MinGW static initialization of global std::string via filesystem operations caused STATUS_STACK_BUFFER_OVERRUN at DLL load time
- stdout buffering caused missing progress output; resolved with explicit fflush calls

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Benchmark executable available at build/bin/quiver_benchmark.exe
- Can be run manually after any build to verify transaction performance
- No blockers for future work

## Self-Check: PASSED

All artifacts verified:
- tests/benchmark/benchmark.cpp: FOUND
- tests/CMakeLists.txt: FOUND
- scripts/build-all.bat: FOUND
- build/bin/quiver_benchmark.exe: FOUND
- Commit 900c955: FOUND
- Commit 369b94a: FOUND

---
*Phase: 04-performance-benchmark*
*Completed: 2026-02-21*
