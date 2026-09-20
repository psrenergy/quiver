# Quiver — UI Metadata in `describe`

## What This Is

Quiver is a SQLite wrapper library with a C++ core, a C API for FFI, and five language bindings
(Julia, Dart, Python, JS/Bun, plus embedded Lua). This milestone teaches the C++ core to read the
`database/ui/` TOML sidecar that every PSR Julia model already ships, and to render three things
per scalar attribute — **label, tooltip, enum labels** — in the existing text reports.

It is for the LLM agents that read a PSR study database through Quiver. Today an agent calling
`describe` sees an INTEGER column and cannot tell that it is an enumeration, let alone what `0`
means.

## Core Value

An agent calling `describe` on a PSR study database sees what an INTEGER enum column actually
**means** — `0 = User Defined Forecast, 1 = Model` — not bare codes.

Everything else is elaboration. If only that ships, the milestone succeeded.

## Current Milestone: v0.10.8 UI Metadata in `describe`

**Goal:** Read the `ui/` sidecar beside a model's migrations and render label, tooltip and enum
labels in `describe` / `describe_collection` / `summarize_collection`.

**Target features:**
- A UI reader in the C++ core, reached only from `from_migrations`
- Three fields per attribute: `label`, `tooltip`, `enum_labels`
- Rendered in all three text reports; byte-identical output when no `ui/` dir exists
- Zero C API symbols, zero binding code, patch version bump

## Requirements

### Validated

<!-- Inferred from the existing codebase (.planning/codebase/), shipped and relied upon. -->

- ✓ C++ core with SQLite-backed CRUD, transactions, dry runs, label-addressed writes — existing
- ✓ Text reports `describe()`, `describe_collection()`, `summarize_collection()` — existing
- ✓ All three reach the C API as `new_c_str(...)` wrappers over `std::string`, so their payload is
  opaque and richer content needs no new symbol — existing (`src/c/database.cpp:129-167`)
- ✓ Lazy schema metadata loading via `Impl::require_schema` / `load_schema_metadata` — existing
- ✓ `tomlplusplus` v3.4.0 vendored and linked **PRIVATE** to the `quiver` target, exercised by
  `src/binary/binary_metadata.cpp` — existing

### Active

- [ ] `from_migrations` resolves the `ui/` sibling of the migrations directory and parses every
      collection file in it
- [ ] `enum.toml` vocabularies are parsed and joined to attributes by the attribute's `enum` value,
      stored as `std::map<int64_t, std::string> enum_labels`
- [ ] `describe` and `describe_collection` render `label`, `tooltip` and the enum labels per scalar
      attribute when a UI config is present
- [ ] `summarize_collection` annotates its integer value histogram with enum labels
- [ ] With no `ui/` directory present, all three reports are byte-identical to today

### Out of Scope

- **`main.toml`** — not parsed at all. A non-recursive `directory_iterator` over `ui/*.toml`,
  keeping files that carry both a top-level string `id` and an `attribute` array, self-selects
  exactly the 67 collection files across the whole corpus. That removes the `collections` array,
  the snake_case→PascalCase filename mapping, the orphan-file case and the listed-but-missing case.
- **The other seven `[[attribute]]` keys** — `hide` (360 occurrences), `unit` (324), `format` (141),
  `tab` (68), `enabled_if` (13), and `type` beyond its join role. Parsed and ignored; tomlplusplus
  needs no schema declaration, so ignoring is free.
- **`[[attribute_group]]`** — its ids collide with `[[attribute]]` ids in 15 real files, it has
  `label` but never `tooltip`, and merging the two arrays into one id-keyed map silently overwrites
  one label with the other.
- **Collection-level metadata** (`icon`, top-level `label`, `help`, `description`, display order) —
  the agent's question is about columns, not chrome.
- **`[[card]]`, `themes/`, `[[attribute_query]]`, `[[scalar_tab]]`** — pure Hub presentation.
- **A structured getter through the C API and bindings** — `describe*` already returns a string, so
  the whole feature reaches all five bindings and Lua for free. A getter roughly doubles the
  milestone and its test surface for a consumer that does not exist yet.
- **Any change to `DatabaseOptions`, `ScalarMetadata` or `GroupMetadata`** — all three are C ABI
  breaks with silent-corruption failure modes in JS (see Constraints).
- **Locales other than English** — `en` if the value is a table, the string itself if it is a
  string. No option, no API surface.
- **A `hide` filter** — 360 of 736 attributes carry `hide = true`. `describe` describes the schema,
  not the UI, so a hidden attribute still renders with its label. Deciding otherwise would make
  `hide` a fourth field.
- **`validate_ui_config()`** — cross-checking the sidecar against the live schema is a separate
  milestone. See the accepted risk below.
- **Reading `ui/` from `open()` or `from_schema`** — neither has a migrations path to resolve a
  sibling from, and PSR models reach both create and load through `from_migrations`
  (`Foresight.jl/src/inputs.jl:50,75`). `validate_migrations` does not route through
  `from_migrations` either (`src/database.cpp:260-284` builds its own in-memory `Database`), so it
  needs no UI handling.

## Context

### The triggering defect

An agent calling `describe_data` → `quiverdb.describe()` gets, per scalar: name, `DataType`,
`PRIMARY KEY`, `NOT NULL`. `summarize_collection` additionally emits an integer histogram —
`values {0: 8, 1: 4}` — for non-PK INTEGER columns under `kMaxDistributionCardinality = 64`
(`src/database_describe.cpp:16`). That constant's own comment calls it *"the enum/category case"*.
Quiver renders the enum **codes** exactly where the **labels** belong.

### The corpus

13 model repos ship `database/ui/` as a sibling of `database/migrations/` — BESSOperation, Boost,
CarbSteeler, CHain, Foresight, GNoMo, HyCO, HydroThermalDispatch, OptBio, SCE, SORA, SORA2,
`Templates/PSRExample.jl`. 117 files: 13 `main.toml`, 67 collection files, 12 `enum.toml`, 25
`themes/*.toml`. All 117 parse with a stock TOML parser; 0 failures, 0 BOMs.

736 `[[attribute]]` entries, of which **127** carry `type = "enum"` and `enum = "<vocab>"` (the two
sets coincide exactly). 62 vocabularies, 164 entries, **0 dangling references**, 4 dead vocabularies.
Of the 127 enum-bound attributes, **121 are main-table scalars** — so `summarize_collection`, which
iterates `list_scalar_attributes`, reaches 121 of them; the other 6 are group columns visible only
in `describe_collection`.

The sibling layout survives packaging: the compiled PSR app ships the whole `database/` directory
next to the binary (`Foresight.jl/compile/compile.jl:26,43`; `src/inputs.jl:30` resolves migrations
under `Sys.BINDIR`). Had that been false the feature would be dead everywhere it matters.

### Parser tolerances that are non-negotiable

Each was measured against the real corpus, not inferred:

- **Collection key is the file's own top-level `id`, never derived from the filename.**
  `dc_line.toml` declares `id = "DCLine"`.
- **Two shapes for every localizable value, decided per value.** A bare `label = "ARIMA"` means
  *the same in every language*; a table `label.en = "..."` means *this language specifically*.
  So a string is used as-is, and a table is read at `en`. At `[[attribute]]` level: label 678 table
  / 35 bare / 23 absent, tooltip 476 / 31 / 229. **0 of 817 locale tables lack `en`**, so
  hardcoding `en` is exact today; an "else first key" line is one-line insurance, never a fallback
  to another language's text presented as English.
- **One accessor serves all three fields**, including the `label` inside each `enum.toml` entry —
  those are locale-shaped too.
- **137 of 1751 label/tooltip strings contain a literal newline** (`"Mean\nProduction\nFactor"` —
  UI column-header wrapping). They must collapse to a space or they shred a line-oriented report.
- **344 strings are non-ASCII UTF-8** (`hm³`, `m³/s`, `°C`, Portuguese accents). Copy bytes through.
- **Enum ids are arbitrary, never positional.** `weekday` is `[1..7]`; `initial_volume_type` and
  `granularity_type` are gapped `[0, 2]`.
- **The enum join key is the attribute's `enum` value, not its `id`** — 46 attributes share `bool`.
- **Absent, empty and partial are all normal.** Boost has no `enum.toml`; CHain, SORA and
  PSRExample ship zero-byte ones; HTD's `Interconnection` table has no ui file at all.
- **The UI is a partial overlay on the schema, not the other way round.** 15 of 736 attributes name
  a column that does not exist. Iterate the schema, look the UI up by name.
- **Collection lookup is case-sensitive** — `Schema` stores tables in a `std::map`
  (`include/quiver/schema.h:104`) while SQLite matches table names case-insensitively. A ui `id`
  whose casing differs from the `CREATE TABLE` spelling matches nothing, silently. Accepted: the
  overlay is partial by design, and there is no diagnostic.

### Two resolution traps

**`parent_path()` is not enough.** Compiled and verified: `"C:/x/database/migrations"` →
`C:/x/database\ui` (correct), but `"C:/x/database/migrations/"` → `.../migrations\ui`, and a bare
relative `"migrations"` → `ui` resolved against the **process CWD**, which can load an unrelated
`./ui`. Trailing-slash migrations paths already appear in this repo's tests
(`tests/test_c_api_database_lifecycle.cpp:229,242`, `tests/test_database_lifecycle.cpp:265`). Use
`fs::weakly_canonical(migrations_path).parent_path() / "ui"` — the same call
`src/lua_runner.cpp`'s `resolve_sandboxed_path` already uses.

**The `migrate_up` early-return trap.** `migrate_up` calls `impl_->load_schema_metadata()` at
`src/database.cpp:437`, but early-returns at `:398-401` (no versioned subdirs) and `:406-409`
(already up to date) before reaching it — both early returns are pinned by tests. Foresight's
`load_study`, which opens an existing study, is exactly the already-up-to-date case. **UI parsing
must therefore live in `from_migrations` itself**, not hooked onto schema loading, which would work
on create and silently no-op on load.

### Output budget

A real study grows the report materially: GNoMo has 236 attributes (its `Configuration` alone has
64), mean English tooltip 49 chars, max 266. Whole-DB `describe()` on GNoMo gains roughly 15–20 KB.
Acceptable for an LLM reader, but named here so it is a decision rather than a surprise. If it
bites, the lever is rendering `tooltip` only in `describe_collection` / `summarize_collection` and
keeping `describe()` to label + enum labels.

### The accepted risk

`HydroThermalDispatch.jl/src/collections/hydro_plant.jl` declares `HasCommitment` as `YES = 0 /
NO = 1`; its `enum.toml` says `[[bool]] id = 0 → "Disable"`. **2 of the 8 mechanically-checkable
attributes are inverted.** Today that is one Flutter app rendering a wrong string. After this
milestone Quiver hands the same inversion to an LLM agent that will reason on it. A reader alone
makes this class of bug *worse*, not better. `validate_ui_config()` is the only mitigation and is
explicitly out of scope — so this is a recorded, accepted state, not a discovery.

## Constraints

- **Tech stack**: C++20 core. `tomlplusplus` v3.4.0 is already linked **PRIVATE** on `quiver`
  (`src/CMakeLists.txt:81`) — zero CMake change. But PRIVATE does not propagate, so `quiver_c`,
  `quiver_cli` and `quiver_tests` cannot include toml++: the parser must be a `.cpp` in
  `QUIVER_SOURCES`, and the C++ suite must drive it through the public API.
  `tests/test_binary_metadata.cpp` is the precedent — 819 lines, zero toml++ includes.
- **Error handling**: do **not** copy `binary_metadata.cpp`'s error posture. It throws
  (`toml::parse_error`, and `std::bad_optional_access` from bare `.value()` on a missing key). The
  whole UI load belongs in one try/catch → `logger->warn` → empty map, following the
  `src/database.cpp:80-86` precedent. Failing the open would break every existing caller whose ui
  dir moved. Note PSR passes `QUIVER_LOG_OFF`, so the warning only reaches `quiver_database.log`.
- **Compatibility**: no field may be added to `ScalarMetadata` or `GroupMetadata`.
  `bindings/js/src/metadata.ts:30-31` hardcodes `SCALAR_METADATA_SIZE = 56` /
  `GROUP_METADATA_SIZE = 32` with hand-written byte offsets and allocates them as native **out**
  buffers. Growing either C struct is a native write past a JS-owned buffer, with no compile error.
  The same holds for `DatabaseOptions` (`ffi-helpers.ts:10-16` builds the 8-byte struct by hand).
- **Behaviour**: with no `ui/` present, all three reports must be byte-identical to today. This is
  **free, not engineered** — every existing describe test in every layer opens via `from_schema`,
  never `from_migrations`, so none of them can see the feature.
- **Rendering**: three brittle *negative* assertions break if injected text ever contains
  `Vectors:` / `Sets:` / `Time Series:` or `values {`
  (`tests/test_database_lifecycle.cpp:396-436`, `:438-460`, `:487-500`), and
  `:463-485` pins the literal prefix `"    - <name> "` including its trailing space. UI text must be
  appended after `name (TYPE)` and the flags, never before or around the name.
- **Testing**: a committed `tests/schemas/ui/` would become a live sibling of
  `tests/schemas/migrations` for **every** `from_migrations` call in the C++, C API, Julia, Dart,
  Python and JS suites, plus the Lua migrations test that copies the tree recursively. Build
  `migrations/` + `ui/` in a per-test temp dir instead (`tests/test_migrations.cpp:12-32`, `:194-199`
  idiom). Binding test churn is **zero** — all four binding describe suites assert only "returns a
  string" and say so in a comment.
- **Release**: ships as **0.10.8**, already bumped across all five manifests (#292) and untagged.
  `CHANGELOG.md` heads `## [0.10.7] — unreleased` while `v0.10.7` is already tagged — that heading
  must be reconciled to `0.10.8` (with its compare link) before this milestone writes into it.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Three fields only: `label`, `tooltip`, `enum_labels` | User's explicit scope. The other 7 real `[[attribute]]` keys are parsed and ignored for free | — Pending |
| `enum_labels` is a `std::map<int64_t, std::string>`, not a new type | Labeling the histogram needs lookup by code at render time. A std container is not "a struct to handle the enum" | — Pending |
| Field named `enum_labels`, not `details` | It holds an int→label map; naming it `details` would misdescribe it | ✓ Good |
| No `main.toml` parsing; select collection files by shape | A file carrying both a top-level string `id` and an `attribute` array is a collection file — true for 67/67, false for every `main.toml`/`enum.toml`/theme. Deletes a parser, the filename→table mapping and two edge cases | — Pending |
| `ui/` read only in `from_migrations`, as a sibling of the migrations dir | The only construction path with a migrations path to resolve from, and the one PSR models use for both create and load. True for 13/13 repos, and survives compilation | — Pending |
| `weakly_canonical` before `parent_path` | Raw `parent_path()` misfires on a trailing separator and resolves a bare relative path against the process CWD. Both were compiled and confirmed; trailing-slash paths already exist in this repo's tests | — Pending |
| Render in all three reports | `write_collection_section` serves `describe` + `describe_collection` (one edit site); `summarize_collection` has its own scalar loop (a second). The histogram annotation is the user's explicit ask | — Pending |
| No C API symbol, no binding code | `describe*` already returns `std::string` through `new_c_str`, so the feature reaches all five bindings and Lua for free | — Pending |
| English only: a string is used as-is, a table is read at `en` | A bare value means "same in every language"; a table means "this language specifically". 0 of 817 corpus tables lack `en` | — Pending |
| Read `[[attribute]]` only, never `[[attribute_group]]` | 15 real files reuse one id for both; merging silently overwrites a label with no error | — Pending |
| Drive rendering from the schema, look UI up by name | 15 of 736 attributes name a column that does not exist (2% drift) | — Pending |
| Hidden attributes still render | `describe` describes the schema, not the UI. 360 of 736 carry `hide = true`; filtering them would make `hide` a fourth field | — Pending |
| A missing or malformed `ui/` degrades silently | Failing the open would break every existing caller whose ui dir moved. Warn via `spdlog`; the reports render as today | — Pending |
| Keep `enum.toml` despite the research recommending it be cut | The completeness critic argued to cut it, reasoning from an earlier framing of `details`. User decided otherwise after seeing the cost and the `HasCommitment` inversion | — Pending |
| `validate_ui_config()` deferred; `enum.toml` errors repeated verbatim | Accepted with the `HasCommitment` inversion on the table | ⚠️ Revisit |

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
*Last updated: 2026-09-20 after milestone v0.10.8 initialization*
