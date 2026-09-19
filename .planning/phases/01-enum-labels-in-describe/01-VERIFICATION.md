---
phase: 01-enum-labels-in-describe
verified: 2026-09-19T05:00:00Z
status: passed
score: 5/5 must-haves verified
behavior_unverified: 0
overrides_applied: 0
re_verification: No — initial verification
---

# Phase 01: Enum Labels in Describe Verification Report

**Phase Goal:** An agent calling `describe` / `describe_collection` / `summarize_collection` on a
PSR model database reads `values {0: 8 (Disabled), 1: 4 (Enabled)}` instead of bare codes — in
C++, the C API, Julia, Dart, Python, JS and Lua — delivered on the `<db_dir>/ui/` convention path
with no ABI change, no new C symbol and no file under `bindings/` touched.

**Verified:** 2026-09-19
**Status:** passed
**Re-verification:** No — initial verification

## Method

Every claim below was checked against the actual codebase, not against SUMMARY.md prose: builds
were run, test binaries were executed, `git diff` ranges were inspected directly, and — for the
one criterion most plausibly fakeable (byte-identical no-sidecar output) — the golden-capture
commit's `src/database_describe.cpp` was diffed byte-for-byte against `master`'s copy, independent
of any test asserting the same thing. A live CLI probe (`quiver_cli.exe`) was also run against the
`bess_like` fixture to observe real output outside of any committed test, including the int64
extremes (`-1`, `9223372036854775807`) that code review had originally flagged as parsed-but-dead.

## Goal Achievement

### Observable Truths (ROADMAP Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `summarize_collection`'s histogram shows code + label; `describe_collection` names the full declared vocabulary including zero-row codes | ✓ VERIFIED | Unit tests `DatabaseUiDescribe.EnumLabelsRenderBesideCodes`, `VocabularyFullValueListRendered` pass. Live CLI probe on `bess_like` (int64-extreme `look_ahead` vocabulary): `describe_collection` emitted `enum look_ahead {-1: Unknown, 0: Immediate, 3: Short Term, 7: Long Term, 9223372036854775807: Unbounded}` — codes `0` and `7` have zero data rows and still render; the histogram only showed the two codes actually present in data (`-1`, `9223372036854775807`), each with its label, matching the "code and label, full declared list independent of data" claim exactly. |
| 2 | `describe`/`describe_collection` render collection/scalar UI label and unit, tag `hide=true` `[hidden]` (not dropped), header names config path + locale | ✓ VERIFIED | `src/database_describe.cpp:70` emits `[hidden]` (grep-confirmed, attribute never dropped — `internal_code` fixture literal L5). Live CLI probe showed `UI config: .../ui (locale: en)` as line 1 of both `describe_collection` and `summarize_collection`, and `Collection: Storage (2 elements) — "Storage Units"` (collection label). Unit tests `HeaderLineInAllThreeReports`, `CollectionLabelRendered`, `UnitAndLabelRendered`, `HiddenAttributeTaggedNotDropped` all pass. |
| 3 | No/malformed `ui/` → `open()` succeeds, warning logged, `has_ui_config()` false, nothing partial, three reports byte-identical to today's output — proven by diff, not "tests pass" | ✓ VERIFIED | Independently diffed `git show 966bfec:src/database_describe.cpp` (the golden-capture commit) against `git show master:src/database_describe.cpp` — **byte-identical**, confirming the goldens were captured before any renderer edit (commit order: `966bfec` capture precedes `b612dfa` first renderer edit). `git diff --name-status master...HEAD -- tests/schemas/ui_golden/` shows only `A` lines, never `M`. Ran `DatabaseUiGolden.*` (2/2 pass) and a live CLI probe on the golden schema with no `ui/` dir — output matches the golden shape exactly, no header line. WR-01 (a `ui/` dir missing `main.toml` silently "succeeding" empty) was found in code review and fixed in `08aea4d`; confirmed live: `src/ui_config.cpp:274` now throws when `main.toml` is absent, routed through the warn-log catch path; `DirectoryWithoutMainTomlIsTreatedAsMalformed`/`...LogsAtWarn` both pass. |
| 4 | Shared fixtures under `tests/schemas/ui/` pin every parser tolerance individually; enum rendering asserted on exact strings from C++, Lua, and all five binding suites against those same fixtures, never a copy | ✓ VERIFIED | 11 fixture directories confirmed on disk (`bess_like`, `foresight_like`, `htd_like`, `no_enum`, `empty_enum`, `unknown_keys`, `format_table`, `orphan_collection`, `enum_basic`, `malformed`, `no_ui_dir`, `no_main_toml`). `DatabaseUiCorpus.*` (5/5), `DatabaseUiParse.*` (15/15) pass. Exact-string assertions confirmed by direct grep in all four binding suites (`values {0: 8 (Disabled), 1: 4 (Enabled)}` literal present in Julia/Dart/Python/JS, plus non-ASCII `Seasonal Naïve`/`Regresión Lineal`), C API (`DatabaseCApiDescribe`, 6/6 pass) and Lua (`LuaRunnerDescribe`, cases pass). WR-02 (bess_like's int64-extreme vocabulary parsed but never rendered) fixed in `6a83ff6` — confirmed live via CLI probe (see truth 1) and unit test `Int64ExtremeVocabularyCodesRenderExactly`. `FixturesAreNeverCopiedIntoABinding` passes; each binding suite's fixture helper (`uiFixture`, `ui_fixture_path`, etc.) is grep-confirmed to reference `tests/schemas/ui/` by path, not a local copy. |
| 5 | Ships as a patch: no file under `bindings/` (source) changed, `CHANGELOG.md` heads the version `CMakeLists.txt` carries before any bump | ✓ VERIFIED | `git diff --name-status master...HEAD -- bindings/` shows only test files (`M` on 3 existing test files, `A` on one new Python test file) — zero source files. `git diff master...HEAD -- include/quiver/database.h` shows exactly one added declaration (`has_ui_config()`) plus its comment — the phase's only public header edit. `include/quiver/c/`, `src/c/`, `include/quiver/options.h`, `include/quiver/attribute_metadata.h` all show empty diffs against master. `uv run python scripts/assert_version.py` reports `0.10.6` across all five manifests; `CHANGELOG.md` heads `## [0.10.6] — 2026-09-11` (IN-01 fixed in `fc1a5f7`: date now matches `git log -1 --format=%ai v0.10.6` = `2026-09-11`, not the reconciliation commit's own date) with a fresh `## [0.10.7] — unreleased` section for this phase's entry. |

**Score:** 5/5 truths verified (0 present, behavior-unverified)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/ui_config.h` / `src/ui_config.cpp` | Private TOML sidecar parser | ✓ VERIFIED | Not under `include/`, no `QUIVER_API` (grep confirms 0 matches). Implements `UIConfigSet::from_directory`, locale fallback chain, `Impl::require_ui_config()`, `Database::has_ui_config()`. |
| `include/quiver/database.h` | Only new public symbol: `has_ui_config()` | ✓ VERIFIED | Diff shows exactly the declaration + comment, nothing else. |
| `tests/schemas/ui/*` (11 dirs) | Fixture corpus, one tolerance per dir + BESSOperation/Foresight/HTD-distilled cases | ✓ VERIFIED | All present, `DatabaseUiCorpus` walk confirms structural completeness and README cross-reference. |
| `tests/schemas/ui_golden/*.txt` | Pre-change byte baseline | ✓ VERIFIED | Captured before renderer edit (commit-order + byte-diff proof above), pinned `eol=lf`, no CR bytes. |
| C API / Lua / 4 binding describe suites | Exact-string DESC-07 proofs | ✓ VERIFIED | `DatabaseCApiDescribe` (6/6), `LuaRunnerDescribe` cases, and all four binding suites' literal grep hits confirmed above. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/database_describe.cpp` | `src/database_impl.h` | `require_ui_config()` called before histogram/scalar loops | ✓ WIRED | Confirmed by passing `DatabaseUiDescribe`/`DatabaseUiGolden` suites and live CLI output. |
| `src/database_impl.h` | `src/ui_config.h` | `mutable std::optional<UIConfigSet> ui_config` | ✓ WIRED | Present, grep-confirmed. |
| `src/CMakeLists.txt` | `src/ui_config.cpp` | `QUIVER_SOURCES` entry, no new `tomlplusplus` link line | ✓ WIRED | `grep -c 'ui_config.cpp'` = 1; no new link line in diff. |
| Binding test suites | `tests/schemas/ui/` | Path reference, not copy | ✓ WIRED | `uiFixture`/`ui_fixture_path` helpers grep-confirmed to build a path into the shared directory; `FixturesAreNeverCopiedIntoABinding` passes. |

### Behavioral Spot-Checks (live, outside committed tests)

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Full six-suite pass | `quiver_tests.exe`, `quiver_c_tests.exe`, `bindings/{julia,dart,python,js}/test/test.bat` | C++ 1163/1163, C API 563/563, Julia 1428/1428, Dart 422/422, Python 308/308, JS 209/209 | ✓ PASS |
| int64-extreme vocabulary renders (WR-02 regression) | CLI probe against `bess_like`, creating elements with `look_ahead = -1` and `9223372036854775807` | `values {-1: 1 (Unknown), 9223372036854775807: 1 (Unbounded)}` in `summarize_collection`; full declared list (incl. zero-row `0`, `3`, `7`) in `describe_collection` | ✓ PASS |
| No-sidecar output unchanged | CLI probe against `tests/schemas/ui_golden/schema.sql` with no `ui/` directory | Output matches golden shape exactly, no header line | ✓ PASS |
| `main.toml`-missing degrades as malformed (WR-01 regression) | `DirectoryWithoutMainTomlIsTreatedAsMalformed`/`...LogsAtWarn` | Both pass; `src/ui_config.cpp:274` throws, routed through warn-log catch | ✓ PASS |
| Version/changelog reconciliation | `uv run python scripts/assert_version.py` | `0.10.6` across all 5 manifests; `CHANGELOG.md` head reads `## [0.10.6] — 2026-09-11` | ✓ PASS |

### Requirements Coverage

All 22 requirement IDs (`PARSE-01..12`, `DESC-01..07`, `CORPUS-01..03`) are claimed across the six
plans' `requirements-completed` frontmatter, and every ID's supporting test(s) were confirmed
running and passing above or in the referenced SUMMARY coverage tables (spot-checked: `PARSE-11`,
`PARSE-12`, `DESC-01`, `DESC-05` via 01-01; `DESC-02..06` via 01-03; `PARSE-01,05-10` via 01-04;
`DESC-07` via 01-05/01-06; `CORPUS-01..03` via 01-02/01-06). No orphaned requirement IDs found in
`REQUIREMENTS.md`'s Phase 1 mapping.

`PARSE-06` carries a **declared verification ceiling**, honestly recorded (not a gap): the `format`
table form is confirmed to parse without aborting the file (`FormatTableFormDoesNotAbortTheFile`
passes, `resolve_format_table` grep-confirmed in `src/ui_config.cpp:88`), but no report renders
`format`, so verbatim round-trip is unverified until Phase 3's META-03. This is a `backstop` truth
per the plan's own frontmatter — judged as intended scope, not a defect.

### Anti-Patterns Found

None. Scanned every file in `git diff --name-only master...HEAD -- src/ include/ tests/ bindings/
CHANGELOG.md CLAUDE.md` (72 files) for `TBD|FIXME|XXX|TODO|HACK|PLACEHOLDER` — zero matches.

### Code Review Findings (all fixed, confirmed live)

- **WR-01** (`ui/` missing `main.toml` silently treated as valid-empty): fixed `08aea4d`, confirmed live above.
- **WR-02** (`bess_like`'s int64-extreme vocabulary parsed but dead): fixed `6a83ff6`, confirmed live above via CLI probe with the actual extreme values.
- **IN-01** (CHANGELOG date wrong): fixed `fc1a5f7`, confirmed against `git log -1 --format=%ai v0.10.6`.

### Human Verification Required

None. Every criterion was settled by running a build, a test binary, a live CLI probe, or a direct
`git diff`/`git log` inspection — no item was punted to human judgment.

### Gaps Summary

No gaps found. All five ROADMAP success criteria hold under direct, independent verification (not
merely "the tests pass" — the byte-identity criterion in particular was checked via source-diff at
the golden-capture commit, and the int64-extreme and no-sidecar behaviors were reproduced live via
`quiver_cli.exe` outside any committed test). All three code-review findings are fixed and their
fixes hold under re-run. The patch-only constraint (no binding source, no ABI/C-symbol change) is
confirmed by direct `git diff` inspection of every named boundary. Phase goal achieved.

---

_Verified: 2026-09-19_
_Verifier: Claude (gsd-verifier)_
