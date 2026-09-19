---
phase: 02-config-path-locale-and-struct-size-safety
plan: 07
subsystem: release-tooling
tags: [changelog, semver, release-process]

requires:
  - phase: 02-config-path-locale-and-struct-size-safety
    provides: "02-01..02-06: the full cross-layer OPT-01..06/SAFE-01..03 delivery this entry describes (C++/C API struct growth, and the JS/Python/Dart/Julia/Lua binding updates)"
provides:
  - "CHANGELOG.md's unreleased heading renamed from the never-tagged `## [0.10.7]` to `## [0.11.0] — unreleased`, compare link updated to `v0.10.6...v0.11.0`"
  - "A BREAKING-prefixed entry naming the `quiver_database_options_t` 8 -> 24 byte growth, its offsets, and the explicit recompile/update-your-FFI-layer instruction, ordered first in the Added block"
  - "A non-breaking Added entry for `ui_config_dir`/`ui_locale` as optional parameters on open/from_schema/from_migrations in every binding, `--ui-config-dir`/`--ui-locale` on quiver_cli, and `has_ui_config()` reaching every layer"
  - "The release-timing checkpoint resolved as hold-for-phase-3 and recorded in STATE.md's Pending Todos, so the deferred single part=minor dispatch cannot be forgotten between phases"
affects: []

actuals:
  tokens: 950
  tasks: 3
  commits: 1

tech-stack:
  added: []
  patterns: []

key-files:
  created: []
  modified:
    - CHANGELOG.md
    - .planning/STATE.md

key-decisions:
  - "Checkpoint (Task 2) was pre-resolved by the orchestrator as hold-for-phase-3, not decided during this execution. The ROADMAP's own Phase 2 note ('Buildable concurrently with Phase 3 ... Both are native changes and should ship in the same release') and Phase 5's title ('validate_ui_config() and Milestone Release') both point the same way, so no independent re-derivation was needed or performed."
  - "The existing 0.10.7 Added bullet describing DatabaseOptions/has_ui_config/sizeof accessors (written during Phase 1, ahead of Phase 2's actual delivery) was folded into the new BREAKING entry plus a non-breaking Added entry, rather than left standing alongside near-duplicate new bullets -- it described the C++ core work only and did not yet say BREAKING, name the byte counts, or cover the five bindings' actual surface."
  - "No Bump Version workflow dispatch, no manifest hand-edit, no tag: all three prohibitions in the plan's frontmatter were honored by construction (only CHANGELOG.md and STATE.md were touched)."

requirements-completed: [OPT-03, OPT-05, OPT-06, SAFE-01, SAFE-02, SAFE-03]

coverage:
  - id: D1
    description: "CHANGELOG.md heads a correct `## [0.11.0] — unreleased` section (renamed from 0.10.7, no duplicate heading, no v0.10.7 reference) with a compare link to v0.10.6...v0.11.0"
    requirement: SAFE-01
    verification:
      - kind: unit
        ref: "grep -c '^## \\[0.11.0\\] — unreleased$' CHANGELOG.md == 1; grep -c '0.10.7' CHANGELOG.md == 0; grep -c 'compare/v0.10.6...v0.11.0' CHANGELOG.md == 1"
        status: pass
    human_judgment: false
  - id: D2
    description: "The 0.11.0 section carries an actionable BREAKING entry naming quiver_database_options_t, 8, 24, and telling a caller to recompile / update its hand-written FFI layer"
    requirement: SAFE-02
    verification:
      - kind: unit
        ref: "grep -n '^- \\*\\*BREAKING — \\`quiver_database_options_t\\`' CHANGELOG.md"
        status: pass
    human_judgment: false
  - id: D3
    description: "No manifest hand-edited and no tag created: scripts/assert_version.py still reports all five files at 0.10.6, and git tag --points-at HEAD is empty"
    requirement: SAFE-03
    verification:
      - kind: unit
        ref: "uv run python scripts/assert_version.py (exit 0, reports 0.10.6); git tag --points-at HEAD (empty)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Release-timing decision (hold-for-phase-3) executed: no Bump Version workflow dispatched, and the deferral is recorded in STATE.md's Pending Todos"
    requirement: OPT-06
    verification:
      - kind: manual_procedural
        ref: ".planning/STATE.md Pending Todos section: 'Deferred release dispatch' entry"
        status: pass
    human_judgment: false

duration: 20min
completed: 2026-09-19
status: complete
---

# Phase 2 Plan 7: Release Preparation Summary

**Renamed the never-tagged 0.10.7 unreleased changelog heading to 0.11.0 with an actionable BREAKING entry for the options-struct ABI break, and held the Bump Version dispatch for Phase 3 per the ROADMAP's same-release note.**

## Performance

- **Duration:** 20 min
- **Tasks:** 3
- **Files modified:** 2 (CHANGELOG.md; STATE.md via the state-update step)

## Accomplishments

- `CHANGELOG.md`'s unreleased heading is now `## [0.11.0] — unreleased` (renamed, not duplicated, from Phase 1's never-tagged `## [0.10.7]`), with its compare link updated to `v0.10.6...v0.11.0`.
- The section now leads with a **BREAKING** entry naming `quiver_database_options_t`'s growth from 8 to 24 bytes (offsets `0`/`4`/`8`/`16`), telling a hand-rolled FFI consumer to recompile and update its own layout, and noting the three new `*_sizeof` accessors every first-party binding now checks at load time.
- A second Added entry documents the non-breaking new capability: `ui_config_dir`/`ui_locale` as optional parameters on `open`/`from_schema`/`from_migrations` in every binding plus `--ui-config-dir`/`--ui-locale` on `quiver_cli`, and `has_ui_config()` reaching every layer including Lua.
- Phase 1's existing "reads a PSR `database/ui/` sidecar" Added bullet was kept, reordered after the two new entries (BREAKING first, then Added, per the plan's ordering requirement).
- `scripts/assert_version.py` still reports all five manifests in agreement at `0.10.6` — no manifest was touched.
- The release-timing checkpoint (dispatch now vs. hold for Phase 3) was **pre-resolved by the orchestrator as hold-for-phase-3** before this execution began. No Bump Version workflow was dispatched; the deferral (dispatch exactly once, `part=minor`, after Phase 3 lands) is now recorded in `.planning/STATE.md`'s Pending Todos so it survives the phase boundary.
- No git tag was created.

## Task Commits

1. **Task 1: Rename the unreleased heading to 0.11.0 and write the BREAKING entry** - `673c037` (docs)
2. **Task 2: Checkpoint (release timing decision)** - pre-resolved by the orchestrator as hold-for-phase-3; no commit (decision only)
3. **Task 3: Execute the decision and record it** - no code commit (STATE.md Pending Todos edit is part of the state-update step at the end of this plan's execution, not a separate task commit)

**Plan metadata:** (final commit, made together with STATE.md/ROADMAP.md/REQUIREMENTS.md per the standard state-update step)

## Files Created/Modified

- `CHANGELOG.md` - Renamed 0.10.7 -> 0.11.0, added BREAKING entry + expanded Added entry
- `.planning/STATE.md` - Pending Todos entry recording the deferred `part=minor` dispatch

## Decisions Made

- Checkpoint resolution (hold-for-phase-3) was supplied by the orchestrator rather than re-derived during execution — see `key-decisions` in frontmatter for the supporting evidence read directly from `.planning/ROADMAP.md` (Phase 2's "should ship in the same release" note and Phase 5's "...and Milestone Release" title).
- Folded Phase 1's pre-existing DatabaseOptions/has_ui_config/sizeof-accessor bullet into the new BREAKING + Added entries rather than leaving it as a third, overlapping bullet — see `key-decisions` in frontmatter.

## Deviations from Plan

None - plan executed exactly as written, with the checkpoint answered per the orchestrator's explicit pre-resolution (documented in the task prompt as already settled, not a guess made during execution).

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 2 (all 7 plans) is now complete. `CHANGELOG.md` heads a correct, actionable, unreleased 0.11.0 section covering both Phase 1 and Phase 2.
- The Bump Version dispatch (`part=minor`, exactly once) remains deferred until Phase 3 (Structured Attribute Metadata) lands, per `.planning/STATE.md`'s Pending Todos. Do not dispatch `part=patch` at any point — it would tag a spurious `v0.10.7` shipping only Phase 1's content.
- No blockers for Phase 3.

---
*Phase: 02-config-path-locale-and-struct-size-safety*
*Completed: 2026-09-19*
