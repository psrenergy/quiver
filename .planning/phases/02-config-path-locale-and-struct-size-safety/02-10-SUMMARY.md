---
phase: 02-config-path-locale-and-struct-size-safety
plan: 10
subsystem: bindings/js
tags: [ffi, bun, struct-layout, safety, javascript]

# Dependency graph
requires:
  - phase: 02-config-path-locale-and-struct-size-safety (plan 02-08)
    provides: quiver_csv_options_sizeof() native accessor, pinned by a static_assert
provides:
  - "CSV_OPTIONS_SIZE and seven CSV_OPTIONS_OFFSET_* named constants in ffi-helpers.ts, consumed by csv.ts's buildCsvOptionsBuffer"
  - "JS's four-struct load-time gate (options, scalar metadata, group metadata, csv options), with checkedStructNames() wiring evidence"
  - "bindings/js/CLAUDE.md rewritten to describe the closed four-struct gate, deferral note removed"
affects: [02-13]

# Actuals (#2632)
actuals:
  tokens: 3928
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "Wiring-evidence list (_checkedStructs / checkedStructNames()) mirroring Python's _CHECKED_STRUCTS from 02-08, so a JS test can observe the gate ran without re-driving checkStructSize itself"

key-files:
  created: []
  modified:
    - bindings/js/src/ffi-helpers.ts
    - bindings/js/src/csv.ts
    - bindings/js/src/loader.ts
    - bindings/js/test/struct-sizes.test.ts
    - bindings/js/CLAUDE.md

key-decisions:
  - "Placed the quiver_csv_options_sizeof symbol-table entry inside csvSymbols (thematic grouping by C API domain, matching this file's existing convention) rather than beside the other three *_sizeof accessors in lifecycleSymbols/metadataSymbols."
  - "Rewrote the JS CLAUDE.md 'Load-time struct-size gate' bullet by extending existing lines and appending new sentences at their original line breaks, rather than reflowing the whole paragraph, so the untouched Number(...) and openLibrary() constraint sentences stay on byte-identical lines (verified via git diff) per the plan's acceptance criteria."

patterns-established:
  - "checkedStructNames() is the JS mirror of Python's _CHECKED_STRUCTS wiring-evidence pattern from 02-08: a module-level array cleared on entry to assertNativeStructSizes and appended after each check passes, exported for a test to assert the exact ordered record."

requirements-completed: [OPT-05, SAFE-01, SAFE-02, SAFE-03]

coverage:
  - id: D1
    description: "csv.ts's buildCsvOptionsBuffer writes all seven quiver_csv_options_t fields from named CSV_OPTIONS_OFFSET_* constants sized from CSV_OPTIONS_SIZE, no bare numeric literal remaining"
    requirement: OPT-05
    verification:
      - kind: unit
        ref: "bindings/js/test/test.bat (239 pass, 0 fail) plus grep verification of csv.ts's setBigUint64 call sites"
        status: pass
    human_judgment: false
  - id: D2
    description: "assertNativeStructSizes checks four structs in fixed order (options, scalar metadata, group metadata, csv options), short-circuiting on first mismatch, every *_sizeof wrapped in Number(...)"
    requirement: SAFE-01
    verification:
      - kind: unit
        ref: "bindings/js/test/struct-sizes.test.ts (quiver_csv_options_sizeof reports 56 test; happy-path describe block)"
        status: pass
    human_judgment: false
  - id: D3
    description: "checkedStructNames() records the four-struct ordered check evidence, and a test asserting it fails when the gate is unwired"
    requirement: SAFE-02
    verification:
      - kind: mutation
        ref: "Manual mutation of assertNativeStructSizes body (`void lib;`) and deletion of its call site in loadLibrary() -- both turned the ordered-record test red; restored to green"
        status: pass
    human_judgment: false
  - id: D4
    description: "Gate stays outside openLibrary()/initLibrary()'s three try/catch tiers, on the memoized loadLibrary() path"
    requirement: SAFE-03
    verification:
      - kind: unit
        ref: "grep -n 'assertNativeStructSizes' bindings/js/src/loader.ts -- single call site inside loadLibrary(), none inside openLibrary/initLibrary"
        status: pass
    human_judgment: false
---

# Phase 02 Plan 10: JS CSV Options Struct Safety Gate Summary

Closed the JS half of Gap 6 and Gap 2: `quiver_csv_options_t` (the largest hand-allocated struct
in the JS binding) joined the load-time struct-size gate as its fourth member, and its seven
bare-literal buffer offsets in `csv.ts` became named constants.

## What Was Built

**Task 1 — Named CSV option offsets.** Added `CSV_OPTIONS_SIZE` (56) and the seven
`CSV_OPTIONS_OFFSET_*` constants (0, 8, 16, 24, 32, 40, 48, in field-declaration order matching
`include/quiver/c/options.h`) to `bindings/js/src/ffi-helpers.ts`, mirroring the existing
`OPTIONS_OFFSET_*` block. `bindings/js/src/csv.ts`'s `buildCsvOptionsBuffer` now allocates
`new Uint8Array(CSV_OPTIONS_SIZE)` and writes each of its seven `setBigUint64` calls through the
named offset, with no behavioral change. Confirmed `allocPtrOut`/`allocUint64Out`'s unrelated
8-byte allocations in `ffi-helpers.ts` are untouched (`grep -c 'new Uint8Array(8)'` still returns
2) — the exact D-11 regression the acceptance criteria guard against.

**Task 2 — Four-struct gate with wiring evidence.** Added `quiver_csv_options_sizeof: { args: [],
returns: USIZE }` to `loader.ts`'s `csvSymbols` block, and a fourth `checkStructSize` call in
`assertNativeStructSizes` for `quiver_csv_options_t`, placed last (options, scalar metadata,
group metadata, csv options — the same order Python uses after 02-08). Added a module-level
`_checkedStructs: string[]`, cleared on entry to `assertNativeStructSizes` and appended after each
struct's check returns without throwing, exported via `checkedStructNames()`. Added a test in
`struct-sizes.test.ts` asserting the exact four-name ordered array after `getSymbols()` — this
test does **not** call `assertNativeStructSizes` itself, so it cannot repopulate the record and
recreate a vacuous pass. Extended the adjacency test to cover `quiver_csv_options_t` at 55/57
bytes. The gate itself was not moved: `loadLibrary()` still calls it once, outside every
`initLibrary()` try/catch tier.

**Task 3 — CLAUDE.md deferral closed.** Deleted the "Known, deliberately unfixed" bullet about
`quiver_csv_options_t`. Extended the "Load-time struct-size gate" paragraph to name all four
structs and their four `*_sizeof` accessors, and added `checkedStructNames()` as documented wiring
evidence with the one-sentence reason it exists (a test driving only `checkStructSize` stayed
green with the gate deleted). The `SCALAR_METADATA_SIZE`/`GROUP_METADATA_SIZE` bullet was extended
to include `CSV_OPTIONS_SIZE` and the seven CSV offset constants. The `Number(...)` wrapping rule,
the "must not move inside `openLibrary()`" constraint, and the `LUA_DB_API_REFERENCE` paragraphs
are byte-identical to before (verified via `git diff` showing no changed lines in those spans).

## Mutation Criterion (executed both directions)

1. **Gutted `assertNativeStructSizes`'s body** with `void lib;` (script-driven, not committed).
   `bun test test/struct-sizes.test.ts` failed: the new ordered-record test expected
   `["quiver_database_options_t", "quiver_scalar_metadata_t", "quiver_group_metadata_t",
   "quiver_csv_options_t"]` and received `[]` (10 pass, 1 fail). Restored the original file; suite
   returned to 11 pass, 0 fail.
2. **Deleted the `assertNativeStructSizes(lib.symbols);` call site** from `loadLibrary()`
   (script-driven, not committed). Same test failed identically (10 pass, 1 fail; empty record).
   Restored the original file; suite returned to 11 pass, 0 fail.

Both mutations were applied and reverted via a scratch Python script against the working tree —
neither mutation was committed.

## Verification

- `bindings/js/test/test.bat`: **239 pass, 0 fail, 438 expect() calls** across 23 files (baseline
  was 231; grew by 8 across the two new struct-sizes.test.ts describe blocks and the two `_sizeof`
  happy-path assertions).
- Native rebuild: `cmake --build build --config Debug` reported "no work to do" — the
  `quiver_csv_options_sizeof` accessor from plan 02-08 was already built into `build/bin/
  libquiver_c.dll`, so no C++ recompilation was needed for this JS-only plan.
- `bunx biome check` on the four touched source/test files: 3 pre-existing errors, all
  CRLF/`core.autocrlf`-related (confirmed identical count present on `git stash` of this plan's
  changes — i.e., present in the pre-plan baseline, not introduced here) plus 2 pre-existing
  `useTemplate` info-level hints at unrelated lines (154, 196) in `ffi-helpers.ts` untouched by
  this plan. No new lint findings from the added code.
- Grep-verified acceptance criteria: `setBigUint64(` in `csv.ts` shows no bare numeric literal
  first argument (the one remaining literal-looking call, `entryCountsDv.setBigUint64(i * 8, ...)`,
  writes into a separate parallel-array allocation, not the struct buffer, and was untouched by
  this plan). `new Uint8Array(56)` count in `csv.ts` is 0. `new Uint8Array(8)` count in
  `ffi-helpers.ts` is still 2.

## Deviations from Plan

None — plan executed exactly as written. No unplanned merge-dropped regressions were found in the
JS binding (the heads-up about wave 2's Julia c_api.jl regression did not surface an analogous JS
issue).

## Known Stubs

None.

## Self-Check: PASSED

- `bindings/js/src/ffi-helpers.ts` — FOUND (CSV_OPTIONS_SIZE + 7 offset constants present)
- `bindings/js/src/csv.ts` — FOUND (buildCsvOptionsBuffer uses named constants)
- `bindings/js/src/loader.ts` — FOUND (quiver_csv_options_sizeof symbol, fourth checkStructSize call, checkedStructNames())
- `bindings/js/test/struct-sizes.test.ts` — FOUND (11 tests, ordered-record assertion)
- `bindings/js/CLAUDE.md` — FOUND (deferral bullet removed, four-struct gate documented)
- Commit `0ee7355` — FOUND in `git log --oneline`
- Commit `f9f5f37` — FOUND in `git log --oneline`
- Commit `2581a73` — FOUND in `git log --oneline`
