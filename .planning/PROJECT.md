# Quiver — UI Metadata Layer

## What This Is

Quiver is a SQLite wrapper library with a C++ core, a C API for FFI, and five language
bindings (Julia, Dart, Python, JS/Bun, plus embedded Lua). This milestone teaches it to read
the PSR `database/ui/` TOML sidecar that every PSR Julia model already ships, and to surface
that metadata — enum vocabularies, labels, tooltips, units, formats, display order — through
its existing schema-description and metadata surfaces, in every layer.

It is for the LLM agents and applications that read a PSR study database through Quiver.
Today an agent calling `describe_data` sees an INTEGER column and cannot tell that it is an
enumeration, let alone what `0` means.

## Core Value

An agent (or any consumer) calling Quiver's `describe` on a PSR model database sees what an
INTEGER column actually **means** — `values {0: 8 (Disabled), 1: 4 (Enabled)}`, not bare codes.

Everything else in this milestone is elaboration. If only that ships, the milestone succeeded.

## Requirements

### Validated

<!-- Inferred from the existing codebase (.planning/codebase/), shipped and relied upon. -->

- ✓ C++ core with SQLite-backed CRUD, transactions, dry runs, label-addressed writes — existing
- ✓ Schema introspection: `describe()`, `describe_collection()`, `summarize_collection()` text reports — existing
- ✓ Typed metadata readers: `get_{scalar,vector,set,time_series}_metadata()`, `list_*` group readers — existing
- ✓ Lazy schema metadata loading via `Impl::require_schema` / `load_schema_metadata` — existing
- ✓ C API covering the full public C++ surface, single error channel (`quiver_get_last_error`) — existing
- ✓ Five bindings (Julia, Dart, Python, JS, Lua) kept homogeneous by the cross-layer naming contract — existing
- ✓ CSV export/import with caller-supplied enum label mapping via `CSVOptions::enum_labels` — existing, unused by any shipped caller
- ✓ Binary (`.qvr`) + expression subsystems, Julia and Lua only — existing
- ✓ `tomlplusplus` v3.4.0 vendored and linked `PRIVATE` into the `quiver` target — existing, used by `src/binary/binary_metadata.cpp`

### Active

- [ ] Quiver parses the PSR `database/ui/` TOML sidecar (`main.toml`, per-collection files, `enum.toml`) in C++
- [ ] Enum vocabularies (name + code→label) are attached to scalar attributes and rendered by `summarize_collection`'s value histogram
- [ ] `describe` and `describe_collection` render UI labels, units, and enum vocabularies when a config is present
- [ ] `DatabaseOptions` accepts an explicit UI config path and a locale, exposed as optional parameters in all five bindings
- [ ] A structured getter returns UI metadata per attribute (label, tooltip, unit, format, hidden, enum name, enum values) through the C API and all five bindings
- [ ] Collection-level UI metadata (label, icon, help, `main.collections` display order) is exposed
- [ ] `[[attribute_group]]` metadata is exposed, absorbing both spellings of the time-series dimension column
- [ ] `validate_ui_config()` cross-checks the sidecar against the live SQL schema and reports drift (advisory, ships last)

### Out of Scope

- **`[[card]]` dashboard queries** — 68 SQL strings across the corpus; an agent gets strictly more by running the SQL itself. Pure Hub presentation.
- **`themes/*.toml`** — a flat ARGB map feeding a Flutter `ColorScheme`. Zero agent or model-code value.
- **`[[attribute_query]]` and `[[scalar_tab]]`** — Hub view affordances, 6 and 2 files respectively.
- **`[[card]].conditions` and `[[enum item]].hide`** — 1 and 0 occurrences in the corpus; no parser reads them. Do not implement keys nobody reads.
- **String-keyed enum vocabularies** — HydroThermalDispatch stores 5 closed vocabularies as TEXT (`cut_strategy`, `solver`, `parallelization`, `state_initialization`, `stage_scenario_opening_map_type`). `enum.toml` has no way to declare them today, so supporting them means extending the TOML format itself. `UIEnumEntry.code` stays `int64_t`. Deferred — user's call: "don't care about HTD now".
- **Write-side scaffolding** (generating a collection TOML when a migration adds a table) — a reader fixes consumption, not drift. Doubles the milestone; separate milestone if wanted.
- **Enforcing enum domains on write** — contradicts the settled boolean decision in CLAUDE.md, which ruled that `CHECK (col IN (0,1))` in the schema is where domain enforcement belongs. This milestone is descriptive.
- **Two locales live at once** — locale resolves once at parse time to a single string. The `string | table` union never reaches the C API or any binding.

## Context

### The triggering defect

An agent (`claw`) calling `describe_data` → `quiverdb.describe()` gets, per scalar: name,
`DataType`, `PRIMARY KEY`, `NOT NULL`. With `summary=true` it additionally gets an integer
histogram — `values {0: 8, 1: 4}` — for non-PK INTEGER columns under
`kMaxDistributionCardinality = 64`. That histogram is the defect in miniature: Quiver renders
the enum **codes** in exactly the position where the **labels** belong, and that constant's own
comment calls it "the enum/category case".

`Claw/claw/src/prompt.ts` already *promises* the agent that `describe_data` reports
enumerations. That promise is false today.

The agent's only workaround is
`read_data(sql = "SELECT name, sql FROM sqlite_master WHERE type='table' AND name IN (...)")`
followed by parsing DDL comment prose — schema introspection by regex.

### Where enum knowledge lives today (four unsynced places)

1. `Foresight.jl/src/enums.jl` — 17 `@enumx` blocks, plus `TimeSeries_Model` in
   `src/time_series_models/base.jl`.
2. `database/migrations/1/up.sql` — 46 free-text Portuguese comment lines covering 18 of 25
   enum-suspect columns, in three mutually incompatible formats, with **zero** CHECK constraints.
3. `database/ui/enum.toml` — the only source that is machine-readable **and** column-bound.
4. A hand-written agent skill (`foresight-case-from-csv/SKILL.md`) that reverse-engineered codes
   from a CSV because `Consumption.forecast_model` (10 values) appears in no comment at all.

Nothing tests that these agree, and they already don't. The block comment above
`CREATE TABLE Consumption` claims `2 = híbrido` for a two-valued enum, names the column `model`
when it is `forecast_model`, and — being *outside* the parens — is dropped by `sqlite_master`
entirely. The agent's workaround cannot even reach the one place those semantics were written down.

### Why the DDL-comment route was rejected

Prose, three formats, one human language (Portuguese), missing 7 of 25 columns including the
worst one, the richest instance invisible to `sqlite_master`, and Quiver has never parsed a byte
of SQL text (`src/schema.cpp` selects `name` only). The TOML pair is the only complete,
machine-readable, column-bound source.

### The format and its owner

The spec is **one Flutter app's parser behaviour**: `C:/Development/Hub/hub1/lib/models/configuration/*.dart`
(15 classes, ~719 lines) plus `lib/models/utils/toml_utils.dart`. That accept-set is what a C++
parser must match.

There is a written spec at `Hub/hub1/.claude/skills/psrhub-ui/references/toml-schema.md`, but it
**is already wrong**: it claims "attributes declared after a group belong to that group"; the
parser does not implement that — it keys one flat per-collection map and recovers group membership
by joining `[[attribute_group]].id` against `{Collection}_vector_{id}` / `_time_series_{id}` table
names in SQLite. **Write Quiver's parser against the Dart source, not the doc.**

### Corpus scale and consistency

13 model repos ship `database/ui/`: BESSOperation, Boost, CHain, CarbSteeler, Foresight, GNoMo,
HyCO, HydroThermalDispatch, OptBio, SCE, SORA, SORA2, and `Templates/PSRExample.jl`.
Corpus: 117 files in scope — 13 `main.toml`, 67 collection files, 12 `enum.toml`
(9 non-empty; Boost has none). 736 attributes, 75 attribute groups, 62 vocabularies,
164 enum entries, 127 enum bindings, **0 dangling references**, 4 dead vocabularies.

**A single parser can serve all 13 — adversarially verified, CONFIRMED.** All 117 files parse
with a stock TOML parser. Across the whole corpus there is **exactly one** unknown key
(`conditions`, in `GNoMo/floating_storage_unit.toml`). No key's type varies by model.
Per-model absences are `containsKey` / `existsSync` guards, not branches on model identity.

Realistically the reach is **6–8 models**: Boost, CHain, SORA and SORA2 have empty or near-empty
`enum.toml` and stub `configuration.toml` — likely dead or pre-launch.

### Parser tolerances that are non-negotiable

Derived from the Dart parser's actual behaviour:

- **(a)** Localizable fields are `string | table` — and the *same file* mixes both
  (Foresight 12 bare / 711 dotted; GNoMo 1/735; BESSOperation 78/0).
- **(b)** `enum.toml` and `themes/dark.toml` may be absent or zero-byte.
- **(c)** Unknown keys are ignored, never rejected.
- **(d)** Enum ids are arbitrary ints, never positional — GNoMo `weekday` is 1–7; HTD
  `initial_volume_type` and SCE `granularity_type` are gapped `[0, 2]`.
- **(e)** Collections load **only** from `main.collections`, never a directory scan
  (SCE's `agent.toml` is a fully-formed orphan and must stay unloaded).
- **(f)** Attribute ids and group ids are **separate namespaces**
  (`BESSOperation/storage.toml` uses `degradation` for both).
- **(g)** `[[attribute]]` / `[[attribute_group]]` interleaving is legal and flattens — 15 files.
- **(h)** Collection `id` is PascalCase (the SQL table name) while `main.collections` lists
  snake_case filenames.
- **(i)** The time-series dimension column has two spellings: `attribute_group.date_time.*`
  (SCE, BESSOperation, GNoMo) vs a plain `[[attribute]] id = "date_time"` (all 6 HTD group files,
  GNoMo/historical_conditions).
- **(j)** `format` carries two grammars (`{:.2f}` on values, `yyyy-MM-dd` on dates) **and** accepts
  a 4-key table form that no file currently uses. Accept both from day one — four lines — or a
  string-only parser passes every test today and breaks the first time someone uses it.

### The accepted risk

`HydroThermalDispatch.jl/src/collections/hydro_plant.jl` declares `HasCommitment` as
`YES = 0 / NO = 1`. Its `enum.toml` says `[[bool]] id=0 → "Disable"`. **The UI has been showing
the inverse of the code's meaning and nothing catches it.**

Today that is one Flutter app rendering a wrong string. After this milestone Quiver hands the
same inversion to an LLM agent that will reason on it. A reader alone makes this class of bug
*worse*, not better — `validate_ui_config()` is the only mitigation, and it ships last by
explicit decision. There is a window where Quiver authoritatively repeats labels it has not checked.

The Julia `@enumx` declarations are a third, also-incomplete source — the sets diverge both ways
(HTD: 19 `@enumx` vs 11 TOML vocabularies; GNoMo: 3 `@enumx` vs 11 TOML vocabularies, with
`optimization_solver`, `solution_method`, `scenario_tree`, `weekday`, `plant_initial_state` having
no Julia declaration at all). Naming is unmappable across three styles. Foresight's own
`enum.toml` header says the values *"mirror the @enumx definitions"* — mirror, not generate.
So codegen is not available; the TOML is the input, and the validator reports disagreement rather
than resolving it.

### The committed consumer

`Claw/claw/src/core/study-config.ts` carries the comment *"enriching it from the TOML belongs in
quiverdb"*, and the user has confirmed this was an agreed decision: **claw will consume the
structured getters** and drop the per-attribute half of `study-config.ts`. That is what makes the
later phases real work rather than speculation. Hub consuming it is not a goal of this milestone.

Note that claw's read sandbox (`Claw/claw/src/tools/native.ts` `readRoots`) currently cannot
reach `database/ui` — which is precisely why the host-supplies-a-parsed-map alternative was
rejected: it would have blocked claw indefinitely.

### Governance gaps

- **Zero version mechanism.** 311 `database/ui/*.toml` files across ~30 repos; a grep for
  `version` / `schema_version` / `format_version` returns nothing. Neither Hub nor Claw reads one.
- **Zero enforcement.** No CI job anywhere references `database/ui`. No model repo tests its own
  TOMLs. Hub's `test/schema/invalid/` has 7 negative fixtures that never run against a model repo.
- **Both sides still move.** Hub parsers: 59 commits, latest 2026-06-24 (added `view = "chart"`).
  Foresight's TOMLs were edited 2026-09-17 (today); CarbSteeler 2026-09-03; GNoMo 2026-09-02.
- **The Hub clones already disagree** — `format_configuration.dart` differs between hub1 and hub3.
  hub1 is what this milestone was surveyed against.

## Constraints

- **Tech stack**: C++20 core, C API for FFI, five bindings. Logic lives in C++; bindings stay thin
  (root `CLAUDE.md` principle). A new metadata reader is a C++ feature bound outward, never five parsers.
- **Dependencies**: `tomlplusplus` v3.4.0 is already vendored and linked `PRIVATE` to the `quiver`
  target — **zero CMake change** required. Copy the `from_toml_content` / `from_toml_file` split
  from `src/binary/binary_metadata.cpp`.
- **Compatibility**: adding a field to `DatabaseOptions` is a **C ABI break** (the options struct
  grows 8 → 24 bytes). The highest-risk edit in the milestone is `bindings/js/src/ffi-helpers.ts`
  `makeDefaultOptions`, which hardcodes `new Uint8Array(8)` with `setInt32(0)` / `setInt32(4)`.
  Bun cannot call `quiver_database_options_default` (struct-by-value, bun#6139), so JS has no
  generated fallback — get this wrong and C writes 16 bytes past a `Uint8Array`. Needs its own
  plan and a runtime assert.
- **Compatibility**: do **not** add fields to `ScalarMetadata` / `GroupMetadata`.
  `bindings/js/src/metadata.ts` hardcodes `SCALAR_METADATA_SIZE = 56` and
  `GROUP_METADATA_SIZE = 32`, and those constants are the **out-buffer allocations** passed to C.
  Growing either struct turns a stale JS constant into a native out-of-bounds write. Use a new
  struct with its own size constant.
- **Compatibility**: Dart's `bindings.dart` must be **hand-edited**, not regenerated — a full
  ffigen regen flips enums and breaks Hub. Clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/`
  or tests silently run the old layout.
- **Compatibility**: Python's CFFI cdef is ABI-mode — a stale cdef corrupts silently with no
  compile error.
- **Behaviour**: with no UI config present, `describe` / `describe_collection` /
  `summarize_collection` output must be **byte-identical** to today. This keeps every existing
  assertion in `tests/test_database_describe.cpp` (including the brittle negative assertions) and
  `tests/test_lua_runner_describe.cpp` passing unchanged. New behaviour is proven by new fixtures.
- **Release**: any native change requires the full ritual — version bump across all five manifests,
  `publish-s3` → tag → Julia/Python/JS in parallel, Dart published by hand. **`CHANGELOG.md`
  currently heads `## [0.10.4] — unreleased` against `CMakeLists.txt` 0.10.6 — reconcile before
  the Bump Version workflow runs.**
- **Testing**: per `tests/CLAUDE.md`, a cross-layer feature adds cases to the C++ suite, the C API
  suite, and all five binding suites, with shared schemas under `tests/schemas/` (never copied into
  a binding). Note the four binding describe suites currently assert only "returns a String" — they
  prove nothing today and must be strengthened deliberately or left alone honestly.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Scope is the full UI metadata layer, not enums only | The TOMLs hold labels, units, tooltips, formats, order and grouping that SQL cannot express; an agent reading a describe output wants all of it | — Pending |
| Quiver reads the TOML itself, in C++ | One parser serves Lua and all five bindings; the host-supplies-a-map alternative needs five parsers and blocks claw, whose read sandbox cannot reach `database/ui` | — Pending |
| Reach is all PSR Julia models, not Foresight alone | 13 repos already ship the convention; a single parser is adversarially confirmed to serve them all | — Pending |
| Exclude cards, themes, `attribute_query`, `scalar_tab` | Pure Hub presentation with no agent or model-code value; a card's SQL is better run than read | — Pending |
| Enum codes stay `int64_t`; HTD's TEXT vocabularies deferred | `enum.toml` cannot declare them today, so support means extending the format itself. User: "don't care about HTD now" | — Pending |
| `validate_ui_config()` is advisory and ships last | User's explicit call, made with the `HasCommitment` inversion on the table. Accepts a window where Quiver repeats unchecked labels | ⚠️ Revisit |
| A missing or malformed `ui/` dir degrades silently | Failing the schema load would break every existing caller whose UI dir moved. Log a warning; `has_ui_config()` returns false; `open()` still succeeds | — Pending |
| Locale resolves once at parse time | `DatabaseOptions.ui_locale` (default `"en"`) with Hub's fallback chain (exact → `en` → first key). The `string \| table` union never reaches the C API or any binding — the single biggest available simplification | — Pending |
| Parser written against Hub's Dart source, not `toml-schema.md` | The only written spec is already wrong about group membership | — Pending |
| Descriptive, never enforcing | Contradicts the settled boolean decision: `CHECK (col IN (0,1))` in the schema is where domain enforcement belongs | — Pending |
| The enum-in-describe fix ships first, on convention, with no ABI change | `describe*` already returns a `std::string` through the C API, so it reaches all five bindings and Lua for free. Ships as a patch. If the milestone stalls there, the triggering defect is still closed | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-09-17 after initialization*
