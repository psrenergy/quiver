---
phase: 02-config-path-locale-and-struct-size-safety
plan: 12
subsystem: bindings/js
tags: [ffi, bun, loader, lifetime-safety, javascript]

# Dependency graph
requires:
  - phase: 02-config-path-locale-and-struct-size-safety (plan 02-10)
    provides: "The four-struct load-time gate (quiver_database_options_t / quiver_scalar_metadata_t / quiver_group_metadata_t / quiver_csv_options_t), checkedStructNames() wiring evidence, and the current loader.ts/ffi-helpers.ts this plan parameterizes and rewrites"
provides:
  - "resolveLibrary(symbols) in loader.ts -- a probe-based version-skew diagnosis distinguishing a stale native (present, missing *_sizeof exports) from no native at all"
  - "makeDefaultOptions() returning a single self-contained Allocation with no keepalive, eliminating a proven (if low-exposure) GC lifetime defect"
  - "bindings/js/CLAUDE.md documentation of both remedies and both rejected alternatives"
affects: [02-13]

# Actuals (#2632)
actuals:
  tokens: 6502
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "Probe-based version-skew diagnosis: re-run the same tiered resolveLibrary() with a minimal PROBE_SYMBOLS map to determine 'present but stale' vs 'absent', rather than annotating a collapsed lastError"
    - "Single self-contained Allocation with a variable-length tail: fixed-shape structs (a known, bounded set of optional strings) get their child data copied into the same buffer instead of a keepalive array, eliminating the second-allocation GC hazard by construction"

key-files:
  created: []
  modified:
    - bindings/js/src/loader.ts
    - bindings/js/src/ffi-helpers.ts
    - bindings/js/src/database.ts
    - bindings/js/test/struct-sizes.test.ts
    - bindings/js/test/ffi-helpers.test.ts
    - bindings/js/CLAUDE.md

key-decisions:
  - "Gap 3 shape: probe, not catch-and-annotate. initLibrary() already collapses three failure paths into one lastError, so annotating it could only ever guess 'maybe stale' and would guess it just as loudly with no native at all. A second, independent resolution with PROBE_SYMBOLS (quiver_get_last_error, which predates this milestone) is a determination, not a guess."
  - "Gap 4 remedy chosen was elimination, not reinforcement: makeDefaultOptions no longer allocates child string buffers at all -- both option strings are copied into the tail of the same Uint8Array that holds the struct header, and their pointer fields are computed via ptr(buf, tailOffset). There is no second allocation left for the GC to reclaim."
  - "Gap 4 remedy 2 (reading OPTIONS_SIZE from the accessor at allocation time) rejected, as the plan specified: getSymbols() provably precedes makeDefaultOptions at all three call sites, so the pinned OPTIONS_SIZE literal is already guarded by the load-time gate; reading it per-call would add an FFI call to every database open for zero additional safety."
  - "buildCsvOptionsBuffer in csv.ts deliberately keeps its own [Allocation, Allocation[]] keepalive tuple -- it needs a variable number of child pointer tables (enum label/locale/value arrays) and cannot collapse into one buffer the way a fixed two-string options struct can. This asymmetry is now recorded in bindings/js/CLAUDE.md so it is not 'unified' by a future edit."

patterns-established:
  - "PROBE_SYMBOLS / resolveLibrary(symbols): any future loader change that needs to distinguish 'library absent' from 'library present but missing an export' can reuse this probe shape rather than inventing a new one."

requirements-completed: [OPT-05, SAFE-02]

coverage:
  - id: D1
    description: "A native library that loads but lacks the four *_sizeof exports produces a version-skew diagnosis naming all four accessors, driven through the real dlopen against the real native library (not a stub)"
    requirement: SAFE-02
    verification:
      - kind: unit
        ref: "bindings/js/test/struct-sizes.test.ts \"resolveLibrary diagnoses a native that loads but lacks the size accessors\" > \"a symbol map with a nonexistent symbol produces a version-skew diagnosis, not a not-found\" (pass)"
        status: pass
    human_judgment: false
  - id: D2
    description: "makeDefaultOptions returns a single self-contained Allocation with no keepalive; the pointer fields round-trip the exact strings passed in, including a non-ASCII value"
    requirement: OPT-05
    verification:
      - kind: unit
        ref: "bindings/js/test/ffi-helpers.test.ts \"makeDefaultOptions\" describe block (7 tests, including the non-ASCII round-trip test), plus bindings/js/test/database-ui-options.test.ts (12 tests pushing uiConfigDir/uiLocale across the real FFI boundary)"
        status: pass
    human_judgment: false

duration: 45min
completed: 2026-09-19
status: complete
---

# Phase 02 Plan 12: JS Loader Version-Skew Diagnosis and Options Allocation Lifetime Fix Summary

Closed Gap 3 (JS could not diagnose a stale native library missing the `*_sizeof` exports) and
Gap 4 (a proven GC lifetime defect in `makeDefaultOptions`'s keepalive pattern) — the two
JS-only defects `02-VERIFICATION.md` proved but could not reach through the public API.

## Performance

- **Duration:** 45 min
- **Tasks:** 3
- **Files modified:** 6

## Accomplishments

- **Version-skew diagnosis (Gap 3).** `openLibrary(dir, symbols)` and `initLibrary(symbols)` now
  take the symbol map as a parameter instead of closing over `allSymbols`. A new exported
  `resolveLibrary(symbols)` runs `initLibrary(symbols)`; on failure it re-runs the same three
  tiers with `PROBE_SYMBOLS` — one symbol (`quiver_get_last_error`) that has existed since before
  this milestone. A successful probe means a Quiver native library is present and loadable but
  lacks the newer `*_sizeof` exports — the only version skew a published native can currently
  produce, since Phase 2 introduces those exports and no already-published native has them — so
  the throw names all four accessors (`SIZEOF_ACCESSOR_NAMES`) and says the native predates this
  release of the JS binding. If the probe also fails, the original error is rethrown **unchanged**
  (`throw e`, not a new error), so a genuine not-found still surfaces the existing "Cannot load
  native library ... Searched: ..." text. `loadLibrary()` now calls `resolveLibrary(allSymbols)`;
  the struct-size gate did not move — it still runs exactly once, outside every `initLibrary()`
  tier and outside `resolveLibrary`'s own `try`/`catch`.
- **Options allocation lifetime fix (Gap 4).** `makeDefaultOptions` no longer returns
  `[Allocation, Allocation[]]`. It returns a single `Allocation` whose `Uint8Array` holds the
  24-byte struct header AND both option strings, each copied into the buffer's own tail at a
  running offset; each pointer field is computed with `ptr(buf, tailOffset)` into that same
  buffer. There is no second allocation left for Bun's GC to reclaim between the call and the
  native read of its pointer — eliminating the mechanism the verification's byte-for-byte replica
  proved defective, rather than binding the old keepalive harder against it. `allocPtrOut`,
  `allocUint64Out`, and `buildCsvOptionsBuffer` (which genuinely needs child pointer tables) are
  untouched. All three `database.ts` factories (`fromSchema`, `fromMigrations`, `open`) now bind
  a single `optionsBuf` value.
- **Documentation.** `bindings/js/CLAUDE.md` now documents `resolveLibrary`/`PROBE_SYMBOLS`
  (including why catch-and-annotate was rejected), the single-`Allocation` shape of
  `makeDefaultOptions` and its deliberate asymmetry with `buildCsvOptionsBuffer`, and why
  `OPTIONS_SIZE` stays a pinned literal validated once at load rather than re-read per allocation.
  The existing "must not move inside `openLibrary()` or any tier" paragraph now also names
  `resolveLibrary`.

## Task Commits

1. **Task 1: JS diagnoses a native library that loads but lacks the size accessors** -
   `d4b91f4` (feat)
2. **Task 2: Remove the options keepalive by making the buffer self-contained** - `bd3df03` (fix)
3. **Task 3: Record the loader diagnosis and the keepalive elimination in
   bindings/js/CLAUDE.md** - `d7300e3` (docs)

## Files Created/Modified

- `bindings/js/src/loader.ts` - `openLibrary`/`initLibrary` parameterized by symbol map;
  `PROBE_SYMBOLS`, `SIZEOF_ACCESSOR_NAMES`, `resolveLibrary(symbols)` added; `loadLibrary()` now
  calls `resolveLibrary(allSymbols)`.
- `bindings/js/src/ffi-helpers.ts` - `makeDefaultOptions` rewritten to a single self-contained
  `Allocation` with a variable-length tail; return type changed from
  `[Allocation, Allocation[]]` to `Allocation`.
- `bindings/js/src/database.ts` - all three `makeDefaultOptions` call sites (`fromSchema`,
  `fromMigrations`, `open`) updated to bind a single value, dropping `_keepalive`.
- `bindings/js/test/struct-sizes.test.ts` - added a `describe` block driving `resolveLibrary`
  against the real native library with a nonexistent symbol, asserting the version-skew message.
- `bindings/js/test/ffi-helpers.test.ts` - the four `makeDefaultOptions` tests updated to the new
  single-value return; added buffer-length assertions, a CString round-trip helper, and a
  non-ASCII round-trip test.
- `bindings/js/CLAUDE.md` - documents both remedies and both rejected alternatives.

## Exact Text Recorded (per plan's `<output>` instruction)

**Version-skew message** (from `resolveLibrary`, `loader.ts`):
```
Native library 'libquiver_c.dll' was found and loaded, but it does not export
quiver_database_options_sizeof, quiver_scalar_metadata_sizeof, quiver_group_metadata_sizeof,
quiver_csv_options_sizeof. This native library predates this release of the JS binding --
reinstall a matching native library.
```

**Buffer-length assertions that passed** (`ffi-helpers.test.ts`):
- `makeDefaultOptions()` → `alloc.buf.length === OPTIONS_SIZE` (24), both pointer slots `0n`.
- `makeDefaultOptions({ uiConfigDir: "/x" })` → `alloc.buf.length === OPTIONS_SIZE + 3` (27;
  `"/x"` + NUL), `readStringAt(alloc, OPTIONS_OFFSET_UI_CONFIG_DIR) === "/x"`.
- `makeDefaultOptions({ uiConfigDir: "/x", uiLocale: "es" })` → `alloc.buf.length ===
  OPTIONS_SIZE + 3 + 3` (30), both strings round-trip.
- `makeDefaultOptions({ uiConfigDir: "/café" })` → non-ASCII string round-trips exactly.
- `makeDefaultOptions({ uiConfigDir: "" })` → `alloc.buf.length === OPTIONS_SIZE` (24), no tail
  bytes reserved for the empty option.

## Decisions Made

See `key-decisions` in the frontmatter above. In summary: Gap 3 uses a probe (not
catch-and-annotate) to distinguish a stale native from an absent one; Gap 4 is fixed by
eliminating the second allocation entirely (not by hardening the keepalive), and
`buildCsvOptionsBuffer`'s own keepalive tuple is deliberately kept for its genuinely different
shape (a variable number of child pointer tables).

## Deviations from Plan

**1. [Rule 2 — clarification, not a code change] Second "genuine not-found" test omitted.**
- **Found during:** Task 1.
- **Issue:** The plan's acceptance criteria required one committed test driving `resolveLibrary`
  with a nonexistent symbol (done — see D1 above). I initially also wrote a second test intended
  to prove the "when no native is loadable at all, the original error still surfaces" truth from
  `must_haves`. On review, that second test could not actually exercise the genuine-not-found path
  without deleting the real native library out from under the rest of the suite (there is no
  loadable-directory parameter to redirect `resolveLibrary` at an empty search tree), so the test
  I had written was structurally incapable of failing — a vacuous assertion of exactly the kind
  the CLAUDE.md memory rule "no human-verification punts" and the phase's own "no vacuous tests"
  finding warn against.
- **Fix:** Removed the vacuous test and replaced it with a code-pointer comment explaining why the
  path is a direct code-read (`resolveLibrary`'s inner `catch` rethrows `e`, the original caught
  error, unchanged, only when the `PROBE_SYMBOLS` resolution also throws) rather than shipping a
  test that could not fail.
- **Files modified:** `bindings/js/test/struct-sizes.test.ts`.
- **Commit:** `d4b91f4` (part of Task 1 commit — the vacuous test was never committed separately).

**2. [Rule 2 — scope clarification] Acceptance criterion "grep -c 'keepalive' returns 0" cannot
be satisfied literally without an out-of-scope rename.**
- **Found during:** Task 2.
- **Issue:** After rewriting `makeDefaultOptions`, `grep -c 'keepalive' bindings/js/src/ffi-helpers.ts`
  returns 2, not 0. Both remaining occurrences are in `allocNativeStringArray` — a pre-existing,
  unrelated function (used by the group-column writers) whose return type is
  `{ table: Allocation; keepalive: Allocation[] }`. That function's `keepalive` field is genuinely
  needed (it holds native string allocations for an arbitrary-length array) and the plan's
  `<read_first>`/`<action>` sections never named it as in scope; renaming it would touch call
  sites in `csv.ts` and `group-columns.ts` for zero benefit to this plan's two gaps, violating the
  scope boundary ("Only auto-fix issues DIRECTLY caused by the current task's changes").
- **Fix:** Reworded my own new doc comment in `makeDefaultOptions` to avoid using the literal word
  "keepalive" (describing the eliminated pattern by its shape instead), so the only remaining
  occurrences of the word in the file are the two pre-existing, unrelated ones in
  `allocNativeStringArray`. `grep -c 'keepalive' bindings/js/src/ffi-helpers.ts` is 2, not 0 as the
  acceptance criterion states, but `makeDefaultOptions` itself has zero — the check as written did
  not anticipate the pre-existing unrelated usage.
- **Files modified:** `bindings/js/src/ffi-helpers.ts`.
- **Commit:** `bd3df03`.

---

**Total deviations:** 2 (both clarifications of test/grep scope, no functional code deviation).
**Impact on plan:** None on the two gaps closed. Both deviations narrow scope toward what the
plan's own design decisions (`<design_decisions_recorded_here>`) actually require, rather than
widening it.

## Issues Encountered

None beyond the two deviations above.

## Verification

- `cd bindings/js && bun test test/struct-sizes.test.ts` → **12 pass, 0 fail, 33 expect() calls**
  (was 11 pass before this plan; +1 test, the version-skew diagnosis).
- `cd bindings/js && bun test test/ffi-helpers.test.ts test/database-ui-options.test.ts` →
  **20 pass, 0 fail, 48 expect() calls**.
- Full suite: `PATH=...build/bin bun test test` (the same command `test.bat` runs) →
  **241 pass, 0 fail, 448 expect() calls across 23 files** (baseline in this plan's verification
  section was 231; prior plan 02-10 had already grown it to 239; this plan added 2 net new tests
  after the vacuous-test removal in Deviation 1).
- `cmake --build build --config Debug` → `ninja: no work to do` (no C++ changes; the four
  `*_sizeof` accessors this plan reads were already built by earlier plans in this phase).
- `bunx biome check` on all six touched files → 2 pre-existing CRLF-related formatter errors
  (confirmed identical via `git stash`/`biome check` on the pre-plan baseline — one on
  `ffi-helpers.ts`, one on `struct-sizes.test.ts`, both present before this plan's edits) plus 2
  pre-existing `useTemplate` info-level hints at lines 170/212 of `ffi-helpers.ts`, both in
  `toCString`/`allocNativeString`, untouched by this plan. No new lint findings from the added or
  changed code.
- Grep-verified acceptance criteria: `assertNativeStructSizes(` appears at its declaration and
  exactly one call site (inside `loadLibrary()`); `allSymbols` appears at its declaration, its
  `QuiverLib` type alias, and the single `resolveLibrary(allSymbols)` call; `new Uint8Array(8)` in
  `ffi-helpers.ts` is still exactly 2 (`allocPtrOut`, `allocUint64Out`, both byte-identical per
  `git diff`); `bindings/js/src/csv.ts` has zero diff for this plan.

## Known Stubs

None.

## Next Phase Readiness

Both JS-only gaps from `02-VERIFICATION.md` (SC3 refutation b, SC4) are closed. Plan 02-13 owns
the CHANGELOG entry for this gap-closure wave and was explicitly out of scope here.

## Self-Check: PASSED

- `bindings/js/src/loader.ts` — FOUND (`resolveLibrary`, `PROBE_SYMBOLS`, `SIZEOF_ACCESSOR_NAMES`
  present; `openLibrary`/`initLibrary` take a symbol-map parameter)
- `bindings/js/src/ffi-helpers.ts` — FOUND (`makeDefaultOptions` returns a single `Allocation`,
  no keepalive array)
- `bindings/js/src/database.ts` — FOUND (all three call sites bind a single `optionsBuf`)
- `bindings/js/test/struct-sizes.test.ts` — FOUND (12 tests, version-skew diagnosis test present)
- `bindings/js/test/ffi-helpers.test.ts` — FOUND (updated `makeDefaultOptions` tests, non-ASCII
  round-trip test present)
- `bindings/js/CLAUDE.md` — FOUND (`resolveLibrary`/`PROBE_SYMBOLS` documented, keepalive
  elimination documented)
- Commit `d4b91f4` — FOUND in `git log --oneline`
- Commit `bd3df03` — FOUND in `git log --oneline`
- Commit `d7300e3` — FOUND in `git log --oneline`

---
*Phase: 02-config-path-locale-and-struct-size-safety*
*Completed: 2026-09-19*
