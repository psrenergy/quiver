---
phase: 02-config-path-locale-and-struct-size-safety
plan: 11
subsystem: bindings/julia, bindings/dart
tags: [ffi, clang.jl, ffigen, struct-layout, safety, julia, dart]

# Dependency graph
requires:
  - phase: 02-config-path-locale-and-struct-size-safety (plan 02-08)
    provides: quiver_csv_options_sizeof() native accessor, pinned by a static_assert
provides:
  - "Julia's four-struct load-time gate (options, scalar metadata, group metadata, csv options), with _CHECKED_STRUCTS wiring evidence, authored only in generator/prologue.jl"
  - "Dart's four-struct load-time gate, with checkedStructNames wiring evidence, quiver_csv_options_sizeof hand-added to bindings.dart (no ffigen regen)"
  - "bindings/julia/CLAUDE.md and bindings/dart/CLAUDE.md rewritten to describe the closed four-struct gate"
affects: [02-13]

# Actuals (#2632)
actuals:
  tokens: 6698
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "Wiring-evidence list (_CHECKED_STRUCTS / checkedStructNames) mirroring Python's 02-08 pattern, extended to Julia and Dart"
    - "A test observing wiring evidence must run before any test in the same suite that calls the gate function directly -- a direct call repopulates the record regardless of the real call site's own wiring, masking a mutation of that call site for every test that runs after it (found and fixed live in this plan's Dart mutation check)"

key-files:
  created: []
  modified:
    - bindings/julia/generator/prologue.jl
    - bindings/julia/src/c_api.jl
    - bindings/julia/test/test_struct_sizes.jl
    - bindings/dart/lib/src/ffi/bindings.dart
    - bindings/dart/lib/src/ffi/library_loader.dart
    - bindings/dart/test/struct_sizes_test.dart
    - bindings/julia/CLAUDE.md
    - bindings/dart/CLAUDE.md

key-decisions:
  - "Moved Dart's new wiring-evidence test to run FIRST in struct_sizes_test.dart (before any group), rather than appending it after the existing 'assertNativeStructSizes ordering' test as originally drafted -- that pre-existing test calls assertNativeStructSizes(bindings) DIRECTLY, which repopulates checkedStructNames regardless of the bindings getter's own call site. Discovered via the mutation check itself: the mutation (commenting out the getter's call site) did not turn the suite red until the wiring test was reordered to run before that direct call, and before any other access to `bindings` in the file."
  - "Julia's _CHECKED_STRUCTS is authored only in generator/prologue.jl (never src/c_api.jl), following the established rule that a hand-written safety check in c_api.jl is silently deleted by the next generator.bat run -- confirmed regeneration-proof by running the generator twice and diffing byte-for-byte."

patterns-established:
  - "The mutation-test ordering hazard: any wiring-evidence test sharing process state (a module-level list) with a test that calls the gate function directly must run before that direct-call test, or the direct call masks a deleted real call site."

requirements-completed: [OPT-06, SAFE-02, SAFE-03]

coverage:
  - id: D1
    description: "Julia's _assert_struct_sizes checks four structs (options, scalar metadata, group metadata, csv options) in that fixed order, recorded in _CHECKED_STRUCTS, authored only in generator/prologue.jl and proven regeneration-proof"
    requirement: SAFE-02
    verification:
      - kind: unit
        ref: "bindings/julia/test/test_struct_sizes.jl -- 'the gate ran and checked all four structs in order' testset"
        status: pass
      - kind: unit
        ref: "generator.bat run twice, git diff on bindings/julia/src/c_api.jl empty on the second run (byte-for-byte diff confirmed)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Deleting Julia's _assert_struct_sizes() call from __init__ (mutation) turns the ordered-record test red; restoring it turns the suite green again"
    requirement: SAFE-03
    verification:
      - kind: unit
        ref: "manual mutation: prologue.jl's __init__ call site commented out, regenerated, bindings/julia/test/test.bat -> 1 Fail (empty [] vs 4-name list); restored, regenerated, diff confirmed byte-identical to the pre-mutation state, test.bat -> 1490/1490 pass"
        status: pass
    human_judgment: false
  - id: D3
    description: "Dart's assertNativeStructSizes checks the same four structs in the same fixed order, quiver_csv_options_sizeof hand-added to bindings.dart (12 lines, not ffigen output), recorded in checkedStructNames"
    requirement: SAFE-02
    verification:
      - kind: unit
        ref: "bindings/dart/test/struct_sizes_test.dart -- 'checkedStructNames records the exact four-name ordered wiring evidence on first access' test"
        status: pass
      - kind: other
        ref: "git diff --stat bindings/dart/lib/src/ffi/bindings.dart -- 12 lines changed (under the 20-line acceptance threshold; a full ffigen regen would show hundreds)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Commenting out assertNativeStructSizes(_cachedBindings!) in Dart's bindings getter (mutation) turns the wiring-evidence test red; restoring it turns the suite green again"
    requirement: SAFE-03
    verification:
      - kind: unit
        ref: "manual mutation: bindings getter's call site commented out, dart test test/struct_sizes_test.dart -> 1 failure ([] vs 4-name list, after reordering the wiring test to run first); restored, dart test test/struct_sizes_test.dart -> 10/10 pass; full bindings/dart/test/test.bat -> 443/443 pass"
        status: pass
    human_judgment: false
---

# Phase 2 Plan 11: Julia and Dart Fourth-Struct Load-Time Gate Summary

**Extended the four-struct load-time safety gate (options, scalar metadata, group metadata, csv options) to Julia and Dart, the two bindings 02-08 (Python) and 02-10 (JS) had not yet reached, and fixed a live mutation-masking bug discovered while proving Dart's gate cannot be silently unwired.**

## Performance

- **Duration:** ~65 min (including two full Julia suite runs, two full Dart suite runs with native rebuilds, and mutation checks in both directions for both bindings)
- **Tasks:** 3 (all `type="auto"`)
- **Files modified:** 8

## Accomplishments

- **Julia**: `generator/prologue.jl` gained `_CHECKED_STRUCTS` (a module-level `String[]`, cleared
  on entry to `_assert_struct_sizes` and appended after each struct's check passes) and a fourth
  `_check_struct_size` call for `quiver_csv_options_t`, placed last (options, scalar metadata,
  group metadata, csv options). Regenerated `bindings/julia/src/c_api.jl` via `generator.bat`,
  which picked up both the prologue change and the new `quiver_csv_options_sizeof` ccall wrapper.
  A second `generator.bat` run produced a byte-identical file (confirmed via `diff`, not just
  `git diff --stat`), proving the gate's `prologue.jl`-only authoring survives regeneration.
- **Julia tests**: `test_struct_sizes.jl`'s `@test true` testset (whose comment claimed module
  load alone proved the gate ran) was deleted and replaced with an assertion on the exact
  four-element `Quiver.C._CHECKED_STRUCTS` ordered list. Added the happy-path pair for
  `quiver_csv_options_sizeof` (== `sizeof` and == 56) and the failure-path adjacency pair for
  `quiver_csv_options_t` at 55 and 57 bytes.
- **Dart**: `bindings.dart` gained a hand-added `quiver_csv_options_sizeof` wrapper (12 lines,
  copying the `quiver_database_options_sizeof` block's shape) -- no ffigen regeneration, per the
  binding's standing rule (a full regen flips three enums and breaks Hub).
  `library_loader.dart`'s `assertNativeStructSizes` gained a fourth `checkStructSize` call for
  `quiver_csv_options_t`, and a new `_checkedStructs`/`checkedStructNames` wiring-evidence pair
  mirroring Julia's `_CHECKED_STRUCTS`.
- **Dart tests**: added the happy-path and adjacency-failure assertions for the fourth accessor,
  plus a wiring-evidence test asserting the exact four-name ordered `checkedStructNames` list.
  This test had to be moved to run **first** in the file (see Decisions Made) after the mutation
  check revealed the pre-existing `assertNativeStructSizes ordering` test's direct call to the
  gate function was masking the mutation for any test running after it.
- **CLAUDE.md**: both `bindings/julia/CLAUDE.md` and `bindings/dart/CLAUDE.md` rewritten to
  describe the four-struct gate (was three), document `_CHECKED_STRUCTS`/`checkedStructNames`,
  and restate the authoring constraints (`prologue.jl`-only for Julia, hand-edit-only for Dart).

## Task Commits

Each task was committed atomically:

1. **Task 1: Julia — regenerate for the fourth accessor, gate it in the prologue, delete the `@test true`** — `7d01109` (feat)
2. **Task 2: Dart — hand-edit bindings.dart for the fourth accessor and make the gate observable** — `cd93d1b` (feat)
3. **Task 3: Record the four-struct gate in the Julia and Dart CLAUDE.md files** — `00251a1` (docs)

_No separate plan-metadata commit was made for this file; per `<sequential_execution>` this
SUMMARY is committed as part of the standard non-worktree flow below._

## Files Created/Modified

- `bindings/julia/generator/prologue.jl` — `_CHECKED_STRUCTS`, fourth `_check_struct_size` call.
- `bindings/julia/src/c_api.jl` — regenerated (prologue content + `quiver_csv_options_sizeof` wrapper).
- `bindings/julia/test/test_struct_sizes.jl` — `@test true` deleted, ordered-record assertion added, fourth accessor's happy/failure pairs added.
- `bindings/dart/lib/src/ffi/bindings.dart` — `quiver_csv_options_sizeof` hand-added (12 lines).
- `bindings/dart/lib/src/ffi/library_loader.dart` — fourth `checkStructSize` call, `_checkedStructs`/`checkedStructNames`.
- `bindings/dart/test/struct_sizes_test.dart` — wiring-evidence test (reordered to run first), fourth accessor's happy/failure pairs.
- `bindings/julia/CLAUDE.md` / `bindings/dart/CLAUDE.md` — four-struct gate documentation.

## Decisions Made

- **Reordered Dart's wiring-evidence test to run first in the file**, rather than placing it
  after the pre-existing `assertNativeStructSizes ordering` test as first drafted. That
  pre-existing test calls `assertNativeStructSizes(bindings)` **directly** (to prove the ordering
  claim doesn't throw against the live library), which repopulates `checkedStructNames`
  unconditionally -- regardless of whether the `bindings` getter's own call site still calls the
  gate. Running the mutation check (comment out the getter's call site, run the suite) surfaced
  this immediately: the suite stayed green even with the getter's call site deleted, because the
  direct-call test ran first and repopulated the record before the new wiring test observed it.
  Moving the wiring test to be the very first test in the file (before any other access to
  `bindings` anywhere in the suite) fixed this -- confirmed by re-running the mutation, which then
  correctly failed with `Actual: []`.
- **Julia's `_CHECKED_STRUCTS` is authored only in `generator/prologue.jl`**, matching the
  established rule that `c_api.jl`'s `__init__` is verbatim prologue content and a hand edit made
  directly to `c_api.jl` is silently deleted by the next `generator.bat` run. Verified by running
  the generator twice and diffing the two `c_api.jl` outputs byte-for-byte (not just against
  HEAD), confirming true idempotency rather than merely "same diff shape."
- **Dart's `quiver_csv_options_sizeof` was hand-added, not ffigen-regenerated**, per the binding's
  standing constraint (`bindings/dart/CLAUDE.md`): a full regen flips `quiver_data_type_t` /
  `quiver_error_t` / `quiver_log_level_t` from int constants into real Dart enums and breaks Hub.
  The diff to `bindings.dart` is 12 lines, well under the 20-line acceptance threshold that
  distinguishes a hand edit from an accidental regeneration.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed a self-inflicted test-ordering bug that masked the Dart mutation criterion**
- **Found during:** Task 2's mutation check (executing the plan's own `<verify><mutation>` step)
- **Issue:** The wiring-evidence test, as first drafted, was placed inside the
  `assertNativeStructSizes ordering` group, after a pre-existing test that calls
  `assertNativeStructSizes(bindings)` directly. That direct call repopulates the shared
  `_checkedStructs` list unconditionally, so commenting out the real call site in the `bindings`
  getter (the plan's specified mutation) did not turn the suite red -- the direct-call test ran
  first within that group and silently restored the correct record before the wiring test checked
  it. This is the exact class of vacuous-pass defect the plan's mutation criterion exists to rule
  out, introduced by this plan's own first draft rather than pre-existing in the codebase.
- **Fix:** Moved the wiring-evidence test out of the `assertNativeStructSizes ordering` group to
  be the first statement in `main()`, before any group and before any other test touches
  `bindings`. Re-ran the mutation (comment out the getter's call site) and confirmed the suite now
  correctly fails (`Actual: []` vs the expected four-name list); restored and confirmed green.
- **Files modified:** `bindings/dart/test/struct_sizes_test.dart`
- **Verification:** `dart test test/struct_sizes_test.dart` — mutated: 9 pass, 1 fail (the
  reordered wiring test); restored: 10/10 pass. Full suite `bindings/dart/test/test.bat`: 443/443
  pass after restoration.
- **Committed in:** `cd93d1b` (Task 2 commit — found and fixed before any commit was made, so no
  separate corrective commit was needed)

---

**Total deviations:** 1 auto-fixed (1 Rule 1 bug — a test-ordering defect introduced and caught
within this plan's own mutation-check step, before any commit)
**Impact on plan:** Necessary for the mutation criterion (an explicit acceptance criterion) to
actually hold. No scope creep — fixed within the same file the plan already listed.

## Mutation Checks (recorded per plan's `<output>` instruction)

**Julia** (Task 1's `<verify><mutation>`):
1. Commented out `_assert_struct_sizes()` in `generator/prologue.jl`'s `__init__`, regenerated
   `c_api.jl` (confirmed the mutation propagated into the generated file), ran
   `bindings/julia/test/test.bat` — **result: 1455 passed, 1 failed** at
   `test_struct_sizes.jl:23`, `Evaluated: String[] == ["quiver_database_options_t", ...]` — the
   exact empty-vs-four-name mismatch the criterion requires.
2. Restored `_assert_struct_sizes()`, regenerated, diffed the restored `c_api.jl` byte-for-byte
   against the pre-mutation post-plan version — **identical**. Ran `bindings/julia/test/test.bat`
   again — **result: 1490/1490 passed**, `Struct Sizes | 35 pass`.

**Dart** (Task 2's `<verify><mutation>`):
1. Commented out `assertNativeStructSizes(_cachedBindings!);` in `library_loader.dart`'s
   `bindings` getter. First run of `dart test test/struct_sizes_test.dart` incorrectly passed
   (see Deviations above) due to test ordering; after reordering the wiring test to run first,
   re-ran — **result: 9 passed, 1 failed** (`checkedStructNames records the exact four-name
   ordered wiring evidence on first access`, `Actual: []`).
2. Restored the getter's call site (confirmed via `git diff --stat` showing the expected 28-line
   diff, matching the pre-mutation state). Ran `dart test test/struct_sizes_test.dart` —
   **10/10 passed**. Ran the full suite `bindings/dart/test/test.bat` (with its standard
   `.dart_tool/` cache clear) — **443/443 passed**.

Both mutations were applied and reverted against the working tree only; neither mutated state was
committed.

## Issues Encountered

None beyond the self-caught test-ordering defect documented above.

## User Setup Required

None — no external service configuration required.

## Verification

- `cmake --build build --config Debug` — "no work to do" (no native changes in this plan; the
  `quiver_csv_options_sizeof` native accessor was already built by plan 02-08).
- `bindings/julia/generator/generator.bat` run twice — second run's `c_api.jl` byte-identical to
  the first (confirmed via `diff`, not just `git diff --stat`).
- `bindings/julia/test/test.bat` (full suite) — **1490/1490 passed** (baseline 1478 from 02-09;
  +12 in `Struct Sizes`, now 35/35).
- `bindings/dart/test/test.bat` (full suite, with `.dart_tool/` cache cleared per the binding's
  standard flow) — **443/443 passed** (baseline 434 from 02-09).
- `git diff --stat bindings/dart/lib/src/ffi/bindings.dart` — 12 lines changed (under the 20-line
  acceptance threshold).
- `grep -c '@test true' bindings/julia/test/test_struct_sizes.jl` — 0 (only comment-text mentions
  of the phrase remain, documenting why the old test was wrong).
- `grep -n '_assert_struct_sizes' bindings/julia/test/test_struct_sizes.jl` /
  `grep -n 'assertNativeStructSizes' bindings/dart/test/struct_sizes_test.dart` — no direct call
  from the new wiring-evidence tests (only from the pre-existing `assertNativeStructSizes
  ordering` test, which exists to prove that specific claim and is unaffected).
- `bindings/dart/test/test.bat` confirmed still CRLF (`file` reports "DOS batch file ... with CRLF
  line terminators"); the file itself was not touched by this plan.
- `git diff --name-only` across all three task commits confines changes to exactly the eight
  files this plan's frontmatter lists.

## Next Phase Readiness

- All four bindings (Python via 02-08, JS via 02-10, Julia and Dart via this plan) now gate the
  same four structs in the same fixed order, each with its own wiring-evidence mechanism proven
  by an executed mutation check in both directions.
- Gap 6 (`quiver_csv_options_sizeof` missing from the Julia/Dart safety gates) and Gap 2 (a
  binding's struct-size test staying green while its gate is deleted) from
  `02-VERIFICATION.md` are now closed for all four bindings.
- The test-ordering hazard found and fixed here (a wiring-evidence test must run before any test
  in the same suite that calls the gate function directly) is now a documented pattern
  (`patterns-established` above) worth checking if a fifth struct is ever added to any binding's
  gate.

---
*Phase: 02-config-path-locale-and-struct-size-safety*
*Completed: 2026-09-19*

## Self-Check: PASSED

- All 9 files listed above confirmed present on disk.
- Commits `7d01109`, `cd93d1b`, `00251a1` confirmed present in `git log --oneline --all`.
