---
phase: 02-config-path-locale-and-struct-size-safety
plan: 08
subsystem: database
tags: [c-api, ffi, cffi, struct-layout, safety, python]

# Dependency graph
requires:
  - phase: 02-config-path-locale-and-struct-size-safety (plans 02-01..02-07)
    provides: quiver_database_options_sizeof / quiver_scalar_metadata_sizeof / quiver_group_metadata_sizeof and Python's original load-time gate
provides:
  - "quiver_csv_options_sizeof() native accessor, pinned by a static_assert"
  - "Python's four-struct load-time gate, restored and made observable via _CHECKED_STRUCTS"
  - "Promoted gate rule recorded in src/c/CLAUDE.md and bindings/python/CLAUDE.md"
affects: [02-09, 02-10, 02-11, 02-12, 02-13]

# Actuals (#2632)
actuals:
  tokens: 4018
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "Wiring-evidence list (_CHECKED_STRUCTS) so a test can observe a gate ran without re-driving its pure helper"

key-files:
  created: []
  modified:
    - include/quiver/c/options.h
    - src/c/options.cpp
    - tests/test_c_api_database_options.cpp
    - bindings/python/src/quiverdb/_c_api.py
    - bindings/python/src/quiverdb/_loader.py
    - bindings/python/tests/test_struct_sizes.py
    - src/c/CLAUDE.md
    - bindings/python/CLAUDE.md

key-decisions:
  - "Confirmed (not merely suspected) that Python's struct-size gate was unwired in HEAD -- both call sites in _loader.py read `pass  # MUTATION: gate unwired`, corroborating 02-VERIFICATION.md's finding and correcting the record: this was a shipped defect, not just an undertested path."
  - "Promoted the gate rule from 'the three structs the gates check' to the general rule: every C struct a binding hand-allocates a raw buffer for gets a *_sizeof accessor, a static_assert, and an entry in all four load-time gates, in one fixed order -- recorded in src/c/CLAUDE.md and bindings/python/CLAUDE.md so a fifth struct (e.g. Phase 3's attribute-metadata struct) joins by default."

patterns-established:
  - "Wiring-evidence pattern: a module-level list recording what a gate actually checked (cleared on entry, appended after each passing check) lets a test assert the gate ran end-to-end without calling the gate itself from the test (which would recreate a vacuous-pass defect)."

requirements-completed: [SAFE-01, SAFE-02, SAFE-03]

coverage:
  - id: D1
    description: "quiver_csv_options_sizeof() exists, returns 56 on a 64-bit target, and is pinned by a static_assert"
    requirement: SAFE-01
    verification:
      - kind: unit
        ref: "tests/test_c_api_database_options.cpp#DatabaseCApiOptions.SizeofAccessorsMatchNativeLayout"
        status: pass
    human_judgment: false
  - id: D2
    description: "Python's load-time gate checks all four structs on both load paths (bundled + development), and its wiring is observable"
    requirement: SAFE-02
    verification:
      - kind: unit
        ref: "bindings/python/tests/test_struct_sizes.py#test_gate_ran_and_checked_all_four_structs_in_order"
        status: pass
      - kind: unit
        ref: "bindings/python/tests/test_struct_sizes.py#test_native_accessors_match_cdef_sizes"
        status: pass
    human_judgment: false
  - id: D3
    description: "Deleting the gate call from _loader.py turns the gate-observation test red (mutation criterion), and restoring it turns it green again"
    requirement: SAFE-03
    verification:
      - kind: unit
        ref: "manual mutation check: pass substituted for _assert_struct_sizes(ffi, lib) on the development load path -> pytest failure; restored -> pytest pass (see Mutation Check section below)"
        status: pass
    human_judgment: false

duration: 24min
completed: 2026-09-19
status: complete
---

# Phase 2 Plan 08: Fourth Struct-Size Accessor and Python Gate Restoration Summary

**Added `quiver_csv_options_sizeof()` (pinned by a `static_assert`) and restored Python's
load-time struct-size gate, which was found unwired in HEAD (`pass  # MUTATION: gate unwired`
at both call sites in `_loader.py`) despite a fully green test suite.**

## Performance

- **Duration:** ~24 min
- **Tasks:** 3 (1 tracer + 2 auto)
- **Files modified:** 8

## Accomplishments

- `quiver_csv_options_sizeof()` added to the C API (`include/quiver/c/options.h`,
  `src/c/options.cpp`), returning `sizeof(quiver_csv_options_t)` (56 bytes), pinned by
  `static_assert(sizeof(quiver_csv_options_t) == 56, ...)`.
- Python's `_loader.py` gate restored at both call sites (bundled and development load paths) —
  confirmed the shipped defect was real, not hypothetical: both sites literally read
  `pass  # MUTATION: gate unwired` in HEAD.
- `_STRUCT_SIZEOF_ACCESSORS` extended to four entries in the fixed order options → scalar
  metadata → group metadata → csv options.
- New `_CHECKED_STRUCTS` module-level list in `_loader.py` gives the gate observable wiring
  evidence: cleared on entry to `_assert_struct_sizes`, appended after each struct's check
  passes. A test can now assert the gate *ran*, not just that its pure helper works in isolation.
- `bindings/python/tests/test_struct_sizes.py` gained
  `test_gate_ran_and_checked_all_four_structs_in_order` (observes `_CHECKED_STRUCTS` after
  forcing a load via `get_lib()`, never calling `_assert_struct_sizes` itself) plus two new
  adjacency mismatch cases for `quiver_csv_options_t` (`56` vs `55` and vs `57`).
- `tests/test_c_api_database_options.cpp`'s `SizeofAccessorsMatchNativeLayout` extended with the
  fourth accessor pair, same fixed order, no new test file.
- `src/c/CLAUDE.md` and `bindings/python/CLAUDE.md` updated to state the promoted rule: every
  struct a binding hand-allocates a raw buffer for joins the accessor/assert/four-gate list by
  default, not a closed three-struct (now four-struct) enumeration.

## Task Commits

Each task was committed atomically:

1. **Task 1: One struct-size gate proven end to end** — `2a2ab2c` (feat)
2. **Task 2: The C API suite asserts four accessors, not three** — `2b66005` (test)
3. **Task 3: Record the promoted gate rule in the two nearest CLAUDE.md files** — `15287b9` (docs)

_No separate plan-metadata commit was made for this file; per `<sequential_execution>` this
SUMMARY is committed as part of the standard non-worktree flow below._

## Files Created/Modified

- `include/quiver/c/options.h` — `quiver_csv_options_sizeof()` declaration.
- `src/c/options.cpp` — `static_assert` + `quiver_csv_options_sizeof()` definition.
- `tests/test_c_api_database_options.cpp` — extended `SizeofAccessorsMatchNativeLayout`.
- `bindings/python/src/quiverdb/_c_api.py` — matching cdef declaration.
- `bindings/python/src/quiverdb/_loader.py` — gate restored, extended to 4 structs,
  `_CHECKED_STRUCTS` added.
- `bindings/python/tests/test_struct_sizes.py` — wiring-observation test + adjacency cases.
- `src/c/CLAUDE.md` / `bindings/python/CLAUDE.md` — promoted-rule documentation.

## Decisions Made

- **The verification record needed correcting, not just closing.** 02-VERIFICATION.md described
  Python's gate as "untested" (SC3 skeptic (a) found every binding's struct-size test passes
  vacuously when the gate is deleted). Reading `_loader.py` in full at the start of this plan
  confirmed something stronger: the gate wasn't merely untested against deletion, it was already
  deleted (`pass  # MUTATION: gate unwired`) and shipped that way through merge `e8d35b9`. This
  plan's mutation check (below) is against the *actual* shipped state, not a hypothetical one.
- **Promotion over enumeration** (assumption-delta decision, already resolved by the plan): the
  gate rule is now written as "every hand-allocated struct" rather than a fixed list, so 02-10/
  02-11 (JS, Dart, Julia) and Phase 3's new metadata struct extend the same rule instead of
  re-deciding it.

## Deviations from Plan

None — plan executed exactly as written. All acceptance criteria and the mutation criterion were
verified directly (see below), matching the plan's `<verify>`/`<acceptance_criteria>` blocks
task-by-task.

## Mutation Check (recorded per plan's `<output>` instruction)

Executed exactly as specified in Task 1's `<verify><mutation>` block, against the real
development-mode load path (no `_libs/` bundle present in this checkout, so the development path
is the one actually exercised):

1. **Before this plan (confirmed, not assumed):** `bindings/python/src/quiverdb/_loader.py` HEAD
   contained `pass  # MUTATION: gate unwired` at both call sites in `load_library` — this was
   read directly (`git show HEAD:...`) before any edit was made.
2. **Re-applied the defect** on the development load path only: replaced
   `_assert_struct_sizes(ffi, lib)` with `pass`.
   Ran `uv run pytest bindings/python/tests/test_struct_sizes.py -v` (with `build/bin` on PATH
   for DLL discovery, per `bindings/python/CLAUDE.md`'s dev-mode note) —
   **result: 1 failed, 7 passed.** The one failure was exactly
   `test_gate_ran_and_checked_all_four_structs_in_order`, with
   `AssertionError: assert [] == ['quiver_database_options_t', ...]` — an empty list where the
   ordered four-name list was expected. Every other test in the file still passed, including the
   ones that call `_check_struct_size` and the raw accessors directly — confirming those tests
   alone would NOT have caught the wiring defect, exactly as the plan predicted.
3. **Restored** `_assert_struct_sizes(ffi, lib)` at that call site. Re-ran the same command —
   **result: 8 passed.**

This closes Gap 2 from `02-VERIFICATION.md`: the struct-size safety mechanism can no longer be
silently unwired without a test noticing.

## Verification

- `cmake --build build --config Debug` — succeeded.
- `./build/bin/quiver_c_tests.exe --gtest_filter=DatabaseCApiOptions.*` — 4/4 passed.
- `./build/bin/quiver_c_tests.exe` (full suite) — **567/567 passed** (matches the Phase 2 baseline
  exactly; Task 2 only extended an existing `TEST`, added none).
- `./build/bin/quiver_tests.exe` (full C++ core suite) — **1301/1301 passed** (higher than the
  02-VERIFICATION.md-recorded baseline of 1175; this repo's C++ suite count has grown since that
  verification ran, unrelated to this plan's scope — not investigated further here).
- `bindings/python/tests/test.bat` (full Python suite via the correct PATH-prefixed runner) —
  **321/321 passed** (baseline 318 + 3 new: the wiring-observation test + 2 csv-options mismatch
  parametrize cases).
- `uv run ruff check` on the three touched Python files — all checks passed.
- `grep -rn 'MUTATION' bindings/ src/ include/ tests/ --include=*.py --include=*.ts --include=*.jl --include=*.dart --include=*.cpp --include=*.h` — **no matches** (adjusted two explanatory
  comments that had quoted the literal defect string, so the phase-level verification grep in
  this plan's `<verification>` block stays clean while still documenting the history).
- `clang-format --dry-run --Werror` on the three touched C++ files — one pre-existing violation
  found in `tests/test_c_api_database_options.cpp` at an unrelated line (confirmed present in
  HEAD before this plan's changes via `git stash`); out of this task's scope per the deviation
  rules' scope boundary, not fixed.

## Issues Encountered

None beyond the above pre-existing clang-format finding, which was investigated and confirmed
out of scope rather than left unexplained.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- SAFE-01 is no longer PARTIAL for the native + Python half: an accessor exists for all four
  hand-allocated structs, pinned by a `static_assert`, and Python's gate checks all four with
  wiring evidence a test can observe.
- The promoted "every hand-allocated struct" rule is written in both nearest CLAUDE.md files,
  ready for 02-10 (JS, including the missing-accessor diagnosis gap) and 02-11 (Dart, Julia) to
  extend the same four-struct list rather than re-deriving it.
- 02-VERIFICATION.md's Gap 2 (Python gate deletable without a test noticing) and half of Gap 6
  (missing `quiver_csv_options_sizeof`, native + Python side) are now closed. The JS/Dart/Julia
  sides of Gap 6, and the JS missing-accessor diagnosis from SC3, remain for 02-10/02-11.

---
*Phase: 02-config-path-locale-and-struct-size-safety*
*Completed: 2026-09-19*
