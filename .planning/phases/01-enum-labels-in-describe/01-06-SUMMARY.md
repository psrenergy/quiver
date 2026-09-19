---
phase: 01-enum-labels-in-describe
plan: 06
subsystem: testing
tags: [julia, dart, python, bun, changelog, ui-config]

# Dependency graph
requires:
  - phase: 01-enum-labels-in-describe (plans 01-05)
    provides: the UI sidecar renderer (src/ui_config.{h,cpp}, database_describe.cpp), the C API
      surface, the tests/schemas/ui/ fixture corpus and its README literal table, and the
      C++/C API/Lua exact-string proofs this plan mirrors into the four FFI bindings
provides:
  - Exact-string DESC-07 proof in Julia, Dart, Python, and JS (enum vocabulary, value
    histogram, header line, unit/hidden decoration, accented UTF-8 label, no-sidecar case)
  - The D-31 written call ("strengthen and keep") executed in all four binding suites
  - A reconciled CHANGELOG.md head (0.10.6, no longer "unreleased") with a fresh 0.10.7
    unreleased section for this phase
  - Three updated CLAUDE.md files (root, src/, tests/) documenting the UI sidecar feature
  - Both phase structural gates (no binding source changed, no ABI/C-symbol change) run and
    recorded, plus a full scripts/test-all.bat pass
affects: [phase-2-opt-04, release-ritual]

# Actuals (#2632) — chars/4 over the realized diff, not a harness token count.
actuals:
  tokens: 21000
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "Per-binding UI-fixture helper (ui_fixture_path/uiFixture) that joins onto the binding's
      existing tests_path()/schemas_path()-style helper, never copies a fixture"
    - "Per-test-case database filename inside the shared fixture directory (julia_/dart_/python_/js_
      prefix + case stem) to avoid concurrent-runner collisions (Dart's test package, potential
      parallel ctest)"

key-files:
  created:
    - bindings/python/tests/test_database_describe.py
  modified:
    - bindings/julia/test/test_database_describe.jl
    - bindings/dart/test/describe_test.dart
    - bindings/js/test/database-describe.test.ts
    - CHANGELOG.md
    - CLAUDE.md
    - src/CLAUDE.md
    - tests/CLAUDE.md

key-decisions:
  - "D-31 executed as written: strengthen and keep. All four binding suites keep their original
    isa String/isA<String>/isinstance/typeof smoke tests (DESC-05's binding-side sidecar-less
    guard) and gain six new exact-string cases each. The stale 'covered by the C++ core tests'
    comment is deleted from Julia/Dart/JS (Python's new file never carried it)."
  - "The bindings/ scope gate reads CONTEXT.md's own wording literally: 'no file under
    bindings/src', not 'no file under bindings/' — so new/strengthened test files are in scope
    and do not violate the patch-only constraint. Verified via
    git diff --name-only master...HEAD -- bindings/ | grep -v '/tests?/' == empty."
  - "Python's new fixtures build the database inside the fixture directory itself (schemas_path
    fixture + 'ui' + name), never the tmp_path-based collections_db fixture — a tmp_path database
    has no ui/ sibling and would pass by silently reading no sidecar."
  - "CHANGELOG.md's head section (previously '## [0.10.4] — unreleased', compare link naming the
    nonexistent v0.11.0) is relabeled '## [0.10.6]' with a note that 0.10.4/0.10.5/0.10.6 were all
    cut from the same unreleased block before this reconciliation. A fresh
    '## [0.10.7] — unreleased' section carries this phase's entry. No manifest was bumped;
    scripts/assert_version.py still reports 0.10.6 across all five."

requirements-completed: [DESC-07, CORPUS-03]

coverage:
  - id: D1
    description: "Julia asserts exact enum/header/unit/hidden/accented/no-sidecar strings against
      tests/schemas/ui/ fixtures"
    requirement: DESC-07
    verification:
      - kind: unit
        ref: "bindings/julia/test/test_database_describe.jl (Describe testset, 11 tests)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Dart asserts the same six exact-string cases"
    requirement: DESC-07
    verification:
      - kind: unit
        ref: "bindings/dart/test/describe_test.dart"
        status: pass
    human_judgment: false
  - id: D3
    description: "Python asserts the same six exact-string cases in a new test file, without
      reusing the tmp_path-based fixtures"
    requirement: DESC-07
    verification:
      - kind: unit
        ref: "bindings/python/tests/test_database_describe.py::TestEnumRendering (6 tests)"
        status: pass
    human_judgment: false
  - id: D4
    description: "JS/Bun asserts the same six exact-string cases"
    requirement: DESC-07
    verification:
      - kind: unit
        ref: "bindings/js/test/database-describe.test.ts"
        status: pass
    human_judgment: false
  - id: D5
    description: "CHANGELOG.md head reconciled to the version CMakeLists.txt actually carries;
      no manifest bumped"
    verification:
      - kind: other
        ref: "uv run python scripts/assert_version.py (exit 0, reports 0.10.6 across all five manifests)"
        status: pass
    human_judgment: false
  - id: D6
    description: "Both phase structural gates and the full six-suite + CLI-smoke test run pass"
    verification:
      - kind: other
        ref: "scripts/test-all.bat (C++ 1160/1160, C API 563/563, Julia/Dart/JS/Python all green,
          CLI smoke PASS)"
        status: pass
    human_judgment: false

duration: 40min
completed: 2026-09-19
status: complete
---

# Phase 01 Plan 06: Binding boundary proofs, CHANGELOG reconciliation, and phase gates Summary

**Julia, Dart, Python, and JS each assert exact-string enum rendering (`values {0: 8 (Disabled), 1: 4 (Enabled)}`, `Seasonal Naïve`) against the shared `tests/schemas/ui/` fixtures; CHANGELOG.md reconciled to 0.10.6 with a fresh 0.10.7 entry; both phase structural gates plus the full six-suite `test-all.bat` pass.**

## Performance

- **Duration:** 40 min
- **Tasks:** 3
- **Files modified:** 8 (1 created, 7 modified)

## Accomplishments

- Closed DESC-07 at all four FFI boundaries: Julia, Dart, Python, and JS/Bun now assert the exact
  rendered strings for a declared-but-unused enum vocabulary, a twelve-element value histogram, the
  `UI config: ... (locale: en)` header line, `[MW]`/`[hidden]` decoration, the canonical accented
  label `Seasonal Naïve` (via `describeCollection("EconomicDriver")` on `foresight_like`), and the
  no-sidecar byte-identical case — never merely "returns a String".
- Executed the D-31 written call (strengthen and keep): all four suites' original type-only smoke
  tests survive untouched (they remain the only per-binding proof that `describe*` still works on a
  sidecar-less `:memory:` database — DESC-05's binding-side guard), and the now-false "covered by
  the C++ core tests" comment is gone from Julia/Dart/JS.
- Every new/strengthened test reaches `tests/schemas/ui/` through each binding's existing
  `tests_path()`/`schemas_path()`-style helper, and opens its database inside the fixture directory
  under a filename unique to that binding and test case (`julia_declared.sqlite`,
  `dart_histogram.sqlite`, `python_header.sqlite`, `js_unit_hidden.sqlite`, ...) — no fixture was
  copied (CORPUS-03), and no fixture was mutated (`git status --porcelain -- tests/schemas/ui/` is
  empty throughout).
- Reconciled `CHANGELOG.md`: the head section (`## [0.10.4] — unreleased`, with a compare link
  naming the nonexistent `v0.11.0`) is relabeled `## [0.10.6]` with the correct `v0.10.3...v0.10.6`
  link and a note that 0.10.4/0.10.5/0.10.6 were all cut from the same unreleased block before this
  reconciliation. A fresh `## [0.10.7] — unreleased` section carries this phase's entry. No manifest
  was bumped and no Bump Version workflow was dispatched — `scripts/assert_version.py` still reports
  `0.10.6` across all five manifests.
- Documented the UI sidecar feature in the three nearest CLAUDE.md files: root (`has_ui_config()`
  in the Core API list, a new Design Decisions bullet), `src/CLAUDE.md` (a new `## UI Sidecar
  Config` section covering `ui_config.{h,cpp}`'s type set, the `from_directory`/content-parse
  split, the locale fallback chain, the unknown-key debug line, the `format` precedence, and
  `require_ui_config()`'s swallow-on-malformed behavior), and `tests/CLAUDE.md` (the `ui/` and
  `ui_golden/` fixture corpora, `test_ui_fixture.h`, the per-test database-stem rule, the
  `.gitattributes` golden pin, and the five UI test files).
- Ran and recorded both phase structural gates: no file under `bindings/` outside a test directory
  changed, and no file under `include/quiver/c/`, `src/c/`, `include/quiver/options.h`, or
  `include/quiver/attribute_metadata.h` changed (both verified empty via `git diff --name-only`).
  `include/quiver/database.h` carries exactly the one `has_ui_config()` declaration added by an
  earlier plan in this phase.
- Ran the full `scripts/test-all.bat`: C++ 1160/1160, C API 563/563, Julia all green, Dart 422/422,
  JS 209/209, Python 308/308, and the CLI smoke test PASS.

## Task Commits

1. **Task 1: Julia and Dart exact-string enum rendering suites** - `40ba5a8` (test)
2. **Task 2: Python and JS exact-string enum rendering suites** - `09a4f80` (test)
3. **Task 3: CHANGELOG reconciliation, CLAUDE.md updates, and the two phase gates** - `ef18358` (docs)

## Files Created/Modified

- `bindings/julia/test/test_database_describe.jl` - Added `ui_fixture_path`/`open_ui_fixture`
  helpers and six exact-string cases; removed the stale comment
- `bindings/dart/test/describe_test.dart` - Added `uiFixture`/`openUiFixture` helpers and six
  exact-string cases; removed the stale comment
- `bindings/python/tests/test_database_describe.py` - New file: `ui_fixture` fixture built on the
  existing `schemas_path` fixture, six exact-string cases, deliberately not using `tmp_path`
- `bindings/js/test/database-describe.test.ts` - Added `uiFixture`/`openUiFixture` helpers and six
  exact-string cases; removed the stale comment
- `CHANGELOG.md` - Relabeled the head section `## [0.10.6]`, fixed its compare link, opened
  `## [0.10.7] — unreleased` with this phase's entry
- `CLAUDE.md` - Added `has_ui_config()` to the Core API list and a UI-sidecar Design Decisions bullet
- `src/CLAUDE.md` - Added `ui_config.h`/`ui_config.cpp` to the file map and a new `## UI Sidecar
  Config` section
- `tests/CLAUDE.md` - Documented the `ui/`/`ui_golden/` fixture corpora, `test_ui_fixture.h`, the
  per-test database-stem rule, the `.gitattributes` golden pin, and the five UI test files

## Decisions Made

- D-31 executed as written (strengthen and keep) — see `key-decisions` in frontmatter.
- The `bindings/` scope gate follows CONTEXT.md's literal wording ("no file under `bindings/src`"),
  so test-directory changes are correctly in scope for this plan.
- Python's fixtures are built inside the fixture directory, never via the `tmp_path`-based
  `collections_db` fixture, to avoid silently asserting the old unchanged output.
- CHANGELOG reconciliation relabels rather than rewrites history: the existing entries under the
  old `## [0.10.4] — unreleased` head are kept verbatim under `## [0.10.6]`, with only the label,
  compare link, and an explanatory note changed.

## Deviations from Plan

**1. [Rule 1 - Bug] Removed the literal string `tmp_path` from the new Python file's explanatory
comment**
- **Found during:** Task 2 acceptance-criteria verification
- **Issue:** The acceptance criterion is `grep -c 'tmp_path' bindings/python/tests/test_database_describe.py` must return 0. My first draft's comment explaining *why* the file avoids the `tmp_path`-based fixtures used the literal token `tmp_path` twice, which the strict grep check would fail even though no `tmp_path` fixture was actually used.
- **Fix:** Reworded the comment to describe the same rationale ("pytest temp-directory fixtures", "a bare temp directory") without using the literal identifier.
- **Files modified:** `bindings/python/tests/test_database_describe.py`
- **Verification:** `grep -c 'tmp_path' bindings/python/tests/test_database_describe.py` returns 0; re-ran the six-test file in isolation, all pass.
- **Committed in:** `09a4f80` (Task 2 commit — caught before the commit was made, so no separate fix commit was needed)

---

**Total deviations:** 1 auto-fixed (Rule 1 — a wording-only self-correction caught during acceptance-criteria verification, before the task's commit; no test behavior changed).
**Impact on plan:** None — the fix was cosmetic (comment wording only) and was resolved before the task commit landed.

## Issues Encountered

None. `bindings/python/tests/test.bat` and `bindings/js/test/test.bat` both required backgrounding
during this session purely because of this session's own tool timeout limits, not because of any
test failure — both completed green on their own.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 01 (enum-labels-in-describe) is now fully executed: all six plans complete, DESC-07 and
  CORPUS-03 requirements closed (both were already marked `[x]` in `.planning/REQUIREMENTS.md` by
  plan 01-05's completion commit, ahead of this plan's actual binding work landing — noted here for
  the record, not something this plan needed to change).
- `CHANGELOG.md` is now honest for the next Bump Version dispatch: the head names 0.10.6 (matching
  every manifest) and a clean 0.10.7 unreleased section is ready to receive this phase's release
  notes verbatim.
- Phase 2's OPT-04 (bringing `has_ui_config()` to the C API and all five bindings) has a
  documented, C++-only starting point — no binding surface needs to change shape, only gain a
  thin wrapper over the existing C++ method once the C API exposes it.
- No blockers. The pre-existing "Accepted risk" note (Quiver becomes an authoritative repeater of
  unchecked labels; HTD's `HasCommitment` inversion) remains open by design, closing at Phase 5.

---
*Phase: 01-enum-labels-in-describe*
*Completed: 2026-09-19*

## Self-Check: PASSED

All 8 files created/modified by this plan confirmed present on disk; all 3 task commit hashes
(`40ba5a8`, `09a4f80`, `ef18358`) confirmed present in `git log --oneline --all`.
