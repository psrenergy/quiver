---
phase: 01-enum-labels-in-describe
reviewed: 2026-09-19T00:00:00Z
depth: standard
files_reviewed: 20
files_reviewed_list:
  - src/ui_config.h
  - src/ui_config.cpp
  - src/database_describe.cpp
  - src/database_impl.h
  - include/quiver/database.h
  - tests/test_ui_fixture.h
  - tests/test_database_ui_golden.cpp
  - tests/test_database_ui_describe.cpp
  - tests/test_database_ui_corpus.cpp
  - tests/test_database_ui_parse.cpp
  - tests/test_c_api_database_describe.cpp
  - tests/test_lua_runner_describe.cpp
  - bindings/julia/test/test_database_describe.jl
  - bindings/dart/test/describe_test.dart
  - bindings/python/tests/test_database_describe.py
  - bindings/js/test/database-describe.test.ts
  - CHANGELOG.md
  - .gitattributes
  - src/CLAUDE.md
  - tests/CLAUDE.md
findings:
  critical: 0
  warning: 2
  info: 1
  total: 3
fixed: 2026-09-19T00:00:00Z
fixes:
  - id: WR-01
    disposition: fixed
    commit: 08aea4d
    summary: >-
      Guarded main.toml with fs::exists in UIConfigSet::from_directory, throwing Pattern 3 so a
      ui/ directory missing main.toml routes through the existing catch-and-warn path instead of
      publishing an empty-but-"loaded" config. Added the no_main_toml fixture and two regression
      tests (DirectoryWithoutMainTomlIsTreatedAsMalformed, DirectoryWithoutMainTomlLogsAtWarn);
      confirmed both fail without the guard and pass with it.
  - id: WR-02
    disposition: fixed
    commit: 6a83ff6
    summary: >-
      Added a look_ahead INTEGER column to bess_like's Storage table and bound it via
      enum = "look_ahead" in storage.toml, so the fixture's gapped/negative/int64-max vocabulary
      actually reaches append_scalar_ui_clauses. Added
      DatabaseUiParse.Int64ExtremeVocabularyCodesRenderExactly asserting the exact rendered line
      (-1 through 9223372036854775807). Registered as L17 in tests/schemas/ui/README.md's
      authoritative literal table and its mirror in 01-02-PLAN.md, and in FixtureLiteralsArePinned.
  - id: IN-01
    disposition: fixed
    commit: fc1a5f7
    summary: >-
      Corrected CHANGELOG.md's "## [0.10.6]" header date from 2026-09-19 (the reconciliation
      commit's date) to 2026-09-11 (the tag's actual date per `git log -1 --format=%ai v0.10.6`).
      No manifest touched; scripts/assert_version.py still reports 0.10.6 across all five.
verification:
  full_suite: scripts/test-all.bat
  result: all_passed
  suites: [cpp, c_api, julia, dart, javascript, python, cli_smoke]
status: fixed
---

# Phase 01: Code Review Report

**Reviewed:** 2026-09-19
**Depth:** standard
**Files Reviewed:** 20
**Status:** issues_found (no blockers)

## Summary

This phase adds a private TOML sidecar parser (`src/ui_config.{h,cpp}`) and wires it into
`describe()` / `describe_collection()` / `summarize_collection()` via a nullable
`const UIConfigSet*` that gates every new render clause. I traced the seven specific correctness
questions the task called out and confirmed all seven are handled correctly:

1. **Byte-identity** — every new clause (`write_ui_header`, `append_collection_label`,
   `append_scalar_ui_clauses`, the `print_group_columns` dimension-label addition, and the
   `summarize_collection` histogram-label lookup) returns immediately on a null/absent
   `UIConfigSet*`/`vocabulary` pointer. This is a structural guard, not schema-shape-dependent, so
   the two golden shapes actually tested (`:memory:`, file-backed) generalize to any schema.
2. **Failure containment** — `require_ui_config()`'s `try` block's only statement is the
   assignment to `ui_config`, so any exception mid-parse leaves the optional at its prior (unset)
   value; `toml::parse_error` derives from `std::runtime_error` and `std::filesystem::filesystem_error`
   derives from `std::system_error`/`std::exception`, so both are caught by `catch (const
   std::exception&)`. Confirmed against the vendored toml++ source.
3. **`:memory:` short-circuit** — it is the first statement in `require_ui_config()`, before any
   `fs::path` computation, and it is the only place in the codebase that computes a UI directory.
   The `MemoryDatabaseNeverLoadsUiConfig` test genuinely proves this (a real `ui/` planted at the
   process cwd, empirically confirmed to fail when the guard is removed per the plan summary).
4. **Lifetime** — `ui_config` is assigned at most once per `Impl` (guarded by `ui_load_attempted`),
   never reassigned or cleared afterward, so pointers returned by `find_attribute`/`find_vocabulary`
   cannot dangle within a `Database`'s lifetime.
5. **Locale determinism** — confirmed against the vendored toml++ source that `toml::table` is
   backed by `std::map<toml::key, node_ptr, std::less<>>` with a lexicographic `key::operator<`,
   so "first key in map order" is genuinely deterministic, not hash-ordered.
6. **Integer handling** — `UIEnumEntry::code` and the histogram code are both `int64_t`; TOML
   integers are already 64-bit signed, so `9223372036854775807` round-trips through toml++ and
   SQLite (`sqlite3_column_int64`) with no narrowing.
7. **Test honesty** — `MemoryDatabaseNeverLoadsUiConfig` and the Python binding suite are both
   honest (Python's fixtures are built inside the shared fixture directory, never via a `tmp_path`
   fixture).

Two real gaps surfaced despite all seven guard questions checking out, plus one factual doc error.
None are blockers; both warnings are narrow, reachable-but-unlikely edge cases with low blast
radius, exactly the kind of thing that's cheap to fix now and easy to forget later.

## Warnings

### WR-01: A `ui/` directory with no `main.toml` is silently treated as a valid, empty sidecar

**File:** `src/ui_config.cpp:268-271` (contrast with the explicit guard at `src/ui_config.cpp:289-295`)

**Issue:** `enum.toml` is optional and is handled with an explicit `fs::exists(enum_path)` check
before reading it (`src/ui_config.cpp:290`) — PARSE-09's documented tolerance. `main.toml` has no
equivalent check: `read_file(main_path)` (`src/ui_config.cpp:142-145`) opens an `std::ifstream` on
a path that may not exist, which fails silently and yields an empty string, and
`toml::parse("")` returns an empty table without throwing. The result: a `ui/` directory that
exists (passing the `fs::exists(ui_dir) && fs::is_directory(ui_dir)` gate in
`require_ui_config`) but is missing `main.toml` — e.g. a typo'd filename, a partial deployment, or
a case-sensitivity slip on Linux (`Main.toml`) — falls into neither of the two documented
degradation paths (D-24's "absent → debug" or "malformed → warn"). Instead it succeeds:
`has_ui_config()` returns `true`, the `UI config: <path> (locale: en)` header prints in all three
reports, and `main.toml`'s optional `enum.toml` sibling (if present) is still loaded into
`config.vocabularies` — yet `config.collections` stays empty, so no label/unit/hidden/vocabulary
clause can ever resolve. An agent or caller sees "a UI sidecar loaded" with nothing to show for it,
which is a worse outcome than either honest degradation path: silence would say nothing, and
a warning would say "something is wrong here."

This exact scenario has no fixture and no test — every fixture directory under `tests/schemas/ui/`
that has a `ui/` subdirectory also has a `ui/main.toml` inside it (confirmed: no fixture directory
lacks it), and `DatabaseUiCorpus.EveryFixtureIsStructurallyComplete` (`tests/test_database_ui_corpus.cpp:56`)
only asserts fixtures *have* `main.toml`, never exercises the case where it's absent from an
otherwise-present `ui/` directory.

**Fix:** Check `fs::exists(main_path)` explicitly, mirroring the `enum.toml` guard, and either
throw (routing through the existing catch-and-warn path, treating a `ui/` directory without
`main.toml` as malformed) or `logger->debug(...)` and return early (treating it as functionally
absent — no header, `has_ui_config()` false):
```cpp
const auto main_path = dir / "main.toml";
if (!fs::exists(main_path)) {
    throw std::runtime_error("Failed to load UI config: missing " + main_path.string());
}
toml::table main_tbl = toml::parse(read_file(main_path));
```

### WR-02: The `bess_like` fixture's extreme-int64 vocabulary (`look_ahead`) is parsed but never rendered — the claimed integer-edge-case coverage doesn't reach the code path that matters

**File:** `tests/schemas/ui/bess_like/ui/enum.toml:1-19` (codes `-1, 0, 3, 7, 9223372036854775807`);
not referenced from `tests/schemas/ui/bess_like/ui/storage.toml` or any test file.

**Issue:** The 01-02 plan summary explicitly cites this vocabulary ("a gapped/negative/int64-max
`look_ahead` vocabulary (`-1, 0, 3, 7, 9223372036854775807`)") as proof that the parser/renderer
tolerates the full `int64_t` range, and the phase's own review brief asks specifically whether
narrowing or UB is possible at these extremes. `look_ahead` is declared in `enum.toml` but is
**never bound to any attribute** — no `[[attribute]]` block in `bess_like/ui/storage.toml` sets
`enum = "look_ahead"` — and the string `look_ahead` appears nowhere else in `tests/` or
`bindings/` (confirmed by a repo-wide grep). `parse_enum_content` will parse the array into
`config.vocabularies["look_ahead"]` (proving toml++ itself parses `INT64_MAX` correctly — already
covered by toml++'s own upstream test suite), but `find_vocabulary`/`find_attribute` never resolve
it in any test, so `append_scalar_ui_clauses`'s `out << (*vocabulary)[i].code << ": " <<
(*vocabulary)[i].label` (`src/database_describe.cpp:84`) and `summarize_collection`'s
`e.code == code` comparison (`src/database_describe.cpp:291`) — the two places that actually
consume the extreme values end-to-end — are never exercised with them. The risk is low (`ostream
<< int64_t` and `int64_t == int64_t` are not narrowing operations), but the specific claim of
coverage is false: this vocabulary is presently dead weight that looks like a test but isn't one.

**Fix:** Either bind `look_ahead` to a real attribute in `bess_like/storage.toml` (e.g. add
`enum = "look_ahead"` to one of the existing `[[attribute]]` blocks) and add one assertion in
`tests/test_database_ui_parse.cpp` or `test_database_ui_describe.cpp` rendering the full
`{-1: Unknown, 0: Immediate, 3: Short Term, 7: Long Term, 9223372036854775807: Unbounded}` line, or
update the 01-02 summary/README to stop claiming this proves the render path (only the parse path)
if leaving it unbound is intentional.

## Info

### IN-01: CHANGELOG.md's reconciled `## [0.10.6]` header carries the wrong date

**File:** `CHANGELOG.md` (diff: `## [0.10.6] — 2026-09-19`)

**Issue:** The 01-06 plan relabeled the changelog's stale unreleased head to `## [0.10.6] —
2026-09-19` (today's date, i.e. the date of this reconciliation commit). `git log -1 --format=%ai
v0.10.6` shows the tag was actually cut on `2026-09-11`. The reconciliation fixed the version
number and compare-link correctly but stamped the wrong date on an already-tagged, already-released
version — a minor factual error in a checked-in changelog that a future reader (or a release
script diffing dates) could take at face value.

**Fix:** Change the date to `2026-09-11` (the tag's actual date) or omit the date and note it was
backfilled, e.g. `## [0.10.6] — 2026-09-11 (backfilled)`.

---

## Fix Disposition

All three findings fixed, each in its own commit, verified against the real source before editing
(per WR-01's requirement, the regression test was confirmed to fail without the guard and pass
with it — the guard was temporarily reverted, the test run, and the fix restored).

- **WR-01 — fixed** (`08aea4d`): `UIConfigSet::from_directory` now guards `main.toml` with
  `fs::exists` and throws Pattern 3 (`"Failed to load UI config: missing <path>"`), routing a
  `ui/` directory that lacks `main.toml` through the existing catch-and-warn path instead of
  publishing an empty-but-"loaded" config. New fixture `tests/schemas/ui/no_main_toml/` and two
  regression tests in `tests/test_database_ui_describe.cpp`.
- **WR-02 — fixed** (`6a83ff6`): `bess_like`'s `look_ahead` vocabulary is now bound to a real
  `Storage.look_ahead` column, and `DatabaseUiParse.Int64ExtremeVocabularyCodesRenderExactly`
  asserts the exact rendered line spanning the negative and int64-max codes. Registered as L17 in
  both copies of the authoritative fixture-literal table and in `FixtureLiteralsArePinned`.
- **IN-01 — fixed** (`fc1a5f7`): `CHANGELOG.md`'s `## [0.10.6]` header now reads `2026-09-11`
  (the tag's actual date), not the reconciliation commit's own date.

**Final gate:** `scripts/test-all.bat` — all seven suites (C++, C API, Julia, Dart, JavaScript,
Python, CLI smoke test) passed, no regressions. Full C++ suite: 1163/1163. Full C API suite:
563/563.

_Reviewed: 2026-09-19_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
_Fixed: 2026-09-19_
_Fixer: Claude (gsd-code-fixer)_
