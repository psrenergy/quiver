---
phase: 01-enum-labels-in-describe
plan: 03
subsystem: database
tags: [describe, sqlite, gtest, toml]

# Dependency graph
requires:
  - phase: 01-01
    provides: "src/ui_config.{h,cpp} parser, Database::Impl::require_ui_config(), tests/test_ui_fixture.h, tests/schemas/ui_golden/, tests/schemas/ui/{enum_basic,malformed,no_ui_dir}/"
  - phase: 01-02
    provides: "tests/schemas/ui/{bess_like,foresight_like,htd_like,no_enum,empty_enum,unknown_keys,format_table,orphan_collection}/, tests/schemas/ui/README.md"
provides:
  - "src/database_describe.cpp -- kEmDash, write_ui_header, append_collection_label, append_scalar_ui_clauses; write_collection_section takes a nullable UIConfigSet*"
  - "UI config header line in describe()/describe_collection()/summarize_collection()"
  - "Collection label clause on every Collection: line"
  - "Scalar-line unit/[hidden]/label/vocabulary clauses in the fixed D-02 order"
affects: [01-04, 01-05, 01-06]

# Actuals (#2632)
actuals:
  tokens: 6300
  tasks: 3
  commits: 3

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Explicit UTF-8 byte-escape constant (kEmDash) for a non-ASCII literal, avoiding MSVC's default-source-charset hazard (no /utf-8 flag in cmake/CompilerOptions.cmake)"
    - "One shared write_collection_section renderer parameterized by a nullable UIConfigSet*, called identically from describe() and describe_collection() -- never forked (D-03)"
    - "Scratch, test-local schema+ui/ sidecar (ScratchSidecarDir, RAII-cleaned) for a clause combination the tracked fixture corpus does not isolate, keeping Task 3 scoped to the test file only"

key-files:
  created: []
  modified:
    - src/database_describe.cpp
    - tests/test_database_ui_describe.cpp

key-decisions:
  - "The 4 scalar-line clauses (append_scalar_ui_clauses) were implemented in the same edit as the header/collection-label clauses (Task 1's commit) rather than split into Task 2's commit, because both additions share the same write_collection_section edit site and splitting them would have meant a half-working intermediate commit. Task 2's commit is therefore test-only, pinning the clause behavior that landed one commit earlier than the plan's task boundary implied."
  - "Task 3's five clause-combination cases: enum_basic's storage.toml has no attribute that isolates 'unit present, label absent' (every unit-bearing attribute there also carries a label) -- confirmed by grepping every 'unit =' occurrence across the whole tests/schemas/ui/ corpus, all two of which pair with a label. Since Task 3 is scoped to tests/test_database_ui_describe.cpp only (no fixture corpus edit), the four non-vocabulary combinations (unit-only, label-only, both, neither) are proven against a small scratch schema+ui/ sidecar written and torn down by the test itself (ScratchSidecarDir), not against a new tracked fixture. The fifth combination (vocabulary resolved vs. unresolved) uses the real htd_like/no_enum fixtures exactly as the plan specifies."

requirements-completed: [DESC-02, DESC-03, DESC-04, DESC-05, DESC-06]

coverage:
  - id: D1
    description: "All three reports carry UI config: <path> (locale: en) when a sidecar loaded; describe_collection/summarize_collection have it as line 1, describe() has it on line 3"
    requirement: "DESC-05"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.HeaderLineInAllThreeReports"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.HeaderAbsentWithoutSidecar"
        status: pass
    human_judgment: false
  - id: D2
    description: "A collection with a UI label renders it on its Collection: line in all three reports, via the one shared write_collection_section renderer"
    requirement: "DESC-04"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.CollectionLabelRendered"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.DescribeUsesTheSameRenderer"
        status: pass
    human_judgment: false
  - id: D3
    description: "Unit and label render together in the fixed order, verbatim (literal L4); a hidden attribute is tagged [hidden] and never dropped (literal L5, DESC-06)"
    requirement: "DESC-02, DESC-06"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.UnitAndLabelRendered"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.HiddenAttributeTaggedNotDropped"
        status: pass
    human_judgment: false
  - id: D4
    description: "An enum attribute's line lists every declared vocabulary entry in declaration order, independent of data (even with zero elements); an unresolvable binding renders (undeclared vocabulary) rather than silence"
    requirement: "DESC-03"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.VocabularyFullValueListRendered"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.UndeclaredVocabularyNamed"
        status: pass
    human_judgment: false
  - id: D5
    description: "An unconfigured attribute (id, label) and a configured-with-empty-label attribute (notes) both render today's line exactly, with no em-dash clause -- the attribute id is never synthesised into a missing label (D-12)"
    requirement: "DESC-05"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.UnconfiguredAttributeRendersTodaysLine"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.EmptyLabelRendersTodaysLine"
        status: pass
    human_judgment: false
  - id: D6
    description: "Every clause combination (unit-only, label-only, both, neither, vocabulary resolved vs. unresolved) renders as exactly one well-formed line with no doubled or trailing separator"
    requirement: "DESC-02, DESC-03, DESC-04"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.ClauseCombinationsHaveNoDoubledOrTrailingSeparator"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.VocabularyPresentWithAndWithoutResolution"
        status: pass
    human_judgment: false
  - id: D7
    description: "The label clause inside summarize_collection()'s histogram lives inside the kMaxDistributionCardinality=64 boundary; describe_collection()'s declared-vocabulary list is independent of that boundary"
    requirement: "DESC-02"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.CardinalityAt64RendersLabels"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.CardinalityAt65SuppressesHistogram"
        status: pass
    human_judgment: false
  - id: D8
    description: "A no-sidecar database's three reports remain byte-identical to the pre-change goldens after every render clause added by this plan"
    requirement: "DESC-05"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_describe.cpp#DatabaseUiDescribe.NoSidecarOutputStillMatchesGolden"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_golden.cpp#DatabaseUiGolden.MemoryDescribeByteIdentical"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_golden.cpp#DatabaseUiGolden.FileBackedWithoutSidecarByteIdentical"
        status: pass
    human_judgment: false

# Metrics
duration: 35min
completed: 2026-09-19
status: complete
---

# Phase 1 Plan 3: Enum Labels in Describe (Full Render) Summary

**`src/database_describe.cpp` gains the `UI config:` header line, a collection-label clause, and the four scalar-line clauses (unit/`[hidden]`/label/vocabulary) — every one gated on a loaded `UIConfigSet*` so the no-sidecar goldens captured before this plan stay byte-identical, proven by 23 new/extended `DatabaseUiDescribe` cases and a green 1140-test full suite.**

## Performance

- **Duration:** ~35 min
- **Tasks:** 3/3 completed
- **Files modified:** 2 (0 created, 2 modified)

## Accomplishments
- `kEmDash` (explicit `"\xE2\x80\x94"` UTF-8 byte constant), `write_ui_header`, `append_collection_label`, and `append_scalar_ui_clauses` added to `src/database_describe.cpp`'s anonymous namespace; `write_collection_section` gained a nullable `const UIConfigSet*` parameter and is still the single renderer called from both `describe()` and `describe_collection()` (D-03 — grepped: appears once, called from two sites).
- All three public report methods (`describe`, `describe_collection`, `summarize_collection`) now call `impl_->require_ui_config()` and emit the `UI config: <path> (locale: en)` header exactly where D-05/D-06 specify: line 3 of `describe()` (after `Database:`/`Version:`), line 1 of the other two.
- The scalar loop inside `write_collection_section` appends, in the fixed D-02 order, unit → `[hidden]` → label → vocabulary, each independently guarded on `find_attribute` returning non-null and the specific field being non-empty/true — so an unconfigured or partially-configured attribute renders exactly as many clauses as it declares, never more, never a dangling separator.
- The vocabulary clause resolves through `find_vocabulary`: a resolved name renders the full declared `{code: label, ...}` list in `enum.toml` declaration order (independent of data, proven at both 0 and 64/65 elements); an unresolved name renders `(undeclared vocabulary)` — never silence, never an invented label.
- 23 `DatabaseUiDescribe` test cases now cover: the header in all three reports and its absence without a sidecar; the collection label; unit+label together (literal L4); `[hidden]` tagging without dropping the attribute (literal L5); the full vocabulary list at zero elements (literal L3); unconfigured (`id`/`label`) and empty-label (`notes`, literal L7) attributes staying byte-identical to today; an undeclared vocabulary (literal L15); `describe()`/`describe_collection()` sharing one renderer; all five clause combinations with exact full-line assertions; the 64/65 `kMaxDistributionCardinality` boundary; and a final byte-identity re-check against the `ui_golden` baselines.
- Ran the plan's narrowed task-3 gate (`DatabaseUiDescribe.*:DatabaseUiGolden.*:DatabaseDescribe.*:LuaRunnerTest.*`, 212 tests) and, as a final sanity check since 01-02's `DatabaseUiCorpus` has now landed, the whole `quiver_tests.exe` binary (1140 tests, 42 suites) — both green.

## Task Commits

1. **Task 1: UI config header line and the collection label clause** - `5973651` (feat) — this commit also carries the four scalar-line clauses (`append_scalar_ui_clauses`), implemented alongside the header/label work since both share the same `write_collection_section` edit site (see Decisions Made).
2. **Task 2: The four scalar-line clauses — unit, hidden, label and vocabulary** - `1f9f67d` (test) — test-only; pins the render behavior that landed in the Task 1 commit.
3. **Task 3: Clause-combination, cardinality-boundary and byte-identity guards** - `9caa922` (test)

## Files Created/Modified

- `src/database_describe.cpp` - `kEmDash`, `write_ui_header`, `append_collection_label`, `append_scalar_ui_clauses`; `write_collection_section` gains a nullable `UIConfigSet*`; all three public methods call `require_ui_config()` and emit the header
- `tests/test_database_ui_describe.cpp` - 15 new test cases (`HeaderLineInAllThreeReports`, `HeaderAbsentWithoutSidecar`, `CollectionLabelRendered`, `UnitAndLabelRendered`, `HiddenAttributeTaggedNotDropped`, `VocabularyFullValueListRendered`, `UnconfiguredAttributeRendersTodaysLine`, `EmptyLabelRendersTodaysLine`, `UndeclaredVocabularyNamed`, `DescribeUsesTheSameRenderer`, `ClauseCombinationsHaveNoDoubledOrTrailingSeparator`, `VocabularyPresentWithAndWithoutResolution`, `CardinalityAt64RendersLabels`, `CardinalityAt65SuppressesHistogram`, `NoSidecarOutputStillMatchesGolden`), plus test-local helpers `split_lines`, `find_line`, `count_occurrences`, `kEmDash`, and `ScratchSidecarDir`

## Decisions Made

- The four scalar-line clauses were coded together with the header/collection-label clauses in one edit to `write_collection_section`, landing in Task 1's commit rather than Task 2's, because both changes touch the same function body and a mid-function split would have meant committing dead/unused code paths. Task 2's commit is test-only as a result — it pins the scalar-clause behavior with its own dedicated test cases, one commit after the code that implements it, rather than the code and its first tests landing together. All plan-specified tests for both tasks pass; no behavior differs from what the plan describes.
- Task 3's fifth clause combination ("vocabulary present with a label, and vocabulary present without one") is read as vocabulary-resolved-vs-unresolved (`htd_like`'s `has_commitment`, which has both a label and a resolving `bool` vocabulary, versus `no_enum`'s `has_commitment`, same shape but no `enum.toml` to resolve against) rather than label-present-vs-absent, because every vocabulary-bound attribute in the entire `tests/schemas/ui/` corpus carries a label (confirmed by grep) — there is no fixture example of "vocabulary present, label absent". This reading is consistent with the plan's own supporting sentence ("whose TOML declares a vocabulary and \[...\] a label.en, versus no_enum's \[...\] bound attribute") and with the corpus's actual content.
- Task 3's four non-vocabulary combinations (unit-only, label-only, both, neither) are proven against a scratch schema+`ui/` sidecar (`ScratchSidecarDir`, RAII-written and torn down inside the single test that uses it) rather than against `enum_basic`, because `enum_basic/ui/storage.toml` has no attribute isolating "unit present, label absent" — its one unit-bearing attribute (`max_generation`) also carries a label, and grepping every `unit = ` occurrence across all eleven fixture directories confirms this holds corpus-wide. Task 3's `<files>` frontmatter scopes it to `tests/test_database_ui_describe.cpp` only, so a corpus edit was not an option; the scratch sidecar keeps the proof real (an actual `UIConfigSet::from_directory` parse, not a hand-built struct) while staying inside that scope.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking Issue] No fixture isolates "unit present, label absent" for Task 3's clause-combination case**
- **Found during:** Task 3
- **Issue:** The plan's action text for Task 3 states "The fixture already carries the attributes for the first four \[combinations\]" on `enum_basic`, but `enum_basic/ui/storage.toml` has exactly one unit-bearing attribute (`max_generation`), and it also carries a label — there is no attribute with `unit` set and `label` absent. A corpus-wide grep of every `unit = ` occurrence across all eleven `tests/schemas/ui/*/ui/*.toml` files confirms the same is true everywhere else in the corpus (the only other hit, `unknown_keys/ui/storage.toml`'s `capacity`, also carries a label). Task 3's own `<files>` list scopes it to `tests/test_database_ui_describe.cpp` only, so extending a tracked fixture was out of scope.
- **Fix:** Added a small `ScratchSidecarDir` RAII helper in the test file that writes a throwaway schema.sql + ui/main.toml + ui/storage.toml under a scratch directory (relative to the test binary's cwd) and removes it in its destructor. One test (`ClauseCombinationsHaveNoDoubledOrTrailingSeparator`) uses it to declare four isolated attributes (`unit_only`, `label_only`, `both_present`, `neither_present`) and assert each renders as one clean line via the real `UIConfigSet::from_directory` parse path — no renderer logic is mocked or bypassed.
- **Files modified:** `tests/test_database_ui_describe.cpp` (test-only; no `src/` change)
- **Verification:** `DatabaseUiDescribe.ClauseCombinationsHaveNoDoubledOrTrailingSeparator` passes, asserting all four exact full lines.
- **Committed in:** `9caa922` (Task 3 commit)

---

**Total deviations:** 1 auto-fixed (Rule 3 — blocking issue, test-scope-only fix, no renderer or production behavior changed).
**Impact on plan:** None of the plan's `must_haves` or acceptance criteria were weakened; the substitution only changes which fixture proves one already-specified assertion.

## Issues Encountered

None blocking beyond the deviation above. The narrowed task-3 gtest filter (`DatabaseUiDescribe.*:DatabaseUiGolden.*:DatabaseDescribe.*:LuaRunnerTest.*`) and the full `quiver_tests.exe` run (1140 tests) are both green, so the plan's own note that "the whole suite should also be green" now that 01-02's corpus has landed was confirmed directly rather than left as an open question for 01-04.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `src/database_describe.cpp` now renders the full DESC-02/03/04/06 scalar-line surface and the DESC-05/06 header/label clauses, all gated on a loaded `UIConfigSet*` through the one shared `write_collection_section`. `summarize_collection()`'s own line format is untouched apart from the tracer's histogram-label clause (01-01), per the plan's explicit instruction.
- `tests/schemas/ui_golden/*.txt` remain untouched (`git diff --name-status master...HEAD -- tests/schemas/ui_golden/` shows only `A` lines — all four were added by 01-01, never modified by this plan).
- No file under `bindings/`, `include/quiver/c/`, `src/c/`, or `src/ui_config.{h,cpp}` was touched by any of this plan's three commits (confirmed via `git show --stat` on each).
- Plans 01-04/01-05/01-06 (C API + binding surfaces) can now build on a fully-rendered C++ text-report reference: every literal in `tests/schemas/ui/README.md`'s `## Rendered literals` table that this plan owns (L2-L7, L15) is asserted byte-exact here.

## Self-Check: PASSED

- FOUND: src/database_describe.cpp
- FOUND: tests/test_database_ui_describe.cpp
- FOUND commit: 5973651
- FOUND commit: 1f9f67d
- FOUND commit: 9caa922
