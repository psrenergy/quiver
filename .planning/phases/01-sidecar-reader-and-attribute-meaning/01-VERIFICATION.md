---
phase: 01-sidecar-reader-and-attribute-meaning
verified: 2026-09-20T17:29:39Z
status: passed
score: 20/20 must-haves verified
behavior_unverified: 0
overrides_applied: 4  # human sign-off 2026-09-20: 3 flagged assumptions + judgment-tier prohibitions accepted as-is
human_verification:
  - test: "Confirm the planner's root-migrations-path interpretation for READ-01/D-16 is acceptable: `migrations_path` pointing at a filesystem root makes `parent_path()` the root itself, so `ui_dir` resolves to `<root>/ui` and finds nothing (no `/` sibling exists above a root). This case is untested and unspecified by any source artifact (01-01-PLAN.md flagged_assumptions #1)."
    expected: "Either confirm this degrade-to-empty behavior is acceptable for a case that cannot occur in real PSR studies, or file a follow-up to test/handle it explicitly."
    why_human: "Flagged by the planner as an unresolved probe classification, not covered by any must_haves truth or test; a judgment call about scope, not something grep/build can settle."
  - test: "Confirm the planner's SAFE-01 interpretation of 'byte-identical to today's' — verified by comparing two runs of the same binary (sidecar present vs. a mirror with `ui/` removed) inside one test process, not against a stored golden file — matches the milestone's intent."
    expected: "Sign off that in-process mirror comparison is sufficient evidence, or require a golden-file addition."
    why_human: "Flagged unresolved by the planner (01-01-PLAN.md flagged_assumptions #2); the interpretation is implemented and passing but was never independently confirmed."
  - test: "Confirm the planner's SAFE-02 interpretation of 'never fails' — covers every `std::exception`-derived throw inside `load_ui_config`, explicitly NOT a hard crash from resource exhaustion (a sidecar large enough to exhaust memory), which is separately dispositioned `accept` as threat-register row T-01-04."
    expected: "Sign off that resource-exhaustion DoS from a hostile `ui/*.toml` is out of scope for this milestone (consistent with the project's WIP, no-hardening-for-untrusted-input posture), or require a size cap."
    why_human: "Flagged unresolved by the planner (01-01-PLAN.md flagged_assumptions #3); a risk-acceptance judgment call, not a code defect."
  - test: "Confirm the 5 `must_haves.prohibitions` (judgment-tier, no `status` field recorded) are actually honored: (1) sidecar text never replaces/reorders/omits a schema-derived fact, (2) a code-to-label pair is rendered verbatim even when it looks wrong (e.g. the recorded `HasCommitment` inversion), (3) `ui_config.cpp` never writes to the `ui/` or migrations tree, (4) `tests/test_database_lifecycle.cpp` was not edited to make this phase pass, (5) no fixture file was committed under `tests/schemas/ui/`."
    expected: "All 5 read as satisfied from source/git inspection performed during this verification (see Prohibitions Reviewed below) — human sign-off closes the judgment-tier gate per the project's verification-override contract."
    why_human: "`verification: judgment` prohibitions route to an explicit human checkpoint rather than an automated gate; this verifier's own read of the code found no violation, but the tier requires the human decision to be recorded, not inferred."
---

# Phase 1: Sidecar Reader and Attribute Meaning Verification Report

**Phase Goal:** An agent calling `describe` or `describe_collection` on a PSR study database opened with `from_migrations` sees each scalar attribute's English label, tooltip and enum code→label list; a database with no `ui/`, or a broken one, reads exactly as it does today.
**Verified:** 2026-09-20T17:29:39Z
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Roadmap Success Criteria (the six-item spine)

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | `describe_collection` shows label+tooltip after `name (TYPE)` + flags; `\n`/`\r` collapse to one line; non-ASCII UTF-8 byte-for-byte | ✓ VERIFIED | `write_ui_clauses`/`normalize_ui_text` (`src/database_describe.cpp:56-89,124-146`); tests `RenderLabelAndTooltipClauses`, `LocalizedNewlineCollapse`, `LocalizedControlCharacterCollapse`, `LocalizedUtf8Passthrough` (all pass — ran `--gtest_filter=*UiConfig*:*DatabaseUiMetadata*`, 34/34) |
| 2 | Enum-bound attribute shows codes/labels from `enum.toml`, joined by `enum` value not `id`, code from entry's own `id`; gapped and 1-based both render real codes | ✓ VERIFIED | `parse_vocabularies`/join logic (`src/ui_config.cpp:38-61,89-100`); tests `EnumGappedCodesRenderVerbatim`, `EnumOneBasedCodesRenderVerbatim`, `EnumJoinedByEnumValueNotAttributeId` all pass |
| 3 | Undescribed scalar renders today's line in all three corpus cases | ✓ VERIFIED | `UiConfig::find` returns `nullptr` on miss (`src/ui_config.h:31-40`); tests `UndescribedCollectionRendersUnchanged`, `UndescribedAttributeRendersUnchanged`, `UndescribedDanglingUiColumnRendersUnchanged` all pass, each diffed against a live mirror run with `ui/` removed |
| 4 | No/empty/zero-byte/unparseable sidecar still opens, warns only for malformed, byte-identical to sidecar-deleted run | ✓ VERIFIED | Nested try/catch degrade-never-throw (`src/ui_config.cpp:114-176`); tests `NoUiDirReportsUnchanged`, `MalformedEmptyUiDirRendersIdentical`, `MalformedZeroByteEnumRendersIdentical`, `MalformedInvalidSyntaxRendersIdentical`, `MalformedWrongTypeAttributeRendersIdentical` all pass |
| 5 | Trailing-separator / relative migrations path resolve to same `ui/`; never `<migrations>/ui`, never CWD-relative | ✓ VERIFIED | `fs::weakly_canonical(...).parent_path() / "ui"` (`src/ui_config.cpp:116`); tests `PathResolutionTrailingSeparator`, `PathResolutionRelativeMigrationsPath`, `PathResolutionNeverReadsUiUnderMigrations` (decoy `migrations/ui/` planted, sibling wins) all pass |
| 6 | Fixtures build `migrations/`+`ui/` in per-test temp dirs; nothing committed under `tests/schemas/ui/`; four pre-existing lifecycle assertions pass unmodified | ✓ VERIFIED | `git status --porcelain tests/schemas/` empty, `test ! -d tests/schemas/ui` succeeds, `git diff --stat tests/test_database_lifecycle.cpp` empty; ran the four named filters directly — 4/4 pass |

### Observable Truths (PLAN frontmatter must_haves, 20 authored across 3 plans)

All 20 authored truths (including the 5 `verification: backstop` markers — one in 01-00, three in 01-01, one in 01-02) were checked against source and/or a passing test. None failed. Representative sample (full list cross-referenced against `src/ui_config.{h,cpp}`, `src/database_describe.cpp`, `src/database_impl.h`, `src/database.cpp`, `tests/test_database_ui_metadata.cpp`):

| # | Truth (abbreviated) | Status | Evidence |
|---|---|---|---|
| 1 | Temp-dir fixture, nothing under `tests/schemas/ui/` | ✓ VERIFIED | `UiTempTreeFixture::SetUp/TearDown` (`tests/test_database_ui_metadata.cpp:23-46`); `git status --porcelain tests/schemas/` empty |
| 2 | No-`ui/` baseline unchanged (SAFE-01) | ✓ VERIFIED | `NoUiDirReportsUnchanged` passes |
| 3 (backstop) | Fixture temp dir removed in both `SetUp` and `TearDown` | ✓ VERIFIED (source) | Both methods call `fs::remove_all(root)` conditionally — read directly in `tests/test_database_ui_metadata.cpp:23-46` |
| 4 | Label+tooltip render after name/type/flags | ✓ VERIFIED | `RenderLabelAndTooltipClauses` passes |
| 5 | Enum codes/labels from `enum.toml`, joined by `enum` not `id` | ✓ VERIFIED | `EnumJoinedByEnumValueNotAttributeId` passes |
| 6 | `describe()` never renders tooltip; `describe_collection()` renders all three, tooltip last | ✓ VERIFIED | `RenderDescribeOmitsTooltip`, clause order in `write_ui_clauses` (`src/database_describe.cpp:124-163`) |
| 7 | `describe()` line is strict prefix of `describe_collection()` line | ✓ VERIFIED | `PrefixInvariantDescribeIsPrefixOfDescribeCollection` passes |
| 8 | `ui/` resolved via `weakly_canonical(...).parent_path()/"ui"` | ✓ VERIFIED | `src/ui_config.cpp:116`; `PathResolutionTrailingSeparator`/`PathResolutionRelativeMigrationsPath` pass |
| 9 | No `toml::` symbol in `ui_config.h`/`database_impl.h`/`database_describe.cpp` | ✓ VERIFIED | `grep -c 'toml::' src/ui_config.h src/database_impl.h src/database_describe.cpp` → 0 in all three |
| 10 | `summarize_collection` byte-unchanged | ✓ VERIFIED | `git log`/source read: `summarize_collection` body has zero `write_ui_clauses`/`ui_config` reference; REVIEW.md independently confirmed zero diff hunks |
| 11 | Duplicate top-level `id` across two files: later file wins, no throw | ✓ VERIFIED (source) | `config.collections[parsed->first] = std::move(...)` overwrite semantics, `src/ui_config.cpp:161-166`; not independently tested by name but directly follows from map-assignment semantics and the passing `MalformedOneFileKeepsOtherCollections` proves the per-file try/catch isolation this depends on |
| 12 | Empty `ui/` / empty `attribute` array → empty map, byte-identical | ✓ VERIFIED | `MalformedEmptyUiDirRendersIdentical` passes |
| 13 | Case-sensitive `std::map` lookup, no diagnostic on mismatch | ✓ VERIFIED (source) | `Schema` keys tables in `std::map<std::string,...>` (`include/quiver/schema.h:104`, unmodified this phase); `UiConfig::find` is a byte-exact `std::map::find`, no case-folding anywhere in `src/ui_config.cpp` |
| 14 | Render order is `column_order`; sidecar never reorders/inserts/removes | ✓ VERIFIED (source) | `write_collection_section`'s scalar loop iterates `table_def->column_order` unconditionally (`src/database_describe.cpp:172-183`); the sidecar only appends via `write_ui_clauses`, never touches the loop's iteration |
| 15 (backstop) | `load_ui_config` runs once, `Impl::ui_config` never rewritten | ✓ VERIFIED (source) | Single call site `src/database.cpp:258`; `grep -c load_ui_config src/database_impl.h` → 0 |
| 16 | Duplicate `[[attribute]]` id → later entry wins; `attribute_group` never read | ✓ VERIFIED (source) | `attrs[*attr_id] = std::move(meta)` overwrite (`src/ui_config.cpp:107`); only `tbl["attribute"]` is read, no `attribute_group` reference anywhere in the file |
| 17 | No-`id`/empty-`id` entry skipped; all-empty record renders no clause | ✓ VERIFIED (source+test) | `if (!attr_id \|\| attr_id->empty()) continue;` (`src/ui_config.cpp:92`); `write_ui_clauses` returns immediately-empty output when label/tooltip/enum all empty (implicit in `UndescribedAttributeRendersUnchanged`) |
| 18 | Attribute id matched byte-for-byte; label/tooltip untranscoded | ✓ VERIFIED (source) | `std::string` equality via `std::map` key, no transcoding step anywhere in `read_localized`/`parse_collection_file` |
| 19 | `[[attribute]]` array order has no observable effect | ✓ VERIFIED (source) | Storage is a `std::map` keyed by attribute id; render order is `column_order`, independent of source array order |
| 20 (backstop) | Sequential, non-recursive, single-threaded scan; interrupted load → caught exception, one warn, empty map | ✓ VERIFIED (source) | `fs::directory_iterator(ui_dir)` single loop, no threads spawned; outer catch returns `{}` (`src/ui_config.cpp:114,172-178`) |
| 21 | Absent/table-without-`en`/empty/normalizes-empty localizable value treated as absent | ✓ VERIFIED (source+test) | `read_localized` returns `nullopt` on all four cases (`src/ui_config.cpp:20-30`); `normalize_ui_text` empty-result convention documented and exercised by `LocalizedControlCharacterCollapse` |
| 22 | Normalization tests byte as unsigned char < 0x20 \|\| == 0x7F | ✓ VERIFIED | `src/database_describe.cpp:64-65` exact match; `LocalizedUtf8Passthrough` passes |
| 23 | Unknown/empty/all-empty-label vocabulary → no enum clause | ✓ VERIFIED | `EnumUnknownVocabularyRendersNoClause`, `EnumEmptyVocabularyRendersNoClause` pass |
| 24 | Vocabulary joined byte-exactly; code from entry's own `id` | ✓ VERIFIED | `EnumGappedCodesRenderVerbatim`, `EnumOneBasedCodesRenderVerbatim` pass |
| 25 | No label/tooltip/enum → today's exact line, no dangling separator | ✓ VERIFIED | `NoUiDirReportsUnchanged`, `UndescribedAttributeRendersUnchanged` pass |
| 26 | Every clause separator ASCII; quoting escapes only `\` and `"` | ✓ VERIFIED | `quote_ui_text` (`src/database_describe.cpp:91-101`); `RenderEscapesQuotesAndBackslashes` passes |
| 27 | Dangling ui column never reached (iterate `column_order`, lookup by name) | ✓ VERIFIED | `UndescribedDanglingUiColumnRendersUnchanged` passes |
| 28 | All 3 undescribed cases byte-identical to today | ✓ VERIFIED | Same 3 tests above, each diffed against a live `ui/`-removed mirror |
| 29 | Clauses emitted label→enum→tooltip regardless of source key order | ✓ VERIFIED (source) | Fixed emission order hardcoded in `write_ui_clauses` (`src/database_describe.cpp:124-163`), independent of TOML key order (TOML parse discards source order for map-style lookup here) |
| 30 (backstop) | `describe`/`describe_collection` read `ui_config` without mutating; concurrent calls render identical text | ✓ VERIFIED (source) | `Database::Impl::ui_config` non-`mutable`, no method in `src/database_describe.cpp` writes to it; both describe entry points are `const` |
| 31 | Trailing-separator / relative path → same `ui/`, never `<migrations>/ui`, never CWD | ✓ VERIFIED | `PathResolutionTrailingSeparator`, `PathResolutionRelativeMigrationsPath`, `PathResolutionNeverReadsUiUnderMigrations` pass |
| 32 | `main.toml`/`enum.toml`/`themes/`/non-`.toml` never mistaken for a collection file; no filename→table-name translation | ✓ VERIFIED | `ShapeSelectionIgnoresNonCollectionFiles`, `ShapeSelectionUsesFileIdNotFilename` pass |
| 33 | Undescribed scalar renders today's line, all 3 corpus cases | ✓ VERIFIED | (same as roadmap criterion 3) |
| 34 | No/empty/zero-byte/unparseable sidecar opens, warns only malformed, byte-identical | ✓ VERIFIED | (same as roadmap criterion 4) |
| 35 | One malformed file costs only its own collection | ✓ VERIFIED | `MalformedOneFileKeepsOtherCollections` passes |
| 36 | `hide = true` attribute still renders | ✓ VERIFIED | `HiddenAttributeStillRenders` passes |
| 37 | Fixtures per-test temp dirs; nothing under `tests/schemas/ui/` | ✓ VERIFIED | (same as roadmap criterion 6) |
| 38 | Four pre-existing lifecycle assertions pass unmodified | ✓ VERIFIED | Ran the exact named filter — 4/4 pass; `git diff --stat` empty |
| 39 | `CHANGELOG.md` D-15 reconciliation | ✓ VERIFIED | `## [0.10.7] — 2026-09-17`, `compare/v0.10.6...v0.10.7`, new `## [0.10.8] — unreleased` with `compare/v0.10.7...HEAD` at top of link block — all present, in order |
| 40 (backstop) | Malformed-sidecar warning reaches logger, not swallowed | ✓ VERIFIED (source+test) | Confirmed by the exact evidence the truth's own text specifies: no thrown exception (`Malformed*RendersIdentical` tests all pass) plus the degraded/sidecar-free-equivalent report; `logger.warn(...)` call sites read directly in `src/ui_config.cpp` |

**Score:** 20/20 authored truths (40 individually itemized rows above map to the 20 `must_haves.truths` entries across the three plans, several of which bundle multiple assertions) verified, 0 failed, 0 present-but-behavior-unverified.

### Prohibitions Reviewed (5, judgment-tier, no recorded `status`)

| # | Prohibition | This verifier's finding | Evidence |
|---|---|---|---|
| 1 | Sidecar MUST NOT replace/rename/reorder/omit a schema-derived fact | No violation found | `write_collection_section`'s pre-existing column loop (`src/database_describe.cpp:172-183`) is untouched; `write_ui_clauses` only appends |
| 2 | MUST NOT silently correct/filter/editorialize a code-label pair | No violation found | `parse_vocabularies`/join copy the sidecar's label verbatim with no cross-check against other declarations |
| 3 | MUST NOT create/modify/delete/lock any file under `ui/` or migrations | No violation found | `src/ui_config.cpp` opens only `std::ifstream` (read); no `std::ofstream`, no `fs::remove`/`fs::rename`/`fs::permissions` call anywhere in the file |
| 4 | MUST NOT edit `tests/test_database_lifecycle.cpp` to make this phase pass | No violation found | `git diff --stat tests/test_database_lifecycle.cpp` is empty |
| 5 | MUST NOT commit a fixture under `tests/schemas/ui/` | No violation found | `test ! -d tests/schemas/ui` succeeds; `git status --porcelain tests/schemas/` empty |

Per this project's judgment-tier prohibition routing, these findings are non-authoritative and are carried into Human Verification below for explicit sign-off rather than silently marked passed.

### Flagged Assumptions Carried Forward (3, left `unresolved` by the planner)

01-01-PLAN.md's `<flagged_assumptions>` block records three probe-classification edges the planner resolved by judgment rather than by a cited source artifact, and explicitly leaves `unresolved`. None of the three has a corresponding `must_haves.truths` entry or test, by the plan's own design (they are edge cases outside the 20 authored truths). See Human Verification below.

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/ui_config.h` | `UiAttribute`/`UiConfig`/`load_ui_config`, plain std types, no `toml::` | ✓ VERIFIED | Present, no parser symbol, matches spec exactly |
| `src/ui_config.cpp` | Path resolution, shape scan, localizable accessor, vocab parse, degrade posture | ✓ VERIFIED | Present, matches D-09/D-16/D-17/D-18/D-19 |
| `src/database_impl.h` | Non-mutable `UiConfig ui_config` member | ✓ VERIFIED | `src/database_impl.h:80` |
| `src/database.cpp` | Single `load_ui_config` call inside `from_migrations` | ✓ VERIFIED | `src/database.cpp:258`, one call site |
| `src/database_describe.cpp` | `normalize_ui_text`/`quote_ui_text`/`squash`/`write_ui_clauses`, 2-param `write_collection_section` | ✓ VERIFIED | All present; `summarize_collection` untouched |
| `src/CMakeLists.txt` | `ui_config.cpp` in `QUIVER_SOURCES` | ✓ VERIFIED | 1 occurrence |
| `tests/test_database_ui_metadata.cpp` | Full test coverage (34 tests across 2 fixtures) | ✓ VERIFIED | 1046 lines, 34 `TEST_F` cases, all pass |
| `tests/CMakeLists.txt` | Test file registered | ✓ VERIFIED | grep confirms 1 occurrence |
| `CHANGELOG.md` | 0.10.8 unreleased section + dated 0.10.7 + fixed links | ✓ VERIFIED | Confirmed by direct read |
| `src/CLAUDE.md` | `ui_config` documentation | ✓ VERIFIED | Section present, reviewed and corrected (WR-01 fix) |
| `tests/CLAUDE.md` | `test_database_ui_metadata` documentation | ✓ VERIFIED | Present |

### Key Link Verification

| From | To | Via | Status | Details |
|------|-----|-----|--------|---------|
| `src/database.cpp` | `src/ui_config.cpp` | `load_ui_config` call after `migrate_up` | ✓ WIRED | `src/database.cpp:258`, single line between `migrate_up` and `return db` |
| `src/database_describe.cpp` | `src/database_impl.h` | `&impl_->ui_config` passed to `write_collection_section` | ✓ WIRED | Both `describe()` and `describe_collection()` pass it; `describe()` with `with_tooltip=false`, `describe_collection()` with `true` |
| `src/ui_config.cpp` | `src/CMakeLists.txt` | `QUIVER_SOURCES` entry | ✓ WIRED | Confirmed compiled into `quiver` (build succeeded, symbols resolved, tests exercise it through the public API) |
| `tests/test_database_ui_metadata.cpp` | `src/ui_config.cpp` | Public-API-only exercise via `from_migrations` | ✓ WIRED | No parser include in the test file (`grep` confirms), all coverage routes through `Database::from_migrations`/`describe*` |

### Behavioral Spot-Checks / Full Suite Execution

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Full C++ core suite | `./build/bin/quiver_tests.exe` | 1270 tests, 1270 passed | ✓ PASS |
| Full C API suite | `./build/bin/quiver_c_tests.exe` | 557 tests, 557 passed | ✓ PASS |
| Phase-specific suites | `--gtest_filter=*UiConfig*:*DatabaseUiMetadata*` | 34 tests, 34 passed | ✓ PASS |
| Pre-existing lifecycle describe assertions | `--gtest_filter=*DescribeVectorsHeaderPrintedOnce*:*DescribeSetsHeaderPrintedOnce*:*DescribeTimeSeriesWithDimensionColumn*:*DescribeNoCategoryHeaderWhenEmpty*` | 4 tests, 4 passed | ✓ PASS |
| Version manifest agreement | `uv run python scripts/assert_version.py` | All 5 manifests at 0.10.8 | ✓ PASS |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|--------------|--------|----------|
| READ-01 | 01, 02 | `ui/` resolved via `weakly_canonical` | ✓ SATISFIED | Path resolution tests + source |
| READ-02 | 01, 02 | Shape-based collection file self-selection | ✓ SATISFIED | Shape selection tests + source |
| READ-03 | 01 | `[[attribute]]` label/tooltip keyed by file id + attr id | ✓ SATISFIED | Source review (no dedicated `verification: judgment` test needed; direct code inspection) |
| READ-04 | 01, 02 | Localizable value: string or `en` table key, control-char collapse, UTF-8 passthrough | ✓ SATISFIED | `Localized*` tests |
| READ-05 | 01 | `enum.toml` vocabularies joined by `enum` value | ✓ SATISFIED | `Enum*` tests |
| RENDER-01 | 01, 02 | Clauses after name/type/flags in both reports | ✓ SATISFIED | `Render*` tests |
| RENDER-03 | 02 | Undescribed attribute renders exactly as today | ✓ SATISFIED | `Undescribed*` tests |
| SAFE-01 | 00, 02 | No `ui/` → byte-identical | ✓ SATISFIED | `NoUiDirReportsUnchanged`, `Malformed*RendersIdentical` |
| SAFE-02 | 02 | Missing/empty/unparseable/partial `ui/` warns and degrades | ✓ SATISFIED | `Malformed*` tests |

No orphaned requirements: all 9 IDs REQUIREMENTS.md maps to Phase 1 (READ-01..05, RENDER-01, RENDER-03, SAFE-01, SAFE-02) appear in a plan's `requirements` field; RENDER-02 is correctly deferred to Phase 2 and not claimed here.

### Anti-Patterns Found

None. Scanned all 5 source files this phase modified (`src/ui_config.h`, `src/ui_config.cpp`, `src/database_impl.h`, `src/database.cpp`, `src/database_describe.cpp`) plus the test file for `TBD`/`FIXME`/`XXX`/`TODO`/`HACK`/`PLACEHOLDER`/stub patterns. Zero hits (the only "placeholder" string matches are unrelated pre-existing SQL bind-parameter code in `database_impl.h`/`database.cpp`).

### Code Review Findings (already resolved before this verification)

`01-REVIEW.md` found 1 Warning + 2 Info issues (0 Critical). `01-REVIEW-FIX.md` confirms all 3 were fixed and re-verified (commits `9554868`, `fe2bc1f`, `ba224df`), with the fix session re-running both full suites (1270/1270, 557/557) — the same numbers this verification independently reproduced.

### Human Verification Required

1. **Root-migrations-path edge case (READ-01/D-16, flagged assumption #1)**
   **Test:** Open `from_migrations` with a `migrations_path` that is a filesystem root (e.g. `C:\` or `/`).
   **Expected:** Confirm whether "resolves to `<root>/ui`, finds nothing, degrades silently" is acceptable, or whether this needs explicit handling/testing.
   **Why human:** The planner classified this probe edge as unresolved and untested; no source artifact specifies correct behavior for this input.

2. **SAFE-01 "byte-identical to today's" interpretation (flagged assumption #2)**
   **Test:** Review whether comparing two in-process runs (sidecar present vs. `ui/`-removed mirror) is sufficient evidence for "byte-identical to today's", vs. requiring a stored golden-file comparison.
   **Expected:** Sign off on the in-process mirror-comparison approach used by all `Malformed*RendersIdentical` / `Undescribed*RendersUnchanged` tests.
   **Why human:** Explicitly left unresolved by the planner; implemented consistently but never independently confirmed.

3. **SAFE-02 "never fails" scope (flagged assumption #3)**
   **Test:** Confirm resource-exhaustion DoS from an oversized/deeply-nested `ui/*.toml` is out of scope (threat-register T-01-04, dispositioned `accept`).
   **Expected:** Sign off on the risk acceptance, or require a size/depth cap before shipping against untrusted sidecars.
   **Why human:** A risk-acceptance judgment call the planner flagged rather than resolved; not a code defect.

4. **5 judgment-tier prohibitions (see Prohibitions Reviewed table)**
   **Test:** Confirm this verifier's source-level finding of "no violation" for each of the 5 prohibitions.
   **Expected:** Human sign-off recorded (e.g. as an `overrides:`-style acknowledgment or simply approval) closing the judgment-tier gate.
   **Why human:** `verification: judgment` items route to an explicit human checkpoint by project convention; this verifier's finding is non-authoritative.

### Gaps Summary

No gaps. Every roadmap success criterion, every authored `must_haves.truths` entry (including all 5 `backstop`-tier truths, each confirmed via the exact evidence its own statement specifies), every artifact, and every key link is verified against source and/or a passing test. Both full test suites reproduce their expected counts (1270/1270, 557/557) exactly. The phase's own prerequisite (`CHANGELOG.md` D-15 reconciliation) landed correctly and version manifests agree at 0.10.8. The only reason this report is not `passed` is that the plan itself carries 3 explicitly `unresolved` flagged assumptions and 5 judgment-tier prohibitions with no recorded resolution — per this project's honest-verifier and prohibition-routing conventions, those route to a human checkpoint rather than a silent pass, not because any code or test defect was found.

---

_Verified: 2026-09-20T17:29:39Z_
_Verifier: Claude (gsd-verifier)_
