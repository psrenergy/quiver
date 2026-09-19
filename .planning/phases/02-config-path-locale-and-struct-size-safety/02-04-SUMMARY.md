---
phase: 02-config-path-locale-and-struct-size-safety
plan: 04
subsystem: dart-binding
tags: [dart, ffi, struct-layout, i18n, config-path]

requires:
  - phase: 02-config-path-locale-and-struct-size-safety
    plan: 01
    provides: "quiver_database_options_t grown to 24 bytes, quiver_database_options_sizeof / quiver_scalar_metadata_sizeof / quiver_group_metadata_sizeof, quiver_database_has_ui_config -- the C symbol set this plan binds"
provides:
  - "Dart quiver_database_options_t struct hand-edited to 24 bytes (ui_config_dir@8, ui_locale@16), plus four hand-added symbol bindings (three *_sizeof accessors, quiver_database_has_ui_config)"
  - "Database.open/fromSchema/fromMigrations gain optional named uiConfigDir/uiLocale, threaded through _makeOptions with no keepalive mechanism (Arena already owns the allocation)"
  - "Database.hasUiConfig() mirroring isHealthy()'s no-throw shape"
  - "library_loader.dart's bindings getter gates on assertNativeStructSizes, actively calling all three *_sizeof accessors once per isolate, throwing a StateError naming the struct and both numbers on mismatch"
  - "test.bat/test.sh clear .dart_tool/hooks_runner and .dart_tool/lib before every dart test run"
affects: []

actuals:
  tokens: 5900
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns: ["hand-edited ffigen output with an explicit anti-regen note per addition", "load-time layout gate memoized separately from the bindings cache, guarding against lazy late-final symbol resolution"]

key-files:
  created:
    - bindings/dart/test/database_ui_options_test.dart
    - bindings/dart/test/struct_sizes_test.dart
  modified:
    - bindings/dart/lib/src/ffi/bindings.dart
    - bindings/dart/lib/src/database.dart
    - bindings/dart/lib/src/ffi/library_loader.dart
    - bindings/dart/test/test.bat
    - bindings/dart/test/test.sh
    - bindings/dart/CLAUDE.md

key-decisions:
  - "checkStructSize/assertNativeStructSizes are memoized by their own boolean flag (_structSizesChecked), separate from _cachedBindings, so the three native calls run exactly once per isolate rather than on every single `bindings` access -- satisfying the plan's 'once per isolate' concurrency note while still calling assertNativeStructSizes immediately after `_cachedBindings ??= ...` as instructed."
  - "test.bat's cache-clearing lines were written with a raw `printf` producing literal CRLF bytes and verified with `xxd`/`cat -A` before editing, rather than risking an editor or unix tool silently normalizing the file to LF."
  - "database_ui_options_test.dart places the database file in a fresh Directory.systemTemp.createTempSync() directory (no ui/ sibling) so the explicit uiConfigDir proof cannot be satisfied by the convention path accidentally succeeding."

requirements-completed: [OPT-01, OPT-02, OPT-03, OPT-04, OPT-06, SAFE-02, SAFE-03]

coverage:
  - id: D1
    description: "sizeOf<quiver_database_options_t>() is 24, and the hand-edited struct's field order (ui_config_dir before ui_locale) matches the native header"
    requirement: OPT-06
    verification:
      - kind: unit
        ref: "test/database_ui_options_test.dart#options struct is 24 bytes"
        status: pass
    human_judgment: false
  - id: D2
    description: "Database.open/fromSchema accept uiConfigDir/uiLocale; an explicit uiConfigDir loads a sidecar from a directory with no ui/ sibling of its own"
    requirement: OPT-01, OPT-03
    verification:
      - kind: unit
        ref: "test/database_ui_options_test.dart#explicit uiConfigDir loads a config not beside the database"
        status: pass
    human_judgment: false
  - id: D3
    description: "uiLocale='es' renders Ingenuo Estacional/Tendencia Lineal Local and not the English forms; the default locale renders the inverse"
    requirement: OPT-02
    verification:
      - kind: unit
        ref: "test/database_ui_options_test.dart#spanish locale renders spanish labels, #default locale renders english labels"
        status: pass
    human_judgment: false
  - id: D4
    description: "hasUiConfig() is false for a missing config directory and never throws"
    requirement: OPT-04
    verification:
      - kind: unit
        ref: "test/database_ui_options_test.dart#missing uiConfigDir degrades without throwing"
        status: pass
    human_judgment: false
  - id: D5
    description: "The load-time gate actively calls all three *_sizeof accessors (not just references their late-final pointer fields) and throws a StateError naming both numbers on a deliberately wrong expected value, for all three structs"
    requirement: SAFE-02, SAFE-03
    verification:
      - kind: unit
        ref: "test/struct_sizes_test.dart#happy path, #checkStructSize failure path (options/scalar/group)"
        status: pass
    human_judgment: false
  - id: D6
    description: "test.bat/test.sh clear the native-assets cache before every run, and a second consecutive run genuinely rebuilds the native library (not a skipped/cached build)"
    verification:
      - kind: unit
        ref: "manual: libquiver_c.dll timestamp 2026-09-19T11:55:41 (first run) vs 2026-09-19T12:07:03 (second run), 434/434 Dart tests passing both times"
        status: pass
    human_judgment: false

duration: ~50min
completed: 2026-09-19
status: complete
---

# Phase 2 Plan 4: Dart Binding — Config Path, Locale, and Struct-Size Safety Summary

**Hand-edited the 24-byte options struct into `bindings.dart` (never regenerated), threaded `uiConfigDir`/`uiLocale` through all three factories plus a new `hasUiConfig()`, and installed a load-time struct-size gate that actively calls three native accessors — closing the exact "constructing bindings proves nothing" trap Dart's lazy symbol resolution sets, and making `test.bat`/`test.sh` incapable of silently passing against a pre-phase native build.**

## Performance

- **Duration:** ~50 min
- **Completed:** 2026-09-19
- **Tasks:** 3
- **Files touched:** 8 (2 new test files, 6 modified)

## Accomplishments

- `quiver_database_options_t` in `bindings.dart` hand-edited to 24 bytes: `ui_config_dir` then
  `ui_locale` appended after `console_level`, in that order (order is layout — verified with a
  grep that `ui_config_dir` appears before `ui_locale` in source). Four new binding blocks added
  in the file's existing three-part `late final` shape: `quiver_database_options_sizeof`,
  `quiver_scalar_metadata_sizeof`, `quiver_group_metadata_sizeof`, and
  `quiver_database_has_ui_config`. No ffigen run — `grep -c 'abstract class quiver_log_level_t'`
  stays 1, and the diff to `bindings.dart` is 56 lines, confined to the options struct and the
  four added blocks.
- `Database._makeOptions` extended with `String? uiConfigDir, String? uiLocale`, writing
  `toNativeUtf8(allocator: arena).cast()` for a non-empty value and `nullptr` otherwise — no
  keepalive mechanism needed, since the `Arena` already owns the allocation until
  `arena.releaseAll()` runs in the caller's `finally`, strictly after the native call reads it.
  All three call sites (`fromSchema`, `fromMigrations`, `open`) gained the two named parameters
  and forward them. `hasUiConfig()` added, mirroring `isHealthy()` exactly.
- `library_loader.dart` gained `checkStructSize` (pure, parameterized, throws `StateError` naming
  the struct and both numbers) and `assertNativeStructSizes` (runs the three checks in fixed
  order options/scalar/group, short-circuiting on the first mismatch). Called from the `bindings`
  getter immediately after `_cachedBindings ??= QuiverDatabaseBindings(library)`, memoized once
  per isolate via a separate `_structSizesChecked` flag — the call is a genuine invocation of
  each `*_sizeof` accessor, not a reference to its `late final` pointer field, which is the only
  thing that can catch a version-skewed native library given Dart's lazy symbol resolution.
- `test.bat` (CRLF preserved byte-for-byte, verified with `xxd`/`cat -A` before and after) and
  `test.sh` now remove `.dart_tool/hooks_runner` and `.dart_tool/lib` before `dart test`. Verified
  the DLL genuinely rebuilds on two consecutive runs (`11:55:41` then `12:07:03`), with the full
  suite (434/434) passing both times.
- Two new test files: `database_ui_options_test.dart` (5 cases: 24-byte struct, explicit
  `uiConfigDir` from a directory with no `ui/` sibling, Spanish locale, default locale,
  missing-directory degradation) and `struct_sizes_test.dart` (7 cases: the happy path against
  the live native library, plus the failure path for all three structs — a wrong expected value
  actually throws, which is the criterion no binding had a test for before this phase).
- `bindings/dart/CLAUDE.md` updated: the hand-edited struct and its four new symbols, the
  load-time gate's active-call requirement and its separate memoization, the cache-clearing
  rationale (and its CRLF-editing caution), and the Arena note contrasting with JS/Python's
  explicit keepalive requirement.

## Task Commits

1. **Task 1: Hand-edit the bindings, thread the options, read a Spanish label** — `0c2d064` (feat)
2. **Task 2: Load-time gate that calls the accessor, because Dart resolves symbols lazily** — `740be4d` (feat)
3. **Task 3: Stop the suite from testing a stale native layout, and record it** — `cc62f83` (docs)

## Files Created/Modified

- `bindings/dart/lib/src/ffi/bindings.dart` — struct grown to 24 bytes; four hand-added binding blocks
- `bindings/dart/lib/src/database.dart` — `_makeOptions` extended, three factories gain named params, `hasUiConfig()` added
- `bindings/dart/lib/src/ffi/library_loader.dart` — `checkStructSize`, `assertNativeStructSizes`, gated `bindings` getter
- `bindings/dart/test/database_ui_options_test.dart` (new) — 5 cases
- `bindings/dart/test/struct_sizes_test.dart` (new) — 7 cases
- `bindings/dart/test/test.bat` / `test.sh` — cache clearing before `dart test`
- `bindings/dart/CLAUDE.md` — documents all of the above

## Decisions Made

- `_structSizesChecked` as a separate memoization flag, rather than folding the check into the
  `_cachedBindings == null` branch, keeps the call site textually "immediately after
  `_cachedBindings ??= ...`" as the plan specified, while still running the three native calls
  exactly once per isolate rather than on every `bindings` access.
- `test.bat`'s two new lines were produced with a raw `printf` writing literal `\r\n` bytes,
  verified with `xxd` before and `cat -A` after, rather than trusting an editor or the `Edit` tool
  to preserve CRLF on a file unix tooling is known to silently rewrite.
- The explicit-`uiConfigDir` test opens the database file in a fresh
  `Directory.systemTemp.createTempSync()` directory rather than reusing a fixture directory that
  already has its own `ui/` sibling — otherwise the convention path could satisfy the assertion
  by accident, proving nothing about the explicit override.

## Deviations from Plan

None — plan executed exactly as written. No bugs, no blocking issues, no architectural questions
surfaced.

## Issues Encountered

None. The precondition check (`quiver_c_tests.exe --gtest_filter='DatabaseCApiOptions.SizeofAccessorsMatchNativeLayout'`)
passed on the first try, confirming 02-01's native rebuild was already in place before this plan
started.

## Next Phase Readiness

- Dart's binding surface for this phase is complete: 24-byte options struct, `uiConfigDir`/
  `uiLocale` on all three factories, `hasUiConfig()`, and a load-time gate over all three
  ABI-frozen structs with a tested failure path.
- No blockers for 02-05 (Julia) or any other binding — this plan touched only `bindings/dart/`.
- `.dart_tool/hooks_runner/` and `.dart_tool/lib/` are left in a rebuilt state (from the second
  verification run) reflecting the current native library; a future ABI change will be caught
  automatically by `test.bat`/`test.sh`'s cache clearing rather than requiring a manual step.

## Self-Check: PASSED

- All 8 files confirmed present on disk.
- All three commit hashes (`0c2d064`, `740be4d`, `cc62f83`) confirmed present in `git log`.
- Full Dart suite (434/434) confirmed passing on two consecutive `test.bat` runs, with a
  genuinely rebuilt native library each time.

---
*Phase: 02-config-path-locale-and-struct-size-safety*
*Completed: 2026-09-19*
