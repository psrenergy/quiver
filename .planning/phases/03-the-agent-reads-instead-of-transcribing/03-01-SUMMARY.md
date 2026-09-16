---
phase: 03-the-agent-reads-instead-of-transcribing
plan: 01
subsystem: docs
tags: [lua, csv, agent-reference, js-binding, system-prompt]

requires:
  - phase: 02-the-dirty-files-parse-correctly
    provides: "db:read_csv / db:read_csv_stream, header_row option, both fixtures and their regression tests"
provides:
  - "LUA_DB_API_REFERENCE tells the model to read a data file with db:read_csv instead of transcribing it, at the no-io sentence (D-30)"
  - "A single worked example over both real Maranhão CSV shapes, proven to run against the committed fixtures"
affects: [03-02, ship-gate]

actuals:
  tokens: 840
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Doc examples lifted verbatim from passing regression tests, then re-run through quiver_cli before shipping, rather than composed fresh and eyeballed."

key-files:
  created: []
  modified:
    - bindings/js/src/lua-api.ts

key-decisions:
  - "D-30 followed exactly: the read-don't-transcribe sentence was appended to the Standard library bullet only, not duplicated at the head of the CSV section."
  - "D-31 followed exactly: the full ~35-line example over both real files, not the compact form; final fenced block is 30 lines."
  - "Task split matched the plan's tracer/auto structure: task 1 committed the instruction + Energia half only, verified alone; task 2 added the GD half and re-verified both halves in one run plus the sync test."

patterns-established: []

requirements-completed: [DOC-02, DOC-03]

coverage:
  - id: D1
    description: "The Standard library bullet states that no io does not mean file data is unreachable, and points at db:read_csv / db:read_csv_stream (DOC-02)"
    requirement: DOC-02
    verification:
      - kind: other
        ref: "grep confirms the sentence appears exactly once, at bindings/js/src/lua-api.ts line ~98, not duplicated in the CSV section"
        status: pass
    human_judgment: false
  - id: D2
    description: "bun test lua-api-sync.test.ts still passes after the edit (stdlib sentence and db: literal tokens undisturbed)"
    verification:
      - kind: unit
        ref: "bindings/js/test/lua-api-sync.test.ts (6 pass, 0 fail)"
        status: pass
    human_judgment: false
  - id: D3
    description: "The CSV file reading section carries one worked example over both real dirty file shapes (Energia + GD), including the tonumber/gsub parenthesis-trap comment (DOC-03)"
    requirement: DOC-03
    verification:
      - kind: integration
        ref: "example's Lua copied verbatim into build/example-check/example.lua, run via quiver_cli against tests/fixtures/ma_energia_residencial.csv and tests/fixtures/ma_gd_data.csv, asserting 2005-01/93943, 2023-07/386433, 2014-05/33, 2021-07/51818.33 — exit 0"
        status: pass
    human_judgment: false

duration: 25min
completed: 2026-09-16
status: complete
---

# Phase 3 Plan 1: The agent reads instead of transcribing (agent-reference edit) Summary

**`LUA_DB_API_REFERENCE` now tells a model to read a dirty CSV off disk instead of pasting it into the script, and ships one 30-line worked example — lifted from the two passing regression tests and re-run through `quiver_cli` before commit — covering both real Maranhão file shapes plus the silent `tonumber`/`gsub` parenthesis trap.**

## Performance

- **Duration:** 25 min
- **Started:** 2026-09-16T00:00:00Z (approx, session start)
- **Completed:** 2026-09-16
- **Tasks:** 2/2
- **Files modified:** 1 (`bindings/js/src/lua-api.ts`)

## Accomplishments
- DOC-02: appended the read-don't-transcribe counterpoint to the Standard library bullet, immediately after the sentence that lists `io` as absent — the exact spot D-30 specified, and nowhere else.
- DOC-03: added one worked example to the `## CSV file reading` section covering both real fixture shapes (junk-row-above/units-row-below header + apostrophe thousands separators + DD/MM/YYYY for Energia; quoted-comma date + English month names for GD), including the `tonumber`/`gsub` parenthesis-trap comment.
- Proved the example is executable truth: copied its Lua verbatim into a throwaway harness (`build/example-check/`, gitignored, not committed), ran it via `quiver_cli` against both committed fixtures, and got the exact values the two named regression tests assert.

## Task Commits

Each task was committed atomically:

1. **Task 1: The read-don't-transcribe instruction plus the Energia half of the example, run end to end** - `834dc4e` (docs)
2. **Task 2: The GD half of the example, then the budget and gate sweep** - `ff5699b` (docs)

_No separate plan-metadata commit for this run; STATE/ROADMAP updates are captured in this same execution session's final commit per the standard executor flow._

## Files Created/Modified
- `bindings/js/src/lua-api.ts` - Standard library bullet extended with the read-don't-transcribe sentence (DOC-02); `## CSV file reading` section extended with a 30-line worked example over both real fixtures (DOC-03).

## Decisions Made
- Followed D-30 and D-31 exactly as specified in `03-CONTEXT.md` — no deviation, no re-litigation.
- Split the single planned example edit across the two task commits (Energia-only in task 1, GD added in task 2) to match the plan's per-task atomic-commit structure and verification gates, even though both edits land in the same file region.

## Deviations from Plan

None — plan executed exactly as written. Both `<verify>` commands ran and passed at each task boundary; no auto-fixes were needed.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Example verification detail

Ran (Windows, from repo root):
```
./build/bin/quiver_cli.exe --schema tests/schemas/valid/basic.sql build/example-check/check.sqlite build/example-check/example.lua
```
against a copy of both committed fixtures inside `build/example-check/` (gitignored via the
existing `build/` rule; not committed). The script, containing the example's Lua copied verbatim
out of the reference plus appended `assert`s, exited 0 with:
- Energia: first data row `2005-01 93943`, row 224 `2023-07 386433` (matches
  `EnergiaRegressionJunkRowAboveUnitsRowBelowHeader`).
- GD: first row `2014-05 33`, row 71 `2021-07 51818.33` (matches
  `GdRegressionQuotedCommaAndEnglishMonthNames`).

`bun test bindings/js/test/lua-api-sync.test.ts` — 6 pass, 0 fail.

Final example block: 30 lines of Lua (lines 667-696 of `bindings/js/src/lua-api.ts`), within the
~35-line budget D-31 sets. The read-don't-transcribe instruction appears exactly once (line ~98),
confirmed by grep — not duplicated at the head of the CSV section.

Nothing in the reference turned out to be untrue; no correction was needed to existing prose.

## Known Stubs

None.

## Next Phase Readiness

DOC-02 and DOC-03 are complete. Plan 03-02 (DOC-04: nearest `CLAUDE.md` files + changelog repair
per D-33, and TEST-05: the Release-build gate per D-32) is unblocked and has no dependency on this
plan's specific wording beyond the file being in its final state.

---
*Phase: 03-the-agent-reads-instead-of-transcribing*
*Completed: 2026-09-16*
