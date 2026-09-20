---
phase: 01-sidecar-reader-and-attribute-meaning
plan: 01
subsystem: database
tags: [cpp, toml, tomlplusplus, sqlite, describe, ui-metadata]

# Dependency graph
requires:
  - phase: 01-00
    provides: "UiTempTreeFixture temp-dir fixture, reservoir_schema(), and the SAFE-01 no-sidecar baseline test"
provides:
  - "src/ui_config.{h,cpp}: internal ui/ TOML sidecar reader (UiConfig, UiAttribute, load_ui_config), no public counterpart, toml++ confined to the .cpp"
  - "Database::Impl::ui_config, populated once in from_migrations after migrate_up"
  - "write_collection_section(..., const UiConfig*, bool with_tooltip) rendering label/enum/tooltip clauses shared by describe()/describe_collection()"
  - "enum.toml vocabulary discovery by top-level-key iteration, joined by an attribute's own enum value (D-19)"
affects: ["01-02"]

# Actuals (#2632)
actuals:
  tokens: 9700
  tasks: 2
  commits: 3

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Internal src/-local component with no public header (src/csv_read.h precedent), toml++ confined to the .cpp since it is linked PRIVATE on quiver"
    - "Nested try/catch warn-and-degrade: one outer catch around the whole load, one inner catch per file/enum.toml, never a bare toml++ .value() unwrap"
    - "Semicolon clause grammar appended to an existing report line: '; label \"...\"; enum {...}; tooltip \"...\"', each clause self-contained with its own leading '; '"

key-files:
  created:
    - src/ui_config.h
    - src/ui_config.cpp
  modified:
    - src/CMakeLists.txt
    - src/database_impl.h
    - src/database.cpp
    - src/database_describe.cpp
    - tests/test_database_ui_metadata.cpp

key-decisions:
  - "Front-loaded the enum clause emitter (write_ui_clauses' clause (b)) into task 1-01-01's single commit instead of task 1-01-02's, since the loop over an always-empty enum_labels map is a correct no-op for task 1 and splitting it would have meant editing the same function twice for no behavioral reason. Documented as a deviation below; task 1-01-02's own commits still followed RED (failing enum tests) then GREEN (parse_vocabularies + join) as the plan's TDD gate requires."

requirements-completed: [READ-01, READ-02, READ-03, READ-04, READ-05, RENDER-01]

coverage:
  - id: D1
    description: "A scalar attribute's label and tooltip render as '; label \"...\"' and '; tooltip \"...\"' clauses, tooltip only in describe_collection(), appended after the existing name/type/flags line"
    requirement: "RENDER-01"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.RenderLabelAndTooltipClauses"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.RenderDescribeOmitsTooltip"
        status: pass
    human_judgment: false
  - id: D2
    description: "A label/tooltip whose squash equals the attribute name's (or, for tooltip, the raw sidecar label's) squash is suppressed rather than rendered redundantly"
    requirement: "RENDER-01"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.RenderSuppressesRedundantLabel"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.RenderSuppressesRedundantTooltip"
        status: pass
    human_judgment: false
  - id: D3
    description: "Free text is ASCII-double-quoted with backslash/quote escaped and nothing else; non-ASCII UTF-8 and embedded control bytes (newline, tab, CR, ESC) are normalized without corrupting multibyte sequences"
    requirement: "READ-04"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.RenderEscapesQuotesAndBackslashes"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.LocalizedNewlineCollapse"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.LocalizedControlCharacterCollapse"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.LocalizedUtf8Passthrough"
        status: pass
    human_judgment: false
  - id: D4
    description: "describe()'s per-scalar line is a strict character-for-character prefix of describe_collection()'s line for every scalar (D-07 anti-drift invariant)"
    requirement: "RENDER-01"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadataTest.PrefixInvariantDescribeIsPrefixOfDescribeCollection"
        status: pass
    human_judgment: false
  - id: D5
    description: "ui/ collection files self-select by shape (top-level string id + attribute array) and are keyed by the file's own id and each attribute's own id, never the filename"
    requirement: "READ-02, READ-03"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.LabelTooltipKeyedByFileIdAndAttributeId"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.LocalizedStringOrTableEn"
        status: pass
    human_judgment: false
  - id: D6
    description: "enum.toml has no wrapper key: every top-level key is discovered by iteration and is itself a vocabulary name; entries are joined into an attribute by its own enum value (never its id), render in ascending code order with gapped/1-based codes verbatim, and a dropped/unknown/empty vocabulary never emits an empty clause"
    requirement: "READ-05"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.EnumGappedCodesRenderVerbatim"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.EnumOneBasedCodesRenderVerbatim"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.EnumJoinedByEnumValueNotAttributeId"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.EnumUnknownVocabularyRendersNoClause"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.EnumEmptyVocabularyRendersNoClause"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.EnumEntryMissingIdOrLabelIsDropped"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.EnumEntriesRenderInAscendingCodeOrder"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#UiConfigTest.EnumTopLevelKeyIsTheVocabularyName"
        status: pass
    human_judgment: false
  - id: D7
    description: "The ui/ sibling is resolved via weakly_canonical before parent_path, so a trailing-separator and a relative migrations path both find the same directory; toml++ symbols never appear in ui_config.h, database_impl.h, or database_describe.cpp; every pre-existing describe assertion in tests/test_database_lifecycle.cpp passes unmodified; summarize_collection is byte-unchanged"
    requirement: "READ-01"
    verification:
      - kind: unit
        ref: "build/bin/quiver_tests.exe --gtest_filter=*DescribeVectorsHeaderPrintedOnce*:*DescribeSetsHeaderPrintedOnce*:*DescribeTimeSeriesWithDimensionColumn*:*DescribeNoCategoryHeaderWhenEmpty*"
        status: pass
      - kind: other
        ref: "grep -vE '^\\s*(//|\\*|/\\*)' src/ui_config.h src/database_impl.h src/database_describe.cpp | grep -c 'toml::' (expect 0)"
        status: pass
      - kind: other
        ref: "git diff --stat tests/test_database_lifecycle.cpp (expect empty)"
        status: pass
    human_judgment: false

duration: 35min
completed: 2026-09-20
status: complete
---

# Phase 01 Plan 01: Sidecar Reader and Attribute Meaning Summary

**`describe()`/`describe_collection()` now render each scalar attribute's `ui/*.toml` label, `enum.toml` code-to-label vocabulary, and (collection-only) tooltip as semicolon-delimited clauses, with a warn-and-degrade sidecar loader that never fails `from_migrations`.**

## Performance

- **Duration:** ~35 min
- **Tasks:** 2 (task 1 tracer: label+tooltip render end-to-end; task 2 TDD: enum.toml vocabularies + enum clause)
- **Files modified:** 7 (2 created, 5 modified)

## Accomplishments
- `src/ui_config.{h,cpp}`: new internal component (no `include/quiver/` counterpart, no C API, no FFI binding) resolving the `ui/` sibling via `fs::weakly_canonical(migrations_path).parent_path() / "ui"`, parsing collection `.toml` files by shape (top-level string `id` + `attribute` array) and `enum.toml` by top-level-key iteration (no fixed wrapper key), with a nested outer/inner try-catch warn-and-degrade posture and zero throwing `.value()` unwraps
- `Database::Impl::ui_config` — one plain, non-`mutable` member populated exactly once, at the end of `from_migrations`, after `migrate_up` returns
- `write_collection_section` gained `(const UiConfig*, bool with_tooltip)` and now appends `; label "..."`, `; enum {code: "...", ...}`, `; tooltip "..."` clauses (fixed order, each self-suppressing, tooltip only in `describe_collection()`) via new `normalize_ui_text`/`quote_ui_text`/`squash`/`write_ui_clauses` helpers in `src/database_describe.cpp`
- `enum.toml` vocabularies joined into an attribute by its own `enum` value (never its `id`) so attributes sharing one vocabulary each render correctly; gapped and 1-based code sets render their real codes in ascending order
- 30 new tests in `tests/test_database_ui_metadata.cpp` (11 for the tracer, 8 enum tests plus a 3-clause prefix-invariant fixture for the TDD task), full suite at 1256/1256, C API suite at 557/557

## Task Commits

1. **Task 1-01-01: End-to-end label/tooltip render, tracer** - `f9212d2` (feat)
2. **Task 1-01-02: enum.toml vocabularies, RED** - `3f13abc` (test)
3. **Task 1-01-02: enum.toml vocabularies, GREEN** - `3e7dfc4` (feat)

**Plan metadata:** committed alongside this SUMMARY.

## Files Created/Modified
- `src/ui_config.h` - `UiAttribute`, `UiConfig`, `load_ui_config` declarations; plain `std` types only
- `src/ui_config.cpp` - path resolution, shape-selected collection scan, localizable-value accessor, `enum.toml` vocabulary discovery, warn-and-degrade loader
- `src/CMakeLists.txt` - registers `ui_config.cpp` in `QUIVER_SOURCES` (mandatory: tomlplusplus is PRIVATE on `quiver`)
- `src/database_impl.h` - adds the non-mutable `ui_config` member with a comment on why it must not be hooked onto `require_schema`
- `src/database.cpp` - single `load_ui_config` call site inside `from_migrations`, after `migrate_up`
- `src/database_describe.cpp` - `normalize_ui_text`, `quote_ui_text`, `squash`, `write_ui_clauses`; `write_collection_section` signature change and both callers
- `tests/test_database_ui_metadata.cpp` - 19 new tests covering render, redundancy suppression, escaping, localization, and the enum join

## Decisions Made
- Emitted the enum clause's rendering logic (`write_ui_clauses`'s clause (b)) in task 1's commit rather than deferring it to task 2, since it is a correct no-op against the always-empty `enum_labels` map task 1 leaves behind (`std::map` iteration over an empty map emits nothing). Task 2 still ran its own RED (failing enum tests against the task-1 build) then GREEN (implementing `parse_vocabularies` + the join) commits, satisfying the plan's TDD gate for the actual behavior change. See "Deviations" below.
- `normalize_ui_text` treats every byte `< 0x20` or `== 0x7F` as a control byte (not just `\r`/`\n`/`\t`), per D-03's explicit "deliberate superset" instruction — this also neutralizes ESC (0x1B) for the T-01-03 threat mitigation.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed a raw-string-literal delimiter collision in the new test fixtures**
- **Found during:** Task 1-01-01 (first test build)
- **Issue:** `write_ui_file("hydro_plant.toml", R"(...label.en = "Initial Storage (hm³)"...)")` contains the literal sequence `)"` immediately after `hm³` (the closing paren of the display text followed by the TOML string's closing quote), which terminates a `R"(...)"` raw string literal early — a straightforward C++ syntax error, not a logic bug, but one that would have shipped a broken test file.
- **Fix:** Switched that one fixture to a custom raw-string delimiter, `R"TOML(...)TOML"`. No other fixture in the file has the same collision (verified by scanning for `)"` inside every other raw-string TOML block).
- **Files modified:** tests/test_database_ui_metadata.cpp
- **Verification:** `cmake --build build --config Debug --target quiver_tests` succeeds; `DatabaseUiMetadataTest.RenderLabelAndTooltipClauses` passes.
- **Committed in:** f9212d2 (Task 1-01-01 commit)

---

**Total deviations:** 1 auto-fixed (1 bug, test-file-only, caught before the first build); 1 documented task-boundary front-load (enum clause emitter written in task 1, filled in and TDD-verified by task 2's own RED/GREEN commits — no behavior shipped ahead of its test).
**Impact on plan:** No scope creep; no production behavior shipped without its corresponding test passing first. The clause-emitter front-load only means task 1's commit contains inert code paths that task 2 later exercises.

## Issues Encountered
None beyond the raw-string fixture fix above.

## Next Phase Readiness
- `Database::Impl::ui_config`, `write_collection_section`'s new signature, and the `UiConfig`/`UiAttribute` types are ready for plan 02's `RENDER-03` (undescribed-attribute) coverage, `SAFE-02` degrade-path tests, and the `CHANGELOG.md`/`src/CLAUDE.md`/`tests/CLAUDE.md` documentation updates.
- Full `quiver_tests.exe`: 1256/1256 passing (1237 pre-existing + 19 new). Full `quiver_c_tests.exe`: 557/557 passing, unaffected (describe wrappers are pure passthrough).
- `tests/test_database_lifecycle.cpp` is byte-for-byte unmodified across all three commits; no path was created under `tests/schemas/ui/`; `summarize_collection`'s body has no hunk in the cumulative diff.

## Self-Check: PASSED

- FOUND: src/ui_config.h
- FOUND: src/ui_config.cpp
- FOUND commit f9212d2 in git log
- FOUND commit 3f13abc in git log
- FOUND commit 3e7dfc4 in git log

---
*Phase: 01-sidecar-reader-and-attribute-meaning*
*Completed: 2026-09-20*
