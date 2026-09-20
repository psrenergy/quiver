---
phase: 01-sidecar-reader-and-attribute-meaning
plan: 02
subsystem: testing
tags: [cpp, googletest, toml, changelog, docs]

# Dependency graph
requires:
  - phase: 01-00
    provides: "UiTempTreeFixture temp-dir fixture, reservoir_schema(), SAFE-01 no-sidecar baseline"
  - phase: 01-01
    provides: "src/ui_config.{h,cpp} loader, Database::Impl::ui_config, write_collection_section clause rendering, enum.toml vocabularies"
provides:
  - "Path-resolution coverage: trailing-separator, relative migrations path, and a migrations/ui/ decoy all resolve to the real sibling directory (READ-01)"
  - "Shape-selection coverage: main.toml, a theme file, notes.txt, and a themes/ subdirectory are never mistaken for a collection file; a file's own top-level id (never its filename) is the join key (READ-02)"
  - "Three RENDER-03 undescribed-case tests: no ui file for a collection, no [[attribute]] entry for one attribute (siblings still render), and a ui entry naming a nonexistent column -- all byte-identical to a sidecar-free run"
  - "D-20 coverage: hide = true still renders every clause"
  - "Four SAFE-02 malformed-sidecar cases (empty ui/, zero-byte enum.toml, invalid TOML syntax, wrong-typed attribute key) plus D-09's per-file-catch isolation test -- all opening successfully and rendering byte-identical to a sidecar-free run"
  - "CHANGELOG.md reconciled per D-15: dated 0.10.7 section, corrected compare link, new 0.10.8 unreleased section with this milestone's entry"
  - "src/CLAUDE.md and tests/CLAUDE.md document the ui_config reader and its test suite"
affects: []

# Actuals (#2632)
actuals:
  tokens: 8100
  tasks: 2
  commits: 3

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "ui-free mirror comparison: copy the whole temp tree to a sibling directory, remove its ui/, open a fresh database there, and diff against it -- rather than mutating the tree under an already-open handle"

key-files:
  created: []
  modified:
    - tests/test_database_ui_metadata.cpp
    - CHANGELOG.md
    - src/CLAUDE.md
    - tests/CLAUDE.md

key-decisions:
  - "No production code change was needed in src/ui_config.cpp or src/database_describe.cpp -- every new test in task 1 passed against plan 01's existing implementation on the first build. The plan listed both files under files_modified as a possibility, not a requirement."
  - "Compared describe()'s SAFE-02/undescribed byte-identity assertions on the extracted per-collection section, not the raw string -- describe()'s first two lines embed the database's own path, which legitimately differs between a main tree and its ui-free mirror in different temp directories. describe_collection()/summarize_collection() carry no such line and are compared verbatim."
  - "Used four separate TEST_F cases for the SAFE-02 malformed-sidecar coverage (one per broken shape) instead of a single gtest-parameterized fixture -- same filter-matchable names, less machinery for four one-off setups."
  - "Placed the ui-free mirror in a directory sibling to the fixture's root, never a subdirectory of it -- fs::copy(root, root/'mirror', recursive) would try to copy root into its own descendant."

requirements-completed: [READ-01, READ-02, READ-04, RENDER-03, SAFE-01, SAFE-02]

coverage:
  - id: D1
    description: "A trailing-separator migrations path, a relative migrations path, and a migrations/ui/ decoy directory all resolve to the same real ui/ sibling -- never <migrations>/ui, never a CWD-relative ui/, never the decoy"
    requirement: "READ-01"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.PathResolutionTrailingSeparator"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.PathResolutionRelativeMigrationsPath"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.PathResolutionNeverReadsUiUnderMigrations"
        status: pass
    human_judgment: false
  - id: D2
    description: "main.toml, a theme-shaped file, a plain text file, and a themes/ subdirectory are never selected as a collection file and never throw; a file's own top-level id (not its filename) is the join key"
    requirement: "READ-02"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.ShapeSelectionIgnoresNonCollectionFiles"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.ShapeSelectionUsesFileIdNotFilename"
        status: pass
    human_judgment: false
  - id: D3
    description: "The three RENDER-03 undescribed corpus cases (no ui file for the collection, no [[attribute]] entry for one attribute, a ui entry naming a nonexistent column) render byte-identically to a sidecar-free run, while a sibling attribute with a real entry still renders its clause"
    requirement: "RENDER-03"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.UndescribedCollectionRendersUnchanged"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.UndescribedAttributeRendersUnchanged"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.UndescribedDanglingUiColumnRendersUnchanged"
        status: pass
    human_judgment: false
  - id: D4
    description: "D-20: an attribute carrying hide = true still renders its clauses -- describe describes the schema, not the UI"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.HiddenAttributeStillRenders"
        status: pass
    human_judgment: false
  - id: D5
    description: "An empty ui/ directory, a zero-byte enum.toml, invalid TOML syntax, and a wrong-typed attribute key each let from_migrations open successfully and render byte-identically to a sidecar-free run; a malformed collection file costs only its own collection (D-09)"
    requirement: "SAFE-02"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.MalformedEmptyUiDirRendersIdentical"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.MalformedZeroByteEnumRendersIdentical"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.MalformedInvalidSyntaxRendersIdentical"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.MalformedWrongTypeAttributeRendersIdentical"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.MalformedOneFileKeepsOtherCollections"
        status: pass
    human_judgment: false
  - id: D6
    description: "The four pre-existing describe assertions in tests/test_database_lifecycle.cpp pass unmodified, and nothing is committed under tests/schemas/ui/"
    verification:
      - kind: unit
        ref: "build/bin/quiver_tests.exe --gtest_filter=*DescribeVectorsHeaderPrintedOnce*:*DescribeSetsHeaderPrintedOnce*:*DescribeTimeSeriesWithDimensionColumn*:*DescribeNoCategoryHeaderWhenEmpty*"
        status: pass
      - kind: other
        ref: "git diff --stat tests/test_database_lifecycle.cpp (expect empty)"
        status: pass
      - kind: other
        ref: "test ! -d tests/schemas/ui && git status --porcelain tests/schemas/ (expect empty)"
        status: pass
    human_judgment: false
  - id: D7
    description: "CHANGELOG.md dates the stale 0.10.7 heading, corrects its compare link, and opens a new 0.10.8 unreleased section with this milestone's entry; src/CLAUDE.md and tests/CLAUDE.md document the sidecar reader and its suite; the root CLAUDE.md is untouched"
    verification:
      - kind: other
        ref: "grep -c '^## \\[0.10.8\\] — unreleased' CHANGELOG.md; grep -c '^## \\[0.10.7\\] — 2026-09-17' CHANGELOG.md; grep -c 'compare/v0.10.7\\.\\.\\.HEAD' CHANGELOG.md; grep -qi ui_config src/CLAUDE.md; grep -qi test_database_ui_metadata tests/CLAUDE.md; git diff --stat CLAUDE.md (expect empty)"
        status: pass
      - kind: other
        ref: "uv run python scripts/assert_version.py (all five manifests at 0.10.8)"
        status: pass
    human_judgment: false

duration: 55min
completed: 2026-09-20
status: complete
---

# Phase 01 Plan 02: Sidecar Reader Coverage and Attribute Meaning Paperwork Summary

**Closed the SAFE-01/SAFE-02 degrade-never-throw proof and the READ-01/READ-02/RENDER-03 edge cases with 20 new gtest cases against plan 01's untouched implementation, then reconciled `CHANGELOG.md`'s stale 0.10.7 heading and documented `src/ui_config.{h,cpp}` in the two nearest `CLAUDE.md` files.**

## Performance

- **Duration:** ~55 min
- **Tasks:** 2
- **Files modified:** 4 (0 created, 4 modified)

## Accomplishments
- 20 new gtest cases in `tests/test_database_ui_metadata.cpp`: 3 path-resolution (trailing separator, relative path, `migrations/ui/` decoy), 2 shape-selection (`main.toml`/theme/`themes/` rejection, file-id-not-filename), 3 undescribed-case (no ui file, no attribute entry, dangling column), 1 hidden-attribute, 4 malformed-sidecar (empty dir, zero-byte `enum.toml`, invalid syntax, wrong-typed `attribute` key), 1 per-file-catch isolation test — every one passed against plan 01's existing `src/ui_config.cpp`/`src/database_describe.cpp` with zero production changes
- `open_ui_free_mirror()` fixture helper: copies the whole temp tree to a sibling directory, deletes that copy's `ui/`, and opens a fresh database there — the reusable "same tree with the sidecar removed" comparison baseline for every SAFE-02/RENDER-03 byte-identity assertion
- `CHANGELOG.md` reconciled per D-15: `## [0.10.7] — unreleased` → `## [0.10.7] — 2026-09-17` (the tag's actual commit date), its compare link corrected to `v0.10.6...v0.10.7`, and a new `## [0.10.8] — unreleased` section (with a `v0.10.7...HEAD` compare link at the top of the link block) carrying this milestone's `describe`/`describe_collection` entry
- `src/CLAUDE.md` gained a full section on `ui_config.h`/`ui_config.cpp` (path resolution, shape selection, the enum join, the nested warn-and-degrade posture, the `normalize_ui_text`/`squash` rationale) at the same depth as the existing `csv_read` section; `tests/CLAUDE.md` gained the `test_database_ui_metadata.cpp` suite entry with its temp-dir-only fixture rule

## Task Commits

1. **Task 1-02-01: Path-resolution, shape-selection, undescribed and degradation coverage** - `8b2d322` (test)
2. **Style follow-up: clang-format the new test coverage** - `4a7c154` (style)
3. **Task 1-02-02: Reconcile CHANGELOG.md and update the nearest CLAUDE.md files** - `9957eb1` (docs)

**Plan metadata:** committed alongside this SUMMARY.

## Files Created/Modified
- `tests/test_database_ui_metadata.cpp` - 20 new test cases plus `open_ui_free_mirror()`, `extract_collection_section()`, and `expect_reports_match()` fixture/anon-namespace helpers
- `CHANGELOG.md` - dated `0.10.7` heading, corrected compare link, new `0.10.8` section and entry
- `src/CLAUDE.md` - new `ui_config.h`/`ui_config.cpp` documentation section
- `tests/CLAUDE.md` - `test_database_ui_metadata.cpp` suite entry

## Decisions Made
- See `key-decisions` in frontmatter: no production code change needed; describe() byte-identity comparisons use the extracted per-collection section (the whole-report string embeds a path that legitimately differs between the main tree and its mirror); four separate `TEST_F` cases instead of one parameterized fixture for the four malformed shapes; the ui-free mirror lives in a sibling directory, never a subdirectory of the fixture root.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed a raw-string-literal delimiter collision in a new test fixture**
- **Found during:** Task 1-02-01 (first test build)
- **Issue:** `write_ui_file("thermal_plant.toml", R"(...label.en = "Installed Capacity (MW)"...)")` contains the literal sequence `)"` immediately after `(MW)`, which terminates a `R"(...)"` raw string literal early — the same class of collision plan 01's summary documented for `hm³`, recurring here with a different unit string.
- **Fix:** Switched that one fixture to a custom raw-string delimiter, `R"TOML(...)TOML"`.
- **Files modified:** tests/test_database_ui_metadata.cpp
- **Verification:** `cmake --build build --config Debug --target quiver_tests` succeeds; `UiConfigTest.MalformedOneFileKeepsOtherCollections` passes.
- **Committed in:** 8b2d322 (Task 1-02-01 commit)

**2. [Rule 1 - Bug] Ran clang-format on the new test coverage after committing it unformatted**
- **Found during:** Plan-level verification step 8 (`scripts/format.bat` equivalent check)
- **Issue:** `clang-format --dry-run -Werror tests/test_database_ui_metadata.cpp` flagged ~20 lines the task 1 commit did not run through the formatter (a few pre-existing lines from plan 00, several new ones from this plan's additions).
- **Fix:** `clang-format -i tests/test_database_ui_metadata.cpp`; rebuilt and reran the full `*UiConfig*:*DatabaseUiMetadata*` filter to confirm no behavioral change (34/34 still pass).
- **Files modified:** tests/test_database_ui_metadata.cpp
- **Verification:** `clang-format --dry-run -Werror` reports no violations; `quiver_tests.exe --gtest_filter='*UiConfig*:*DatabaseUiMetadata*'` — 34/34 pass.
- **Committed in:** 4a7c154 (separate style commit, since it touches only formatting on top of an already-landed task commit)

---

**Total deviations:** 2 auto-fixed (1 bug in a new test fixture, 1 formatting gap caught by the plan's own verification step). No scope creep; no production behavior changed.
**Impact on plan:** Both fixes are test-file-only. The plan's core claim — that plan 01's implementation already satisfies every edge case this plan tests — held with zero production changes.

## Issues Encountered
None beyond the two auto-fixed items above.

## Next Phase Readiness
- Phase 01 (Sidecar Reader and Attribute Meaning) is now fully executed: all three plans (00, 01, 02) complete, all nine phase requirements (READ-01..05, RENDER-01, RENDER-03, SAFE-01, SAFE-02) covered by passing tests.
- Full `quiver_tests.exe`: 1270/1270 passing (1256 after plan 01 + 14 new). Full `quiver_c_tests.exe`: 557/557 passing, unaffected.
- `tests/test_database_lifecycle.cpp` is byte-for-byte unmodified across every commit in this plan; no path was created under `tests/schemas/ui/`.
- `CHANGELOG.md`, `src/CLAUDE.md`, `tests/CLAUDE.md` are current with this milestone's change; the root `CLAUDE.md` is untouched, matching this plan's own prohibition.
- No further plans remain in this phase's ROADMAP entry.

## Self-Check: PASSED

- FOUND: tests/test_database_ui_metadata.cpp (modified)
- FOUND: CHANGELOG.md (modified)
- FOUND: src/CLAUDE.md (modified)
- FOUND: tests/CLAUDE.md (modified)
- FOUND commit 8b2d322 in git log
- FOUND commit 4a7c154 in git log
- FOUND commit 9957eb1 in git log

---
*Phase: 01-sidecar-reader-and-attribute-meaning*
*Completed: 2026-09-20*
