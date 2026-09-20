# Roadmap: Quiver — UI Metadata Layer

## Overview

Quiver learns to read the PSR `database/ui/` TOML sidecar and hand what it finds to the agents
that read a study database. The journey starts at the triggering defect and closes it in one
phase: a C++ parser, a lazy member on `Database::Impl`, and three append-only format strings in
`describe` — which reach all five bindings and Lua for free, because `describe*` already returns a
`std::string` through the existing C API wrappers. That first slice ships as a patch and stands
alone; if the milestone stopped there, the defect an agent hits today would be closed.

That first slice turned out to be the whole of the core value, and the milestone was re-scoped
after Phase 2 to say so (see **Scope Correction** below). Phase 2 paid off the
silent-struct-corruption hazard class and is kept for that alone. Phase 3 now *removes* the
options and structured-metadata surface Phase 2 and its own first two plans had added, returning
`quiver_database_options_t` to its published 8-byte layout. Phase 4 adds the collection- and
group-level knowledge SQL cannot express — rendered into `describe`, with no new public surface.
Phase 5 is the release ritual alone.

**The sidecar has exactly one consumer: `describe`.** No getter, no new C struct, no binding
change. That is the whole design, and every requirement that contradicted it has been retired.

**Mode:** MVP. Each phase is a vertical slice that a consumer can observe through the bindings,
not a technical layer. There is no "build the parser" phase followed by a "wire the C API" phase.

## Phases

**Phase Numbering:**

- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Enum Labels in Describe** - An agent calling `describe` sees what an INTEGER column means, in all six layers, with no ABI change (completed 2026-09-19)
- [x] **Phase 2: Config Path, Locale and Struct-Size Safety** - A consumer points Quiver at any UI directory and locale; a drifted binding layout fails loudly instead of corrupting memory (completed 2026-09-19)
- [ ] **Phase 3: UI Metadata Simplification** - the options struct returns to its published 8-byte layout and the sidecar is read only by `describe`; no FFI surface change survives the milestone
- [ ] **Phase 4: Collection and Attribute-Group Metadata** - Display labels, icons, help, `main.collections` order, and one answer for both time-series dimension spellings — rendered in `describe`, no new public surface
- [ ] **Phase 5: Milestone Release** - `assert_version.py bump patch` to 0.10.8, the CHANGELOG rewrite, and the release ritual

## Phase Details

### Phase 1: Enum Labels in Describe

**Goal**: An agent calling `describe` / `describe_collection` / `summarize_collection` on a PSR model database reads `values {0: 8 (Disabled), 1: 4 (Enabled)}` instead of bare codes — in C++, the C API, Julia, Dart, Python, JS and Lua — delivered on the `<db_dir>/ui/` convention path with no ABI change, no new C symbol and no file under `bindings/` touched.
**Mode:** mvp
**Depends on**: Nothing (first phase)
**Requirements**: PARSE-01, PARSE-02, PARSE-03, PARSE-04, PARSE-05, PARSE-06, PARSE-07, PARSE-08, PARSE-09, PARSE-10, PARSE-11, PARSE-12, DESC-01, DESC-02, DESC-03, DESC-04, DESC-05, DESC-06, DESC-07, CORPUS-01, CORPUS-02, CORPUS-03
**Success Criteria** (what must be TRUE):

  1. On a database with a `ui/` sidecar, `summarize_collection`'s value histogram shows the code **and** the label (`values {0: 8 (Disabled), 1: 4 (Enabled)}`), and `describe_collection` names the attribute's vocabulary with its **full declared value list** — including codes that zero rows use.
  2. `describe` and `describe_collection` render each collection's and each scalar's UI label and unit, tag a `hide = true` attribute `[hidden]` rather than dropping it, and emit a header line naming the loaded config path and the resolved locale.
  3. With no `ui/` directory — or a malformed one, or one whose collection file is broken — `open()` still succeeds, a warning is logged, `has_ui_config()` reports false, nothing partial is published, and the three reports are **byte-identical** to today's output, verified by diffing actual output against master rather than by "the tests pass". *(Delivered as written. The `has_ui_config()` clause is **superseded** by the scope correction — Phase 3 removes the method; the same condition is then observed through the absence of the `UI config:` header line. Every other clause stands unchanged.)*
  4. Shared fixtures under `tests/schemas/ui/`, distilled from BESSOperation (all-bare-string), Foresight (mixed en/es/pt) and HydroThermalDispatch (plain `date_time` attribute), pin every parser tolerance individually — bare and dotted localizable values in one file, absent and zero-byte `enum.toml`, an unknown key logged not thrown, 1-based and gapped vocabularies, an unlisted orphan collection file that stays unloaded, `format` as a 4-key table, interleaved `[[attribute]]`/`[[attribute_group]]` blocks, and `degradation` as both an attribute id and a group id — and the enum rendering is asserted on **exact strings** from the C++ suite, the Lua suite and all five binding suites against those same fixtures, never a copy.
  5. The phase ships as a **patch**: `git diff` shows no file under `bindings/` changed, and `CHANGELOG.md` heads the version `CMakeLists.txt` actually carries (0.10.6) before any bump is dispatched.

**Plans**: 6/6 plans executed in 4 waves

Plans:
**Wave 1**

- [x] 01-01-PLAN.md — Tracer: one TOML sidecar reaches `summarize_collection`'s histogram end to end (parser, `Impl` wiring, `has_ui_config()`, golden baseline)

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 01-02-PLAN.md — The `tests/schemas/ui/` fixture corpus, one directory per tolerance, plus the walk-and-never-copied guard
- [x] 01-03-PLAN.md — Render expansion: header line, collection label, unit / `[hidden]` / label / full vocabulary on the scalar line

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 01-04-PLAN.md — Parser tolerances: unknown keys, `format` table form, absent/zero-byte `enum.toml`, PascalCase ids, dual namespace, interleaving, orphan file

**Wave 4** *(blocked on Wave 3 completion)*

- [x] 01-05-PLAN.md — DESC-07 at the C API and Lua boundaries, with exact-string and non-ASCII assertions
- [x] 01-06-PLAN.md — DESC-07 in Julia/Dart/Python/JS, the D-31 written call, CHANGELOG reconciliation, CLAUDE.md updates, phase gates

**Notes:**

- Strictly sequential inside the phase: parser → `Impl` wiring → rendering. There is no renderer without a config on `Impl` and no config without a parser. The `tests/schemas/ui/` corpus is the one work item that parallelizes with the parser.
- Write the parser against Hub's Dart source (`C:/Development/Hub/hub1/lib/models/configuration/*.dart` + `lib/models/utils/toml_utils.dart`). `toml-schema.md` is already wrong about group membership. Budget the read; cite the Dart class per key.
- `tomlplusplus` is `PRIVATE` on the `quiver` target, so `quiver_tests` cannot include it. Exercise the parser through Quiver's public API; do **not** add a toml++ link line to the test target.
- **Accepted-risk record:** from this phase on, Quiver authoritatively repeats labels nothing has checked — `HydroThermalDispatch`'s `HasCommitment` is inverted between Julia and the TOML today. Mitigation: always render the **code beside the label**, never the label alone. With `validate_ui_config()` retired in the scope correction, this is the **permanent** mitigation, not a temporary one — nothing in the PSR ecosystem will check the sidecar against the schema.
- The four binding describe suites currently assert only "returns a String". Make an explicit written call — strengthen them or leave them honest — and never count them as five-layer coverage.
- Do not touch `CSVOptions::enum_labels` or any CSV source in this milestone.

### Phase 2: Config Path, Locale and Struct-Size Safety

> **Partly superseded by the scope correction.** Delivered in full on 2026-09-19. Its struct-size
> safety half (SAFE-01…03) is kept and is live. Its config-path/locale half (OPT-01…06) is retired
> and is removed by Phase 3 — the criteria below record what was built, not what the milestone
> ships. See Coverage Notes.

**Goal**: A consumer in any binding opens a database pointing at a UI config directory anywhere on disk, in a locale of its choosing — unblocking claw, whose config lives at `<installRoot>/database/ui` and whose read sandbox cannot reach it — and any binding whose hardcoded FFI layout constants have drifted from the native library fails loudly at load instead of writing past a caller-owned buffer.
**Mode:** mvp
**Depends on**: Phase 1
**Requirements**: OPT-01, OPT-02, OPT-03, OPT-04, OPT-05, OPT-06, SAFE-01, SAFE-02, SAFE-03
**Success Criteria** (what must be TRUE):

  1. A caller in each of Julia, Dart, Python, JS and Lua's host opens a database with an explicit UI config directory outside `<db_dir>/ui/` and a non-`en` locale — via optional parameters on `open`, `from_schema` and `from_migrations`, matching the existing `read_only` / `console_level` pattern — and reads back a **locale-specific label** through `describe`, proving the value crossed the FFI rather than that `open()` merely returned.
  2. `has_ui_config()` answers in every layer: true when a config loaded, false when the directory is absent or malformed. *(Delivered as written, then **retired** by the scope correction — OPT-04. Phase 3 removes it from all six layers; the `UI config:` header line in `describe` answers the same question and names the path.)*
  3. Loading any binding against a native library whose struct sizes disagree with that binding's hardcoded constants produces a named, loud error at load time — covering the grown options struct **and** the pre-existing `quiver_scalar_metadata_t` (JS `SCALAR_METADATA_SIZE = 56`) and `quiver_group_metadata_t` (JS `GROUP_METADATA_SIZE = 32`), each read from a C API size accessor that returns a plain `size_t` Bun can call.
  4. `bindings/js/src/ffi-helpers.ts` allocates the options buffer from **named offset constants** with a field-order comment, sized from the size accessor — and the file's two unrelated `new Uint8Array(8)` allocations (`allocPtrOut`, `allocUint64Out`) are provably unchanged.
  5. Python's CFFI cdef, Dart's hand-edited `bindings.dart` (no ffigen regen; `.dart_tool/hooks_runner/` and `.dart_tool/lib/` cleared before the suite runs) and Julia's regenerated `c_api.jl` each carry at least one test that a wrong layout would actually fail.

**Plans:** 13/13 plans complete

Plans:
**Wave 1**

- [x] 02-01-PLAN.md — Native ABI freeze: `DatabaseOptions` 8 → 24 bytes, `require_ui_config` override branch, locale threading, three `*_sizeof` accessors, `quiver_database_has_ui_config` (wave 1)

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 02-02-PLAN.md — JS: 24-byte `makeDefaultOptions` from named offsets with string keepalives, load-time three-struct gate (wave 2)
- [x] 02-03-PLAN.md — Python: hand-edited CFFI cdef, options kwargs, load-time gate that calls the accessors (wave 2)
- [x] 02-04-PLAN.md — Dart: hand-edited `bindings.dart`, actively-calling load-time gate, `test.bat` cache clearing (wave 2)
- [x] 02-05-PLAN.md — Julia: regenerated `c_api.jl`, `GC.@preserve` keepalives, load-time gate in `generator/prologue.jl` (wave 2)

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 02-06-PLAN.md — Lua host: `db:has_ui_config()`, `quiver_cli --ui-config-dir` / `--ui-locale`, automated CLI probe (wave 3)

**Wave 4** *(blocked on Wave 3 completion)*

- [x] 02-07-PLAN.md — Release: CHANGELOG 0.10.7 → 0.11.0 rename with BREAKING entry, single `part=minor` dispatch behind a decision checkpoint (wave 4)

*Gap closure — added after `02-VERIFICATION.md` returned `gaps_found` (3/5 success criteria MET, 2 PARTIAL, SAFE-01 incomplete). Waves restart at 1; plans 02-01 … 02-07 are executed history and are not re-planned.*

**Gap-closure wave 1**

- [x] 02-08-PLAN.md — **Tracer:** `quiver_csv_options_sizeof` natively + Python's gate restored (it is unwired in HEAD), widened to four structs, and made observable so a test fails when it is removed

**Gap-closure wave 2** *(blocked on 02-08)*

- [x] 02-09-PLAN.md — Malformed and `:memory:` polarity plus `open`/`from_migrations`/`describe()` coverage across the C API, Lua and all four bindings
- [x] 02-10-PLAN.md — JS: named CSV offset constants, the fourth struct in the load-time gate, a mutation-sensitive test, and the now-false deferral note removed from `bindings/js/CLAUDE.md`
- [x] 02-11-PLAN.md — Julia and Dart: fourth struct in each gate, observable check record, mutation-sensitive tests, `@test true` deleted

**Gap-closure wave 3** *(blocked on 02-10)*

- [x] 02-12-PLAN.md — JS loader: version-skew diagnosis for a native missing the size accessors, and the options keepalive eliminated by a self-contained buffer

**Gap-closure wave 4** *(blocked on all of the above)*

- [x] 02-13-PLAN.md — CHANGELOG 0.10.7 reconciliation (duplicate heading, missing compare link, wrong 0.11.0 base) and correction of the stale planning premises

**Cross-cutting constraints:**

- EDGE/adjacency — an exactly-equal size passes; any difference throws. No tolerance band.
- EDGE/ordering — the three checks run options, scalar_metadata, group_metadata and short-circuit on the first mismatch, identically to the other three bindings.

**Notes:**

- Ships as a **minor** bump (0.x minor signals breaking). The C header edit lands before any of the four FFI options builders; those four are then independent of each other.
- **`bindings/js/src/ffi-helpers.ts` `makeDefaultOptions` gets its own plan.** It is the only place in the repo where a wrong number is a native out-of-bounds write with no compile error, no exception, no generator and no fallback symbol — Bun cannot call `quiver_database_options_default` (struct-by-value, bun#6139). A search-and-replace over "8" in that file is itself the bug.
- **Superseded in part by the scope correction.** OPT-01…OPT-06 are retired and Phase 3 now removes the options surface this phase added. What survives and is kept deliberately is SAFE-01…SAFE-03 — the four `*_sizeof` accessors and the four load-time struct-size gates, which are independent of UI metadata and already caught a real defect (a merge that silently dropped two struct fields from `bindings/julia/src/c_api.jl`, breaking the entire Julia suite at load).
- `quiver_database_options_default` uses designated initializers; `convert_database_options` needs a NULL guard on the two new `const char*` fields.

### Phase 3: UI Metadata Simplification

**Goal**: **As** the Quiver maintainer, **I want** the `ui/` sidecar consumed only by `describe` inside the C++ core, **so that** the milestone ships with no FFI surface change, no ABI delta against v0.10.7, and a patch release instead of a minor one.

*This phase replaced "Structured Attribute Metadata" in the scope correction. Its first two plans (03-01, 03-02) had already shipped a public `UIMetadata` type, a 64-byte `quiver_ui_metadata_t`, three C++ getters, the C API surface and a Python decoder; those are reverted here. Plans 03-03…03-07 were never started.*

**Mode:** mvp
**Depends on**: Phase 2
**Requirements**: SAFE-01, SAFE-02, SAFE-03 (retained from Phase 2). META-01…META-06 and OPT-01…OPT-06 are **retired** — see Coverage Notes.
**Success Criteria** (what must be TRUE):

  1. `ui_config_dir` is gone from every layer. The sidecar directory is derived as `parent_path(migrations_path) / "ui"` inside `Database::migrate_up`, with `<db_dir>/ui` kept as the fallback and the `:memory:` early-return left unconditional. Verified against claw's layout: it resolves migrations as `join(configDir, "..", "migrations")`, so `migrations/` and `ui/` are guaranteed siblings.
  2. `ui_locale` is gone from every layer. `src/ui_config.cpp` resolves at a file-local `constexpr kLocale = "en"` and the parameter is dropped from all six parse signatures. The bare-string-wins and first-key legs of the fallback chain survive, so `foresight_like` renders byte-identically — only the two `es` assertions Phase 2 added for itself are deleted.
  3. `has_ui_config()` is gone from all six layers. The 19 Phase 1 assertions that used it are rewritten against the `UI config:` header line `write_ui_header` already emits from all three reports — strictly more informative than the bool, since it names the path.
  4. `quiver_database_options_t` is `{int; quiver_log_level_t;}` — **8 bytes, byte-for-byte the published v0.10.7 layout**. `git diff v0.10.7 -- include/quiver/c/options.h` shows no layout change, and the BREAKING paragraph is **deleted** from `CHANGELOG.md` rather than answered with a second one.
  5. `UIMetadata` and `UIEnumEntry` are private again in `src/ui_config.h`. `include/quiver/ui_metadata.h`, `quiver_ui_metadata_t` and its nine offset static_asserts, `quiver_ui_metadata_sizeof`, `get_attribute_ui_metadata`, the vocabulary getters and every binding decoder are deleted. `git diff v0.10.7 -- bindings/` shows changes attributable **only** to SAFE-01…03.
  6. `describe` / `describe_collection` / `summarize_collection` output is unchanged, and all seven suites are green.

**Plans**: TBD

**Notes:**

- **Forward-delete, not `git revert`.** Reverting the Phase 2 option plumbing hits 6 conflicts, every one at a seam where a later struct-size-gate commit edited adjacent lines — all resolving the same way (keep the `_sizeof` lines, drop the ui lines), which is 13 revert commits plus 6 hand-resolutions to reach a state one forward-delete commit reaches.
- **Do not run `/gsd-undo` on this repo.** `.planning/.phase-manifest.json` does not exist, so it falls back to git-log matching: `--phase 3` hits 32 commits (20 from already-merged milestones) and `--phase 2` hits 102, truncated at 50.
- **Order matters — each step shrinks the next one's surface:** (a) delete the structured-metadata surface; (b) drop `ui_config_dir` and add the derivation; (c) drop `ui_locale`; (d) drop `has_ui_config` and rewrite the Phase 1 assertions; (e) shrink the options struct across the four bindings and re-tune the gates; (f) CHANGELOG + `assert_version.py bump patch`.
- **Do not drop `ui_locale` alone.** Locale-only leaves the struct at 16 bytes — still an ABI break, still every binding edit and every size gate, for half the deletion. The two fields go together or not at all.
- **Migrate, do not delete, the parser assertions.** `tests/test_database_ui_metadata.cpp` covers the declared-but-empty vocabulary case and the 4-key `format` collapse; neither is covered anywhere else. Move both into `tests/test_database_ui_parse.cpp` before deleting the file.
- **Known gap, accepted.** claw falls back to `Database.open(dbPath)` when it finds no migrations directory, and the derivation does not cover that path. Accepted because that path already has no schema metadata either; if it ever matters the fix is an argument on `open()`, not a field in the options struct.
- Removing an exported C symbol while a binding still names it is a **load failure, not a compile error** (Bun `dlopen`, CFFI lazy resolution, Dart `lookup`). There is no cross-layer symbol-coverage test, so a half-done removal ships silently. Sequence the binding deletions with the C deletions in the same plan.

### Phase 4: Collection and Attribute-Group Metadata

**Goal**: `describe` renders the collection- and group-level knowledge SQL cannot express — a collection's display label, icon, help text and its position in `main.collections`, and each `[[attribute_group]]`'s metadata with its time-series dimension column answered once regardless of which of the two spellings the model repo used.

*Reduced in the scope correction from a structured-getter surface to a `describe`-rendering surface. No new public type, no new C symbol, no binding change.*

**Mode:** mvp
**Depends on**: Phase 3 (which removes the structured surface this phase would otherwise have extended; the parse landed in Phase 1)
**Requirements**: GROUP-01, GROUP-02, GROUP-03, GROUP-04, GROUP-05
**Success Criteria** (what must be TRUE):

  1. `describe` renders a collection's UI label, icon and help text, and orders collections by their index in `main.collections`. Reaching all five bindings and Lua for free, because `describe` already returns a `std::string` through the existing C API wrappers.
  2. `describe_collection` renders an `[[attribute_group]]`'s metadata, with group membership recovered by joining the group's `id` against the `{Collection}_vector_{id}` / `_time_series_{id}` table names in SQLite — never inferred from the order entries appear in the TOML, which is what the only written spec gets wrong.
  3. A group declared with the nested `attribute_group.date_time.*` spelling (SCE, BESSOperation, GNoMo) and one declared with a plain `[[attribute]] id = "date_time"` (all six HTD group files, GNoMo/historical_conditions) return the **same** dimension answer, so no consumer and no renderer branches on the spelling.
  4. With group ids now read, the `degradation`-as-both-an-attribute-and-a-group fixture still resolves both independently, and the unlisted orphan collection file still stays unloaded.

**Plans**: TBD

**Notes:**

- Structure comes from SQL (PRAGMA-derived, as always), decoration from TOML. Quiver still parses no SQL text.
- Revisit the describe byte-identical baseline here: any new group-level rendering is append-only under the same rule.
- **This is the only feature work left in the milestone.** It was kept through the scope correction because it is `describe` rendering — the one consumer the correction preserved — not public API surface. If it is ever cut, the milestone is Phase 1 plus the Phase 3 cleanup plus a patch release.

### Phase 5: Milestone Release

**Goal**: The milestone ships as a **patch** — `0.10.8` — with `CHANGELOG.md` rewritten to drop the BREAKING paragraph that no longer describes anything, and the full release ritual carried out.

*`validate_ui_config()` was **retired** in the scope correction. VALID-01…VALID-08 are retired with it. See the Phase 1 accepted-risk note: with no validator, rendering the code beside the label is the permanent mitigation for the authoritative-wrong-label risk, not a temporary one.*

**Mode:** mvp
**Depends on**: Phase 4
**Requirements**: none (operational work; see Coverage Notes)
**Success Criteria** (what must be TRUE):

  1. `scripts/assert_version.py` confirms `CHANGELOG.md` and all five manifests agree at **0.10.8**, reached via `scripts/assert_version.py bump patch` — a patch, not a minor, because the net ABI delta against published v0.10.7 is **zero**.
  2. `CHANGELOG.md`'s `## [0.11.0] — unreleased` heading and its **BREAKING** paragraph about the grown options struct are **deleted**, not superseded — that struct never shipped.
  3. `git diff v0.10.7 -- include/quiver/c/ bindings/` shows no layout change to any C struct and no binding change beyond the SAFE-01…03 struct-size gates.
  4. The release runs end to end: `publish-s3` → tag → Julia/Python/JS in parallel → **Dart published by hand and verified installed**, since no CI job runs `hook/build.dart` on any OS.

**Plans**: TBD

**Notes:**

- Criterion 4 is operational work with no v1 requirement behind it; see Coverage Notes.
- The publish step is the one irreversible action in the milestone — a yanked PyPI or npm version cannot be reused. It is gated on explicit human authorization, not on autonomous execution.

## Coverage

**30 of 50 v1 requirements remain live. 20 were retired in the scope correction — recorded, not
deleted, so the history says why.**

| Category | Count | Phase | Status |
|----------|-------|-------|--------|
| PARSE-01 … PARSE-12 | 12 | Phase 1 | live — delivered |
| DESC-01 … DESC-07 | 7 | Phase 1 | live — delivered |
| CORPUS-01 … CORPUS-03 | 3 | Phase 1 | live — delivered |
| SAFE-01 … SAFE-03 | 3 | Phase 2 | live — delivered, kept deliberately |
| GROUP-01 … GROUP-05 | 5 | Phase 4 | live — reduced to `describe` rendering |
| ~~OPT-01 … OPT-06~~ | 6 | ~~Phase 2~~ | **retired** — options surface removed in Phase 3 |
| ~~META-01 … META-06~~ | 6 | ~~Phase 3~~ | **retired** — no structured getter; `describe` is the only consumer |
| ~~VALID-01 … VALID-08~~ | 8 | ~~Phase 5~~ | **retired** — no validator |

### Coverage Notes

#### Scope Correction — 20 requirements retired

Recorded after Phase 2 shipped and Phase 3 was two plans in. The trigger was a review of the work
in flight, which found the milestone building a public API surface for a consumer that had no code
path for it.

The governing decision: **`describe` is the only consumer of the `ui/` sidecar.** Everything that
existed to serve a *structured* consumer is retired.

- **OPT-01 … OPT-06 (6)** — the `ui_config_dir` / `ui_locale` options and their binding surface.
  `ui_config_dir` is replaced by deriving `ui/` as a sibling of the migrations directory (three
  lines, no public API); `ui_locale` by hardcoding `"en"`; `has_ui_config()` had zero real callers
  and is answered better by the `UI config:` header `describe` already prints. Dropping both option
  fields returns `quiver_database_options_t` to its published 8-byte layout, so the milestone's
  only ABI break **disappears** rather than being documented. OPT-06's Dart cache-clearing change
  (`.dart_tool/hooks_runner/`, `.dart_tool/lib/`) is **kept** despite the retirement — it protects
  every future ABI change, not just this one.
- **META-01 … META-06 (6)** — the structured attribute-metadata getter. Retired because
  `describe` reads the parsed config directly and no other consumer exists. claw, the committed
  consumer, hands `describe()`'s report straight to an LLM and has no code path that would call a
  getter.
- **VALID-01 … VALID-08 (8)** — `validate_ui_config()`. Retired outright. The cost is real and is
  recorded in the Phase 1 accepted-risk note: nothing in the PSR ecosystem will ever check the
  sidecar against the live schema, so rendering the code beside the label is now the **permanent**
  mitigation for the authoritative-wrong-label risk.
- **SAFE-01 … SAFE-03 are explicitly NOT retired.** They are independent of UI metadata, already
  caught a real defect (a merge that silently dropped two struct fields from
  `bindings/julia/src/c_api.jl`, breaking the whole Julia suite at load), and cost nothing ongoing.

Net effect: no FFI surface change survives the milestone, no ABI delta against v0.10.7, and the
release becomes **0.10.8 (patch)** instead of 0.11.0 — no five-package republish risk.

**Phase 1 already delivered the milestone's stated core value** (`describe` showing
`values {0: 8 (Disabled)}` instead of bare codes). Everything from Phase 2 on is elaboration; the
correction cut the elaboration that served nobody.

#### Criteria with no backing requirement

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
| 1. Enum Labels in Describe | 6/6 | Complete    | 2026-09-19 |
| 2. Config Path, Locale and Struct-Size Safety | 13/13 | Complete (partly superseded) | 2026-09-19 |
| 3. UI Metadata Simplification | 0/TBD | Not started — re-scoped | - |
| 4. Collection and Attribute-Group Metadata | 0/TBD | Not started | - |
| 5. Milestone Release | 0/TBD | Not started | - |

*Phase 3's previous incarnation ("Structured Attribute Metadata") reached 2/7 plans before the
scope correction. Commits `1c1bcca`…`810e7b3` are reverted by the re-scoped phase; plans
03-01/03-02 and their summaries are kept as history, 03-03…03-07 are deleted unstarted.*

---
*Roadmap created: 2026-09-17*
