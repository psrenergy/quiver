---
phase: 02-config-path-locale-and-struct-size-safety
plan: 13
subsystem: docs
tags: [changelog, versioning, release-notes, planning-hygiene]

# Dependency graph
requires:
  - phase: 02-08
    provides: quiver_csv_options_sizeof native accessor + restored Python gate
  - phase: 02-09
    provides: exact-string enum rendering tests (DESC-07) across Julia/Dart/Python/JS
  - phase: 02-10
    provides: JS four-struct load-time gate
  - phase: 02-11
    provides: Julia and Dart four-struct load-time gates
  - phase: 02-12
    provides: JS version-skew diagnosis + makeDefaultOptions lifetime fix
provides:
  - Single, continuous CHANGELOG.md compare chain (0.11.0 -> 0.10.7 -> 0.10.6 -> ... -> 0.10.0)
  - 0.11.0 unreleased entry naming all four *_sizeof accessors, including quiver_csv_options_sizeof
  - STATE.md and 02-VERIFICATION.md corrected to state today's true version/tag facts
affects: [phase-03-planning, release-dispatch]

# Actuals (#2632)
actuals:
  tokens: 2130
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns: []

key-files:
  created: []
  modified:
    - CHANGELOG.md
    - .planning/STATE.md
    - .planning/phases/02-config-path-locale-and-struct-size-safety/02-VERIFICATION.md

key-decisions:
  - "Renamed the first duplicate '## [0.10.6]' heading (Lua CSV feature body) to '## [0.10.7] — 2026-09-17' rather than deleting/rewriting it, preserving its content and PR #290's attribution."
  - "Moved the '10.4/10.5/10.6 unreleased block' explanatory paragraph to the surviving 0.10.6 section, where the statement it makes is actually true."
  - "Extended the existing BREAKING options-struct bullet to name the fourth accessor rather than adding a second bullet, since it is the same story (one load-time gate, four structs)."
  - "Left STATE.md's Pending Todos deferred-dispatch note (part=minor, held for Phase 3) unchanged in substance, per the plan's explicit instruction — the dispatch policy is unaffected by this gap closure."
  - "Did not edit REQUIREMENTS.md — SAFE-01 was already [x] and Complete; editing it would rewrite the requirement rather than record its fulfilment."

patterns-established: []

requirements-completed: [SAFE-01]

coverage:
  - id: D1
    description: "CHANGELOG.md has exactly one heading per shipped version (0.10.6, 0.10.7) with a continuous compare-link chain and no orphaned/duplicate link"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "grep -c '^## \\[0\\.10\\.6\\]' CHANGELOG.md == 1; grep -c '^## \\[0\\.10\\.7\\]' CHANGELOG.md == 1; grep -n '^\\[0\\.1' CHANGELOG.md shows continuous chain"
        status: pass
    human_judgment: false
  - id: D2
    description: "0.11.0 entry names all four *_sizeof accessors including quiver_csv_options_sizeof, still exactly one BREAKING bullet"
    requirement: "SAFE-01"
    verification:
      - kind: unit
        ref: "grep -c 'quiver_csv_options_sizeof' CHANGELOG.md >= 1, inside ## [0.11.0]"
        status: pass
    human_judgment: false
  - id: D3
    description: "scripts/assert_version.py reports all five manifests at 0.10.7, no manifest hand-edited"
    verification:
      - kind: unit
        ref: "uv run python scripts/assert_version.py (exit 0, all project files at 0.10.7)"
        status: pass
    human_judgment: false
  - id: D4
    description: "STATE.md and 02-VERIFICATION.md no longer repeat the stale 'never-tagged 0.10.7' / '0.10.4 vs 0.10.6' premise"
    verification:
      - kind: unit
        ref: "grep -c '0\\.10\\.4' .planning/STATE.md == 0; grep -c 'Post-verification reconciliation' 02-VERIFICATION.md == 1"
        status: pass
    human_judgment: false

duration: ~10min
completed: 2026-09-19
status: complete
---

# Phase 2 Plan 13: CHANGELOG Version-Chain Reconciliation Summary

**Reconciled CHANGELOG.md's duplicate `## [0.10.6]` headings into one `## [0.10.7] — 2026-09-17` section with a continuous compare-link chain, named the fourth `quiver_csv_options_sizeof` accessor in the 0.11.0 entry, and corrected STATE.md / 02-VERIFICATION.md's stale version premises.**

## Performance

- **Duration:** ~10 min
- **Tasks:** 3
- **Files modified:** 3 (CHANGELOG.md, .planning/STATE.md, 02-VERIFICATION.md)

## Accomplishments

- Renamed the mis-dated `## [0.10.6] — 2026-09-11` heading whose body was actually PR #290's Lua
  `db:read_csv` / `db:read_csv_stream` / `db:write_csv` feature and the `weakly_canonical` path
  hardening to `## [0.10.7] — 2026-09-17`, matching the tag that exists locally and on the remote.
  Moved the misplaced "cut from the same unreleased block as 0.10.4 and 0.10.5" paragraph to the
  real 0.10.6 section (the Dart DST-gap fix), where the statement is true.
- Added the missing `[0.10.7]: .../compare/v0.10.6...v0.10.7` link and repointed `[0.11.0]`'s base
  from the never-tagged `v0.11.0` to `v0.10.7`. The compare chain is now continuous from 0.11.0
  down to 0.10.0 with no orphan and no duplicate.
- Extended the 0.11.0 entry's single BREAKING bullet to name all four `*_sizeof` accessors
  (`quiver_database_options_sizeof`, `quiver_scalar_metadata_sizeof`, `quiver_group_metadata_sizeof`,
  `quiver_csv_options_sizeof`), describing what a hand-allocating FFI consumer of
  `quiver_csv_options_t` (56 bytes, seven pointer-width fields) can now verify at runtime, and
  noting the accessor is additive (not part of the breaking change).
- Replaced STATE.md's stale "CHANGELOG.md heads 0.10.4 against CMakeLists.txt 0.10.6" blocker with
  a resolved note, and updated the "four binding describe suites assert only a String" concern to
  record that 02-09 closed the enum-label path (DESC-07) while malformed-config and `:memory:`
  polarity tests outside C++ remain open (follow-up #5).
- Appended a `## Post-verification reconciliation` section to 02-VERIFICATION.md (additions only,
  original findings untouched) mapping all seven recommended follow-ups to the plan(s) that closed
  each, noting the BLOCKER's git half was resolved by merge `7bd1f16`, and recording that the
  Python struct-size-gate mutation reproduced an already-shipped defect (`pass  # MUTATION: gate
  unwired`, merge `e8d35b9`), not a hypothetical one.

## Task Commits

Each task was committed atomically:

1. **Task 1: Reconcile the CHANGELOG version chain** - `fb31d6c` (docs)
2. **Task 2: Name the fourth size accessor in the 0.11.0 entry** - `e6aa7cc` (docs)
3. **Task 3: Correct the stale premises in STATE.md and the verification record** - `f8dce18` (docs)

_No separate plan-metadata commit — this SUMMARY commit doubles as it (see `<final_commit>`)._

## Files Created/Modified

- `CHANGELOG.md` - Duplicate 0.10.6 heading split into 0.10.6 + 0.10.7, compare-link chain fixed, 0.11.0 entry names the fourth accessor
- `.planning/STATE.md` - Stale CHANGELOG/version blocker resolved-in-place, describe-suite concern updated to reflect 02-09
- `.planning/phases/02-config-path-locale-and-struct-size-safety/02-VERIFICATION.md` - Post-verification reconciliation section appended

## Decisions Made

- Renamed rather than duplicated the mis-dated heading, preserving the shipped feature's content.
- Moved the misattributed context paragraph to the section it actually describes instead of deleting it.
- Named the fourth accessor inside the existing BREAKING bullet (one story, one gate, four structs) rather than adding a second bullet.
- Left the Pending Todos deferred-dispatch note unchanged in substance, per the plan's explicit instruction: the `part=minor` dispatch stays held for Phase 3 regardless of this gap closure.
- Did not touch REQUIREMENTS.md — SAFE-01 was already `[x]`/Complete and remains so; this plan documents the fulfilment, it doesn't re-litigate the requirement.

## Deviations from Plan

None - plan executed exactly as written. Every acceptance criterion in all three tasks was verified by running the plan's own grep/`assert_version.py` commands against the actual file state, not assumed.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Gap 1 from 02-VERIFICATION.md is closed: CHANGELOG.md has one heading per shipped version, a correct compare chain, and the 0.11.0 entry describes the full ABI surface (all four size accessors).
- `.planning/STATE.md` and `02-VERIFICATION.md` no longer repeat the "never-tagged 0.10.7" premise anywhere.
- The Bump Version workflow was not dispatched — the single `part=minor` dispatch remains deliberately held until Phase 3 lands, unchanged by this plan.
- This is the last plan in Phase 2; Phase 3 planning can proceed against an accurate CHANGELOG and STATE.md.

---
*Phase: 02-config-path-locale-and-struct-size-safety*
*Completed: 2026-09-19*

## Self-Check: PASSED

- FOUND: `.planning/phases/02-config-path-locale-and-struct-size-safety/02-13-SUMMARY.md`
- FOUND commit `fb31d6c` (Task 1)
- FOUND commit `e6aa7cc` (Task 2)
- FOUND commit `f8dce18` (Task 3)
