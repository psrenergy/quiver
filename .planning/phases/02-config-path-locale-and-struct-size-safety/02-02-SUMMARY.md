---
phase: 02-config-path-locale-and-struct-size-safety
plan: 02
subsystem: bindings-js
tags: [ffi-abi, bun, struct-layout, i18n, ui-config]

requires:
  - phase: 02-config-path-locale-and-struct-size-safety
    provides: "quiver_database_options_t grown to 24 bytes (read_only@0/console_level@4/ui_config_dir@8/ui_locale@16), quiver_database_options_sizeof/quiver_scalar_metadata_sizeof/quiver_group_metadata_sizeof, quiver_database_has_ui_config -- the C symbol set and struct layout this plan binds against (02-01)"
provides:
  - "makeDefaultOptions() rebuilt on named offset constants (OPTIONS_OFFSET_*, OPTIONS_SIZE=24), returning [Allocation, Allocation[]] with all three database.ts call sites binding the keepalive in FFI-call scope"
  - "SCALAR_METADATA_SIZE/GROUP_METADATA_SIZE relocated from metadata.ts into ffi-helpers.ts (values unchanged), consumed by both metadata.ts and loader.ts"
  - "checkStructSize/assertNativeStructSizes load-time gate in loader.ts, wired into loadLibrary()'s memoized path, checking options/scalar-metadata/group-metadata in that order"
  - "DatabaseOptions.uiConfigDir/uiLocale, Database.hasUiConfig() -- the JS binding surface for OPT-01..04"
  - "bindings/js/test/ffi-helpers.test.ts and test/struct-sizes.test.ts -- the first direct tests of ffi-helpers.ts and the first test in any binding proving a wrong struct layout actually throws"
affects: [02-06, 02-07]

actuals:
  tokens: 7100
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns: ["named-offset-constant struct builder returning [Allocation, Allocation[]] keepalive tuple (copied from csv.ts's buildCsvOptionsBuffer)", "load-time native struct-size assertion inside a lazy memoized FFI loader, outside every swallowing try/catch tier"]

key-files:
  created:
    - bindings/js/test/ffi-helpers.test.ts
    - bindings/js/test/struct-sizes.test.ts
    - bindings/js/test/database-ui-options.test.ts
  modified:
    - bindings/js/src/ffi-helpers.ts
    - bindings/js/src/types.ts
    - bindings/js/src/metadata.ts
    - bindings/js/src/database.ts
    - bindings/js/src/loader.ts
    - bindings/js/src/introspection.ts
    - bindings/js/CLAUDE.md

key-decisions:
  - "hasUiConfig's implementation lands in introspection.ts (where isHealthy's real prototype body lives), not database.ts as the plan's action text literally said -- database.ts only carries `declare` lines for every method in the class, and introspection.ts is where isHealthy's own implementation already sits. Following the plan's own 'mirror isHealthy's shape' instruction meant following isHealthy to its actual file."
  - "Kept the plan's exact tuple/keepalive shape from buildCsvOptionsBuffer (csv.ts) rather than any alternative -- no second shape was considered, per the plan's explicit prohibition."

requirements-completed: [OPT-01, OPT-02, OPT-03, OPT-04, OPT-05, SAFE-02, SAFE-03]

coverage:
  - id: D1
    description: "makeDefaultOptions allocates exactly 24 bytes from named offset constants (0/4/8/16), never an inline literal"
    requirement: OPT-05
    verification:
      - kind: unit
        ref: "bindings/js/test/ffi-helpers.test.ts (offset/size constants test, 24-byte buffer test)"
        status: pass
    human_judgment: false
  - id: D2
    description: "makeDefaultOptions returns [Allocation, Allocation[]] and every database.ts call site binds the keepalive in FFI-call scope"
    requirement: OPT-05
    verification:
      - kind: unit
        ref: "bindings/js/test/ffi-helpers.test.ts (keepalive length assertions); grep -n 'makeDefaultOptions' bindings/js/src/database.ts shows three destructuring assignments"
        status: pass
    human_judgment: false
  - id: D3
    description: "allocPtrOut and allocUint64Out are provably unchanged at 8 bytes each"
    requirement: SAFE-02
    verification:
      - kind: unit
        ref: "bindings/js/test/ffi-helpers.test.ts#allocPtrOut/allocUint64Out tests"
        status: pass
    human_judgment: false
  - id: D4
    description: "Load-time assertion over all three structs (options 24, scalar metadata 56, group metadata 32), throw path actually exercised by test, gate placed in loadLibrary() only"
    requirement: SAFE-03
    verification:
      - kind: unit
        ref: "bindings/js/test/struct-sizes.test.ts (happy path + three failure-path assertions + off-by-one adjacency); grep -n assertNativeStructSizes bindings/js/src/loader.ts shows one definition and one call inside loadLibrary"
        status: pass
    human_judgment: false
  - id: D5
    description: "Database.open/fromSchema/fromMigrations accept uiConfigDir/uiLocale; hasUiConfig() answers; Spanish locale renders Spanish labels through the Bun FFI boundary"
    requirement: "OPT-01, OPT-02, OPT-03, OPT-04"
    verification:
      - kind: unit
        ref: "bindings/js/test/database-ui-options.test.ts (all four cases)"
        status: pass
    human_judgment: false

duration: ~25min
completed: 2026-09-19
status: complete
---

# Phase 2 Plan 2: JS Binding — Struct-Size Safety and UI Config Summary

**The JS options buffer grows 8 -> 24 bytes from named offset constants with live child-string pointers, gated by a load-time three-struct size assertion whose throw path is exercised by test, and a Spanish enum label proves the new `uiConfigDir`/`uiLocale` fields actually cross the Bun FFI boundary.**

## Performance

- **Duration:** ~25 min
- **Completed:** 2026-09-19
- **Tasks:** 3
- **Files modified:** 7 modified, 3 created

## Accomplishments

- `makeDefaultOptions()` allocates `new Uint8Array(OPTIONS_SIZE)` (24) and writes every field
  through named constants (`OPTIONS_OFFSET_READ_ONLY`/`_CONSOLE_LEVEL`/`_UI_CONFIG_DIR`/
  `_UI_LOCALE`) — no bare numeric literal anywhere in the function. It now returns
  `[Allocation, Allocation[]]`, copying `buildCsvOptionsBuffer`'s shape from `csv.ts` exactly; all
  three `database.ts` call sites (`fromSchema`, `fromMigrations`, `open`) destructure the tuple
  and bind the keepalive in the same scope as the native call, so a `uiConfigDir`/`uiLocale`
  string buffer cannot be collected between allocation and the FFI read.
- `allocPtrOut` and `allocUint64Out` are untouched — still `new Uint8Array(8)` each — and pinned
  by explicit length assertions in the new `ffi-helpers.test.ts`, this repo's first direct test
  of that module.
- `SCALAR_METADATA_SIZE` (56) and `GROUP_METADATA_SIZE` (32) relocated from `metadata.ts` into
  `ffi-helpers.ts` (values unchanged) so `loader.ts` can import all three struct-size constants
  without creating a `loader -> metadata -> database` import cycle.
- `loader.ts` gained four symbol-table entries (`quiver_database_options_sizeof`,
  `quiver_database_has_ui_config`, `quiver_scalar_metadata_sizeof`, `quiver_group_metadata_sizeof`)
  plus `checkStructSize` (pure, parameterized throw naming the struct and both numbers) and
  `assertNativeStructSizes` (checks options -> scalar metadata -> group metadata, short-circuiting
  on first mismatch). The gate runs exactly once, inside `loadLibrary()`'s memoized path — never
  inside `openLibrary()` or any of `initLibrary()`'s three swallowing `try`/`catch` tiers. Every
  `*_sizeof()` return is wrapped in `Number(...)` before comparison (Bun returns a `bigint` for a
  `usize` return; `24n === 24` is `false`).
- `Database.hasUiConfig()` mirrors `isHealthy`'s exact shape (4-byte int out-param via
  `quiver_database_has_ui_config`). `DatabaseOptions` gained `uiConfigDir?`/`uiLocale?`.
- New `test/database-ui-options.test.ts` opens `tests/schemas/ui/foresight_like` with the
  database file in its own temp directory (no `ui/` sibling) and an explicit `uiConfigDir`
  pointing at the fixture's `ui/` tree, proving `es` locale renders `Ingenuo Estacional` /
  `Tendencia Lineal Local` and not `Seasonal Naïve`, the default locale renders the inverse, and
  a nonexistent `uiConfigDir` degrades `hasUiConfig()` to `false` without throwing.
- `bun test test` is fully green: 231 tests across 23 files (up from 218/21 baseline before this
  plan; +9 ffi-helpers, +9 struct-sizes, +4 database-ui-options... reconciled to +13 net new test
  files across three new suites).

## Task Commits

1. **Task 1: A 24-byte options buffer whose string pointers survive the call** - `ab0cd31` (feat)
2. **Task 2: Load-time struct-size gate, with its throw path actually exercised** - `abcecb2` (feat)
3. **Task 3: The JS surface — uiConfigDir, uiLocale and hasUiConfig, proven with a Spanish label** - `5ed7ee5` (feat)

## Files Created/Modified

- `bindings/js/src/ffi-helpers.ts` — `OPTIONS_OFFSET_*` x4, `OPTIONS_SIZE`, relocated
  `SCALAR_METADATA_SIZE`/`GROUP_METADATA_SIZE`, `makeDefaultOptions` rewritten
- `bindings/js/src/types.ts` — `DatabaseOptions.uiConfigDir`/`uiLocale`
- `bindings/js/src/metadata.ts` — imports the two relocated size constants instead of defining them
- `bindings/js/src/database.ts` — three `makeDefaultOptions` call sites destructure the tuple;
  `hasUiConfig` declared on the class
- `bindings/js/src/loader.ts` — four new symbol entries, `checkStructSize`,
  `assertNativeStructSizes`, gate wired into `loadLibrary()`
- `bindings/js/src/introspection.ts` — `Database.prototype.hasUiConfig` implementation
- `bindings/js/test/ffi-helpers.test.ts` (new) — 9 cases
- `bindings/js/test/struct-sizes.test.ts` (new) — 9 cases (happy path + failure path + adjacency)
- `bindings/js/test/database-ui-options.test.ts` (new) — 4 cases
- `bindings/js/CLAUDE.md` — documents the 24-byte layout, keepalive rule, relocated constants,
  load-time gate, and the deliberately-deferred `csv.ts` 56-byte hazard

## Decisions Made

- `hasUiConfig`'s implementation lands in `introspection.ts`, not `database.ts` as the plan's
  action text literally said — `database.ts` carries only `declare` lines for the whole class
  surface; every other introspection method (`isHealthy`, `currentVersion`, `path`, `describe`,
  ...) has its real prototype body in `introspection.ts`. Following the plan's own instruction to
  "mirror `isHealthy`'s existing shape exactly" meant putting the implementation where `isHealthy`
  itself lives.
- No new buffer shape was invented anywhere — `makeDefaultOptions` and `hasUiConfig` both copy an
  existing in-repo pattern (`buildCsvOptionsBuffer`'s tuple, `isHealthy`'s int out-param) exactly,
  per the plan's explicit "do not invent a new shape" instructions.

## Deviations from Plan

### Auto-fixed / structural

**1. [Rule 3 - blocking] `hasUiConfig` implementation file corrected from `database.ts` to `introspection.ts`**
- **Found during:** Task 3
- **Issue:** The plan's action text said "Add `hasUiConfig(): boolean` to the `Database` class in
  `bindings/js/src/database.ts`", but `database.ts` contains only `declare` type signatures for
  every method — no method bodies. `isHealthy`, the shape this task was told to mirror, has its
  real implementation in `introspection.ts`.
- **Fix:** Added the `declare hasUiConfig: () => boolean;` line to `database.ts` (matching every
  other declared method) and the actual `Database.prototype.hasUiConfig = function (...) {...}`
  implementation to `introspection.ts`, directly below `isHealthy`.
- **Files modified:** `bindings/js/src/database.ts`, `bindings/js/src/introspection.ts` (the
  latter not listed in the plan's `files_modified`).
- **Commit:** `5ed7ee5` (Task 3 commit).

No other deviations — every other instruction (named offset constants, tuple/keepalive shape,
constant relocation, gate placement, `Number(...)` wrapping, check order, fixture usage) was
followed exactly as specified.

## Known limitation: pre-existing lint/format debt, not touched

`bunx biome check` on the touched files reports formatter diffs and two `noBannedTypes` warnings.
Verified these are **pre-existing and environment-caused, not introduced by this plan**:
- The formatter diffs are entirely a CRLF-vs-LF artifact of this Windows checkout's
  `core.autocrlf=true` (confirmed by running the same check against an untouched file, `csv.ts`,
  which reports an identical formatter diff). `.gitattributes` does not list `.ts` in its LF-forced
  set, so this is a pre-existing repo/environment gap, not a regression.
- The two `lint/complexity/noBannedTypes` warnings in `metadata.ts` (lines 146/154, the
  `listMetadata` helper's `Record<string, Function>` casts) are on lines I did not touch — only
  the file's import block and the removal of the two now-relocated constants changed.
- `bunx biome lint` (rule-only, no formatter) on every file this plan touched or created reports
  **zero new errors** — only those same two pre-existing warnings, both on untouched lines.
- Per root `CLAUDE.md`'s "Do Not Fix": "Drive-by fixing pre-existing lint debt in untouched JS
  files" — left alone.

## Known Stubs

None. Every behavior in `<behavior>`/`<acceptance_criteria>` across all three tasks is backed by
a real implementation and a passing test; no placeholder or empty-data path was introduced.

## Threat Flags

None beyond what the plan's own `<threat_model>` already named and mitigated (T-02-03, T-02-05,
T-02-06, T-02-07) — no new network endpoint, auth path, or schema surface was introduced.

## Issues Encountered

None. The precondition check for both Task 1 and Task 2
(`quiver_c_tests.exe --gtest_filter=DatabaseCApiOptions.SizeofAccessorsMatchNativeLayout`) passed
against the already-rebuilt `build/bin/libquiver_c.dll` on the first try, so no C++ rebuild was
needed.

## Next Phase Readiness

- `bindings/js/` now agrees with the 24-byte `quiver_database_options_t` header frozen by 02-01,
  with a load-time gate that will loudly fail if a future header edit drifts from this binding's
  hardcoded constants.
- `csv.ts`'s `quiver_csv_options_t` (56 bytes, no accessor, no assertion) remains a real, recorded,
  deliberately out-of-scope fourth instance of the same hazard class for a future phase.
- No blockers for 02-06/02-07 (whatever consumes the JS binding's UI config surface next).

## Self-Check: PASSED

- All new files confirmed present on disk: `bindings/js/test/ffi-helpers.test.ts`,
  `bindings/js/test/struct-sizes.test.ts`, `bindings/js/test/database-ui-options.test.ts`.
- All three commit hashes (`ab0cd31`, `abcecb2`, `5ed7ee5`) confirmed present in `git log`.
- `cd bindings/js && bun test test` re-run clean: 231 pass, 0 fail.

---
*Phase: 02-config-path-locale-and-struct-size-safety*
*Completed: 2026-09-19*
