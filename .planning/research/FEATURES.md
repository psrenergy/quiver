# Feature Research

**Domain:** UI-metadata layer for a brownfield C++ SQLite wrapper (Quiver: C++ core + C API + Julia/Dart/Python/JS/Lua)
**Researched:** 2026-09-17
**Confidence:** HIGH on format shape, parse feasibility and build cost (two adversarial verifications, one CONFIRMED / one REFUTED-and-corrected, plus a file-by-file cost survey of quiver3). MEDIUM on consumer uptake past claw — Hub consuming Quiver's reader is not a goal of this milestone.

"Feature" here = a UI-metadata capability **Quiver** exposes. Not a product; a milestone on a shipped library.
Scope is settled in `.planning/PROJECT.md` — this document derives phases from it, it does not re-open it.

---

## The defect-closing feature, stated up front

**F3 — enum labels in `summarize_collection`'s value histogram — closes the triggering agent defect, and it requires no ABI change.**

- Today `src/database_describe.cpp` prints `values {0: 8, 1: 4}` for non-PK INTEGER columns with ≤ `kMaxDistributionCardinality = 64` distinct values; that constant's own comment calls it *"the enum/category case"*. It renders the enum **codes** in exactly the position where the **labels** belong.
- `Claw/claw/src/prompt.ts` already tells the agent that `describe_data` reports enumerations; `Claw/claw/src/tools/describe-data.ts` is a pure passthrough to `db.describe()` / `describeCollection()` / `summarizeCollection()`. The promise is false today.
- After F3: `some_integer: 3 non-null, 0 null; values {0: 8 (Disabled), 1: 4 (Enabled)}`.
- **No ABI change:** `describe` / `describe_collection` / `summarize_collection` already return `std::string` through the trivial `new_c_str` wrappers at `src/c/database.cpp` L129/L141/L155. `quiver_scalar_metadata_t` stays 56 bytes, `quiver_group_metadata_t` stays 32, `quiver_database_options_t` stays 8. No new C symbol, no new struct, no binding edit, no `bindings/js/src/metadata.ts` constant, no `bindings/js/src/ffi-helpers.ts` buffer. It reaches all five bindings **and** Lua for free, and ships as a **patch**.
- Prerequisites are F1 (parser) + F2 (enum binding) + F4 (degrade silently) + F5 (convention path) — all C++-core-only. That whole set is the ABI-free first phase.

---

## Feature Landscape

### Table Stakes (milestone fails without these)

| # | Feature | Why expected | Complexity | Layers touched | Depends on |
|---|---------|--------------|------------|----------------|------------|
| F1 | **UI sidecar parser in C++** — `main.toml` + the collection files named by `main.collections` + `enum.toml`, locale resolved once at parse time | Nothing else in this milestone exists without it; and per PROJECT.md the parser is the one thing that must live in C++ so one implementation serves all five bindings + Lua | MEDIUM | C++ core only: new `include/quiver/ui_metadata.h` + `src/database_ui_config.cpp` | — |
| F2 | **Enum vocabulary + code→label attached to scalar attributes** — join `[[attribute]].type="enum"` + `.enum="<vocab>"` against the `[[<vocab>]]` arrays in `enum.toml` | The whole point: an INTEGER column an agent cannot interpret | LOW–MEDIUM given F1 | C++ core (`src/database_ui_config.cpp`, lookup surfaced via `Impl`) | F1 |
| F3 | **Enum labels in `summarize_collection`'s histogram** — `values {0: 8 (Disabled), 1: 4 (Enabled)}` | **The triggering defect.** Core Value in PROJECT.md: if only this ships, the milestone succeeded | LOW | C++ core, one format string in `src/database_describe.cpp` (`summarize_collection`, ~L118). Reaches all 5 bindings + Lua free — `describe*` is already a `std::string` through the C API | F1, F2, F4 |
| F3b | **UI labels + units + enum vocabulary in `describe` / `describe_collection`** — `- max_generation (REAL) [MW] — "Maximum Generation"`, `- has_commitment (INTEGER) enum bool {0: Disable, 1: Enable}` | Second half of the same three format strings; PROJECT.md Active requirement #3. Hidden attributes stay in the report tagged `[hidden]` — `hide` is a GUI affordance, an agent wants them | LOW | C++ core: `write_collection_section` (~L56) and `print_group_columns` (~L20) in `src/database_describe.cpp` | F1, F2, F4 |
| F4 | **Graceful degradation with no `ui/` dir** — missing/empty/malformed sidecar logs a warning through `impl_->logger`, never throws; `has_ui_config()` returns false; `open()` still succeeds; report output **byte-identical** to today | Every existing caller must keep working, and PROJECT.md's Behaviour constraint pins byte-identical output so the brittle assertions survive | LOW to write, HIGH to get right | C++ core (`src/database_impl.h`), plus `has_ui_config()` on the public surface | F1 |
| F5 | **Convention path discovery** — `<db_dir>/ui/`, resolved by the constructor, parsed inside `Impl::load_schema_metadata` | Makes F1–F4 shippable with **zero** ABI change; correct for 100% of the surveyed corpus (every model puts the dir exactly there) | LOW | C++ core: `src/database_impl.h` (path stored on `Impl` at construction; parsed at L342-348 where `db` and `path` are in scope) | F1 |

**Notes that matter to whoever plans these:**

- **F1 tolerances are non-negotiable** (from the Dart parser's actual behaviour, and the CONFIRMED verdict): (a) every localizable key is `string | table` and the *same file* mixes both (Foresight 12 bare / 711 dotted, GNoMo 1/735, BESSOperation 78/0); (b) `enum.toml` may be absent (Boost) or zero-byte (CHain, SORA, PSRExample); (c) unknown keys are **ignored, never rejected** — there is exactly one in the whole corpus (`conditions`, `GNoMo/floating_storage_unit.toml`); (d) enum ids are arbitrary ints, never positional (GNoMo `weekday` 1–7; HTD `initial_volume_type` and SCE `granularity_type` gapped `[0, 2]`); (e) collections load **only** from `main.collections`, never a directory scan (`SCE/SCE.jl/database/ui/agent.toml` is a fully-formed orphan and must stay unloaded — 66 of 67 files ever load); (f) attribute ids and group ids are separate namespaces (`BESSOperation/storage.toml` uses `degradation` for both); (g) `[[attribute]]`/`[[attribute_group]]` interleaving is legal and flattens (15 files); (h) collection `id` is PascalCase (the SQL table name) while `main.collections` lists snake_case filenames; (j) `format` carries two grammars (`{:.2f}`, `yyyy-MM-dd`) **and** a 4-key table form no file uses — accept both from day one, four lines, or a string-only parser passes every test today and breaks on first use.
- **F1 costs no CMake change.** `tomlplusplus` v3.4.0 is already `PRIVATE` on the `quiver` target (`src/CMakeLists.txt`) — which contains every `database_*.cpp`. It does **not** propagate to `quiver_c` / `quiver_cli` / `quiver_tests`, so the parsing must sit in the core (which is where PROJECT.md puts it anyway). Copy the `from_toml_content` / `from_toml_file` split from `src/binary/binary_metadata.cpp` (~L220).
- **F1 writes the parser against `Hub/hub1/lib/models/configuration/*.dart` (15 classes, ~719 lines) + `lib/models/utils/toml_utils.dart`**, not against `Hub/hub1/.claude/skills/psrhub-ui/references/toml-schema.md` — the only written spec is already wrong (it claims attributes declared after a group belong to that group; the parser keys one flat per-collection map and recovers membership by joining `[[attribute_group]].id` against `{Collection}_vector_{id}` / `_time_series_{id}` table names).
- **F4 inherits the publish-nothing-until-valid invariant** documented at `src/database_impl.h` L342-348: a half-loaded UI member must not survive a failed lazy load. Same rule as `schema` / `type_validator`.
- **F3/F3b's safety rests entirely on F4.** The existing assertions are substring `contains`, so an *appended* enrichment survives and an *inserted* one breaks. The two genuinely brittle ones are negative: `tests/test_database_describe.cpp` L84 (`EXPECT_FALSE(... "some_float: 2 non-null, 1 null; values")`) and L104 (`EXPECT_FALSE(... "values {")`). `tests/test_lua_runner_describe.cpp` pins the same formats with Lua patterns. New behaviour is proven by new fixtures under `tests/schemas/ui/`.
- **The corpus is the contract.** 13 model repos, 117 files in scope (13 `main.toml`, 67 collection, 12 `enum.toml` with 9 non-empty, 25 theme), 736 attributes, 75 attribute groups, 62 vocabularies, 164 enum entries, 127 bindings, **0 dangling references**, 4 dead vocabularies. Build `tests/schemas/ui/` from real files — a distilled BESSOperation (all-bare-string), a Foresight slice (en/es/pt), an HTD slice (the plain `[[attribute]] id="date_time"` spelling).

### Differentiators (competitive advantage — nothing else in the ecosystem has these)

| # | Feature | Value proposition | Complexity | Layers touched | Depends on |
|---|---------|-------------------|------------|----------------|------------|
| F6 | **`ui_config_path` + `ui_locale` on `DatabaseOptions`** — point Quiver at a UI dir that is not `<db_dir>/ui`, and pick the locale (default `"en"`, Hub's fallback chain exact → `en` → first key, applied once at parse time) | Unblocks every caller whose UI dir is elsewhere (claw resolves it to `<installRoot>/database/ui` via `Claw/claw/src/core/study.ts` `findConfigDir`) and makes locale explicit rather than hardcoded | **HIGH — the ABI break** | `include/quiver/options.h`; `include/quiver/database.h` (3 sigs); `src/database.cpp` (ctor L94-102, `from_migrations` L244, `from_schema` L287, the `{.console_level = LogLevel::Off}` designated-init L272); `src/cli/main.cpp` L95-97; `include/quiver/c/options.h`; `src/c/options.cpp` (`return {0, QUIVER_LOG_INFO};` — positional aggregate init); `src/c/database_options.h` (NULL guard = no UI dir); Julia `c_api.jl` + `database.jl` `build_quiver_database_options` L11-23; Dart `bindings.dart` L3555 **hand-added** + `database.dart` `_makeOptions` L50-61 + 3 factories; Python `_c_api.py` cdef L27-30 + `database.py` `_make_options` L30-37; **JS `src/ffi-helpers.ts` `makeDefaultOptions`**. Lua: nothing (no lifecycle surface) | F1, F5 |
| F7 | **Structured per-attribute getter** — `get_ui_metadata(collection, attribute)` returning label / tooltip / unit / format / hidden / enum name / enum values, in a **new** C struct with its own size constant | The committed consumer: `Claw/claw/src/core/study-config.ts` carries *"enriching it from the TOML belongs in quiverdb"* and drops its per-attribute half once this lands. No consumer should have to parse TOML to know a column is an enum | HIGH | New `quiver_ui_metadata_t` + `quiver_ui_enum_entry_t` + `quiver_database_get_ui_metadata` / `quiver_database_free_ui_metadata` in `include/quiver/c/database.h` + `src/c/`; all five bindings (new file each); `src/lua_runner.cpp` one `ui_metadata_lua` converter beside `scalar_metadata_lua` (~L1166) | F1, F2, F6 |
| F8 | **Collection-level UI metadata** — label / icon / help, plus `main.collections` **display order** | Display order is knowledge SQL cannot express at all (Foresight's tab order is neither `CREATE TABLE` order nor alphabetical), and `help` is multi-KB Markdown in 20 of 67 files. Lets `describe()` name and order collections the way the model's authors meant | MEDIUM | C++ core (`src/database_ui_config.cpp`, `src/database_describe.cpp`) + a `get_ui_collection_metadata` through the C API and five bindings | F1, F7 (shares the struct/free idiom) |
| F9 | **`[[attribute_group]]` metadata, absorbing both `date_time` spellings** — `attribute_group.date_time.*` (SCE, BESSOperation, GNoMo) **and** a plain `[[attribute]] id = "date_time"` (all 6 HTD group files, GNoMo/`historical_conditions`) resolve to the same dimension-column metadata | 75 groups across 35 files, 39 carrying `date_time`. Without absorbing both spellings a third of the corpus silently loses its dimension label and date format — and Quiver already owns the `dimension_column` concept in `GroupMetadata` | MEDIUM | C++ core (parser + `src/database_describe.cpp` `print_group_columns`), C API getter, five bindings, Lua | F1, F7 |
| F10 | **`validate_ui_config()`** — cross-check the sidecar against the live SQL schema and report drift; advisory, mirrors `validate_migrations` (no out-param, throws on failure), **ships last** | The thing **nothing** in the ecosystem does: no CI job anywhere references `database/ui`, no model repo tests its own TOMLs, Hub's 7 negative fixtures never run against a model. It is also the *only* mitigation for the accepted risk — Quiver otherwise repeats unchecked labels with more authority than the TOML ever had | MEDIUM–HIGH | C++ core (new validator over parsed config × `Schema`), C API (1 symbol), five bindings, Lua (`db:validate_ui_config`, which obligates `bindings/js/src/lua-api.ts` or `lua-api-sync.test.ts` fails) | F1, F2, F8, F9 |

**Notes that matter:**

- **F6 is the highest-risk edit in the milestone, and it is in JS.** `bindings/js/src/ffi-helpers.ts` hardcodes `new Uint8Array(8)` with `setInt32(0)` / `setInt32(4)`; Bun cannot call `quiver_database_options_default` (struct-by-value, bun#6139), so JS has **no generated fallback**. The struct grows 8 → 24 bytes; get this wrong and C writes 16 bytes past a JS-owned buffer with no compile error. Its own plan, its own runtime assert. Python's CFFI cdef is ABI-mode — a stale cdef corrupts silently too. Dart's `bindings.dart` must be hand-edited (a full ffigen regen flips enums and breaks Hub) and `.dart_tool/hooks_runner/` + `.dart_tool/lib/` cleared or tests silently run the old layout. Julia is the safe one: 4 positional `quiver_scalar_metadata_t(...)` constructor calls in `database_metadata.jl` fail loudly with a `MethodError`.
- **F7 must not add fields to `ScalarMetadata` / `GroupMetadata`.** `bindings/js/src/metadata.ts` hardcodes `SCALAR_METADATA_SIZE = 56` / `GROUP_METADATA_SIZE = 32`, and those constants are the **out-buffer allocations** passed to C (L81/96/111/126) — growing either struct turns a stale JS constant into a native out-of-bounds write. A separate struct also keeps the enum map off `list_scalar_attributes`, which would otherwise carry one per column on every whole-collection read.
- **F7 lookup semantics:** a missing attribute or collection returns a default-constructed value (Hub's `?? AttributeConfiguration(id: …)` semantics), **not** a Pattern 2 throw. An explicit `get_ui_enum(name)` on an unknown vocabulary *does* throw Pattern 2 — it is a lookup, not decoration.
- **F10's findings already exist in the corpus** and are what its first run will print: HTD's `Configuration.inflow_type` (attribute declared in `configuration.toml`, vocabulary has 4 labelled entries, SQL column exists, binding missing); HTD's 9 untyped 0/1 `Configuration` flags; `Interconnection` absent from `main.collections` entirely; SCE's orphan `agent.toml`; SCE's `tab = "original_green"` with no matching `[[scalar_tab]]`; the 4 dead vocabularies; the 9 empty `unit.en = ""`.
- **The accepted risk F10 mitigates, in one line:** `HydroThermalDispatch.jl/src/collections/hydro_plant.jl` declares `HasCommitment` as `YES = 0 / NO = 1` while its `enum.toml` says `[[bool]] id=0 → "Disable"`. A reader alone hands that inversion to an LLM agent. PROJECT.md records this as decided and advisory — do not re-litigate the ordering, plan for the window.
- **F6 and F7 both hit the full release ritual**, because they change the native ABI: bump all five manifests (`CMakeLists.txt` is the source of truth, currently 0.10.6), `publish-s3` → tag → Julia/Python/JS in parallel, **Dart published by hand**. `CHANGELOG.md` currently heads `## [0.10.4] — unreleased` against 0.10.6 — reconcile before the Bump Version workflow runs.

### Anti-Features (deliberately NOT built)

Taken verbatim from `.planning/PROJECT.md` **Out of Scope**. These are settled; the reason column is the recorded one.

| Feature | Why it looks appealing | Why not (recorded reason) | Instead |
|---------|------------------------|---------------------------|---------|
| **`[[card]]` dashboard queries** | 68 ready-made SQL summaries across 47 files — looks like free analytics | An agent gets strictly more by running the SQL itself; pure Hub presentation | The agent writes its own SELECT through `read_data` / `query_*` |
| **`themes/*.toml`** | 25 files already sitting in `database/ui/` | A flat ARGB map feeding a Flutter `ColorScheme`; zero agent or model-code value | Leave them to Hub |
| **`[[attribute_query]]`** | A virtual computed attribute looks like a real one | Hub view affordance, 6 files | Same SQL, run by the caller |
| **`[[scalar_tab]]`** | Groups attributes into tabs, looks like structure | Hub view affordance, 2 files | Attribute groups (F9) carry the structure that is schema-backed |
| **`[[card]].conditions`** | Conditional display looks like a real rule | 1 occurrence in the corpus; no parser reads it | Do not implement keys nobody reads |
| **`[[enum item]].hide`** | Symmetry with `[[attribute]].hide` | 0 occurrences; no parser reads it | Do not implement keys nobody reads |
| **String-keyed enum vocabularies** | HTD stores 5 closed vocabularies as TEXT (`cut_strategy`, `solver`, `parallelization`, `state_initialization`, `stage_scenario_opening_map_type`) | `enum.toml` has no way to declare them today, so supporting them means extending the TOML format itself. `UIEnumEntry.code` stays `int64_t`. Deferred — user's call: *"don't care about HTD now"* | Revisit only if the format itself gains a string-keyed form |
| **Write-side scaffolding** (generating a collection TOML when a migration adds a table) | Would fix drift at the source | A reader fixes consumption, not drift. Doubles the milestone | Separate milestone if wanted |
| **Enforcing enum domains on write** | A reader that knows the domain "should" reject a bad code | Contradicts the settled boolean decision in `CLAUDE.md`, which ruled that `CHECK (col IN (0,1))` in the schema is where domain enforcement belongs. This milestone is descriptive | `CHECK` in the schema; F10 reports, never blocks |
| **Two locales live at once** | The files carry en/es/pt; exposing all of them looks more general | Locale resolves once at parse time to a single string. The `string \| table` union never reaches the C API or any binding | `ui_locale` on `DatabaseOptions` (F6), Hub's fallback chain applied inside the parser |

---

## Feature Dependencies

```
F1  UI sidecar parser (C++ core, toml++)
     ├──required by──> F2  enum vocab + code->label on scalars
     │                      ├──required by──> F3  enum labels in summarize_collection   <-- DEFECT CLOSED, no ABI change
     │                      └──required by──> F3b labels/units/enums in describe*
     ├──required by──> F4  graceful degradation + has_ui_config()
     │                      └──guards──> F3, F3b   (byte-identical output with no ui/ dir)
     ├──required by──> F5  convention path <db_dir>/ui
     │                      └──enables──> the whole ABI-free first phase
     ├──required by──> F6  ui_config_path + ui_locale on DatabaseOptions   [ABI BREAK]
     │                      └──required by──> F7  get_ui_metadata (new C struct)
     │                                              ├──shape reused by──> F8 collection metadata
     │                                              └──shape reused by──> F9 attribute_group metadata
     └──required by──> F10 validate_ui_config()   (needs F2 + F8 + F9 to have anything to check)

F6 ──conflicts with──> "do not grow ScalarMetadata/GroupMetadata"  (so F7 gets its OWN struct)
F10 ──mitigates──> the HasCommitment-inversion risk that F2/F3/F7 create
```

### Dependency notes

- **F2–F5 require F1** — there is no metadata without a parser, and it is a C++-core feature by PROJECT.md decision (one parser, not five).
- **F3 requires F4** — the byte-identical-with-no-config rule is what keeps `tests/test_database_describe.cpp` L84/L104 and `tests/test_lua_runner_describe.cpp` passing. Without it F3 is a test-breaking change on day one.
- **F5 replaces F6 for the first phase** — convention path costs nothing and is correct for 100% of the corpus; the explicit option is a *sequencing* addition, not a prerequisite. Do not couple them.
- **F7 requires F6** in practice: a structured getter whose config can only ever come from `<db_dir>/ui` is useless to claw, whose config dir resolves to `<installRoot>/database/ui` and whose read sandbox (`Claw/claw/src/tools/native.ts` `readRoots`) cannot even reach it.
- **F8 and F9 reuse F7's struct + free-function idiom** — plan them after F7 or the same alloc/free pattern gets invented twice. Model the free function on `quiver_database_free_time_series_data`; model grouped parallel arrays on `quiver_csv_options_t`.
- **F10 requires F2, F8, F9** — it cross-checks attributes, collections *and* groups; running it before F8/F9 land means it silently ignores two thirds of the drift it exists to find.
- **F6 conflicts with growing the existing metadata structs** — see the JS out-buffer hazard. This is why F7 is a new struct with its own size constant rather than four new fields.

---

## MVP Definition

### Launch with (v1 — the ABI-free patch)

- [ ] **F1** parser (`main.toml` + `main.collections` files + `enum.toml`, locale resolved at parse time) — everything depends on it
- [ ] **F2** enum vocabulary + code→label bound to scalars — the knowledge the agent is missing
- [ ] **F3** enum labels in `summarize_collection` — **closes the triggering defect; no ABI change; ships as a patch; free in all five bindings and Lua**
- [ ] **F3b** labels/units/enum vocabulary in `describe` / `describe_collection` — same three format strings, same phase
- [ ] **F4** graceful degradation + `has_ui_config()` — without it v1 is a breaking change for every existing caller
- [ ] **F5** convention path `<db_dir>/ui` — what makes v1 ABI-free
- [ ] `tests/schemas/ui/` fixture corpus distilled from the real 117 files + exact-string assertions in the C++ and Lua describe suites

**If the milestone stalls here, it succeeded** — PROJECT.md Core Value is exactly this.

### Add after validation (v1.x — the ABI phase)

- [ ] **F6** `ui_config_path` + `ui_locale` on `DatabaseOptions` — trigger: a consumer whose UI dir is not `<db_dir>/ui` (claw is one today) or who needs a non-`en` locale. Minor bump; JS options buffer gets its own plan and a runtime assert
- [ ] **F7** `get_ui_metadata` structured getter — trigger: claw dropping the per-attribute half of `study-config.ts`, the one committed consumer

### Future consideration (v2+)

- [ ] **F8** collection-level metadata incl. `main.collections` display order — defer: pure elaboration once F7's struct idiom exists
- [ ] **F9** `[[attribute_group]]` metadata absorbing both `date_time` spellings — defer: same, and it is the one that needs the HTD-slice fixture to be honest
- [ ] **F10** `validate_ui_config()` — ships **last** by explicit decision, advisory only. Accepts a window where Quiver repeats unchecked labels

---

## Feature Prioritization Matrix

| # | Feature | User value | Implementation cost | Priority |
|---|---------|-----------|---------------------|----------|
| F1 | UI sidecar parser (C++) | HIGH (enabling) | MEDIUM | P1 |
| F2 | Enum vocab + code→label on scalars | HIGH | LOW | P1 |
| F3 | Enum labels in `summarize_collection` | HIGH | LOW | P1 |
| F3b | Labels/units/enums in `describe*` | MEDIUM | LOW | P1 |
| F4 | Graceful degradation / `has_ui_config()` | HIGH (protects every existing caller) | LOW | P1 |
| F5 | Convention path `<db_dir>/ui` | HIGH (buys the whole ABI-free phase) | LOW | P1 |
| F6 | `ui_config_path` + `ui_locale` on `DatabaseOptions` | HIGH (unblocks claw) | **HIGH** (ABI break, JS memory hazard, full release ritual) | P2 |
| F7 | `get_ui_metadata` structured getter | HIGH (the committed consumer) | HIGH (new C struct + 5 bindings + Lua) | P2 |
| F8 | Collection metadata + display order | MEDIUM | MEDIUM | P3 |
| F9 | `[[attribute_group]]` metadata, both `date_time` spellings | MEDIUM | MEDIUM | P3 |
| F10 | `validate_ui_config()` | HIGH (only mitigation for the wrong-label risk) | MEDIUM–HIGH | P3 (ships last, by decision) |

**Priority key:** P1 = must have for the milestone to have succeeded · P2 = should have, the real work for the committed consumer · P3 = elaboration and the advisory validator.

---

## Prior Art (who already implements this format)

| Capability | PSRHub (Dart/Flutter) | Claw (TypeScript) | DDL comments / Julia `@enumx` | Quiver's approach |
|---|---|---|---|---|
| Parse `main.toml` + collections + `enum.toml` | Full: 15 classes, ~719 lines, `Hub/hub1/lib/models/configuration/` over `lib/models/utils/toml_utils.dart` (pub `toml ^0.17.0`). **The de facto spec** | Deliberately partial: `Claw/claw/src/core/study-config.ts` reads `main.model/processes/collections` + each file's `id`, via `smol-toml` | n/a | One C++ parser (F1) behind the existing `tomlplusplus`, matching the Dart accept-set, not `toml-schema.md` |
| Enum code → label | Yes, rendered in the GUI | No — explicitly deferred: *"enriching it from the TOML belongs in quiverdb"* | `Foresight.jl/database/migrations/1/up.sql`: 46 free-text Portuguese comment lines, 3 formats, 18 of 25 columns, **0 CHECK constraints**; the richest instance is a block comment *outside* the parens and so dropped by `sqlite_master` | F2 + F3: machine-readable, column-bound, rendered where the codes are today |
| Unknown keys | Ignored, never rejected (throws only on unknown `type` / `view` / `form_view`, missing `id`/`query`/`collections`) | n/a | n/a | Same leniency + a warning through `impl_->logger` (F1/F4) |
| Group membership | Recovered from SQLite table names, **not** from the TOML | n/a | Implied by `{C}_vector_{g}` naming | F9, joined against `Schema` — Quiver already owns `dimension_column` |
| Validation vs. the live schema | Nothing. 7 negative fixtures in `Hub/hub1/test/schema/invalid/` that never run against a model repo | Nothing | Nothing — three unsynced sources that already disagree (HTD: 19 `@enumx` vs 11 vocabularies; GNoMo: 3 vs 11) | **F10 — the first one in the ecosystem** |
| Versioning | None. 311 `database/ui/*.toml` across ~30 repos, zero `version` keys, no parser reads one | None | n/a | Quiver's fixture corpus + changelog + semver become the de facto contract |

Existing Quiver surface that is **not** a substitute: `CSVOptions::enum_labels` (`include/quiver/options.h`, attribute → locale → label → value) is caller-supplied, CSV-only, keyed by bare attribute name, never stored, and unused by any shipped caller.

---

## Sources

- `C:/Development/Quiver/quiver3/.planning/PROJECT.md` — the decided scope; authoritative for everything above
- Larger survey journal (PSR UI TOML spec across all 13 models, format owner, Quiver build cost, provenance, + two adversarial verifications — `one-parser-possible` CONFIRMED, `no-owner` REFUTED and corrected): `C:/Users/rsampaio/.claude/projects/C--Development-Quiver-quiver3/09393d1a-ae67-415d-86f8-a8b91ba2b210/subagents/workflows/wf_92804be2-64e/journal.jsonl`
- Earlier survey journal (enum defect, Foresight DDL comments, Quiver describe surface, agent tooling): `C:/Users/rsampaio/.claude/projects/C--Development-Quiver-quiver3/09393d1a-ae67-415d-86f8-a8b91ba2b210/subagents/workflows/wf_e5d70f00-efb/journal.jsonl`
- Reference parser: `C:/Development/Hub/hub1/lib/models/configuration/*.dart`, `C:/Development/Hub/hub1/lib/models/utils/toml_utils.dart`
- Known-wrong spec doc (do not write the parser against it): `C:/Development/Hub/hub1/.claude/skills/psrhub-ui/references/toml-schema.md`
- Committed consumer: `C:/Development/Claw/claw/src/core/study-config.ts`, `src/tools/describe-data.ts`, `src/prompt.ts`, `src/tools/native.ts`, `src/core/study.ts`
- Quiver surfaces this milestone edits: `src/database_describe.cpp`, `src/database_impl.h`, `include/quiver/options.h`, `include/quiver/attribute_metadata.h`, `include/quiver/c/database.h`, `src/c/options.cpp`, `src/c/database_options.h`, `src/binary/binary_metadata.cpp`, `src/lua_runner.cpp`, `src/CMakeLists.txt`
- Layout-coupled binding sites: `bindings/js/src/metadata.ts`, `bindings/js/src/ffi-helpers.ts`, `bindings/js/src/csv.ts`, `bindings/js/src/lua-api.ts`, `bindings/python/src/quiverdb/_c_api.py`, `bindings/dart/lib/src/ffi/bindings.dart`, `bindings/julia/src/c_api.jl`
- Format-pinning tests: `tests/test_database_describe.cpp`, `tests/test_lua_runner_describe.cpp`, `bindings/js/test/lua-api-sync.test.ts`
- Corpus: `database/ui/` in BESSOperation, Boost, CHain, CarbSteeler, Foresight, GNoMo, HyCO, HydroThermalDispatch, OptBio, SCE, SORA, SORA2, `Templates/PSRExample.jl`

---
*Feature research for: Quiver UI-metadata layer (brownfield C++ library milestone)*
*Researched: 2026-09-17*
