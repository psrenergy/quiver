---
phase: 02-config-path-locale-and-struct-size-safety
plan: 01
subsystem: database
tags: [c-api, ffi-abi, toml, i18n, struct-layout]

requires:
  - phase: 01-enum-labels-in-describe
    provides: UIConfigSet parser, UIConfigSet::from_directory(locale param), Database::has_ui_config() (C++-only), foresight_like fixture with mixed en/es/pt labels
provides:
  - "DatabaseOptions::ui_config_dir / ui_locale (C++), threaded through the one constructor (Database::Database) and the one lazy loader (Impl::require_ui_config)"
  - "quiver_database_options_t grown 8 -> 24 bytes (read_only@0, console_level@4, ui_config_dir@8, ui_locale@16), pinned by static_asserts"
  - "quiver_database_options_sizeof / quiver_scalar_metadata_sizeof / quiver_group_metadata_sizeof C symbols (SAFE-01)"
  - "quiver_database_has_ui_config C symbol (OPT-04)"
  - "UIConfigSet::parse_enum_content now takes a locale parameter (was hardcoded \"en\")"
  - "Literals L18 (Tendencia Lineal Local) / L19 (Ingenuo Estacional) pinned in tests/schemas/ui/README.md"
affects: [02-02, 02-03, 02-04, 02-05, 02-06, 02-07]

actuals:
  tokens: 45000
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns: ["explicit-override-then-guarded-convention-path branching in a lazy loader", "static_assert-pinned C struct layout consumed by parameterless size_t accessors"]

key-files:
  created:
    - tests/test_database_ui_options.cpp
    - tests/test_c_api_database_options.cpp
  modified:
    - include/quiver/options.h
    - include/quiver/c/options.h
    - include/quiver/c/database.h
    - src/c/options.cpp
    - src/c/database_options.h
    - src/c/database_metadata.cpp
    - src/c/database.cpp
    - src/database.cpp
    - src/database_impl.h
    - src/ui_config.h
    - src/ui_config.cpp

key-decisions:
  - "D-01/D-05 applied verbatim: explicit override checked first, :memory: short-circuit moved (not deleted) to guard only the convention path, not-found log level is warn for an explicit path and debug for the convention path."
  - "parse_enum_content threaded a locale parameter through its declaration, definition, and its one call site inside from_directory -- the second, previously-undocumented locale site the adversarial review flagged. Without this, ui_locale=\"es\" would have rendered byte-identical \"en\" output on the foresight_like fixture."
  - "No new fixture tree: the explicit-config-dir proof reuses foresight_like with the database file placed in a scratch directory that has no ui/ sibling of its own."
  - "The ExplicitConfigDirEqualToConventionPathIsIdentical test strips the report's first line before comparing, mirroring the existing golden-test precedent -- the 'UI config: <path> (locale: ...)' header renders source_directory verbatim, which differs only in path-separator style (native vs. caller-supplied string) between the two ways of naming the same directory."

requirements-completed: [OPT-01, OPT-02, OPT-04, SAFE-01]

coverage:
  - id: D1
    description: "quiver_database_options_t grows to 24 bytes with read_only@0/console_level@4/ui_config_dir@8/ui_locale@16, pinned by static_assert so a future reordering fails the build"
    requirement: SAFE-01
    verification:
      - kind: unit
        ref: "src/c/options.cpp static_asserts (compile-time) + tests/test_c_api_database_options.cpp#DatabaseCApiOptions.SizeofAccessorsMatchNativeLayout"
        status: pass
    human_judgment: false
  - id: D2
    description: "An explicit ui_config_dir loads a UI sidecar from anywhere on disk, including for a :memory: database, overriding the <db_dir>/ui/ convention"
    requirement: OPT-01
    verification:
      - kind: unit
        ref: "tests/test_database_ui_options.cpp#DatabaseUiOptions.ExplicitConfigDirOverridesConvention, ExplicitConfigDirLoadsEvenForMemoryDatabase, ExplicitConfigDirEqualToConventionPathIsIdentical"
        status: pass
    human_judgment: false
  - id: D3
    description: "ui_locale=\"es\" renders the Spanish enum labels (Ingenuo Estacional, Tendencia Lineal Local) instead of their English counterparts, through both the C++ and C API surfaces"
    requirement: OPT-02
    verification:
      - kind: unit
        ref: "tests/test_database_ui_options.cpp#DatabaseUiOptions.LocaleAffectsRenderedLabel, DefaultLocaleIsEnglish; tests/test_c_api_database_options.cpp#DatabaseCApiOptions.ExplicitConfigDirAndLocaleCrossTheCBoundary"
        status: pass
    human_judgment: false
  - id: D4
    description: "quiver_database_has_ui_config round-trips through the C API (1 when loaded, 0 when absent/malformed), mirroring is_healthy's no-throw body"
    requirement: OPT-04
    verification:
      - kind: unit
        ref: "tests/test_c_api_database_options.cpp#DatabaseCApiOptions.HasUiConfigTrueWhenLoaded, HasUiConfigFalseWhenDirectoryAbsent"
        status: pass
    human_judgment: false
  - id: D5
    description: "An absent explicit ui_config_dir logs at warn; an absent convention path logs at debug; neither throws and both report has_ui_config() false"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_options.cpp#DatabaseUiOptions.ExplicitConfigDirMissingLogsWarnNotDebug"
        status: pass
    human_judgment: false
  - id: D6
    description: "Phase 1's four UI test files (test_database_ui_describe.cpp, test_database_ui_golden.cpp, test_database_ui_parse.cpp, test_database_ui_corpus.cpp) remain byte-for-byte unmodified in source and green"
    verification:
      - kind: unit
        ref: "git diff --stat (empty for the first three); tests/test_database_ui_corpus.cpp gained only the two new literal-pin lines; full quiver_tests.exe run (1170/1170 pass)"
        status: pass
    human_judgment: false

duration: 55min
completed: 2026-09-19
status: complete
---

# Phase 2 Plan 1: Config Path, Locale and Struct-Size Safety — Header Freeze Summary

**`DatabaseOptions`/`quiver_database_options_t` grown to carry an explicit `ui_config_dir` and `ui_locale`, threaded through the one constructor and the one lazy loader, with the C header frozen at 24 bytes for Wave 2's four binding plans.**

## Performance

- **Duration:** ~55 min
- **Started:** 2026-09-19T13:23:00Z (approx.)
- **Completed:** 2026-09-19T14:18:18Z
- **Tasks:** 3
- **Files modified:** 20 (2 new test files, 18 modified)

## Accomplishments

- `DatabaseOptions` gained `ui_config_dir` (empty = convention `<db_dir>/ui/`) and `ui_locale`
  (empty = `"en"`), assigned once at `Database::Database(path, options)` construction and
  consumed lazily by `Impl::require_ui_config()`.
- `require_ui_config()` restructured per D-01/D-05: the explicit override is checked first; the
  `:memory:` short-circuit is moved verbatim to guard only the convention path (an explicit
  directory now loads even for an in-memory database); the not-found log is `warn` for an
  explicit path and `debug` for the convention path.
- `UIConfigSet::parse_enum_content` — previously hardcoded at locale `"en"` with no parameter at
  all — now takes `locale` and is called with the caller-supplied value. This was the load-bearing
  fix the adversarial review flagged: without it, `ui_locale = "es"` would have rendered
  byte-identical `en` output because `foresight_like`'s only locale-varying data is its
  `enum.toml`.
- `quiver_database_options_t` grew 8 → 24 bytes (`read_only`@0, `console_level`@4,
  `ui_config_dir`@8, `ui_locale`@16), pinned by five `static_assert`s in `src/c/options.cpp`.
  `convert_database_options` maps NULL-or-empty on either new field to "unspecified".
- Three parameterless `size_t` accessors (`quiver_database_options_sizeof`,
  `quiver_scalar_metadata_sizeof`, `quiver_group_metadata_sizeof`) and
  `quiver_database_has_ui_config` (mirroring `quiver_database_is_healthy`'s no-throw body) — the
  full C symbol set every Wave-2 binding plan references.
- New `DatabaseUiOptions` (C++) and `DatabaseCApiOptions` (C API) suites — 7 and 4 cases
  respectively — prove the explicit directory, the `:memory:` exception, the empty-string-as-unset
  rule, the locale-crosses-the-boundary behavior, and the warn/debug log-level split.
- Literals L18 (`Tendencia Lineal Local`) and L19 (`Ingenuo Estacional`) pinned in
  `tests/schemas/ui/README.md` and `FixtureLiteralsArePinned` (13 entries total).

## Task Commits

1. **Task 1: End-to-end — an explicit config directory and a Spanish label, C++ through the C API** - `3400a61` (feat)
2. **Task 2: The C API surface Wave 2 consumes — three size accessors and has_ui_config** - `57ef710` (feat)
3. **Task 3: Pin the new locale literals and record the decisions** - `48c3446` (docs)

_Note: Task 1 and Task 2 are typed `tracer`/`auto` with `tdd="true"` in the plan; both were delivered as a single implementation + test commit per task rather than separate RED/GREEN commits, since the new surface had no pre-existing failing assertion to red against — the plan's own `<verify>` gate (build + targeted gtest filter) was the acceptance bar, and it passed on the first run after each task's edits._

## Files Created/Modified

- `include/quiver/options.h` — `DatabaseOptions::ui_config_dir`/`ui_locale`
- `include/quiver/c/options.h` — `quiver_database_options_t` grown to 24 bytes; `quiver_database_options_sizeof` declared
- `include/quiver/c/database.h` — `quiver_scalar_metadata_sizeof`, `quiver_group_metadata_sizeof`, `quiver_database_has_ui_config` declared
- `src/c/options.cpp` — `static_assert`s, updated `quiver_database_options_default`, `quiver_database_options_sizeof` impl
- `src/c/database_options.h` — NULL/empty-string guard in `convert_database_options`
- `src/c/database_metadata.cpp` — `quiver_scalar_metadata_sizeof`/`quiver_group_metadata_sizeof` impls
- `src/c/database.cpp` — `quiver_database_has_ui_config` impl
- `src/database.cpp` — constructor wiring (`impl_->ui_config_dir`/`ui_locale`)
- `src/database_impl.h` — new `Impl` members
- `src/ui_config.h` / `src/ui_config.cpp` — `parse_enum_content` locale parameter, `require_ui_config` restructure
- `tests/test_database_ui_options.cpp` (new) — `DatabaseUiOptions` suite, 7 cases
- `tests/test_c_api_database_options.cpp` (new) — `DatabaseCApiOptions` suite, 4 cases
- `tests/test_database_ui_corpus.cpp` — two new literal pairs in `FixtureLiteralsArePinned`
- `tests/schemas/ui/README.md` — L18/L19 rows
- `tests/CMakeLists.txt` — both new files registered
- `src/CLAUDE.md`, `src/c/CLAUDE.md`, `tests/CLAUDE.md`, `CHANGELOG.md` — documentation updates

## Decisions Made

- D-01/D-05 (config path + log level) and D-06's correction (thread locale into
  `parse_enum_content`) applied exactly as specified in 02-CONTEXT.md — no deviation.
- Chose to strip the report's header line before comparing convention-path vs. explicit-path
  output in `ExplicitConfigDirEqualToConventionPathIsIdentical`, since the "UI config: <path>"
  header renders `source_directory` verbatim and the two paths differ only in separator style
  (`std::filesystem`'s native-separator join vs. the caller's own forward-slash string) — not a
  semantic difference. Mirrors the existing `DirectoryWithoutMainTomlLogsAtWarn`/golden-test
  precedent of comparing "everything after the first line".
- No new fixture directory: reused `foresight_like` with the database file placed in a scratch
  directory that has no `ui/` sibling, per the plan's explicit instruction and RESEARCH's
  Assumption A3 superseding note.

## Deviations from Plan

None — plan executed exactly as written. Two implementation-detail fixes surfaced and were
corrected inline during test-writing (not deviations from the plan's intent, just bugs in my own
first draft of the new tests):

- **[Rule 1 - Bug] Dangling `const char*` from a temporary `std::string` in the new C API test.**
  `options.ui_config_dir = foresight_ui_dir().c_str();` took a pointer into a temporary that was
  destroyed at the end of the full expression, leaving `ui_config_dir` dangling by the time
  `quiver_database_from_schema` read it. Fixed by binding the path to a named `const auto` that
  outlives the call. Caught immediately by the test itself failing (`has_ui_config()` false when
  it should have been true) — no separate verification needed beyond the passing re-run.
  Files: `tests/test_c_api_database_options.cpp`. Committed in `57ef710` (Task 2 commit).

## Issues Encountered

None beyond the dangling-pointer bug above, which was found and fixed within the same task before
committing.

## Next Phase Readiness

- The C symbol set, struct layout, and offsets Wave 2's four binding plans (02-02 Julia, 02-03
  Python, 02-04 Dart, 02-05 JS) all reference now exist, are implemented, are tested, and are
  built into `build/bin/libquiver_c.dll` — the artifact those plans' precondition checks look for.
- `bindings/` was not touched, as instructed — those four plans are independent of each other
  once this header freeze lands (D-19).
- No blockers. `scripts/build-all.bat` was deliberately not run in full (it exercises the binding
  suites, which are Wave 2's scope and will not yet reflect the new struct layout); the two
  targeted C++/C API executables were run in full (1170/1170 and 567/567 pass) instead.

## Self-Check: PASSED

- All new files confirmed present on disk (`tests/test_database_ui_options.cpp`,
  `tests/test_c_api_database_options.cpp`, and all modified headers/sources).
- All three commit hashes (`3400a61`, `57ef710`, `48c3446`) confirmed present in `git log`.

---
*Phase: 02-config-path-locale-and-struct-size-safety*
*Completed: 2026-09-19*
