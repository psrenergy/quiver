---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_phase: 02
current_phase_name: config-path-locale-and-struct-size-safety
status: executing
stopped_at: Completed 02-10-PLAN.md
last_updated: "2026-09-19T22:53:17.716Z"
last_activity: 2026-09-19
last_activity_desc: Phase 01 execution started
progress:
  total_phases: 2
  completed_phases: 1
  total_plans: 19
  completed_plans: 16
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-09-17)

**Core value:** An agent calling Quiver's `describe` on a PSR model database sees what an INTEGER column actually means — `values {0: 8 (Disabled), 1: 4 (Enabled)}`, not bare codes.
**Current focus:** Phase 02 — config-path-locale-and-struct-size-safety

## Current Position

Phase: 02 (config-path-locale-and-struct-size-safety) — EXECUTING
Plan: 4 of 13
Status: Ready to execute
Last activity: 2026-09-19 — Phase 02 execution started

Progress: [████████░░] 84%

## Performance Metrics

**Velocity:**

- Total plans completed: 6
- Average duration: —
- Total execution time: —

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1 | 6 | - | - |

**Recent Trend:**

- Last 5 plans: —
- Trend: —

*Updated after each plan completion*
**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 01 P01 | 40min | 3 tasks | 25 files |
| Phase 01 P02 | 10min | 2 tasks | 37 files |
| Phase 01 P03 | 35min | 3 tasks | 2 files |
| Phase 01 P04 | 45min | 2 tasks | 5 files |
| Phase 01 P05 | 25min | 2 tasks | 3 files |
| Phase 01 P06 | 35min | 3 tasks | 8 files |
| Phase 02 P01 | 55min | 3 tasks | 20 files |
| Phase 02 P03 | 16min | 3 tasks | 6 files |
| Phase 02 P02 | 25min | 3 tasks | 10 files |
| Phase 02 P05 | 30min | 3 tasks | 6 files |
| Phase 02 P04 | 50min | 3 tasks | 8 files |
| Phase 02 P07 | 15min | 3 tasks | 1 files |
| Phase 02 P08 | 24min | 3 tasks | 8 files |
| Phase 02 P09 | 55min | 3 tasks | 7 files |
| Phase 02 P10 | unspecified | 3 tasks | 5 files |

## Accumulated Context

### Decisions

Full log in PROJECT.md Key Decisions. Affecting current work:

- The enum-in-describe fix ships **first**, on the `<db_dir>/ui/` convention, with no ABI change — `describe*` already returns a `std::string` through the C API, so it reaches all five bindings and Lua free. Ships as a patch.
- The `DatabaseOptions` ABI break (8 → 24 bytes) is **isolated in Phase 2**, never folded into Phase 1.
- Enum rendering shows the **code and the label** — partial mitigation for the unchecked-label window.
- `validate_ui_config()` ships **last**, by explicit user decision with the `HasCommitment` inversion on the table.
- Parser is written against Hub's Dart source, not `toml-schema.md` (already wrong about group membership).
- Never add fields to `ScalarMetadata` / `GroupMetadata` — new struct, own size constant, own free function.
- [Phase ?]: UI sidecar parser (src/ui_config.h/.cpp) is private, mirrors BinaryMetadata's from_toml_file/from_toml_content split, and swallows load failures instead of propagating them (D-25)
- [Phase ?]: Golden baseline for describe()/describe_collection()/summarize_collection() captured from unmodified renderer before any edit, pinned to eol=lf so DESC-05 is provable byte-for-byte on every platform
- [Phase ?]: Pinned tests/schemas/ui/**/*.toml and *.md to eol=lf in .gitattributes so byte-pinned literals survive core.autocrlf=true checkouts
- [Phase ?]: format_table/ fixture needed a second collection file (empty_collection.toml) for the zero-attribute-blocks edge, beyond the plan's frontmatter files_modified list
- [Phase ?]: Task 2's commit is test-only: the four scalar-line clauses landed together with the header/label clauses in Task 1's commit since both touch the same write_collection_section edit site.
- [Phase ?]: Task 3's fifth clause combination is read as vocabulary-resolved-vs-unresolved (htd_like vs no_enum), not label-present-vs-absent, since every vocabulary-bound attribute in the corpus carries a label.
- [Phase ?]: Task 3's unit-only/label-only/both/neither combinations use a scratch RAII sidecar (ScratchSidecarDir) instead of a new tracked fixture, since enum_basic has no unit-without-label attribute and the task is scoped to the test file only.
- [Phase ?]: 01-04: print_group_columns gained a nullable UIConfigSet*/collection-name pair to decorate a time series dimension column's label (deviation from stated file-scope prohibition, required by literal L13's acceptance criterion)
- [Phase ?]: 01-05: C API describe tests build databases via quiver_database_from_schema directly (not the C++ test_ui_fixture.h helper), proving the char** boundary itself
- [Phase ?]: 01-05: Lua describe tests use plain TEST(...) with open_ui_fixture instead of TEST_F(LuaRunnerTest, ...), since LuaRunnerTest's temp-dir sandbox has no ui/ sidecar sibling (D-28)
- [Phase ?]: DESC-07 closed: Julia, Dart, Python, and JS each assert exact-string enum rendering against the shared tests/schemas/ui/ fixtures; D-31 (strengthen and keep) recorded and executed
- [Phase ?]: CHANGELOG.md head reconciled to 0.10.6 (matching CMakeLists.txt); fresh 0.10.7 unreleased section opened for this phase; no manifest bumped
- [Phase ?]: D-01/D-05 applied verbatim in require_ui_config: explicit override checked first, :memory: short-circuit moved (not deleted) to guard only the convention path, warn vs debug log split by override-vs-convention.
- [Phase ?]: Threaded a locale parameter into UIConfigSet::parse_enum_content (previously hardcoded en with no parameter) -- the second locale site the adversarial review flagged; without it ui_locale=es would render byte-identical en output.
- [Phase ?]: No new fixture tree for the explicit-config-dir proof: reused foresight_like with the database file placed in a scratch directory with no ui/ sibling of its own.
- [Phase ?]: Python _EXPECTED_STRUCT_SIZES implemented as inline ffi.sizeof(...) calls inside _assert_struct_sizes rather than a module-level dict, since _loader.py deliberately avoids importing quiverdb._c_api at module scope.
- [Phase ?]: Python UI-options tests build every database under pytest's tmp_path (never the shared foresight_like fixture dir) so has_ui_config()==True can only come from the explicit ui_config_dir argument.
- [Phase ?]: JS: makeDefaultOptions rebuilt on named offset constants, returns [Allocation, Allocation[]] keepalive tuple copied from csv.ts's buildCsvOptionsBuffer
- [Phase ?]: JS: SCALAR_METADATA_SIZE/GROUP_METADATA_SIZE relocated from metadata.ts into ffi-helpers.ts to avoid a loader<->database import cycle
- [Phase ?]: JS: hasUiConfig implementation placed in introspection.ts (where isHealthy's real body lives), not database.ts as plan text literally said
- [Phase ?]: Julia has zero export statements anywhere; has_ui_config follows house convention (Quiver.has_ui_config) rather than adding the codebase's first export
- [Phase ?]: Load-time struct-size gate written only to bindings/julia/generator/prologue.jl, proven regeneration-proof by re-running generator.bat twice with an empty resulting diff on c_api.jl
- [Phase ?]: Dart: memoize the load-time struct-size gate with its own _structSizesChecked flag, separate from _cachedBindings, so the three native *_sizeof calls run once per isolate rather than on every bindings access
- [Phase ?]: Dart: wrote test.bat's cache-clearing lines via raw printf with literal CRLF bytes, verified with xxd/cat -A, to avoid unix tooling silently rewriting the CRLF .bat file to LF
- [Phase ?]: Release-timing checkpoint resolved as hold-for-phase-3: CHANGELOG.md 0.11.0 unreleased section is complete but the Bump Version dispatch is deferred until Phase 3 lands, per ROADMAP's same-release note; deferral recorded in STATE.md Pending Todos.
- [Phase ?]: Promoted the struct-size gate rule from a fixed three-struct list to 'every hand-allocated struct gets a *_sizeof accessor', recorded in src/c/CLAUDE.md and bindings/python/CLAUDE.md
- [Phase ?]: Confirmed Python's load-time struct-size gate was actually unwired in HEAD (pass # MUTATION: gate unwired at both call sites), not merely undertested; restored and made observable via _CHECKED_STRUCTS
- [Phase ?]: [Phase 02]: Restored ui_config_dir/ui_locale fields silently dropped from bindings/julia/src/c_api.jl by a prior merge (e8d35b9) -- Rule 1 auto-fix, blocked the entire Julia test suite
- [Phase ?]: [Phase 02]: from_migrations() cross-layer coverage uses a runtime-generated scratch migrations directory (from foresight_like/schema.sql's own DDL) rather than a new committed fixture, in Julia/Dart/Python/JS
- [Phase ?]: JS: quiver_csv_options_t joined the four-struct load-time gate (last, mirroring Python); named CSV_OPTIONS_* offset constants replace bare literals in csv.ts; CLAUDE.md deferral note closed.

### Pending Todos

- **Deferred release dispatch:** `CHANGELOG.md` heads a complete, unreleased `## [0.11.0]`
  section (Phase 1 + Phase 2 content). Per the ROADMAP's Phase 3 note ("should ship in the same
  release" as Phase 2), the Bump Version workflow dispatch is held until Phase 3 lands, then
  dispatched exactly once with `part=minor` (never `part=patch` first — that would tag a
  spurious `v0.10.7` shipping only Phase 1). Phase 3 entries append to the same unreleased
  0.11.0 section; no second heading.

### Blockers/Concerns

- **Pre-Phase-1:** `CHANGELOG.md` heads `## [0.10.4] — unreleased` against `CMakeLists.txt` 0.10.6. Reconcile before any bump dispatch.
- **Accepted risk (opens at Phase 1, closes at Phase 5):** Quiver becomes an authoritative repeater of unchecked labels. HTD's `HasCommitment` is inverted between Julia and its `enum.toml` today.
- **Phase 2 hazard:** `bindings/js/src/ffi-helpers.ts` `makeDefaultOptions` — a wrong number is a native out-of-bounds write with no compile error and no fallback symbol. Gets its own plan.
- The four binding describe suites assert only "returns a String" — do not count them as five-layer coverage.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| GOV | Format version field, model-repo CI, named arbiter | v2 | Requirements |
| VOCAB | Boolean vocabulary standardization; `HasCommitment` reconciliation | v2 | Requirements |

## Session Continuity

Last session: 2026-09-19T22:53:17.672Z
Stopped at: Completed 02-10-PLAN.md
Resume file: None
