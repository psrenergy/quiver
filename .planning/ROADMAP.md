# Roadmap: Quiver — UI Metadata Layer

## Overview

Quiver learns to read the PSR `database/ui/` TOML sidecar and hand what it finds to the agents
that read a study database. The journey starts at the triggering defect and closes it in one
phase: a C++ parser, a lazy member on `Database::Impl`, and three append-only format strings in
`describe` — which reach all five bindings and Lua for free, because `describe*` already returns a
`std::string` through the existing C API wrappers. That first slice ships as a patch and stands
alone; if the milestone stopped there, the defect an agent hits today would be closed.

Everything after it serves the committed consumer. Phase 2 breaks the `DatabaseOptions` C ABI so
claw can point Quiver at a UI directory its sandbox can actually reach, and pays off the whole
silent-struct-corruption hazard class at the same time. Phase 3 hands claw structured metadata
through a *new* C struct so the 56-byte and 32-byte constants four bindings mirror by hand never
move. Phase 4 adds the collection- and group-level knowledge SQL cannot express. Phase 5 ships
the advisory validator — the first thing in the PSR ecosystem that checks the sidecar against
anything — and takes the milestone through the release ritual.

**Mode:** MVP. Each phase is a vertical slice that a consumer can observe through the bindings,
not a technical layer. There is no "build the parser" phase followed by a "wire the C API" phase.

## Phases

**Phase Numbering:**

- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Enum Labels in Describe** - An agent calling `describe` sees what an INTEGER column means, in all six layers, with no ABI change
- [ ] **Phase 2: Config Path, Locale and Struct-Size Safety** - A consumer points Quiver at any UI directory and locale; a drifted binding layout fails loudly instead of corrupting memory
- [ ] **Phase 3: Structured Attribute Metadata** - claw reads label/tooltip/unit/format/enum as data instead of scraping a text report
- [ ] **Phase 4: Collection and Attribute-Group Metadata** - Display labels, icons, help, `main.collections` order, and one answer for both time-series dimension spellings
- [ ] **Phase 5: `validate_ui_config()` and Milestone Release** - The sidecar gets checked against the live SQL schema for the first time, and the milestone ships

## Phase Details

### Phase 1: Enum Labels in Describe

**Goal**: An agent calling `describe` / `describe_collection` / `summarize_collection` on a PSR model database reads `values {0: 8 (Disabled), 1: 4 (Enabled)}` instead of bare codes — in C++, the C API, Julia, Dart, Python, JS and Lua — delivered on the `<db_dir>/ui/` convention path with no ABI change, no new C symbol and no file under `bindings/` touched.
**Mode:** mvp
**Depends on**: Nothing (first phase)
**Requirements**: PARSE-01, PARSE-02, PARSE-03, PARSE-04, PARSE-05, PARSE-06, PARSE-07, PARSE-08, PARSE-09, PARSE-10, PARSE-11, PARSE-12, DESC-01, DESC-02, DESC-03, DESC-04, DESC-05, DESC-06, DESC-07, CORPUS-01, CORPUS-02, CORPUS-03
**Success Criteria** (what must be TRUE):

  1. On a database with a `ui/` sidecar, `summarize_collection`'s value histogram shows the code **and** the label (`values {0: 8 (Disabled), 1: 4 (Enabled)}`), and `describe_collection` names the attribute's vocabulary with its **full declared value list** — including codes that zero rows use.
  2. `describe` and `describe_collection` render each collection's and each scalar's UI label and unit, tag a `hide = true` attribute `[hidden]` rather than dropping it, and emit a header line naming the loaded config path and the resolved locale.
  3. With no `ui/` directory — or a malformed one, or one whose collection file is broken — `open()` still succeeds, a warning is logged, `has_ui_config()` reports false, nothing partial is published, and the three reports are **byte-identical** to today's output, verified by diffing actual output against master rather than by "the tests pass".
  4. Shared fixtures under `tests/schemas/ui/`, distilled from BESSOperation (all-bare-string), Foresight (mixed en/es/pt) and HydroThermalDispatch (plain `date_time` attribute), pin every parser tolerance individually — bare and dotted localizable values in one file, absent and zero-byte `enum.toml`, an unknown key logged not thrown, 1-based and gapped vocabularies, an unlisted orphan collection file that stays unloaded, `format` as a 4-key table, interleaved `[[attribute]]`/`[[attribute_group]]` blocks, and `degradation` as both an attribute id and a group id — and the enum rendering is asserted on **exact strings** from the C++ suite, the Lua suite and all five binding suites against those same fixtures, never a copy.
  5. The phase ships as a **patch**: `git diff` shows no file under `bindings/` changed, and `CHANGELOG.md` heads the version `CMakeLists.txt` actually carries (0.10.6) before any bump is dispatched.

**Plans**: 3/6 plans executed in 4 waves

Plans:
**Wave 1**

- [x] 01-01-PLAN.md — Tracer: one TOML sidecar reaches `summarize_collection`'s histogram end to end (parser, `Impl` wiring, `has_ui_config()`, golden baseline)

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 01-02-PLAN.md — The `tests/schemas/ui/` fixture corpus, one directory per tolerance, plus the walk-and-never-copied guard
- [x] 01-03-PLAN.md — Render expansion: header line, collection label, unit / `[hidden]` / label / full vocabulary on the scalar line

**Wave 3** *(blocked on Wave 2 completion)*

- [ ] 01-04-PLAN.md — Parser tolerances: unknown keys, `format` table form, absent/zero-byte `enum.toml`, PascalCase ids, dual namespace, interleaving, orphan file

**Wave 4** *(blocked on Wave 3 completion)*

- [ ] 01-05-PLAN.md — DESC-07 at the C API and Lua boundaries, with exact-string and non-ASCII assertions
- [ ] 01-06-PLAN.md — DESC-07 in Julia/Dart/Python/JS, the D-31 written call, CHANGELOG reconciliation, CLAUDE.md updates, phase gates

**Notes:**

- Strictly sequential inside the phase: parser → `Impl` wiring → rendering. There is no renderer without a config on `Impl` and no config without a parser. The `tests/schemas/ui/` corpus is the one work item that parallelizes with the parser.
- Write the parser against Hub's Dart source (`C:/Development/Hub/hub1/lib/models/configuration/*.dart` + `lib/models/utils/toml_utils.dart`). `toml-schema.md` is already wrong about group membership. Budget the read; cite the Dart class per key.
- `tomlplusplus` is `PRIVATE` on the `quiver` target, so `quiver_tests` cannot include it. Exercise the parser through Quiver's public API; do **not** add a toml++ link line to the test target.
- **Accepted-risk record:** from this phase on, Quiver authoritatively repeats labels nothing has checked — `HydroThermalDispatch`'s `HasCommitment` is inverted between Julia and the TOML today. Mitigation until Phase 5: always render the **code beside the label**, never the label alone.
- The four binding describe suites currently assert only "returns a String". Make an explicit written call — strengthen them or leave them honest — and never count them as five-layer coverage.
- Do not touch `CSVOptions::enum_labels` or any CSV source in this milestone.

### Phase 2: Config Path, Locale and Struct-Size Safety

**Goal**: A consumer in any binding opens a database pointing at a UI config directory anywhere on disk, in a locale of its choosing — unblocking claw, whose config lives at `<installRoot>/database/ui` and whose read sandbox cannot reach it — and any binding whose hardcoded FFI layout constants have drifted from the native library fails loudly at load instead of writing past a caller-owned buffer.
**Mode:** mvp
**Depends on**: Phase 1
**Requirements**: OPT-01, OPT-02, OPT-03, OPT-04, OPT-05, OPT-06, SAFE-01, SAFE-02, SAFE-03
**Success Criteria** (what must be TRUE):

  1. A caller in each of Julia, Dart, Python, JS and Lua's host opens a database with an explicit UI config directory outside `<db_dir>/ui/` and a non-`en` locale — via optional parameters on `open`, `from_schema` and `from_migrations`, matching the existing `read_only` / `console_level` pattern — and reads back a **locale-specific label** through `describe`, proving the value crossed the FFI rather than that `open()` merely returned.
  2. `has_ui_config()` answers in every layer: true when a config loaded, false when the directory is absent or malformed.
  3. Loading any binding against a native library whose struct sizes disagree with that binding's hardcoded constants produces a named, loud error at load time — covering the grown options struct **and** the pre-existing `quiver_scalar_metadata_t` (JS `SCALAR_METADATA_SIZE = 56`) and `quiver_group_metadata_t` (JS `GROUP_METADATA_SIZE = 32`), each read from a C API size accessor that returns a plain `size_t` Bun can call.
  4. `bindings/js/src/ffi-helpers.ts` allocates the options buffer from **named offset constants** with a field-order comment, sized from the size accessor — and the file's two unrelated `new Uint8Array(8)` allocations (`allocPtrOut`, `allocUint64Out`) are provably unchanged.
  5. Python's CFFI cdef, Dart's hand-edited `bindings.dart` (no ffigen regen; `.dart_tool/hooks_runner/` and `.dart_tool/lib/` cleared before the suite runs) and Julia's regenerated `c_api.jl` each carry at least one test that a wrong layout would actually fail.

**Plans**: TBD

**Notes:**

- Ships as a **minor** bump (0.x minor signals breaking). The C header edit lands before any of the four FFI options builders; those four are then independent of each other.
- **`bindings/js/src/ffi-helpers.ts` `makeDefaultOptions` gets its own plan.** It is the only place in the repo where a wrong number is a native out-of-bounds write with no compile error, no exception, no generator and no fallback symbol — Bun cannot call `quiver_database_options_default` (struct-by-value, bun#6139). A search-and-replace over "8" in that file is itself the bug.
- Buildable concurrently with Phase 3 (disjoint files, even in JS: `ffi-helpers.ts` vs a new `ui-metadata.ts`). Both are native changes and should ship in the same release.
- `quiver_database_options_default` uses designated initializers; `convert_database_options` needs a NULL guard on the two new `const char*` fields.

### Phase 3: Structured Attribute Metadata

**Goal**: `Claw/claw/src/core/study-config.ts` can drop its per-attribute half — a consumer asks Quiver for a scalar attribute's label, tooltip, unit, format, hidden flag and enum vocabulary as structured data, in every binding and in Lua, instead of parsing a text report.
**Mode:** mvp
**Depends on**: Phase 1 (buildable in parallel with Phase 2; ships in the same release)
**Requirements**: META-01, META-02, META-03, META-04, META-05, META-06
**Success Criteria** (what must be TRUE):

  1. A caller in C++, the C API, Julia, Dart, Python, JS and Lua gets a structured record for a `(collection, attribute)` pair carrying label, tooltip, unit, format, hidden flag, vocabulary name and that vocabulary's full ordered `{code, label}` entries — and an attribute the TOML does not configure returns a default-constructed record rather than throwing.
  2. A caller lists every loaded vocabulary and fetches one vocabulary's entries by name; an unknown name throws the Pattern 2 `not found` message, surfaced identically in all five bindings and Lua.
  3. `format` round-trips **verbatim** in both grammars (`{:.2f}`, `yyyy-MM-dd`) and both shapes — the string form and the 4-key table form no corpus file uses — with Quiver classifying neither.
  4. `git diff` shows `include/quiver/attribute_metadata.h`, `quiver_scalar_metadata_t` and `quiver_group_metadata_t` untouched: the metadata crosses as its **own** struct with its own size accessor and its own free function, and JS adds a new constant only.
  5. `bindings/js/test/lua-api-sync.test.ts` is green — the new `db:` name landed in `src/lua_runner.cpp` and `bindings/js/src/lua-api.ts` as one edit, not two.

**Plans**: TBD

**Notes:**

- Freeze the C header before any binding decoder starts, or five decoders get re-edited simultaneously. The five decoders parallelize once it is frozen.
- Shape precedents to copy rather than invent: `convert_scalar_to_c` / `free_scalar_fields` for the converter pair, `quiver_csv_options_t` for grouped parallel arrays, `quiver_database_free_time_series_data` for the free signature, and `scalar_metadata_lua` (`src/lua_runner.cpp` L1166) for the Lua converter.
- Carry the Phase 1 accepted-risk note forward: the structured getter hands the unchecked label to a consumer that will reason on it, so the enum **code** travels with it.

### Phase 4: Collection and Attribute-Group Metadata

**Goal**: A consumer reads the collection- and group-level knowledge SQL cannot express — a collection's display label, icon, help text and its position in `main.collections`, and each `[[attribute_group]]`'s metadata with its time-series dimension column answered once regardless of which of the two spellings the model repo used.
**Mode:** mvp
**Depends on**: Phase 3 (reuses its struct / converter / free idiom; the parse landed in Phase 1)
**Requirements**: GROUP-01, GROUP-02, GROUP-03, GROUP-04, GROUP-05
**Success Criteria** (what must be TRUE):

  1. A caller in every binding and in Lua reads a collection's UI label, icon and help text, plus its display order — its index in `main.collections`.
  2. A caller reads an `[[attribute_group]]`'s metadata, with group membership recovered by joining the group's `id` against the `{Collection}_vector_{id}` / `_time_series_{id}` table names in SQLite — never inferred from the order entries appear in the TOML, which is what the only written spec gets wrong.
  3. A group declared with the nested `attribute_group.date_time.*` spelling (SCE, BESSOperation, GNoMo) and one declared with a plain `[[attribute]] id = "date_time"` (all six HTD group files, GNoMo/historical_conditions) return the **same** dimension answer, so no consumer and no renderer branches on the spelling.
  4. With group ids now read, the `degradation`-as-both-an-attribute-and-a-group fixture still resolves both independently, and the unlisted orphan collection file still stays unloaded.

**Plans**: TBD

**Notes:**

- Independent of Phase 5; the two can be built concurrently.
- Structure comes from SQL (PRAGMA-derived, as always), decoration from TOML. Quiver still parses no SQL text.
- Revisit the describe byte-identical baseline here: any new group-level rendering is append-only under the same rule.

### Phase 5: `validate_ui_config()` and Milestone Release

**Goal**: The PSR ecosystem gets its first check of a `database/ui/` sidecar against anything — an advisory validator that reports drift between the TOML and the live SQL schema in every layer — and the milestone ships through the full release ritual.
**Mode:** mvp
**Depends on**: Phase 4
**Requirements**: VALID-01, VALID-02, VALID-03, VALID-04, VALID-05, VALID-06, VALID-07, VALID-08
**Success Criteria** (what must be TRUE):

  1. `validate_ui_config()` mirrors `validate_migrations` — no out-parameter, throws on failure — and is callable from C++, the C API, Julia, Dart, Python, JS and Lua, where it is db-scoped and sandboxed under the existing Lua file-operation policy.
  2. Run against fixtures, it names each drift kind distinctly: an attribute bound to a vocabulary `enum.toml` does not declare, a TOML attribute whose column is absent from the SQL schema, a SQL table absent from `main.collections`, a collection TOML on disk that `main.collections` does not list, and a vocabulary that nothing binds.
  3. Run against the real model repos it produces a recorded drift report — expected to surface HTD's `inflow_type`, the nine untyped `Configuration` flags, `Interconnection`, SCE's orphan `agent.toml` and the four dead vocabularies — with findings written down and **nothing auto-fixed**.
  4. The milestone ships end to end: `scripts/assert_version.py` confirms `CHANGELOG.md` and all five manifests agree, then `publish-s3` → tag → Julia/Python/JS in parallel → **Dart published by hand and verified installed**, since no CI job runs `hook/build.dart` on any OS.

**Plans**: TBD

**Notes:**

- Ships last by explicit user decision, made with the `HasCommitment` inversion on the table (PROJECT.md records it as an accepted risk — this is not a sequencing defect). It is **not a stretch goal**: it is the only mitigation that will ever exist for the authoritative-wrong-label risk opened in Phase 1.
- The validator's rule set is not yet enumerated — what counts as drift and what it says about each kind needs investigation during planning.
- The validator reports disagreement; it does not resolve it. Codegen from Julia `@enumx` is unavailable — the sets diverge both ways and naming is unmappable across three styles.
- Criterion 4 is operational work with no v1 requirement behind it; see Coverage Notes.

## Coverage

**All 50 v1 requirements mapped to exactly one phase. No orphans, no duplicates.**

| Category | Count | Phase |
|----------|-------|-------|
| PARSE-01 … PARSE-12 | 12 | Phase 1 |
| DESC-01 … DESC-07 | 7 | Phase 1 |
| CORPUS-01 … CORPUS-03 | 3 | Phase 1 |
| OPT-01 … OPT-06 | 6 | Phase 2 |
| SAFE-01 … SAFE-03 | 3 | Phase 2 |
| META-01 … META-06 | 6 | Phase 3 |
| GROUP-01 … GROUP-05 | 5 | Phase 4 |
| VALID-01 … VALID-08 | 8 | Phase 5 |

### Coverage Notes

Two success criteria intentionally have no backing v1 requirement. Both are real work the
milestone cannot ship without, and neither is a feature:

- **Phase 1, criterion 5 (second half)** — reconciling `CHANGELOG.md` (heads `## [0.10.4] —
  unreleased`) against `CMakeLists.txt` 0.10.6. Pre-existing repo debt that blocks the first bump
  dispatch. Placed in Phase 1 because Phase 1 is the first shipping phase.

- **Phase 5, criterion 4** — the release ritual. Placed in the last phase because that is when the
  milestone ships whole. Dart is the leg with no automation and the easiest to declare "shipped"
  while it is not.

Two requirements carry a deliberate cross-phase relationship worth recording rather than
re-mapping:

- **CORPUS-01** builds the HydroThermalDispatch slice whose plain-`date_time` spelling is not
  *exposed* until GROUP-04 in Phase 4. The fixture is authored in Phase 1 because the parser must
  absorb both spellings at parse time; Phase 4 only surfaces the answer.

- **CORPUS-03** (fixtures shared, never copied into a binding) is enforced from Phase 1 onward by
  DESC-07's binding assertions, and every later phase inherits the rule.

## Parallelization

`parallelization: true` in config. What can actually run concurrently:

- **Inside Phase 1:** the parser and the `tests/schemas/ui/` fixture corpus.
- **Phase 2 ∥ Phase 3** once Phase 1's `Impl` wiring lands — disjoint files in every binding.
- **Inside Phase 2:** the four FFI options builders, once the C header is fixed.
- **Inside Phase 3:** all five binding decoders, once the C header is frozen.
- **Phase 4 ∥ Phase 5.**

Strictly sequential: parser → `Impl` wiring → rendering (Phase 1); C header before every binding
mirror (Phases 2 and 3); the release itself, whole and in order.

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Enum Labels in Describe | 3/6 | In Progress|  |
| 2. Config Path, Locale and Struct-Size Safety | 0/TBD | Not started | - |
| 3. Structured Attribute Metadata | 0/TBD | Not started | - |
| 4. Collection and Attribute-Group Metadata | 0/TBD | Not started | - |
| 5. `validate_ui_config()` and Milestone Release | 0/TBD | Not started | - |

---
*Roadmap created: 2026-09-17*
