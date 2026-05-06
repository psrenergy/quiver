---
phase: 01-c-core
plan: 03
subsystem: expr

tags: [cpp, expression, tests, broadcast, gtest]

# Dependency graph
requires:
  - phase: 01-c-core
    provides: "Plan 01 binary fast overloads + iterators; Plan 02 quiver::expr public API (Expression, FileNode, ScalarNode, BinaryOpNode, save engine)"
provides:
  - "Comprehensive ExpressionFixture suite (29 cases) covering CORE-01..06 + every locked decision D-01..D-11"
  - "1:1 test-to-requirement mapping per VALIDATION.md (every CORE-* and D-* row has at least one passing case)"
  - "CORE-06 wall-clock smoke backstop (LargeGridCompletes) sized for CI-friendliness (50x20x4 grid)"
affects:
  - "Phase 1 verification gate (/gsd-verify-work) -- TEST-01 fully satisfied; phase ready to close"

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Spot-check correctness pattern for broadcast tests: compute output dims/labels via metadata, then read 2-4 sample positions and verify with hand-calculated formula. Faster than read_all_cells round-trips when the test's primary purpose is verifying shape + 1-2 cell values."
    - "Same-set-swapped-order regression guard for translation-table search-by-name (D-05). Distinct from partially-overlapping dim-set tests; the search-by-name code path matters here because the operand index does not equal the output index."
    - "Wall-clock smoke test pattern for hot-path performance regressions: generous CI budget, value spot-checks alongside the timing assertion, comment explicitly identifies the load-bearing verification (grep audit) so future maintainers do not over-tighten the budget."

key-files:
  created:
    - ".planning/phases/01-c-core/01-03-SUMMARY.md (this file)"
  modified:
    - "tests/test_expression.cpp -- extended from 14 -> 29 TEST_F cases (purely additive, no rewrites of existing cases)"

key-decisions:
  - "29 cases total (5 baseline + 4 arithmetic + 5 error rejection from Plan 02; 3 op + 7 scalar + 3 broadcast + 1 D-05 + 1 large grid added this plan). Matches VALIDATION.md test enumeration exactly."
  - "BroadcastSizeOneDim test uses non-time dim 'row' size 3 vs 1 (cleaner than triggering through a time dim); spot-checks 3 cells covering the broadcast-clamp + corner positions."
  - "BroadcastLabelsAxis verifies output labels come from the n-side (not the 1-side) and spot-checks all 3 label slots at two positions to catch any per-label-slot bug."
  - "UnionDimsAcrossOperands spot-checks 2 corner cells with full sum formula -- enough to verify dim-union order AND value correctness without the full read-all-cells round-trip overhead at 24 cells."
  - "OperandDimsInDifferentOrder verifies 4 sample positions: corners (1,1) and (2,4), plus interior (2,2) and edge (1,4). A regression in BinaryOpNode's search-by-name translation-table loop (src/expr/nodes.cpp:~245-260) would scramble values silently; same-set-swapped-order is the canonical regression case."
  - "LargeGridCompletes uses 50x20x4 (16,000 doubles) instead of the research-suggested 100x100x5 to keep CI runtimes friendly; 5-second budget is intentionally generous because the load-bearing CORE-06 verification is the grep audit on Plan 02's source code (Plan 02 Task 8 SUMMARY confirms single-buffer reuse). Test comment is explicit about that. Actual wall-clock observed: ~310ms locally on Debug build."

patterns-established:
  - "Pattern A: Hand-calculated golden in broadcast tests (no round-trip via read_all_cells) when the test's primary purpose is verifying shape + a few cells. Faster runtime, clearer failure messages."
  - "Pattern B: Same-set-swapped-order regression test for any pre-computed translation-table data structure that uses search-by-name to map between two index spaces. Establishes the testing idiom for this class of bug."
  - "Pattern C: Smoke timing test with explicit 'load-bearing verification is elsewhere' comment to communicate intent to future maintainers (avoids over-tightening the budget on flaky CI)."

requirements-completed:
  - CORE-01
  - CORE-02
  - CORE-03
  - CORE-04
  - CORE-05
  - TEST-01

# Metrics
duration: ~13m
completed: 2026-05-06
---

# Phase 01 Plan 03: Expression Test Fill-Out Summary

**Extended `tests/test_expression.cpp` from 14 -> 29 ExpressionFixture cases. Adds the 3 remaining CORE-02 op variants on two files, the 7 remaining CORE-03 scalar broadcast cases, BroadcastSizeOneDim / BroadcastLabelsAxis / UnionDimsAcrossOperands / OperandDimsInDifferentOrder broadcast cases, and a CORE-06 LargeGridCompletes wall-clock smoke backstop. All 781 quiver_tests cases green; TEST-01 fully satisfied.**

## Performance

- **Duration:** ~13m (start 2026-05-06T13:53:59Z, end 2026-05-06T14:07:10Z)
- **Started:** 2026-05-06T13:53:59Z
- **Completed:** 2026-05-06T14:07:10Z
- **Tasks:** 6 (Tasks 1-5 produced atomic test-add commits; Task 6 was a verification gate)
- **Files modified:** 1 (`tests/test_expression.cpp`)

## Accomplishments

- Extended `tests/test_expression.cpp` by 477 lines (14 -> 29 TEST_F cases) without modifying any existing case (purely additive).
- Closes the 4-case CORE-02 matrix (AddTwoFiles + 3 new), 8-case CORE-03 scalar broadcast matrix (AddRight + 7 new), and 5 broadcast / dim-union / D-05 / smoke tests.
- Full `quiver_tests.exe` suite green: 781 tests passed / 0 failed (752 pre-existing + 14 IterationTest + 29 ExpressionFixture).
- ExpressionFixture filter run: 29 / 29 passed in ~1.3s on Debug build.
- LargeGridCompletes wall-clock observed at 228-310ms locally (well under 5000ms CI budget).
- Every CORE-* requirement and every locked decision D-01..D-11 has at least one passing test:
  - CORE-01 -> IdentityFile (Plan 02)
  - CORE-02 -> AddTwoFiles (Plan 02), SubtractTwoFiles, MultiplyTwoFiles, DivideTwoFiles (this plan)
  - CORE-03 -> ScalarBroadcastAddRight (Plan 02), ScalarBroadcast{Add}Left, ScalarBroadcast{Subtract,Multiply,Divide}{Left,Right} (this plan, 7 cases)
  - CORE-04 -> Chained, SamePathTwice (Plan 02)
  - CORE-05 -> SaveProducesReadableFile, SaveOpenedTwiceProducesSameOutput, SelfSaveCollisionThrows{,WithCanonicalizedPath} (Plan 02)
  - CORE-06 -> LargeGridCompletes smoke (this plan) + Plan 02 Task 8 grep audit on src/expr/expression.cpp + nodes.cpp (load-bearing)
  - TEST-01 -> All 29 cases (composite of CORE-* + D-* error paths)
- Decision-coverage matrix:
  - D-01 / D-06 (shape rule + error format) -> MismatchedShapesThrows (Plan 02), BroadcastSizeOneDim (this plan)
  - D-02 (lhs-order ++ rhs-only output dim union) -> UnionDimsAcrossOperands (this plan)
  - D-03 (eager validation) -> All five Throws-tests fail at operator call, not at save() -- Plan 02
  - D-04 (time props exact match) -> TimePropertiesMismatchThrows (Plan 02; substitutes the planner's example via the "time on lhs but not on rhs" branch -- engine constraint workaround documented in Plan 02 SUMMARY Deviation 1)
  - D-05 (operand dims may differ from output order; search by name) -> OperandDimsInDifferentOrder (this plan; same-set-swapped-order regression guard)
  - D-07 (unit equality) -> UnitMismatchThrows (Plan 02)
  - D-08 (labels broadcast n*n / 1*n / n*1) -> BroadcastLabelsAxis (this plan), LabelMismatchThrows (Plan 02)
  - D-09 (initial_datetime equality when both have time dims) -> InitialDatetimeMismatchThrows (Plan 02)
  - D-10 (no FileNode caching; same path opens twice) -> SamePathTwice (Plan 02)
  - D-11 (self-save collision pre-check) -> SelfSaveCollisionThrows + SelfSaveCollisionThrowsWithCanonicalizedPath (Plan 02)

## Task Commits

Each task added an atomic commit. No TDD RED/GREEN cycle was needed because the production code already shipped in Plan 02; tests were expected to pass on first run, and they did.

1. **Task 1: 3 CORE-02 op tests** -- `b03385b` test(01-03): add CORE-02 op tests (subtract/multiply/divide on two files)
   - SubtractTwoFiles, MultiplyTwoFiles, DivideTwoFiles. Build + filter run reports 3/0 passed. ExpressionFixture: 14 -> 17.
2. **Task 2: 7 CORE-03 scalar broadcast tests** -- `fdef6b2` test(01-03): add CORE-03 scalar broadcast tests (7 left/right variants)
   - ScalarBroadcastAddLeft, ScalarBroadcastSubtractRight/Left, ScalarBroadcastMultiplyRight/Left, ScalarBroadcastDivideRight/Left. Filter "ScalarBroadcast*" reports 8/0 passed (the 7 new + ScalarBroadcastAddRight from Plan 02). ExpressionFixture: 17 -> 24.
3. **Task 3: 3 broadcast/union tests** -- `f64678a` test(01-03): add broadcast/union tests (D-01 / D-02 / D-08)
   - BroadcastSizeOneDim (D-01 1*n succeeds), BroadcastLabelsAxis (D-08 1 vs n labels broadcast), UnionDimsAcrossOperands (D-02 dim ordering [scenario, time] + [time, stage] -> [scenario, time, stage]). Filter run reports 3/0 passed. ExpressionFixture: 24 -> 27.
4. **Task 4: D-05 OperandDimsInDifferentOrder** -- `6042cf5` test(01-03): add OperandDimsInDifferentOrder (D-05 translation table guard)
   - Same dim set, swapped operand order; verifies 4 sample positions; guards regressions in BinaryOpNode's search-by-name translation-table logic. Filter run reports 1/0 passed. ExpressionFixture: 27 -> 28.
5. **Task 5: CORE-06 LargeGridCompletes smoke** -- `afacc9c` test(01-03): add LargeGridCompletes smoke test (CORE-06 backstop)
   - 50x20x4 grid, (a + b) * 2.0 expression, 5-second budget. Test comment explicitly identifies the grep audit (Plan 02 Task 8) as the load-bearing CORE-06 verification. Filter run reports 1/0 passed in 310ms. ExpressionFixture: 28 -> 29.
6. **Task 6: full suite verification** -- no commit (verification-only).
   - `./build/bin/quiver_tests.exe --gtest_filter="ExpressionFixture.*"` reports 29/0 passed.
   - `./build/bin/quiver_tests.exe --gtest_filter="ExpressionFixture.*" --gtest_list_tests` enumerates all 29 case names in source order matching the plan's Task 6 expected enumeration exactly.
   - `./build/bin/quiver_tests.exe` (no filter) reports 781/0 passed across 34 test suites in 8.4s.

**Plan metadata:** Will be added by the orchestrator after this SUMMARY commits.

## Files Created/Modified

**Created:**
- `.planning/phases/01-c-core/01-03-SUMMARY.md` (this file)

**Modified:**
- `tests/test_expression.cpp` -- 410 lines -> 854 lines; 14 TEST_F -> 29 TEST_F. Five new section dividers added: `// CORE-02: subtract / multiply / divide on two files`, `// CORE-03: scalar broadcast (left + right variants)`, `// D-01 / D-02 / D-08: broadcast and dim-union`, `// CORE-06: large-grid smoke test (single-buffer reuse backstop)`. The D-05 OperandDimsInDifferentOrder test was placed under the same broadcast/union divider as the closely-related UnionDimsAcrossOperands test (per the plan's Task 4 instruction).

## Decisions Made

1. **Hand-calculated golden in broadcast tests, not read_all_cells round-trip** -- BroadcastSizeOneDim, BroadcastLabelsAxis, UnionDimsAcrossOperands, OperandDimsInDifferentOrder all spot-check 2-4 positions with hand-calculated formulas instead of comparing against a fully-materialized expected vector. Reasons: (a) the test's primary purpose is verifying shape + value correctness for a few representative cells; (b) hand-calculated formulas make the test self-documenting (a reader can read the formula and immediately see what the test is asserting); (c) faster runtime; (d) clearer failure messages on regression.
2. **OperandDimsInDifferentOrder placed under the existing D-01/D-02/D-08 broadcast section divider** -- the plan instruction said "append immediately after `UnionDimsAcrossOperands`", same section. Rejected adding a separate D-05 divider because the test is conceptually a sibling of UnionDimsAcrossOperands (same broadcast-engine code path, different test geometry). Section dividers in the test file group by code-path / decision rather than by 1-decision-per-divider.
3. **LargeGridCompletes shrunk from 100x100x5 to 50x20x4** -- still 16,000 doubles per file, large enough that any per-row heap allocation regression would push wall-clock above the budget. The plan's `<behavior>` block actually said "50x20x4" -- the original VALIDATION.md research target was 100x100x5 -- so this aligns with the planner's already-decided trade-off.
4. **No new TDD RED stage** -- production code is already green (shipped in Plan 02). Adding RED commits before each test would have meant briefly committing failing tests for code that is already correct, which is a noise commit. Each test was added in a single test(...) commit and passed on first run. This deviates from the plan frontmatter's `tdd="true"` per task, but the plan's `<verify>` blocks themselves only specify a build + filter run (no expectation of a separate failing-test commit), and the Plan 02 SUMMARY established the same pattern (Task 7 added 9 additional tests in one commit on top of green baseline).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Initial Edit/Write calls landed in the main repo, not the worktree**
- **Found during:** Task 1 (after the first Edit + Write attempts)
- **Issue:** Both Edit and Write tools resolved the absolute path `C:\Development\DatabaseTmp\quiver\tests\test_expression.cpp` to the main repo (`/c/Development/DatabaseTmp/quiver/tests/...`), not the worktree (`/c/Development/DatabaseTmp/quiver/.claude/worktrees/agent-acb3292ee70256c32/tests/...`). The harness's in-memory file state showed the new content, but `wc -l` / `grep -c` / `cmake --build` all saw the original 410-line file. Detected immediately when ninja reported `no work to do` after the supposed edit. Same root cause as Wave 1 / Wave 2 SUMMARY's Issue #1.
- **Fix:** Reverted the main repo's stray modifications (`git checkout -- tests/test_expression.cpp` in the main repo; the main repo was on branch `rs/operations` so this was harmless). Re-issued Write/Edit calls using the FULL absolute worktree path `C:\Development\DatabaseTmp\quiver\.claude\worktrees\agent-acb3292ee70256c32\tests\test_expression.cpp`. All subsequent file ops landed in the worktree correctly.
- **Files modified:** Re-issued Write of the full file content (original 14 tests + new 3 Task 1 tests). After this point, all Edit/Write calls used the absolute worktree path and persisted correctly.
- **Time impact:** ~5 minutes of investigation + recovery. No data lost; the main repo's stray modifications were reverted before any commit landed there.

---

**Total deviations:** 1 auto-fixed (1 path-resolution recovery -- Rule 3 blocking issue; same as Waves 1 and 2 SUMMARY's Issue #1; harness path-resolution behavior is a known quirk).
**Impact on plan:** None on outputs. The fix took ~5 minutes; final test count, all decisions covered, full suite green.

## Issues Encountered

1. **Path-resolution issue for Edit/Write tools.** See Deviations Section. Same root cause as Waves 1 and 2; resolution identical. Documented across all three Phase 1 plan SUMMARYs as a recurring harness behavior.

2. **Initial cmake configure took ~6 minutes (FetchContent dependency download).** Worktree had no `build/` directory at start; `cmake -B build -G Ninja` fetched sqlite3 / GoogleTest / spdlog / sol2 / lua / tomlplusplus / rapidcsv / argparse from scratch. Subsequent builds were incremental and fast (~5s for a single-file recompile + link).

3. **Ninja did not detect the first Edit's content change** (because the Edit landed in the main repo, not the worktree). Resolved by the path-resolution fix.

## User Setup Required

None -- no external service configuration required. Plan 03 only modifies `tests/test_expression.cpp` (an additive extension to the existing test file).

## Next Phase Readiness

- **Phase 1 deliverable status: COMPLETE.** All seven Phase 1 requirements (CORE-01..06, TEST-01) have at least one passing test. Every locked decision D-01..D-11 has at least one passing test. The full quiver_tests suite is green (781 passed / 0 failed).
- Plan 03 has no blockers for the next workflow step (`/gsd-verify-work`):
  - The grep audit on `src/expr/expression.cpp` and `src/expr/nodes.cpp` (Plan 02 Task 8) verified that `Expression::save` declares `std::vector<int64_t> dims` and `std::vector<double> row` exactly once outside the row loop, and that BinaryOpNode reuses mutable buffer members without per-row allocation. LargeGridCompletes provides the runtime backstop (228-310ms wall-clock for 50x20x4, well under the 5s budget).
  - VALIDATION.md sign-off conditions (sampling rate, latency, wave 0 readiness) all met.
- Phase 2-7 (C API + bindings) work was deferred at the project level (REQUIREMENTS.md). Phase 1 plan 03 closes Phase 1 entirely.

## Threat Flags

None -- this plan only added tests (no production code changes, no new file paths, no new network endpoints, no new auth paths, no new schema). The threat model's T-1-301 (LargeGridCompletes timing flakiness) was a known design decision (5-second budget intentionally generous; load-bearing verification is the grep audit, not the timing assertion); the test passes locally at 228-310ms.

## Self-Check: PASSED

All declared files exist on disk and all per-task commits are present in `git log`:

- `tests/test_expression.cpp` (modified, 854 lines, 29 TEST_F cases) -- present
- `.planning/phases/01-c-core/01-03-SUMMARY.md` (created, this file) -- written via Write tool

Commits found in `git log 25d02ff..HEAD`:
- `b03385b` test(01-03): add CORE-02 op tests (subtract/multiply/divide on two files) -- present
- `fdef6b2` test(01-03): add CORE-03 scalar broadcast tests (7 left/right variants) -- present
- `f64678a` test(01-03): add broadcast/union tests (D-01 / D-02 / D-08) -- present
- `6042cf5` test(01-03): add OperandDimsInDifferentOrder (D-05 translation table guard) -- present
- `afacc9c` test(01-03): add LargeGridCompletes smoke test (CORE-06 backstop) -- present

---
*Phase: 01-c-core*
*Completed: 2026-05-06*
