---
phase: 01-enum-labels-in-describe
plan: 02
subsystem: testing
tags: [toml, tomlplusplus, sqlite, describe, gtest, fixtures]

# Dependency graph
requires:
  - phase: 01-01
    provides: "src/ui_config.{h,cpp} parser, Database::has_ui_config(), tests/test_ui_fixture.h open_ui_fixture helper, tests/schemas/ui/{enum_basic,malformed,no_ui_dir}/"
provides:
  - "tests/schemas/ui/{bess_like,foresight_like,htd_like,no_enum,empty_enum,unknown_keys,format_table,orphan_collection}/ -- eight new hand-written fixture directories, one per parser tolerance"
  - "tests/schemas/ui/README.md -- directory-to-rule map, the phase's authoritative fixture-literal table mirrored verbatim, documented non-requirements"
  - "tests/test_database_ui_corpus.cpp (DatabaseUiCorpus suite) -- structural-completeness walk, schema-load proof, README cross-check, fixture-literal drift gate, binding-leak guard"
affects: [01-03, 01-04, 01-05, 01-06]

# Actuals (#2632)
actuals:
  tokens: 7635
  tasks: 2
  commits: 2

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Sorted std::filesystem::directory_iterator walk over tests/schemas/ui/ for platform-stable fixture enumeration (DatabaseUiCorpus)"
    - "recursive_directory_iterator with it.disable_recursion_pending() to prune build/.dart_tool/node_modules/target while checking bindings/ is never a UI-fixture host"
    - "tests/schemas/ui/**/*.toml and *.md pinned to eol=lf in .gitattributes, mirroring the existing ui_golden/*.txt rule, so byte-pinned literals survive a Windows checkout"

key-files:
  created:
    - tests/schemas/ui/bess_like/schema.sql
    - tests/schemas/ui/bess_like/ui/main.toml
    - tests/schemas/ui/bess_like/ui/enum.toml
    - tests/schemas/ui/bess_like/ui/storage.toml
    - tests/schemas/ui/foresight_like/schema.sql
    - tests/schemas/ui/foresight_like/ui/main.toml
    - tests/schemas/ui/foresight_like/ui/enum.toml
    - tests/schemas/ui/foresight_like/ui/economic_driver.toml
    - tests/schemas/ui/htd_like/schema.sql
    - tests/schemas/ui/htd_like/ui/main.toml
    - tests/schemas/ui/htd_like/ui/enum.toml
    - tests/schemas/ui/htd_like/ui/hydro_plant.toml
    - tests/schemas/ui/htd_like/ui/thermal_plant.toml
    - tests/schemas/ui/no_enum/schema.sql
    - tests/schemas/ui/no_enum/ui/main.toml
    - tests/schemas/ui/no_enum/ui/storage.toml
    - tests/schemas/ui/empty_enum/schema.sql
    - tests/schemas/ui/empty_enum/ui/main.toml
    - tests/schemas/ui/empty_enum/ui/enum.toml
    - tests/schemas/ui/empty_enum/ui/storage.toml
    - tests/schemas/ui/unknown_keys/schema.sql
    - tests/schemas/ui/unknown_keys/ui/main.toml
    - tests/schemas/ui/unknown_keys/ui/enum.toml
    - tests/schemas/ui/unknown_keys/ui/storage.toml
    - tests/schemas/ui/format_table/schema.sql
    - tests/schemas/ui/format_table/ui/main.toml
    - tests/schemas/ui/format_table/ui/storage.toml
    - tests/schemas/ui/format_table/ui/empty_collection.toml
    - tests/schemas/ui/orphan_collection/schema.sql
    - tests/schemas/ui/orphan_collection/ui/main.toml
    - tests/schemas/ui/orphan_collection/ui/enum.toml
    - tests/schemas/ui/orphan_collection/ui/storage.toml
    - tests/schemas/ui/orphan_collection/ui/agent.toml
    - tests/schemas/ui/README.md
    - tests/test_database_ui_corpus.cpp
  modified:
    - tests/CMakeLists.txt
    - .gitattributes

key-decisions:
  - "format_table/ needed two collection TOML files, not one: storage.toml carries the three format-form variants, empty_collection.toml (new, not in the plan's frontmatter files_modified list) carries the zero-[[attribute]]-blocks edge the plan's <action> text explicitly asks for. Both are listed in ui/main.toml's collections array and both are real Storage/EmptyCollection tables in schema.sql."
  - "htd_like/ui/thermal_plant.toml's lone [[attribute]] block indents its id/label keys by four spaces (TOML tolerates leading key whitespace) specifically so the file's only top-level, column-0 'id = ' line is the one the collection-level id key would have occupied -- required to make the acceptance criterion's grep -cE '^id = ' return exactly 0 while the file stays a syntactically normal attribute block."
  - "Pinned tests/schemas/ui/**/*.toml and *.md to eol=lf in .gitattributes (mirroring 01-01's ui_golden/*.txt rule). Without it, local core.autocrlf=true silently re-introduces CRLF on the next checkout -- confirmed via 'git add' warnings before the fix -- which would corrupt every grep-based and byte-exact acceptance criterion in this plan and the downstream literal-pinning test, even though the files were authored as pure LF."
  - "Added the literal string enum = \"model\" verbatim into README.md's foresight_like row (not just the rendered enum model {...} form already covered by literal L8) because DatabaseUiCorpus.FixtureLiteralsArePinned's plan-specified literal set names that exact TOML-syntax string as one of the ten pinned literals, and it did not previously appear anywhere in the README's prose."

requirements-completed: [CORPUS-01, CORPUS-02, CORPUS-03]

coverage:
  - id: D1
    description: "Eight new fixture directories exist under tests/schemas/ui/, each pinning exactly one parser tolerance (or the real BESSOperation/Foresight/HydroThermalDispatch multi-tolerance case), every schema.sql satisfying SchemaValidator"
    requirement: "CORPUS-01"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_corpus.cpp#DatabaseUiCorpus.EveryFixtureIsStructurallyComplete"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_corpus.cpp#DatabaseUiCorpus.EveryFixtureSchemaLoads"
        status: pass
    human_judgment: false
  - id: D2
    description: "The corpus walk is directory-order-independent (sorted before iteration) and concurrency-safe (per-fixture database stem via open_ui_fixture), so ctest -j cannot race two cases on one file"
    requirement: "CORPUS-02"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_corpus.cpp#DatabaseUiCorpus.EveryFixtureSchemaLoads (per-name db_stem = cpp_corpus_<name>)"
        status: pass
    human_judgment: false
  - id: D3
    description: "No UI fixture file (main.toml/enum.toml) has been copied into any bindings/ directory; every binding suite references tests/schemas/ui/ by relative path"
    requirement: "CORPUS-03"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_corpus.cpp#DatabaseUiCorpus.FixturesAreNeverCopiedIntoABinding"
        status: pass
    human_judgment: false
  - id: D4
    description: "tests/schemas/ui/README.md maps every fixture directory to the rule it pins and reproduces the phase's authoritative fixture-literal table verbatim; a literal edited out of either the fixture file or the README fails the suite naming the file"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_corpus.cpp#DatabaseUiCorpus.ReadmeNamesEveryFixture"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_corpus.cpp#DatabaseUiCorpus.FixtureLiteralsArePinned"
        status: pass
    human_judgment: false

# Metrics
duration: 10min
completed: 2026-09-19
status: complete
---

# Phase 1 Plan 2: UI Sidecar Fixture Corpus Summary

**Eight hand-written fixture directories (BESSOperation/Foresight/HydroThermalDispatch-derived plus five synthetic tolerance cases) under `tests/schemas/ui/`, a README mapping each to the rule it pins, and a five-test `DatabaseUiCorpus` gtest suite that walks, opens, cross-checks, and byte-pins every one of them.**

## Performance

- **Duration:** ~10 min
- **Tasks:** 2/2 completed
- **Files modified:** 37 (35 created, 2 modified)

## Accomplishments
- `bess_like/` reproduces BESSOperation's real `storage.toml` collision: `degradation` as both an `[[attribute_group]]` id (`Degradation Curve`) and an `[[attribute]]` id (`Degradation Rate`) with genuinely different SQL-column-backed labels, interleaved with a second group block and two trailing attributes — plus a gapped/negative/int64-max `look_ahead` vocabulary (`-1, 0, 3, 7, 9223372036854775807`). Group value columns are named `rate`/`efficiency`, never `degradation`/`rte`, so `SchemaValidator`'s duplicate-attribute check (`src/schema_validator.cpp:284-288`) doesn't reject the schema.
- `foresight_like/` reproduces Foresight's mixed-locale `enum.toml` (dotted `label.en`/`es`/`pt`, a bare `"ARIMA"` sandwiched between two dotted entries, and an `es`+`pt`-only entry with no `en`) with two accented labels — `Seasonal Naïve` and `Regresión Lineal` — both confirmed to resolve at the Phase-1-hardcoded `en` locale (exact-locale leg and first-key-in-map-order fallback leg respectively), and the `model` vocabulary is actually bound via `forecast_model`'s `enum = "model"`, so every downstream assertion has something real to read.
- `htd_like/` reproduces HydroThermalDispatch's plain `[[attribute]] id = "date_time"` spelling and PascalCase-over-snake_case filename mapping, with a `bool` vocabulary spelled `Disable`/`Enable` (deliberately distinct from `enum_basic`'s `Disabled`/`Enabled`), plus a listed-but-`id`-less `thermal_plant.toml` proving PARSE-10's skip-not-fatal path.
- `no_enum/`, `empty_enum/`, `unknown_keys/`, `format_table/`, `orphan_collection/` each pin one remaining tolerance: absent vs. zero-byte `enum.toml`, unknown keys at all three TOML levels, the synthetic `format` 4-key table form plus a zero-attribute collection, and SCE's real orphan `agent.toml` pattern.
- `tests/test_database_ui_corpus.cpp` adds `DatabaseUiCorpus` (5 tests): a sorted structural-completeness walk, a schema-load proof that opens every fixture (including `malformed`) through `describe()` without throwing, a README-completeness cross-check, a byte-exact literal-drift gate covering 10 of the phase's 16 authoritative literals, and a `bindings/`-walk proving no fixture file has leaked into a binding — with recursion pruned past `build`/`.dart_tool`/`node_modules`/`target`.
- All three of the plan's "must actually fail" proofs were run and confirmed this session (then restored): editing `Degradation Rate` out of `bess_like/ui/storage.toml` fails `FixtureLiteralsArePinned` naming the file; removing `htd_like/ui/main.toml` fails `EveryFixtureIsStructurallyComplete` naming `htd_like`; planting a `ui/main.toml` under `bindings/js/test/` fails `FixturesAreNeverCopiedIntoABinding` naming the planted path.

## Task Commits

1. **Task 1: Author the eight remaining fixture directories** - `872d9c9` (test)
2. **Task 2: Corpus walk guard and the never-copied-into-a-binding check** - `cdbe7e3` (test)

## Files Created/Modified

- `tests/schemas/ui/bess_like/{schema.sql,ui/{main,enum,storage}.toml}` - CORPUS-01 BESSOperation case, PARSE-03/07/08
- `tests/schemas/ui/foresight_like/{schema.sql,ui/{main,enum,economic_driver}.toml}` - CORPUS-01 Foresight case, PARSE-04, the encoding fixture
- `tests/schemas/ui/htd_like/{schema.sql,ui/{main,enum,hydro_plant,thermal_plant}.toml}` - CORPUS-01 HydroThermalDispatch case, PARSE-10
- `tests/schemas/ui/no_enum/{schema.sql,ui/{main,storage}.toml}` - PARSE-09 (absent enum.toml)
- `tests/schemas/ui/empty_enum/{schema.sql,ui/{main,enum,storage}.toml}` - PARSE-09 (zero-byte enum.toml)
- `tests/schemas/ui/unknown_keys/{schema.sql,ui/{main,enum,storage}.toml}` - PARSE-05
- `tests/schemas/ui/format_table/{schema.sql,ui/{main,storage,empty_collection}.toml}` - PARSE-06
- `tests/schemas/ui/orphan_collection/{schema.sql,ui/{main,enum,storage,agent}.toml}` - PARSE-01
- `tests/schemas/ui/README.md` - directory-to-rule map, verbatim `## Rendered literals` table, documented non-requirements
- `tests/test_database_ui_corpus.cpp` - `DatabaseUiCorpus` suite (5 tests)
- `tests/CMakeLists.txt` - registers the new test file
- `.gitattributes` - pins `tests/schemas/ui/**/*.toml` and `*.md` to `eol=lf`

## Decisions Made

- `format_table/` needed a second collection file (`ui/empty_collection.toml`) beyond what the plan's frontmatter `files_modified` list enumerated, because the plan's `<action>` text explicitly requires "a collection file with zero `[[attribute]]` blocks" as a separate edge from the three format-variant attributes, and those can't coexist in one file.
- `htd_like/ui/thermal_plant.toml`'s attribute block indents its `id`/`label` keys by four spaces — valid TOML, and the only way to make `grep -cE '^id = '` return exactly 0 (proving no collection-level `id` key) while the file still declares one ordinary, syntactically normal `[[attribute]]` block.
- Added `.gitattributes` `eol=lf` pins for the new fixture TOML/MD files, mirroring 01-01's precedent for `ui_golden/*.txt`, after observing `git add` warn that the working tree's `core.autocrlf=true` would silently reintroduce CRLF on next checkout — which would have broken every literal-based acceptance criterion in this plan despite the files being authored as pure LF.
- Added the raw literal `enum = "model"` into README.md (previously only the rendered `enum model {...}` form, literal L8, was present) so `FixtureLiteralsArePinned`'s plan-specified 10-literal set has a source for that entry.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Added a second format_table collection file for the zero-attribute edge**
- **Found during:** Task 1
- **Issue:** The plan's `<action>` text for `format_table/` requires both three format-variant attributes and "a collection file with zero `[[attribute]]` blocks", but the plan's frontmatter `files_modified` list only names one collection TOML (`storage.toml`) for that directory — the two requirements cannot share one file.
- **Fix:** Added `ui/empty_collection.toml` (an `EmptyCollection` table with a collection-level `id`/`label` and no `[[attribute]]` blocks at all) and listed it in `ui/main.toml`'s `collections` array alongside `storage`.
- **Files modified:** `tests/schemas/ui/format_table/ui/empty_collection.toml`, `tests/schemas/ui/format_table/ui/main.toml`, `tests/schemas/ui/format_table/schema.sql`
- **Verification:** `DatabaseUiCorpus.EveryFixtureSchemaLoads` opens `format_table` successfully; `describe()` is non-empty.
- **Committed in:** `872d9c9` (Task 1 commit)

**2. [Rule 2 - Missing Critical] Pinned new fixture TOML/MD files to LF in `.gitattributes`**
- **Found during:** Task 1, immediately before the first `git add`
- **Issue:** The plan explicitly requires "Write every `.toml` and `.md` fixture file with LF line endings" because the accented literals are byte-asserted downstream, but `.gitattributes` only forced `eol=lf` for `.cpp/.dart/.h/.jl/.py` and the pre-existing `ui_golden/*.txt` — nothing covered the new `tests/schemas/ui/**/*.toml`/`*.md` files. `git add` warned that the local `core.autocrlf=true` would reintroduce CRLF on the next checkout, which would silently corrupt the exact-byte and `^...$`-anchored grep acceptance criteria this same plan specifies.
- **Fix:** Added `tests/schemas/ui/**/*.toml text eol=lf` and `tests/schemas/ui/**/*.md text eol=lf` to `.gitattributes`, then `git add --renormalize` to confirm the warnings cleared.
- **Files modified:** `.gitattributes`
- **Verification:** Re-running `git add` produced no CRLF warnings; `FixtureLiteralsArePinned` still passes with binary-mode reads.
- **Committed in:** `872d9c9` (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (2 missing critical — both required for the plan's own acceptance criteria to hold, not scope creep).
**Impact on plan:** Neither changes any rule the fixtures pin; both close gaps between the plan's prose requirements and its own file-list bookkeeping / cross-platform byte-exactness guarantee.

## Issues Encountered

None blocking. Confirmed one edge case while designing `bess_like/schema.sql`: `Storage_vector_degradation`'s table name itself contains the substring "degradation" immediately followed by `" ("`, which does not match the acceptance criterion's `degradation (REAL|INTEGER)` regex (which requires a literal `REAL` or `INTEGER` token after the space) — so the criterion correctly counts only the scalar column line, as intended.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- All eleven fixture directories under `tests/schemas/ui/` (three from 01-01, eight from this plan) are structurally complete, schema-load-proven, and README-documented. `tests/schemas/ui/README.md`'s `## Rendered literals` table is the single place plans 01-03/01-04/01-05/01-06 should quote from, and `DatabaseUiCorpus.FixtureLiteralsArePinned` will fail loudly (naming the file) if any of the 10 literals it currently covers drifts — note it covers 10 of the table's 16 rows (L9, L10, L11 (both halves), L13 (partial: `Measurement Date` only, not the full `Hydro Plants`/`enum bool {...}` line), plus enum_basic's L3/L15 halves); L1, L2, L4-L7, L12, L14, L16 and the rest of L13 are pinned only by the plan text and by whichever downstream plan first asserts them.
- `foresight_like`'s `model` vocabulary is genuinely bound (`forecast_model` → `enum = "model"`), so 01-04's `MixedLocaleFormsResolveInOneVocabulary` test has real rendered output to assert against, not a dead vocabulary.
- No blockers for 01-03 (full scalar-line rendering) or the later plans — this plan built only test fixtures and one new gtest file; no `src/`/`include/`/`bindings/` file was touched (confirmed via `git status --porcelain -- src/ include/ bindings/` returning empty both mid-plan and at completion).

## Self-Check: PASSED

- FOUND: tests/schemas/ui/bess_like/ui/storage.toml
- FOUND: tests/schemas/ui/foresight_like/ui/enum.toml
- FOUND: tests/schemas/ui/htd_like/ui/thermal_plant.toml
- FOUND: tests/schemas/ui/empty_enum/ui/enum.toml (0 bytes)
- FOUND: tests/schemas/ui/orphan_collection/ui/agent.toml
- FOUND: tests/schemas/ui/README.md
- FOUND: tests/test_database_ui_corpus.cpp
- FOUND commit: 872d9c9
- FOUND commit: cdbe7e3

---
*Phase: 01-enum-labels-in-describe*
*Completed: 2026-09-19*
