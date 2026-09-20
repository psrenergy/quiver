---
phase: 02-enum-labels-on-the-value-histogram
plan: 01
subsystem: database
tags: [cpp, sqlite, toml, describe, ui-metadata]

# Dependency graph
requires:
  - phase: 01-ui-metadata-reader-and-describe-rendering
    provides: "impl_->ui_metadata (UiMetadata map, populated only by from_migrations), normalize_ui_text / quote_ui_text helpers, the D-01..D-08 clause grammar"
provides:
  - "summarize_collection()'s integer value histogram annotates each observed code with its enum label from Phase 1's UiMetadata map"
  - "Project decision D-09 (D2-06): a label that normalizes to empty drops only the annotation, keeps the entry — deliberate divergence from D-06"
  - "D2-14 documentation closure: no document in the repo still claims summarize_collection omits this metadata"
affects: []

# Actuals (#2632)
actuals:
  tokens: 2398
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - "Enum-label annotation lookup sits inside the cardinality-bounded branch, immediately before '; values {' emission — not at the top of the per-scalar loop, so non-enum scalars pay zero UiMetadata lookups (D2-12)."

key-files:
  created: []
  modified:
    - src/database_describe.cpp
    - tests/test_database_ui_metadata.cpp
    - CHANGELOG.md
    - CLAUDE.md
    - src/CLAUDE.md

key-decisions:
  - "D-09 (D2-06): in summarize_collection's histogram, an enum label that normalizes to empty drops the annotation only and keeps the entry — the entry is an observed row count, not vocabulary, so dropping it would destroy data. Deliberate divergence from D-06, which drops the whole entry in describe's enum {} clause."
  - "ui_metadata is populated only by from_migrations (Phase 1 boundary, reconfirmed): Database::open('study.db').summarize_collection(...) still shows bare codes. Must not be fixed by hooking the load onto load_schema_metadata / require_schema — migrate_up early-returns before it on the open-an-existing-study path."
  - "kMaxDistributionCardinality stays 64: a column with more than 64 distinct codes still renders no distribution clause at all (unchanged ceiling); describe_collection's enum {...} is the fallback."
  - "D2-09 and D2-10 recorded as honesty records, not evidence: the four test_database_lifecycle.cpp SC-3 assertions are from_schema(':memory:') + describe() and unreachable by this diff; quote_ui_text gives a parse-level guarantee, not substring immunity, and summarize additionally emits 'Vectors:'/'Sets:'/'Time Series:' headers that a naive header-count test could break on."

patterns-established: []

requirements-completed: [RENDER-02]

coverage:
  - id: D1
    description: "summarize_collection's histogram renders each observed code with its enum label beside it: values {0 \"Per Unit\": 2, 1: 1}"
    requirement: "RENDER-02"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.SummarizeHistogramAnnotatesCodesWithEnumLabels"
        status: pass
    human_judgment: false
  - id: D2
    description: "An uncovered code and an unobserved vocabulary code both stay/never-appear correctly (D2-05, D2-07)"
    requirement: "RENDER-02"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.SummarizeHistogramAnnotatesCodesWithEnumLabels"
        status: pass
    human_judgment: false
  - id: D3
    description: "Rendering against a real PSR study corpus — histogram entries carry their labels"
    verification: []
    human_judgment: true
    rationale: "Corpora live outside this repo and cannot be committed as fixtures (VALIDATION.md Manual-Only Verifications)."

duration: 20min
completed: 2026-09-20
status: complete
---

# Phase 2 Plan 1: Enum Labels on the Value Histogram Summary

**`summarize_collection()`'s integer value histogram now annotates each observed code with its enum label (`values {0 "Per Unit": 2, 1: 1}`), reusing Phase 1's `UiMetadata` map with one lookup inside the existing cardinality-bounded emission loop — zero new files, zero signature changes.**

## Performance

- **Duration:** ~20 min
- **Started:** 2026-09-20T16:20:00-03:00 (approx)
- **Completed:** 2026-09-20T16:29:12-03:00
- **Tasks:** 2 completed
- **Files modified:** 5

## Accomplishments
- `summarize_collection`'s `; values {` emission loop annotates each observed integer code with its enum label from `impl_->ui_metadata`, when that code is covered by the vocabulary — the milestone's headline output (RENDER-02).
- An observed code the vocabulary does not cover renders bare (unchanged); an unobserved vocabulary code never appears in the histogram (D2-07) — `summarize_collection` reports the table, not the vocabulary.
- A label that normalizes to empty drops only the annotation, never the entry — recorded as project decision D-09, a deliberate divergence from Phase 1's D-06 (which drops the whole entry in the `enum {}` clause).
- `CHANGELOG.md`, root `CLAUDE.md`, and `src/CLAUDE.md` all updated so no document still claims `summarize_collection` omits this metadata (D2-14).

## Task Commits

Each task was committed atomically (task 1 followed TDD RED/GREEN since it carries `tdd="true"`):

1. **Task 2-01-01 (RED): Add failing test for summarize histogram enum labels** - `4f8f68b` (test)
2. **Task 2-01-01 (GREEN): Annotate summarize histogram codes with enum labels** - `2c2a423` (feat)
3. **Task 2-01-02: D2-14 documentation edits and the D-09 decision record** - `24df45d` (docs)

_Note: task 1 (`tdd="true"`) produced two commits — RED then GREEN — per the TDD execution flow._

## Files Created/Modified
- `src/database_describe.cpp` - `Database::summarize_collection`'s `; values {` loop: looks up `impl_->ui_metadata.find(collection, scalar.name)` inside the cardinality branch (D2-12), and on a per-code hit appends `normalize_ui_text` + `quote_ui_text`'d label after the code when non-empty (D2-06/D-09).
- `tests/test_database_ui_metadata.cpp` - New `TEST_F(DatabaseUiMetadataTest, SummarizeHistogramAnnotatesCodesWithEnumLabels)`: three `create_element` calls producing codes 0 (×2) and 1 (×1), a vocabulary covering codes 0 and the unobserved 2, asserting the labelled/bare/absent shape in one string pair.
- `CHANGELOG.md` - `0.10.8 — unreleased` entry's closing sentence replaced: no longer claims `summarize_collection()` omits enum labels; states the rendered form and the two known limits (uncovered code stays bare, >64-code ceiling still suppresses the whole clause).
- `CLAUDE.md` - Core API bullet (line 576) for `summarize_collection(c)` now names the enum-label annotation.
- `src/CLAUDE.md` - `ui_metadata.h`/`ui_metadata.cpp` section: replaced the "does not render any of this yet" sentence with D-09's decision record, the two known limits, and D2-10's honest substring-injection posture.

## Decisions Made
See `key-decisions` in frontmatter: D-09 (annotation-only drop on empty label), the `from_migrations`-only load boundary reaffirmed, `kMaxDistributionCardinality` unchanged at 64, and D2-09/D2-10 recorded explicitly as honesty records rather than evidence.

## Deviations from Plan

None - plan executed exactly as written. The tracer task's `<verify>` (full `quiver_tests` + `quiver_c_tests` + `clang-format --dry-run -Werror`) was run to completion before task 2 began, satisfying the tracer feedback gate's intent (project `mode: "yolo"` runs autonomously without prompts, and both `workflow._auto_chain_active` and `workflow.auto_advance` gate flags were checked and are consistent with continuing once the fully-automated verify passed).

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness

Phase 2 is the last phase of milestone v0.10.8 (per `ROADMAP.md`'s two-phase split). No further plans are queued under `02-enum-labels-on-the-value-histogram/`. Remaining before milestone close: the manual-only verification (rendering against a real PSR study corpus, `02-VALIDATION.md`) and standard milestone completion steps (`/gsd-complete-milestone`).

---
*Phase: 02-enum-labels-on-the-value-histogram*
*Completed: 2026-09-20*
