# Phase 1: Sidecar Reader and Attribute Meaning - Research

**Researched:** 2026-09-20
**Domain:** Brownfield C++20 core — a TOML sidecar reader feeding an existing string-report renderer
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01: Semicolon clause suffix.** Scalar line unchanged (`"    - " << name << " (" << TYPE << ")"`
  + optional `" PRIMARY KEY"` + optional `" NOT NULL"`), then zero to three `"; keyword body"`
  clauses appended before the trailing `"\n"`, fixed order: `label "<text>"`, then
  `enum {<code>: "<text>", …}`, then `tooltip "<text>"` (only in `describe_collection`). Each clause
  carries its own leading `"; "`.
- **D-02:** Every free-text value wrapped in ASCII double quotes; `\` → `\\`, `"` → `\"`, no other
  escaping. Enum labels are quoted too. Non-ASCII UTF-8 passes through byte-for-byte.
- **D-03:** Whitespace normalization happens in the renderer, not trusted from the loader: map
  `\r`/`\n`/`\t` to space, collapse runs, trim; empty result = absent, never emitted as `""`.
- **D-04: `squash(s)`** = ASCII-lowercase (explicit `c >= 'A' && c <= 'Z' ? c + 32 : c`, never
  `std::tolower(char)` — UB on negative `char`, and 344 corpus strings are non-ASCII), keep only
  `a`-`z`/`0`-`9`. If `squash(label) == squash(name)`, omit the label clause.
- **D-05:** Omit the tooltip clause when `squash(tooltip) == squash(name)` **or**
  `squash(tooltip) == squash(label)` — the latter compared against the raw sidecar label even when
  the label clause itself was suppressed by D-04.
- **D-06:** The enum clause is never suppressed. Entries in ascending code order (free from
  `std::map<int64_t, std::string>` iteration). An entry whose label normalizes empty under D-03 is
  dropped; empty map = whole clause omitted.
- **D-07:** Tooltip is last, so `describe()`'s line is a strict prefix of `describe_collection()`'s
  line for every scalar — must be pinned by a test.
- **D-08:** Tooltip renders only in `describe_collection()`, never `describe()`.
  `write_collection_section` signature gains **two** parameters, not one:
  ```cpp
  void write_collection_section(std::ostream& out, const Schema& schema,
                                const std::string& collection, int64_t count,
                                const UiConfig* ui, bool with_tooltip);
  ```
  `describe()` at `:104` passes `false`; `describe_collection()` at `:114` passes `true`. Escape
  hatch if whole-DB growth bites: drop the label clause from `describe()` too, one more bool — do
  not build now.
- **D-09:** Per-file try/catch nested inside one outer try/catch. Outer catch = never fails
  `from_migrations`, warns via `logger->warn`, yields empty map (following
  `src/database.cpp:80-86`; explicitly not copying `binary_metadata.cpp`'s throwing posture). Inner
  per-file catch = one malformed `ui/*.toml` costs only that collection's metadata. Outer catch
  still covers directory iteration itself and `enum.toml`.
- **D-10:** New internal component `src/ui_config.h` + `src/ui_config.cpp`, modelled on
  `src/csv_read.h`/`.cpp`: no `include/quiver/` counterpart, no `QUIVER_API`, no C API symbol, no
  binding. Header exposes plain `std` types only; toml++ included solely in the `.cpp`.
  `ui_config.cpp` goes in `QUIVER_SOURCES` (mandatory — tomlplusplus is PRIVATE on `quiver`).
- **D-11:** One plain, non-`mutable` member on `Database::Impl` (`src/database_impl.h:59`),
  populated in `from_migrations` (`src/database.cpp:242-257`) after `migrate_up` returns. Not
  `mutable` — the three describe readers reach `Impl` through `impl_->`, and constness doesn't
  propagate through `unique_ptr::operator->`. Must **not** hook onto `load_schema_metadata` —
  `migrate_up` early-returns at `:398-401`/`:406-409` before reaching it (the already-up-to-date
  open-existing-study path).
- **D-12:** Tests in new `tests/test_database_ui_metadata.cpp`, registered in `tests/CMakeLists.txt`,
  driving the parser through the public API only (precedent: `tests/test_binary_metadata.cpp`, 819
  lines, zero toml++ includes). Fixture idiom: `tests/test_migrations.cpp:12-32`, `:194-199`.
  Nothing committed under `tests/schemas/ui/` — temp dirs only.
- **D-15:** Changelog — date (not rename) the stale `0.10.7` section:
  1. `## [0.10.7] — unreleased` → `## [0.10.7] — 2026-09-17` (em dash, matching `## [0.10.6] — 2026-09-11`)
  2. `[0.10.7]: .../compare/v0.10.6...HEAD` → `.../compare/v0.10.6...v0.10.7`
  3. Add `## [0.10.8] — unreleased` above it, and `[0.10.8]: .../compare/v0.10.7...HEAD` at the top
     of the link block
- **D-16:** Path resolution is `fs::weakly_canonical(migrations_path).parent_path() / "ui"` — raw
  `parent_path()` misfires on trailing separator and bare relative paths.
- **D-17:** Collection files self-select by shape — non-recursive `ui/*.toml` scan, keep files with
  both a top-level string `id` and an `attribute` array. No `main.toml` parsing, no filename→table
  mapping.
- **D-18:** Read `[[attribute]]` only, never `[[attribute_group]]` (15 files reuse one id for both).
- **D-19:** Enum join key is the attribute's `enum` value, not its `id` (46 attributes share `bool`).
- **D-20:** Hidden attributes (`hide = true`, 360/736) still render — `describe` describes the
  schema, not the UI.

### Claude's Discretion

- The precise `UiConfig` type/field spelling and the internal map shape. Constraint from
  PROJECT.md: the enum map is a `std::map<int64_t, std::string>` named `enum_labels`, not a new
  type and not `details`.
- Whether the collection→attribute store is one nested map or two; whether `ui_config.cpp` exposes
  a free function or a small struct with an accessor.
- The one accessor that reads a localizable value (string used as-is, table read at `en`) serves all
  three fields including each `enum.toml` entry's own `label`; its exact signature is open.
- Test case names and the internal shape of the temp-dir fixture builder.

### Deferred Ideas (OUT OF SCOPE)

- Phase 2 histogram spelling (`summarize_collection` enum annotation, RENDER-02) — decided now so
  grammars don't diverge: `enum {0: "No", 1: "Yes"}` clause reusing D-01's grammar verbatim, added
  after `; values {...}`. Not built this phase.
- The latent `Vectors:`/`Sets:`/`Time Series:` un-anchored negative-assertion defect in
  `test_database_lifecycle.cpp` — not fixed here; those tests use `from_schema` and cannot see UI
  text, and success criterion 6 requires them to pass **unmodified**.
- `validate_ui_config()` (VALID-01), the `@enumx` cross-check (VALID-02), `unit`/`format`
  (META-01), a structured getter (META-02), collection-level metadata (META-03) — all deferred at
  requirements time.
- Locales other than English, a `hide` filter, `[[attribute_group]]`, `main.toml` parsing,
  `[[card]]`/`themes/` — all in REQUIREMENTS.md's Out of Scope table.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| READ-01 | `from_migrations` reads `ui/` sibling of migrations dir via `weakly_canonical` | Verified `from_migrations` body (`src/database.cpp:242-258`) and the two `migrate_up` early returns (`:398-401`, `:406-409`) that force the read to live here, not in `load_schema_metadata`. `resolve_sandboxed_path` (`src/lua_runner.cpp:1189-1224`) is the in-repo `weakly_canonical` precedent. |
| READ-02 | Collection files self-select by shape (`id` string + `attribute` array); `main.toml`/`enum.toml`/`themes/` excluded | Verified against 5 real corpus files this session: `main.toml` has no `id` key at all (flat `model =`/`collections = [...]`); `dc_line.toml`/`hydro_plant.toml` both have top-level `id = "..."` + `[[attribute]]` array; `enum.toml` has neither. |
| READ-03 | `label`/`tooltip` per `[[attribute]]`, keyed by file's own `id` + attribute's own `id` | Verified corpus shape: `hydro_plant.toml`'s `id = "HydroPlant"` differs from filename; each `[[attribute]]` entry's own `id` (e.g. `"initial_volume_type"`) is the join key into the schema column name. |
| READ-04 | Localizable value: string used as-is, table read at `en`; `\n`/`\r` collapsed to space; UTF-8 untranscoded | Verified corpus: `label.en = "Initial\nVolume\nType"` (bare table.en) vs `label.en = "Label"`. toml++ node type-check API (`is_table()`/`as_table()`/`value<std::string>()`) confirmed via official docs. |
| READ-05 | `enum.toml` parsed and joined by attribute's `enum` value; code = entry's own `id` | Verified `enum.toml` shape directly (HydroThermalDispatch, GNoMo, Foresight, CarbSteeler all agree): top-level key is the **vocabulary name** (`[[bool]]`, `[[initial_volume_type]]`), not a fixed `[[vocab]]` array — each entry has `id` (int) + `label` (bare or table). `hydro_plant.toml`'s `enum = "initial_volume_type"` on the `initial_volume_type` attribute is the join key, confirmed matching `enum.toml`'s `[[initial_volume_type]]` entries `{0: "Per Unit", 2: "Volume"}` (gapped, as CONTEXT states). |
| RENDER-01 | `describe`/`describe_collection` show label/tooltip/enum after name/type/flags | Verified exact injection point: `write_collection_section` (`src/database_describe.cpp:56-91`), scalar loop `:62-72`, insert after flags (`:65-70`) and before `out << "\n";` at `:71`. Callers verified at `:104` (`describe()`) and `:114` (`describe_collection()`). |
| RENDER-03 | Undescribed attribute renders exactly as today (no ui file / no entry / dangling column name) | Verified: schema-driven iteration (`table_def->column_order` at `:62`) with UI looked up by name means a missing lookup is a no-op by construction — no special-case code needed if the accessor returns "absent" for a miss. |
| SAFE-01 | No `ui/` → byte-identical reports | Verified exhaustively: grepped every describe/summarize call site across all 6 suites' test files — every one opens via `from_schema`, never `from_migrations`. Confirmed `from_schema` (`src/database.cpp:283-301`) never calls `migrate_up`/`from_migrations`, so it cannot reach the new read path. |
| SAFE-02 | Missing/empty/unparseable/partial `ui/` never fails `from_migrations`; warns and degrades | Verified `binary_metadata.cpp`'s anti-pattern (`tbl["initial_datetime"].value<std::string>().value()` at line 270 — bare `.value()` throws `std::bad_optional_access` on a missing key) as the posture to avoid. Verified the warn-and-degrade precedent (`src/database.cpp:69-86`, spdlog file-sink fallback: try/catch → `logger->warn` → continue). |
</phase_requirements>

## Summary

Nothing is acquired and nothing new is linked. `tomlplusplus` v3.4.0 is already `FetchContent`-ed
(`cmake/Dependencies.cmake:10-14`) and linked `PRIVATE` on `quiver` (`src/CMakeLists.txt:81`), and
`src/binary/binary_metadata.cpp` is a shipped, working precedent for parsing a `.toml` sidecar with
this exact library version — its API shapes (`toml::parse(content)`, `tbl["key"].as_array()`,
`elem.value<std::string>()`) are directly reusable. The whole feature is one new internal component
(`src/ui_config.h`/`.cpp`, modelled on the existing `src/csv_read.h`/`.cpp` precedent for a
`src/`-local component with no public header), one new member on `Database::Impl`, one call site in
`from_migrations`, and a two-parameter signature change to the single shared render function
`write_collection_section` plus its two callers.

Every code-location claim in the phase's CONTEXT.md was independently re-verified this session by
reading the actual files — the line numbers are exact: `write_collection_section` at
`src/database_describe.cpp:56`, its scalar loop at `:62-72` (flags close at `:69`, the appended
clauses land between `:70` and the `out << "\n";` at `:71`), its callers at `:104`/`:114`,
`from_migrations` at `:242-258`, the two `migrate_up` early returns at `:398-401`/`:406-409`,
`Database::Impl` opening at `database_impl.h:59`, and `Schema`'s `std::map<std::string,
TableDefinition> tables_` at `schema.h:104`. The warn-and-degrade precedent
(`src/database.cpp:69-86`) and the throwing anti-pattern to avoid
(`binary_metadata.cpp:270-282`, bare `.value()`) were both read directly. The real `enum.toml`
shape was also independently confirmed against 4 separate corpus repos (HydroThermalDispatch,
GNoMo, Foresight, CarbSteeler): it is **not** a fixed `[[vocab]]` array — each vocabulary is its
own dynamically-named array-of-tables (`[[bool]]`, `[[initial_volume_type]]`, …), which the parser
must discover by iterating `enum.toml`'s top-level table rather than looking up one known key.

**Primary recommendation:** Follow the CONTEXT.md decisions verbatim (they are already
code-grounded and largely pre-verified); the one implementation detail CONTEXT.md's prose slightly
underspecifies — and this research corrects — is that `enum.toml` requires iterating the whole
top-level `toml::table` (each key is a vocabulary name) rather than reading one fixed array key.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Resolve `ui/` sibling path | C++ core (`Database::from_migrations`) | — | Only `from_migrations` has a migrations path to resolve a sibling from; `open()`/`from_schema`/`validate_migrations` don't (Out of Scope table) |
| Parse collection `.toml` + `enum.toml` | C++ core, new internal `src/ui_config.{h,cpp}` | — | toml++ is PRIVATE on `quiver`; no other tier can link it |
| Hold parsed UI metadata for a session | C++ core (`Database::Impl` member) | — | Lifecycle-scoped to one open `Database`; no persistence, no cache invalidation needed |
| Render label/tooltip/enum into report text | C++ core (`database_describe.cpp`) | — | Single shared render seam already exists (`write_collection_section`); reports are opaque strings by design |
| Consume the richer report | All 5 bindings + Lua (unchanged) | — | `describe*` already returns `std::string` through `new_c_str`/JSON passthrough — zero binding code needed |

No browser/frontend/CDN tier exists in this library; the whole feature is core-only by design
(explicit non-goal in REQUIREMENTS.md: "A structured getter through the C API and bindings").

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| tomlplusplus | v3.4.0 [VERIFIED: cmake/Dependencies.cmake:10-14] | TOML parsing | Already vendored and exercised by `src/binary/binary_metadata.cpp` [VERIFIED: src/binary/binary_metadata.cpp:232 `toml::table tbl = toml::parse(content);`]; zero new dependency |

No supporting libraries needed — `std::filesystem` (already used throughout `src/database.cpp`,
`src/lua_runner.cpp`) covers path resolution and directory iteration.

### Alternatives Considered

None — this is a brownfield addition against an already-chosen, already-linked parser. Swapping
toml++ was never in scope (PROJECT.md: "Nothing is acquired").

**Installation:** none required — `tomlplusplus::tomlplusplus` target already exists.

**Version verification:** `cmake/Dependencies.cmake:11-14` pins `GIT_TAG v3.4.0` against
`https://github.com/marzer/tomlplusplus.git` [VERIFIED: cmake/Dependencies.cmake:9-15]. No action
needed.

## Package Legitimacy Audit

**Not applicable.** This phase installs no new external packages. `tomlplusplus` v3.4.0 is an
existing, already-vetted dependency (in the repo before this milestone); no `npm`/`pip`/`cargo`
install occurs. Nothing to audit.

## Architecture Patterns

### System Architecture Diagram

```
                     Database::from_migrations(db_path, migrations_path, options)
                                          │
                                          ▼
                          migrate_up(migrations_path)   ──► (unchanged: applies pending
                                          │                   migrations, or early-returns
                                          │                   if none/up-to-date)
                                          ▼
                     [NEW] resolve ui_dir = weakly_canonical(migrations_path)
                                          .parent_path() / "ui"
                                          │
                             ┌────────────┴─────────────┐
                             │ ui_dir missing/unreadable │──► logger->warn, impl_->ui = {} (empty)
                             └────────────┬─────────────┘
                                          │ ui_dir exists
                                          ▼
                     non-recursive scan of ui_dir/*.toml
                     keep files where top-level `id` is string AND `attribute` is array
                             │                              │
                     [[attribute]] entries            enum.toml (if present)
                     → per-collection map:             → top-level table iterated;
                       attribute id → {label, tooltip,    each key = vocab name,
                                        enum vocab name}   value = array of {id, label}
                             │                              │
                             └──────────────┬───────────────┘
                                            ▼
                          join: attribute's `enum` value → vocab name → code/label map
                                            │
                                            ▼
                     impl_->ui_config (populated, or empty on any failure) — one
                     non-mutable member on Database::Impl, set once, read-only after
                                            │
                    ┌───────────────────────┼────────────────────────┐
                    ▼                       ▼                        ▼
             describe()            describe_collection()      (Phase 2, not this
       write_collection_section(     write_collection_section(  phase) summarize_
         ..., ui=&impl_->ui_config,    ..., ui=&impl_->ui_config,  collection()'s
         with_tooltip=false)           with_tooltip=true)          own scalar loop
                    │                       │
                    └───────────┬───────────┘
                                ▼
                  scalar loop (existing "name (TYPE) [PK] [NOT NULL]")
                  + [NEW] look up ui->label(collection, attr_name)  → "; label \"...\""
                  + [NEW] look up ui->enum_labels(collection, attr) → "; enum {...}"
                  + [NEW, describe_collection only] tooltip          → "; tooltip \"...\""
                  each suppressed independently per D-04/D-05/D-06
```

Entry point: `Database::from_migrations` (public API, called once per study open/create).
Processing stages: migration apply → sidecar resolve → sidecar parse (degrade-on-failure) → store
on `Impl` → later, on-demand render into the three text reports. Decision points: shape-based file
selection (READ-02), locale-value string-vs-table branch (READ-04), and the three independent
suppression predicates (D-04/D-05/D-06). External dependency boundary: toml++ is entered and exited
entirely inside `ui_config.cpp`; nothing above it (`database.cpp`, `database_impl.h`,
`database_describe.cpp`) ever sees a `toml::` symbol.

### Recommended Project Structure

```
src/
├── ui_config.h              # NEW — public-to-src types: UiConfig (or similar), load_ui_config()
├── ui_config.cpp             # NEW — toml++ parsing lives here only; QUIVER_SOURCES entry
├── database.cpp              # from_migrations gains the load_ui_config() call (~line 256-257)
├── database_impl.h           # Impl gains one non-mutable ui_config member (~after line 71)
└── database_describe.cpp     # write_collection_section signature + scalar-loop append logic

tests/
└── test_database_ui_metadata.cpp   # NEW — registered in tests/CMakeLists.txt, public-API-only
```

### Pattern 1: Internal `src/`-local component with no public header (D-10)

**What:** A `.h`/`.cpp` pair in `src/` with no `include/quiver/` counterpart, no `QUIVER_API`
export macro, no C API symbol, no FFI binding — because there is no FFI consumer and the
dependency it wraps (toml++) must stay off the PRIVATE/PUBLIC boundary.

**When to use:** Exactly this situation — a component whose only consumer is other `src/` code and
whose dependency (toml++, PRIVATE-linked) cannot leak into public headers.

**Example (the precedent to model on):**
```cpp
// Source: src/csv_read.h:1-9 [VERIFIED: src/csv_read.h:1-9]
#ifndef QUIVER_SRC_CSV_READ_H
#define QUIVER_SRC_CSV_READ_H

// Internal CSV reader wrapping vincentlaucsb/csv-parser for the Lua-only db:read_csv /
// db:read_csv_stream bindings (src/lua_runner.cpp). No public include/quiver/ counterpart, no
// QUIVER_API, no C API, no FFI binding: Julia/Dart/Python/JS already have native CSV libraries,
// and Lua needs this specifically because `io` is deliberately absent from its sandbox (root
// CLAUDE.md design decisions).
```
`src/ui_config.h`'s header comment should follow the same shape: state which layers deliberately
have no counterpart and why (no FFI consumer; toml++ is PRIVATE).

### Pattern 2: toml++ parse-and-read (from the existing precedent)

**What:** Parse file content into a `toml::table`, then read typed values with the optional-typed
accessor, never the throwing bare form.
**When to use:** Every value read out of a `ui/*.toml` or `enum.toml` file.
**Example:**
```cpp
// Source: src/binary/binary_metadata.cpp:232-249 [VERIFIED: src/binary/binary_metadata.cpp:232-249]
toml::table tbl = toml::parse(content);

std::vector<std::string> dimensions;
if (auto* arr = tbl["dimensions"].as_array()) {
    for (auto& elem : *arr) {
        if (auto val = elem.value<std::string>()) {
            dimensions.push_back(*val);
        }
    }
}
```
Note the **anti-pattern in the same file**, which D-09 says not to copy:
```cpp
// Source: src/binary/binary_metadata.cpp:270 [VERIFIED: src/binary/binary_metadata.cpp:270]
std::string initial_datetime_str = tbl["initial_datetime"].value<std::string>().value();
// bare `.value()` on the optional throws std::bad_optional_access when the key is absent —
// exactly the crash-on-missing-key posture this phase must NOT reproduce.
```

### Pattern 3: Locale-value accessor (string-or-table-at-`en`) — new for this phase

**What:** One function serving `label`, `tooltip`, and each `enum.toml` entry's own `label`: if the
TOML value at a key is a plain string, use it; if it is a table, read its `en` sub-key.
**When to use:** Every localizable field read (READ-04). No existing in-repo precedent for this
exact string-or-table branch — `binary_metadata.cpp` only ever reads flat scalars/arrays — so this
is new code, not adapted code. toml++'s type-check API for it (`is_table()`, `as_table()`,
`(*tbl)["en"]`) is documented at the toml++ public docs (`toml::node` polymorphic interface, same
family as `as_array()` already used in-repo).
```cpp
// Illustrative — not yet in the repo. Shape confirmed against real corpus files this session:
//   label = "ARIMA"                 (bare string — 35/713 at attribute level)
//   label.en = "Initial\nVolume"    (table with `en` — 678/713 at attribute level)
// [CITED: toml++ official docs — toml::node::is_table()/as_table(), verified toml::node API
//  family alongside the in-repo as_array()/value<T>() usage above]
std::optional<std::string> read_localized(const toml::node* n) {
    if (!n) return std::nullopt;
    if (auto s = n->value<std::string>()) return *s;         // bare string form
    if (auto* t = n->as_table()) {
        if (auto en = (*t)["en"].value<std::string>()) return *en;   // table.en form
    }
    return std::nullopt;   // 0 of 817 corpus locale tables lack `en`, but degrade rather than throw
}
```

### Pattern 4: `enum.toml`'s real shape — top-level keys ARE vocabulary names

**What:** `enum.toml` has no wrapper key. Each top-level key in the file is itself a vocabulary
name, and its value is an array of tables (`[[<vocab_name>]]`), each with an `id` (int64) and a
`label` (locale value per Pattern 3).
**Verified directly this session** by reading 4 separate corpus files:
```toml
# Source: C:/Development/HydroThermalDispatch/HydroThermalDispatch.jl/database/ui/enum.toml:1-20
# [VERIFIED: read this file directly, 2026-09-20]
[[yes_no]]
id = 0
label.en = "No"

[[bool]]
id = 0
label.en = "Disable"

[[initial_volume_type]]
id = 0
label.en = "Per Unit"

[[initial_volume_type]]
id = 2
label.en = "Volume"
```
```toml
# Source: C:/Development/Foresight/Foresight.jl/database/ui/enum.toml:1-15
# [VERIFIED: read this file directly, 2026-09-20]
[[forecast_mode]]
id = 0
label.en = "User Defined Forecast"
label.es = "Pronóstico Definido por el Usuario"
label.pt = "Previsão Definida pelo Usuário"

[[forecast_mode]]
id = 1
label.en = "Model"
```
**Implication for the planner:** the `enum.toml` parser cannot look up one known array key (there
is no `[[vocab]]`/`[[enum]]` wrapper). It must iterate the parsed top-level `toml::table`'s
key/value pairs, keep every value that `is_array()` (or `as_array()` non-null) and whose elements
are tables carrying `id` + `label`, and use the **key itself** as the vocabulary name — exactly the
name an attribute's `enum = "..."` field (verified: `hydro_plant.toml`'s
`enum = "initial_volume_type"` [VERIFIED: `C:/Development/HydroThermalDispatch/HydroThermalDispatch.jl/database/ui/hydro_plant.toml:22-24`, quoted: `id = "initial_volume_type"` / `type = "enum"` / `enum = "initial_volume_type"`]) is joining against. `toml::table` supports range-based iteration yielding `(toml::key, toml::node&)` pairs [CITED: toml++ official docs, `toml::table` iterator support — same public API family verified in-repo via `.as_array()`/`tbl["key"]` indexing].

### Pattern 5: Schema-driven render loop, UI as a nullable overlay (D-11, D-17 corollary)

**What:** The existing scalar loop already iterates `table_def->column_order` (the schema's own
attribute order) and looks up each column's `ColumnDefinition` by name. The UI lookup slots into
the exact same loop, keyed by the same `name`, and simply returns "nothing found" for the three
undescribed cases (RENDER-03): no ui file for the collection, no entry for this attribute, or (the
reverse direction, not exercised in this loop but true of the loader) an entry naming a column the
schema doesn't have — the loader is what silently drops that one, since it never gets asked about
it here.
```cpp
// Source: src/database_describe.cpp:62-72 (existing code, exact quote)
// [VERIFIED: src/database_describe.cpp:62-72]
for (const auto& name : table_def->column_order) {
    const auto& col = table_def->columns.at(name);
    out << "    - " << name << " (" << data_type_to_string(col.type) << ")";
    if (col.primary_key) {
        out << " PRIMARY KEY";
    }
    if (col.not_null && !col.primary_key) {
        out << " NOT NULL";
    }
    out << "\n";
}
```
The three new clauses (D-01) are inserted between the `NOT NULL` block (ends `:70`) and the
`out << "\n";` (`:71`) — i.e. replace line 71's bare `out << "\n";` with the clause-emission logic
followed by the same `"\n"`.

### Anti-Patterns to Avoid

- **Copying `binary_metadata.cpp`'s throwing posture.** Bare `.value()` on a toml++ optional throws
  `std::bad_optional_access` with no message naming the file or key
  [VERIFIED: src/binary/binary_metadata.cpp:270-282, three bare `.value()` calls with no
  surrounding try/catch in `from_toml_content`]. This phase's whole UI load is one try/catch →
  warn → empty map (D-09); a bare `.value()` anywhere in `ui_config.cpp` reintroduces exactly the
  failure mode D-09 exists to prevent.
- **Hooking the UI load onto `load_schema_metadata()`.** Verified both early returns that make this
  wrong: `migrate_up`'s `if (migrations.empty())` returns at `src/database.cpp:398-401` (no
  migrations found) and `if (pending.empty())` returns at `:406-409` (already up to date) — both
  *before* `impl_->load_schema_metadata()` is called at line 437. Every "open an existing,
  already-migrated study" call — the common case — takes the second early return and never reaches
  schema loading at all in that call; `require_schema()` triggers it lazily on first metadata
  access instead, decoupled from `from_migrations` entirely. Hooking there would silently no-op on
  every re-open.
- **Deriving the collection key from the filename.** Verified: `dc_line.toml`'s own top-level `id`
  is `"DCLine"`, not `DcLine`/`dc_line`/`DC_Line` — any filename→PascalCase transform would need a
  bespoke mapping table this phase deliberately avoids (READ-02/D-17).
- **Assuming `enum.toml` has a `[[vocab]]`/`[[enum]]` wrapper array.** It does not (Pattern 4
  above) — this is the one place this research diverges in detail from CONTEXT.md's prose
  ("`enum.toml` shows the `[[vocab]]` + `id` + `label.en` shape"), which reads as shorthand for "a
  vocabulary's shape" rather than a literal TOML key named `vocab`. Verified against 4 corpus repos
  directly.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| TOML parsing | A hand-rolled line parser | `toml::parse(content)` (toml++, already vendored) | Handles quoting, nesting, arrays-of-tables, dotted keys (`label.en`) correctly; hand-rolling would re-litigate escaping rules TOML already specifies |
| Lowercasing for `squash()` | `std::tolower(char)` | Explicit ASCII range check `c >= 'A' && c <= 'Z' ? c + 32 : c` | `std::tolower(char)` is UB for a negative `char` (any byte ≥ 0x80 on a signed-char platform), and 344 corpus strings are non-ASCII UTF-8 — this is a documented, deliberate D-04 requirement, not a style preference |
| Path containment/resolution | Custom string manipulation on `migrations_path` | `fs::weakly_canonical(migrations_path).parent_path() / "ui"` | Proven wrong alternatives exist and are documented: raw `parent_path()` on a trailing-slash path yields `<migrations>/ui` (never exists); on a relative path it resolves against the process CWD |

**Key insight:** every "don't hand-roll" item here has already been hand-rolled once in this
codebase and found wrong (the two path-resolution failure modes were compiled and proven;
`std::tolower(char)`'s UB is a textbook C++ pitfall this project's own non-ASCII corpus would
actually trigger). Treat the CONTEXT.md decisions as the fix already applied by research, not as
options to re-litigate.

## Common Pitfalls

### Pitfall 1: Injected text collides with existing negative string-search assertions

**What goes wrong:** `tests/test_database_lifecycle.cpp` has assertions that search the whole
`describe()` output for the literal substrings `Vectors:`, `Sets:`, `Time Series:`, and (in
`summarize_collection`) `values {`, asserting they are **absent** in cases with no groups/no
integer columns.
**Why it happens:** Those assertions use `output.find("Vectors:")`, not an anchored
`"\n  Vectors:\n"` match [VERIFIED: `tests/test_database_lifecycle.cpp` — quoted below, the
`DescribeNoCategoryHeaderWhenEmpty` test uses exactly `output.find("Vectors:")`]. If a label or
tooltip ever contained the literal text `Vectors:` it would false-positive this assertion.
**How to avoid:** D-01/D-02 make this structurally impossible for the *header* words themselves
(they're never emitted by the UI renderer — only `label`/`enum`/`tooltip` keywords are), but a
**user-authored label or tooltip string** containing `"Vectors:"` as literal text is not excluded
by any decision here. Given 736 real corpus strings inspected during requirements gathering
contain no such collision, this is accepted as a known, unmitigated latent risk — matches
CONTEXT.md's own "Deferred" note that this defect is pre-existing and out of scope to fix.
**Warning signs:** A new test using `from_migrations` + a UI fixture whose label/tooltip text
happens to contain `Vectors:`, `Sets:`, `Time Series:`, or `values {` would false-negative silently
if such an assertion existed against it — don't construct fixture text this way.

### Pitfall 2: `describe()` line accidentally diverges from `describe_collection()`'s line

**What goes wrong:** If the tooltip clause were emitted anywhere but strictly last, or if the two
call sites diverged in which suppression rules they apply, `describe()`'s per-scalar line would
stop being a literal string-prefix of `describe_collection()`'s line for that same scalar.
**Why it happens:** Two call sites (`:104` and `:114`) into one shared function
(`write_collection_section`) with only one boolean (`with_tooltip`) differing (D-08) makes this
easy to get right, but a plan that instead branches the label/enum clauses on `with_tooltip` too
would break it.
**How to avoid:** D-07's explicit prescription — only the trailing tooltip clause is conditional on
`with_tooltip`; label and enum clauses are unconditional (present in both reports whenever their
own suppression rule doesn't apply).
**Warning signs:** A test asserting, for every scalar, that the `describe()` line is a strict
prefix of the `describe_collection()` line (D-07's own suggested test) — write it first, since it
is "the cheapest possible anti-drift guarantee."

### Pitfall 3: Case-sensitive collection lookup silently drops the whole file's metadata

**What goes wrong:** `Schema` keys its collection map case-sensitively
(`std::map<std::string, TableDefinition> tables_` [VERIFIED: include/quiver/schema.h:104, quoted:
`std::map<std::string, TableDefinition> tables_;`]) while SQLite itself matches table names
case-insensitively. A ui file whose top-level `id` differs in case from the `CREATE TABLE`
spelling (e.g. schema has `HydroPlant`, ui file says `hydroplant`) will silently match nothing.
**Why it happens:** Two different case-sensitivity policies collide at exactly the point where a ui
file's `id` string is looked up against the schema's collection names.
**How to avoid:** Accepted per CONTEXT.md — "no diagnostic" is the deliberate decision, since the
UI is a partial overlay by design (15/736 attributes already name nonexistent columns with no
diagnostic). Do not add a warning for this specific case; it would single out one drift class
among several equally-silent ones.
**Warning signs:** A ui fixture built with a differently-cased `id` in a test would produce a
report with no UI text and no warning — that is the expected, correct behavior, not a bug to chase.

### Pitfall 4: `require_schema()` triggers schema load, but the UI store must not depend on it

**What goes wrong:** A tempting shortcut is to make the UI lookup call `impl_->require_schema()`
lazily, mirroring how the describe readers do. This would be wrong for a different reason than
Pitfall lookup timing: the UI store's *population* (parsing `ui/`) must happen eagerly in
`from_migrations`, not lazily — but its *read* during rendering can safely assume it's already
populated (or empty), since `from_migrations` always runs before any describe call on that
`Database` instance.
**Why it happens:** Confusing "when is `ui_config` populated" (once, in `from_migrations`) with
"when is `schema` populated" (lazily, on first `require_schema()`) — they are two different fields
with two different lifecycles on the same `Impl`.
**How to avoid:** Populate `impl_->ui_config` unconditionally at the end of `from_migrations`
(success or empty-on-failure per D-09), never inside `require_schema()`/`load_schema_metadata()`.
**Warning signs:** A test that opens via `from_migrations` then calls `describe()` without any
prior schema-touching call still needs to see UI text — if it doesn't, the UI store was
accidentally made to depend on `require_schema()` having run.

## Code Examples

### Reading a top-level string key with graceful-missing semantics (existing precedent, adapt the optional style)

```cpp
// Source: src/binary/binary_metadata.cpp:238-247 [VERIFIED: lines 238-247]
std::vector<std::string> dimensions;
if (auto* arr = tbl["dimensions"].as_array()) {
    for (auto& elem : *arr) {
        if (auto val = elem.value<std::string>()) {
            dimensions.push_back(*val);
        }
    }
}
```
Adapt this shape (optional-checked, never bare `.value()`) for every field this phase reads:
top-level `id`, `attribute` array presence, each attribute's `id`/`label`/`tooltip`/`enum`, and
each `enum.toml` vocab entry's `id`/`label`.

### The warn-and-continue precedent to model D-09's outer catch on

```cpp
// Source: src/database.cpp:69-86 [VERIFIED: lines 69-86]
std::shared_ptr<spdlog::sinks::basic_file_sink_mt> file_sink = nullptr;
try {
    const auto log_file_path = (db_dir / "quiver_database.log").string();
    file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, true);
    file_sink->set_level(spdlog::level::debug);

    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::debug);

    return logger;
} catch (const spdlog::spdlog_ex& ex) {
    // If file sink creation fails, continue with console-only logging
    const auto logger = std::make_shared<spdlog::logger>(logger_name, console_sink);
    logger->set_level(spdlog::level::debug);
    logger->warn("Failed to create file sink: {}. Logging to console only.", ex.what());
    return logger;
}
```
The UI loader's outer catch follows the identical shape: try the whole `ui/` load, catch
`(const std::exception&)` (broader than `spdlog_ex` here since `toml::parse_error` and
`std::filesystem_error` both derive from `std::exception`), `logger->warn(...)`, return an empty
`UiConfig`.

### The scalar render loop's exact append point

```cpp
// Source: src/database_describe.cpp:62-72 [VERIFIED: lines 62-72, exact text]
for (const auto& name : table_def->column_order) {
    const auto& col = table_def->columns.at(name);
    out << "    - " << name << " (" << data_type_to_string(col.type) << ")";
    if (col.primary_key) {
        out << " PRIMARY KEY";
    }
    if (col.not_null && !col.primary_key) {
        out << " NOT NULL";
    }
    out << "\n";
}
```
New clauses replace the standalone `out << "\n";` on the last line with: build up to three
`"; keyword body"` strings per D-01/D-04/D-05/D-06, concatenate them (each self-contained with its
own `"; "`), append, then emit `"\n"`.

### The two call sites, verified verbatim

```cpp
// Source: src/database_describe.cpp:95-116 [VERIFIED: lines 95-116]
std::string Database::describe() const {
    impl_->require_schema();

    std::ostringstream out;
    out << "Database: " << impl_->path << "\n";
    out << "Version: " << current_version() << "\n";

    for (const auto& collection : impl_->schema->collection_names()) {
        out << "\n";
        write_collection_section(out, *impl_->schema, collection, number_of_elements(collection));
    }

    return out.str();
}

std::string Database::describe_collection(const std::string& collection) const {
    impl_->require_collection(collection, "describe_collection");

    std::ostringstream out;
    write_collection_section(out, *impl_->schema, collection, number_of_elements(collection));
    return out.str();
}
```
Both calls gain the two new trailing arguments per D-08: `describe()`'s call passes
`&impl_->ui_config, false`; `describe_collection()`'s passes `&impl_->ui_config, true`.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|---------------|--------|
| INTEGER enum columns render bare codes in `describe`/`describe_collection`/`summarize_collection` | Codes render alongside their `enum.toml` label | This phase (v0.10.8, Phase 1 of 2) | An LLM agent can read `0 = User Defined Forecast, 1 = Model` instead of needing external knowledge of the vocabulary |

Not a "deprecated API" style domain — this is the first time this metadata is read at all, so
there is no prior competing approach to deprecate beyond the status quo (bare codes).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | toml++ `toml::table` supports range-based iteration yielding `(key, node&)` pairs, and `node::is_table()`/`as_table()` exist as documented, for reading `enum.toml`'s dynamically-keyed vocabularies | Architecture Patterns → Pattern 4 | If the exact iteration API differs from the cited docs shape, `ui_config.cpp`'s `enum.toml` parser needs a different loop construct — low risk, since `.as_array()`/`tbl["key"]` indexing (already used in-repo) and table iteration are both core, stable toml++ features present since early versions, and the official docs (fetched this session) confirm `is_table()`/`as_table()` exist on `toml::node` |
| A2 | `impl_->ui_config` should be a plain (non-`mutable`) member per D-11, and no code path reads it before `from_migrations` populates it | Architecture Patterns → Pattern 5, Common Pitfalls → Pitfall 4 | If some other construction path (a future one, not in this phase's scope) ever calls `describe()` without going through `from_migrations` first, the member is simply default-constructed (empty) — same behavior as "no ui/ dir found," so this degrades safely even if the assumption is wrong |

**On D-04/squash's ASCII-only behavior for non-ASCII labels:** this is not tagged as an assumption
— it is one of CONTEXT.md's own locked decisions (D-04), already justified and cross-referenced
against measured corpus data (344 non-ASCII strings), not a claim this research introduced.

## Open Questions

None blocking. The two items in the Assumptions Log above are low-risk and have safe fallback
behavior (an incorrect toml++ API call surfaces as a compile error, not a runtime defect; an
unpopulated `ui_config` degrades identically to "no ui/ present").

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | Build | ✓ | 4.3.1 (msvc1) [VERIFIED: `cmake --version`] | — |
| Ninja | Build (per CLAUDE.md's dev preset) | ✓ | 1.13.2 [VERIFIED: `ninja --version`] | — |
| Git | Version control, changelog tag lookup | ✓ | 2.53.0.windows.2 [VERIFIED: `git --version`] | — |
| tomlplusplus v3.4.0 | TOML parsing | ✓ (FetchContent, already integrated) | v3.4.0 [VERIFIED: cmake/Dependencies.cmake:12] | — |

No missing dependencies. This phase needs no environment changes.

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | GoogleTest v1.17.0 [VERIFIED: root CLAUDE.md Dependencies section; `include(GoogleTest)` at `tests/CMakeLists.txt:1`] |
| Config file | `tests/CMakeLists.txt` (target `quiver_tests`) |
| Quick run command | `build/bin/quiver_tests.exe --gtest_filter=*Ui*` (after building the new test file) |
| Full suite command | `build/bin/quiver_tests.exe` |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| READ-01 | Trailing-separator and relative `migrations_path` both resolve to the correct sibling `ui/` | unit | `build/bin/quiver_tests.exe --gtest_filter=*UiConfig*PathResolution*` | ❌ Wave 0 |
| READ-02 | `main.toml`/`enum.toml`/`themes/*.toml` never treated as a collection file | unit | `build/bin/quiver_tests.exe --gtest_filter=*UiConfig*ShapeSelection*` | ❌ Wave 0 |
| READ-03 | Label/tooltip read and keyed correctly by file `id` + attribute `id` | unit | `build/bin/quiver_tests.exe --gtest_filter=*UiConfig*LabelTooltip*` | ❌ Wave 0 |
| READ-04 | String vs table.en, `\n`/`\r` collapse, UTF-8 passthrough | unit | `build/bin/quiver_tests.exe --gtest_filter=*UiConfig*Localized*` | ❌ Wave 0 |
| READ-05 | Gapped (`[0,2]`) and 1-based (`[1..7]`) enum codes render as their real codes, joined by `enum` value not attribute `id` | unit | `build/bin/quiver_tests.exe --gtest_filter=*UiConfig*Enum*` | ❌ Wave 0 |
| RENDER-01 | `describe`/`describe_collection` show label/tooltip/enum in the D-01 clause format | integration (via public `Database` API) | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*Render*` | ❌ Wave 0 |
| RENDER-03 | 3 undescribed cases render unchanged (no ui file / no entry / dangling column) | integration | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*Undescribed*` | ❌ Wave 0 |
| SAFE-01 | No `ui/` → byte-identical to a `from_schema` baseline | integration/regression | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*NoUiDir*` | ❌ Wave 0 |
| SAFE-02 | Empty/zero-byte/unparseable/partial `ui/` degrades with a warning, never throws | integration | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*Malformed*` | ❌ Wave 0 |
| D-07 (anti-drift) | `describe()` line is a strict prefix of `describe_collection()` line, for every scalar | integration | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*PrefixInvariant*` | ❌ Wave 0 |
| Success criterion 6 | Existing `test_database_lifecycle.cpp` describe assertions (`:396-436`, `:438-460`, `:463-485`, `:487-500`) pass **unmodified** | regression | `build/bin/quiver_tests.exe --gtest_filter=*DescribeVectorsHeaderPrintedOnce*:*DescribeSetsHeaderPrintedOnce*:*DescribeTimeSeriesWithDimensionColumn*:*DescribeNoCategoryHeaderWhenEmpty*` | ✓ (already exists) |

### Sampling Rate

- **Per task commit:** the filtered `--gtest_filter=*Ui*` / `*DatabaseUiMetadata*` subset (seconds)
- **Per wave merge:** full `build/bin/quiver_tests.exe` (all suites, catches the Pitfall-1/2 class
  of cross-report regressions) plus `build/bin/quiver_c_tests.exe` (C API describe wrappers are
  pure passthrough per `src/c/database.cpp:129-167`, but a signature-shape regression there would
  still be caught)
- **Phase gate:** full `build/bin/quiver_tests.exe` green before `/gsd-verify-work`; no binding
  suite needs to run (all four binding describe suites assert only "returns a string" per
  PROJECT.md, and this phase makes zero binding code changes — REQUIREMENTS.md's own "Out of
  Scope" table confirms binding-level tests "prove nothing" here)

### Wave 0 Gaps

- [ ] `tests/test_database_ui_metadata.cpp` — new file, registered in `tests/CMakeLists.txt`,
      covering every row in the map above
- [ ] A temp-dir fixture builder extending the `tests/test_migrations.cpp:12-32`/`:194-199` idiom
      to also write a sibling `ui/` directory with caller-supplied `.toml` file contents
- [ ] Framework install: none — GoogleTest is already a build dependency (`cmake/Dependencies.cmake`)

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-------------------|
| V2 Authentication | No | No auth surface touched by this phase |
| V3 Session Management | No | N/A |
| V4 Access Control | No | N/A |
| V5 Input Validation | Yes | Every `.toml` value is read through the optional-checked toml++ accessor (never a bare `.value()`), and the whole load is wrapped in try/catch → warn → empty map (D-09) — malformed/adversarial TOML content degrades rather than propagating an exception or an unhandled type mismatch |
| V6 Cryptography | No | N/A — no secrets, no crypto in this phase |
| V12 File and Resources | Yes | Path resolution uses `fs::weakly_canonical(migrations_path).parent_path() / "ui"` (D-16) — a fixed, derived sibling of an already-trusted `migrations_path` API parameter, not user-supplied free text; no path traversal surface is introduced because the resolved `ui_dir` is never combined with attacker-controlled path segments (filenames come from a `directory_iterator` over that fixed directory, not from parsed TOML content) |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|----------------------|
| Malformed/truncated TOML causing an unhandled exception to escape `from_migrations` | Denial of Service | D-09's outer try/catch → `logger->warn` → empty `UiConfig`; verified this is exactly the shape of the existing `src/database.cpp:69-86` precedent, and explicitly is **not** the shape of `binary_metadata.cpp`'s throwing `.value()` calls (the anti-pattern this phase must avoid) |
| A crafted `ui/*.toml` with an extremely large or deeply nested `attribute` array causing excessive memory/CPU during parse | Denial of Service | Not separately mitigated in this phase — `ui/` is a sibling of a `migrations_path` the caller already controls and trusts (it is not attacker-supplied network input); out of scope per the project's threat model (a PSR study database, not a network-facing service). No corpus file exceeds ~100 attributes per collection, so this is a theoretical concern only, consistent with the project's WIP/no-hardening-for-untrusted-network-input posture |
| Relative-path or trailing-separator `migrations_path` causing UI metadata to load from an unintended directory | Tampering (of which config source is trusted) | D-16: `fs::weakly_canonical` before `parent_path()` — already compiled and proven to fix both the trailing-separator and bare-relative-path cases (Two Resolution Traps, PROJECT.md) |

This phase's security posture is dominated by V5 (graceful degradation of malformed local input)
rather than classic web-app ASVS categories, because the entire input surface (`ui/*.toml`) is a
local file sibling of an already-trusted caller-supplied path, not remote or user-facing input.

## Sources

### Primary (HIGH confidence — read directly this session)

- `C:/Development/Quiver/quiver4/src/database.cpp` — `from_migrations` (:242-258), `migrate_up`
  early returns (:398-401, :406-409), warn-and-continue precedent (:69-86)
- `C:/Development/Quiver/quiver4/src/database_describe.cpp` — full file (185 lines): `write_collection_section`
  (:56-91), `describe()` (:95-108), `describe_collection()` (:110-116), `summarize_collection()`
  (:118-183)
- `C:/Development/Quiver/quiver4/src/database_impl.h` — full file: `Database::Impl` (:59-423)
- `C:/Development/Quiver/quiver4/src/binary/binary_metadata.cpp` — toml++ usage (:180-282), the
  throwing anti-pattern (:270-282)
- `C:/Development/Quiver/quiver4/src/csv_read.h` — internal-component header-comment precedent (:1-9)
- `C:/Development/Quiver/quiver4/src/lua_runner.cpp` — `resolve_sandboxed_path` (:1188-1224)
- `C:/Development/Quiver/quiver4/src/CMakeLists.txt` — `QUIVER_SOURCES` (:2-40), tomlplusplus
  PRIVATE linkage (:76-87)
- `C:/Development/Quiver/quiver4/cmake/Dependencies.cmake` — tomlplusplus GIT_TAG v3.4.0 (:9-15)
- `C:/Development/Quiver/quiver4/tests/test_migrations.cpp` — temp-dir fixture idiom (:12-32, :194-205)
- `C:/Development/Quiver/quiver4/tests/test_database_lifecycle.cpp` — describe assertions (:383-500)
- `C:/Development/Quiver/quiver4/tests/test_binary_metadata.cpp` — public-API-only precedent (:1-40)
- `C:/Development/Quiver/quiver4/tests/CMakeLists.txt` — test file registration (:1-15)
- `C:/Development/Quiver/quiver4/include/quiver/schema.h` — `Schema::tables_` map (:104)
- `C:/Development/Quiver/quiver4/src/c/database.cpp` — describe/describe_collection/summarize_collection C API passthrough (:129-167)
- `C:/Development/Quiver/quiver4/CHANGELOG.md` — current head (:1-30), 0.10.6 dated-heading format (:111)
- `C:/Development/HydroThermalDispatch/HydroThermalDispatch.jl/database/ui/hydro_plant.toml` — collection file shape, `initial_volume_type` join key
- `C:/Development/HydroThermalDispatch/HydroThermalDispatch.jl/database/ui/enum.toml` — vocabulary shape, gapped `initial_volume_type` `[0, 2]`
- `C:/Development/HydroThermalDispatch/HydroThermalDispatch.jl/database/ui/main.toml` — confirmed no `id` key (shape-exclusion proof)
- `C:/Development/HydroThermalDispatch/HydroThermalDispatch.jl/database/ui/dc_line.toml` — confirmed filename≠id
- `C:/Development/GNoMo/GNoMo.jl/database/ui/enum.toml`, `C:/Development/Foresight/Foresight.jl/database/ui/enum.toml`, `C:/Development/CarbSteeler/CarbSteeler.jl/database/ui/enum.toml` — cross-repo confirmation of the dynamic-top-level-key vocabulary shape
- `.planning/phases/01-sidecar-reader-and-attribute-meaning/01-CONTEXT.md`, `.planning/REQUIREMENTS.md`, `.planning/STATE.md`, `.planning/PROJECT.md`, `.planning/research/SUMMARY.md` — upstream project decisions and prior research

### Secondary (MEDIUM confidence)

- toml++ official docs (`toml::node`, `toml::table` class references, marzer.github.io/tomlplusplus) — `is_table()`/`as_table()` API existence confirmed via web search this session [CITED]

### Tertiary (LOW confidence)

- None — every claim above is either a direct file read this session or an official-docs citation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependency; existing linkage and version verified by reading `cmake/Dependencies.cmake` and `src/CMakeLists.txt` directly
- Architecture: HIGH — every injection point (line numbers, exact surrounding code) verified by reading the actual files this session, not inferred from CONTEXT.md's prose
- Pitfalls: HIGH — each pitfall traces to either a verified existing test assertion or a verified anti-pattern already present in the codebase
- Corpus/enum.toml shape: HIGH — independently verified against 4 separate real repos this session, correcting one imprecision in CONTEXT.md's prose description

**Research date:** 2026-09-20
**Valid until:** No expiry driver — this is a brownfield snapshot of an unchanging local codebase and a fixed corpus; re-verify only if `src/database_describe.cpp`, `src/database.cpp`, or the toml++ pin change before planning executes.
