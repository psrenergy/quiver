# Architecture Research

**Domain:** Brownfield C++20 library milestone — threading a TOML-sourced UI metadata layer through Quiver's existing core → C API → 5 bindings + Lua stack
**Researched:** 2026-09-17
**Confidence:** HIGH (every component, file path and line anchor below was read from the repo or from the two survey journals; nothing is extrapolated)

## Standard Architecture

### System Overview

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  CONSUMERS (outside this repo — not built here, only served)                  │
│   Claw (TS/Bun, describe_data)   Hub (Flutter, quiverdb @ e598fb46 = v0.10.6) │
└───────────────▲──────────────────────────────▲───────────────────────────────┘
                │ std::string reports          │ structured getters
┌───────────────┴──────────────────────────────┴───────────────────────────────┐
│  BINDING LAYER (thin, no logic)                                               │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐   ┌─────────────────────────┐   │
│  │ Julia  │ │  Dart  │ │ Python │ │   JS   │   │ Lua (sol2, in-core)     │   │
│  │c_api.jl│ │bindings│ │_c_api  │ │loader  │   │ src/lua_runner.cpp      │   │
│  │(regen) │ │.dart   │ │.py     │ │.ts     │   │ scalar_metadata_lua     │   │
│  │        │ │(HAND)  │ │(cdef)  │ │(HAND)  │   │ L1166 + lua-api.ts sync │   │
│  └───▲────┘ └───▲────┘ └───▲────┘ └───▲────┘   └───────────▲─────────────┘   │
└──────┼──────────┼──────────┼──────────┼────────────────────┼─────────────────┘
       │          │          │          │   (FFI: C ABI)     │ (direct C++ calls)
┌──────┴──────────┴──────────┴──────────┴────────────────────┼─────────────────┐
│  C API  (quiver_c target — CANNOT include toml++)          │                  │
│   include/quiver/c/database.h   +NEW quiver_ui_metadata_t  │                  │
│   include/quiver/c/options.h    ±quiver_database_options_t │  ABI BREAK       │
│   src/c/database.cpp  describe/describe_collection/summarize → new_c_str      │
│   src/c/database_helpers.h  convert_*_to_c / free_*_fields / copy_strings_to_c│
│   src/c/database_options.h  convert_database_options   src/c/options.cpp      │
└──────────────────────────────▲───────────────────────────────────────────────┘
                               │ (C++ public surface: include/quiver/database.h)
┌──────────────────────────────┴───────────────────────────────────────────────┐
│  C++ CORE  (quiver target — the ONLY target linked to tomlplusplus, PRIVATE)  │
│                                                                              │
│   ┌────────────────────────┐        ┌──────────────────────────────────┐    │
│   │ NEW                    │ owns   │ Database::Impl                   │    │
│   │ include/quiver/        │◄───────┤ src/database_impl.h              │    │
│   │   ui_metadata.h        │        │  path, db, logger                │    │
│   │ src/database_ui_config │        │  mutable schema        (lazy)    │    │
│   │   .cpp   (toml++)      │───────►│  mutable type_validator(lazy)    │    │
│   │  parse + locale resolve│ reads  │  mutable ui_config     (lazy,NEW)│    │
│   └────────────────────────┘ Schema │  require_schema()      L76-79    │    │
│              ▲                      │  load_schema_metadata()L342-349  │    │
│              │ (group membership    └───────┬──────────────────────────┘    │
│              │  joined via table names)     │                               │
│   ┌──────────┴──────────┐                   │ served to                     │
│   │ Schema              │                   ▼                               │
│   │ src/schema.cpp      │   ┌───────────────────────────┬─────────────────┐ │
│   │ PRAGMA-derived ONLY │   │ src/database_describe.cpp │ src/database_   │ │
│   │ (never sqlite_master│   │  write_collection_section │  metadata.cpp   │ │
│   │  .sql text)         │   │    L56-91                 │  get_*_metadata │ │
│   └─────────────────────┘   │  print_group_columns L20  │  list_*         │ │
│                             │  summarize_collection L118│  +NEW           │ │
│                             │  kMaxDistributionCardinal-│   get_ui_*      │ │
│                             │    ity = 64 (the defect)  │   validate_ui_* │ │
│                             └───────────────────────────┴─────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────┘
                               ▲
                  reads at load │ (filesystem, once, lazily)
┌──────────────────────────────┴───────────────────────────────────────────────┐
│  ON DISK, beside the study database:  <db_dir>/ui/                            │
│    main.toml  (model, collections[] = load order AND display order)           │
│    <stem>.toml × N   (id = SQL table name; [[attribute]]; [[attribute_group]])│
│    enum.toml  (N arrays-of-tables; name = vocabulary id; entries {id, label}) │
│    themes/*.toml, [[card]], [[attribute_query]], [[scalar_tab]]  → NOT READ   │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Implementation |
|-----------|----------------|----------------|
| **`include/quiver/ui_metadata.h`** (new) | Value types: `UIEnumEntry{int64_t code; std::string label}`, `UIMetadata{label,tooltip,unit,format,hidden,enum_name,enum_values}`, `UICollectionMetadata{label,icon,help,display_order}`. Rule of Zero, no Pimpl | Plain aggregates, `QUIVER_API`-exported |
| **`src/database_ui_config.cpp`** (new) | Read `main.toml` → the `collections` list → each `<stem>.toml` → `enum.toml`; resolve locale once; build the in-memory model; log a warning per unknown key; never throw on a missing/partial dir | toml++; copy the `from_toml_content` / `from_toml_file` split from `src/binary/binary_metadata.cpp` (L220) |
| **`Database::Impl`** (`src/database_impl.h`) | Owns the parsed config as a third lazy member beside `schema` / `type_validator`; stores the config dir (ctor-time) because `load_schema_metadata` runs long after `open()` | `mutable std::unique_ptr<UIConfig> ui_config;` published only after the whole load succeeds |
| **`src/database_describe.cpp`** | Renders the enrichment into the three text reports. **Append-only** — never insert into an existing token | 3 format strings; no-op when `ui_config` is absent |
| **`src/database_metadata.cpp`** | New structured getters: `has_ui_config`, `get_ui_metadata`, `get_ui_collection_metadata`, `list_ui_enums`, `get_ui_enum`, `validate_ui_config` | Reads the parsed model; missing attribute/collection → default-constructed value (Hub's `?? AttributeConfiguration(id:)` semantics), **not** a Pattern 2 throw; `get_ui_enum` on an unknown name *does* throw Pattern 2 |
| **C API** | `quiver_ui_metadata_t` (NEW struct) + `quiver_database_get_ui_metadata` + `quiver_database_free_ui_metadata`; `quiver_database_options_t` grows 2 fields | Follow `convert_scalar_to_c`/`free_scalar_fields` (`src/c/database_helpers.h` L167-210) for the converter/free pair; grouped parallel arrays (`quiver_csv_options_t`) for shape, `quiver_database_free_time_series_data` for the free-signature style |
| **5 bindings + Lua** | Decode the new struct; pass the report strings through unchanged | Julia regen `c_api.jl`; Dart **hand-edit** `bindings.dart`; Python cdef in `_c_api.py`; JS hand-add to `loader.ts` + new size constant; Lua one `ui_metadata_lua` converter beside `scalar_metadata_lua` (`src/lua_runner.cpp` L1166) |
| **`tests/schemas/ui/`** (new fixture tree) | The de facto conformance corpus, distilled from the real 117 files. **Worth more than the parser** — it is the only artifact that can arbitrate against Hub | Shared under `tests/`, never copied into a binding (`tests/CLAUDE.md`) |

## Recommended Project Structure

```
include/quiver/
├── ui_metadata.h              # NEW: UIMetadata, UIEnumEntry, UICollectionMetadata
├── options.h                  # +ui_config_path, +ui_locale on DatabaseOptions  (ABI phase)
├── database.h                 # +6 method declarations; 3 factory signatures touched
└── c/
    ├── database.h             # NEW quiver_ui_metadata_t + 2 symbols. DO NOT touch
    │                          #   quiver_scalar_metadata_t (56B) / _group_metadata_t (32B)
    └── options.h              # quiver_database_options_t: 8 → 24 bytes  (ABI phase)
src/
├── database_ui_config.cpp     # NEW: the only toml++ TU outside src/binary/
├── database_impl.h            # +ui_config member, +ui dir path, load hook (L342-349)
├── database.cpp               # ctor L94-102, from_migrations L244, from_schema L287,
│                              #   designated-init L272; eager load hooks L437/L495
├── database_describe.cpp      # 3 format strings: L20-36, L56-91, L118-183
├── database_metadata.cpp      # +the structured getters
├── lua_runner.cpp             # +ui_metadata_lua near L1166
└── c/
    ├── database_metadata.cpp  # +2 entry points (alloc/free co-located)
    ├── database_options.h     # convert_database_options + NULL guard on the new char*
    └── options.cpp            # `return {0, QUIVER_LOG_INFO};` must gain entries
tests/
├── test_database_ui_config.cpp   # NEW (+ tests/CMakeLists.txt entry)
├── test_database_describe.cpp    # the brittle file — see Anti-Patterns
├── test_lua_runner_describe.cpp
└── schemas/ui/                   # NEW fixture corpus, .sql + ui/ dir pairs
```

### Structure Rationale

- **`src/database_ui_config.cpp` lives in the core `quiver` target, not `quiver_c`.** `cmake/Dependencies.cmake` vendors tomlplusplus v3.4.0 and `src/CMakeLists.txt` links it **`PRIVATE` to `quiver` only**. Zero CMake change is required for a parser in `src/database*.cpp` — and `quiver_c`, `quiver_cli` **and `quiver_tests`** cannot `#include <toml++/toml.hpp>` without a new link line. **Consequence for the roadmap: the C++ test suite must exercise the parser through Quiver's public API (`UIMetadata`, the report strings), not by constructing `toml::table`s.** Do not add a toml++ link line to `quiver_tests` to make testing easier; that leaks a private dependency into the test target for no gain.
- **A new header, not a field on `attribute_metadata.h`.** See "The key structural decision" below. This is not stylistic.
- **The parser parses the whole accepted key set in the first phase, even though only enums are rendered there.** The tolerances already force it to walk every collection file to find `type`/`enum`; `label`/`tooltip`/`unit`/`format`/`hide` are one `.value<std::string>()` each. Re-opening the parser in every subsequent phase is the expensive path.

## Architectural Patterns

### Pattern 1: New C struct, never a new field on an existing metadata struct

**What:** UI metadata reaches FFI consumers through a brand-new `quiver_ui_metadata_t` with its own getter and its own free function — **not** as extra fields on `quiver_scalar_metadata_t` / `quiver_group_metadata_t`.

**Why this is the single most important constraint in the milestone:** `bindings/js/src/metadata.ts` L30-31 declares

```ts
const SCALAR_METADATA_SIZE = 56;   // = C layout 0/8/12/16/(pad)/24/32/(pad)/40/48
const GROUP_METADATA_SIZE  = 32;   // = C layout 0/8/16/24
```

Those constants are **not only** the stride for walking `list_*` arrays (L62, L153). They are the **out-buffer allocations handed to C**:

```ts
const outBuf = new Uint8Array(SCALAR_METADATA_SIZE);            // L81, 96, 111, 126
check(lib.quiver_database_get_scalar_metadata(this._handle, collBuf.buf, attrBuf.buf, outBuf));
```

Grow `quiver_scalar_metadata_t` by one pointer and C writes 8 bytes **past a JS-owned `Uint8Array`** — a native heap corruption with no compile error, no exception, and no generator to catch it (JS has a hand-written symbol table in `bindings/js/src/loader.ts`; per `bindings/js/CLAUDE.md`, symbols are added by hand). Python's CFFI ABI-mode cdef (`bindings/python/src/quiverdb/_c_api.py` L27-30) and Dart's hand-edited `bindings.dart` fail the same way, silently. Only Julia fails loudly (positional `C.quiver_scalar_metadata_t(...)` constructor calls in `database_metadata.jl` → `MethodError`).

A new struct means JS adds a *new* constant and every existing 56/32 site stays correct by construction.

**Second reason, independent of memory safety:** a code→label map on `ScalarMetadata` would be carried by `list_scalar_attributes` on **every whole-collection read**, for every column, enum or not.

**Trade-off:** one extra C symbol pair and one extra decoder per binding, versus a layout audit across four hand-maintained ABI mirrors. The symbol pair is cheaper and cannot corrupt memory.

```c
typedef struct { int64_t code; const char* label; } quiver_ui_enum_entry_t;
typedef struct {
    const char* label; const char* tooltip; const char* unit; const char* format;
    int hidden;
    const char* enum_name;
    quiver_ui_enum_entry_t* enum_values;
    size_t enum_value_count;
} quiver_ui_metadata_t;

quiver_error_t quiver_database_get_ui_metadata(quiver_database_t*, const char* collection,
                                               const char* attribute, quiver_ui_metadata_t* out);
void quiver_database_free_ui_metadata(quiver_ui_metadata_t*);
```

The same hazard class exists at two more JS sites and must not be disturbed: `bindings/js/src/csv.ts` L24 `new Uint8Array(56)` for `quiver_csv_options_t`, and `bindings/js/src/ffi-helpers.ts` `new Uint8Array(8)` for the options struct (which the ABI phase *does* disturb — deliberately, with its own plan).

### Pattern 2: Locale collapses at parse time; the union never crosses a boundary

**What:** Hub's localizable fields (`label`, `tooltip`, `unit`, `help`, `description`) are a `string | {locale: string}` union, and **the same file mixes both forms** (Foresight 12 bare / 711 dotted; GNoMo 1/735; BESSOperation 78/0). The parser applies Hub's fallback chain — exact locale → `en` → first key — **once, inside `src/database_ui_config.cpp`**, and stores a single `std::string`.

**When:** always. `DatabaseOptions.ui_locale` (default `"en"`) is read at parse time only.

**Trade-off:** no consumer can hold two locales live. No consumer wants to (PROJECT.md: settled). In exchange, `UIMetadata` is flat, `quiver_ui_metadata_t` is flat, no binding needs a variant decoder, and Lua's table stays one level deep. This is the largest available simplification and it is free.

### Pattern 3: Structure from SQL, decoration from TOML — never the reverse

**What:** group membership is **not** in the TOML. `[[attribute_group]].id` is a table-name *suffix*; membership is recovered by joining that id against `{Collection}_vector_{id}` / `{Collection}_set_{id}` / `{Collection}_time_series_{id}` in the already-loaded `Schema`. Attribute entries are a flat per-collection map keyed by column name.

**Why it matters:** `Hub/hub1/.claude/skills/psrhub-ui/references/toml-schema.md` — the only written spec — claims "attributes declared **after** a group belong to that group". **The Dart parser does not implement that.** A second implementer following the doc gets group membership wrong on 15 files that interleave `[[attribute]]` and `[[attribute_group]]`. Write the parser against `C:/Development/Hub/hub1/lib/models/configuration/*.dart` (15 classes, ~719 lines) + `lib/models/utils/toml_utils.dart`.

**Corollary:** an attribute entry naming a dropped column is inert, and an extra TOML entry is never noticed — exactly Hub's behaviour. Reporting that drift is `validate_ui_config()`'s job, and nothing else's.

### Pattern 4: Append-only rendering under a byte-identical baseline

**What:** with no UI config present, `describe` / `describe_collection` / `summarize_collection` output is **byte-identical to today**. With a config, enrichment is *appended* to the end of an existing line, never inserted mid-line.

**Why:** the existing assertions are substring `contains` checks, so an append survives and an insert breaks. `tests/test_database_describe.cpp` breaks on insertion at L33 (`- priority (INTEGER)`), L48 (`- label (TEXT)`), L50 (`[date_time]`), L80/81/86, and carries two assertions that fail on *any* brace-list after a float scalar or any literal `values {`: L84 `EXPECT_FALSE(contains(report, "some_float: 2 non-null, 1 null; values"))` and L104 `EXPECT_FALSE(contains(db.summarize_collection("AllTypes"), "values {"))`. `tests/test_lua_runner_describe.cpp` pins the same formats via Lua patterns. The C API describe tests (`test_c_api_database_metadata.cpp` L175-215, `test_c_api_database_lifecycle.cpp` L507-519) only check `Collection:`, `[date_time]`, `non-null`, `Database: :memory:` — all append-safe.

The target renderings, all append-shaped:

```
some_integer: 3 non-null, 0 null; values {0: 8 (Disabled), 1: 4 (Enabled)}   # summarize_collection
- max_generation (REAL) [MW] — "Maximum Generation"                          # describe_collection
- has_commitment (INTEGER) enum bool {0: Disable, 1: Enable}                 # describe_collection
```

Hidden attributes stay in the report (an agent wants them) tagged `[hidden]`; `hide` is a GUI affordance, and it is always `true` where present (360 occurrences).

### Pattern 5: Degrade, log, never throw

A missing, unreadable or malformed `ui/` dir logs a warning through the existing `impl_->logger` and leaves `has_ui_config() == false`. `open()` still succeeds. Failing the schema load would break every existing caller whose UI dir moved — turning a descriptive feature into a breaking change (PROJECT.md: settled). Unknown keys are ignored with a warning, never rejected (tolerance (c)); Quiver being loud about keys it does not know is the *only* drift signal that exists, since no CI job anywhere references `database/ui`.

## Data Flow

### Load flow (lazy, once)

```
Database(path, options)                 ← ctor stores ui dir + locale on Impl. Reads NOTHING.
        │                                  (convention: <db_dir>/ui/ ; options override = ABI phase)
        │  ... any const reader: describe(), get_scalar_metadata(), a CRUD call ...
        ▼
Impl::require_schema()                     src/database_impl.h L76-79
        ▼
Impl::load_schema_metadata()               src/database_impl.h L342-349
        ├─ Schema::from_database(db)                  (PRAGMA only — never sqlite_master.sql)
        ├─ SchemaValidator(*loaded).validate()        throws ⇒ nothing published
        ├─ TypeValidator(*loaded)
        ├─ parse <ui_dir>/main.toml → collections[] → each <stem>.toml → enum.toml
        │     └─ resolve locale to one string, per field, HERE
        │     └─ any failure ⇒ log warning, ui_config stays null, LOAD STILL SUCCEEDS
        └─ publish: type_validator = ..., schema = std::move(loaded), ui_config = std::move(ui)
```

Also invoked eagerly from `src/database.cpp` L437 (`migrate_up`) and L495 (apply schema) — `from_schema` / `from_migrations` validate at construction, `open()` does not.

### The publish-nothing-until-valid invariant

`load_schema_metadata` publishes **neither** `schema` **nor** `type_validator` until `SchemaValidator::validate()` has passed, because a half-loaded state (schema set, validator null) survives a failed lazy load and crashes the next call. The comment in `src/database_impl.h` says exactly that.

**A UI-config load must honour it in both directions:**

1. `ui_config` is assigned **after** `schema` and `type_validator`, in the same terminal block — never mid-function, never before validation, never from the constructor.
2. A UI parse failure must **not** un-publish or block the schema. Since a bad sidecar degrades rather than throws, the UI branch is wrapped so that it can only ever leave `ui_config` null; it cannot leave `schema` published-but-inconsistent, and it cannot leave a partially-populated `ui_config`. Build the whole config into a local `unique_ptr` and `std::move` it once, exactly as `schema` does.
3. The UI dir path and locale live on `Impl` as **plain members set by the constructor**, not as parameters of `load_schema_metadata` — the load runs long after `open()` returned.

### What crosses the FFI boundary

| Crosses | Never crosses |
|---------|---------------|
| `char*` report strings from `describe*` (already, via `new_c_str`, `src/c/database.cpp` L129/141/155) | the `string \| table` localizable union |
| already-resolved single-locale `const char*` (label, tooltip, unit, format, enum_name) | a locale map, a per-locale array, or a locale parameter on a getter |
| `int64_t` enum codes + `const char*` labels as `quiver_ui_enum_entry_t[]` + count | TOML text, a `toml::table`, or a file path to be parsed binding-side |
| `int hidden` | `[[card]]`, `[[attribute_query]]`, `[[scalar_tab]]`, themes (out of scope) |
| (ABI phase) `const char* ui_config_path`, `const char* ui_locale` on the options struct | |

### Key data flows

1. **Enum label into the histogram.** `summarize_collection` (`src/database_describe.cpp` L118-183) already runs `SELECT col, COUNT(*) … GROUP BY col` for non-PK INTEGER columns with ≤ `kMaxDistributionCardinality = 64` distinct values — and the constant's own comment calls that "the enum/category case". The flow is: column name → collection's attribute map → `type == "enum"` → `enum` name → `enum.toml` vocabulary → code→label lookup per histogram row → append ` (Label)`. **That one substitution is the entire triggering defect.**
2. **Structured getter.** `(collection, attribute)` → parsed model → `UIMetadata` → `convert_ui_to_c` → caller-owned `quiver_ui_metadata_t` → per-binding decoder → freed by `quiver_database_free_ui_metadata`. Alloc and free co-located in `src/c/database_metadata.cpp`, per the existing rule.
3. **Display order.** `main.collections` index → `UICollectionMetadata.display_order` (−1 = not listed) → `describe()` collection ordering. This is also the load list: a collection file absent from `collections` is **never loaded** (tolerance (e)).

## Parser Requirements (the ten non-negotiable tolerances)

Every one is *intra*-model variation the single Dart parser already absorbs — none is a per-model fork. Adversarially verified CONFIRMED across all 117 files: zero parse failures, exactly **one** unknown key in the entire corpus.

| # | Tolerance | Parser requirement |
|---|-----------|--------------------|
| **(a)** | Localizable fields are `string \| table`, mixed **within one file** (Foresight 12/711, GNoMo 1/735, BESSOperation 78/0) | One accessor used for all five localizable fields: if the node is a string take it, else apply exact → `en` → first-key. Resolve to `std::string` at parse time. |
| **(b)** | `enum.toml` and `themes/dark.toml` may be absent or zero-byte (Boost has no `enum.toml`; CHain/SORA/PSRExample ship 0-byte ones) | `exists()` guard, then treat an empty parse as an empty table. Absence is not an error at any level. |
| **(c)** | Unknown keys are ignored, never rejected (corpus-wide: only `conditions` in `GNoMo/floating_storage_unit.toml`) | Read the keys you know; log a warning naming file + key for anything else; never throw. |
| **(d)** | Enum ids are arbitrary ints, never positional (GNoMo `weekday` 1–7; HTD `initial_volume_type` and SCE `granularity_type` gapped `[0, 2]`) | `id` is a required explicit `int64_t`; never infer from array position. A missing `id` is the one enum-entry error worth reporting. `label` is optional and falls back to the id rendered as text. |
| **(e)** | Collections load **only** from `main.collections`, never a directory scan (`SCE/database/ui/agent.toml` is a fully-formed orphan and must stay unloaded — 66 of 67 files are ever loaded) | Iterate `main.collections`; do not enumerate the directory. A listed-but-missing file warns; an unlisted file is invisible. |
| **(f)** | Attribute ids and group ids are **separate namespaces** (`BESSOperation/storage.toml` uses `degradation` for both) | Two maps per collection. Never one keyed map, never a collision check between them. |
| **(g)** | `[[attribute]]` / `[[attribute_group]]` interleaving is legal and flattens — **15 files** | Append to the respective map in encounter order; declaration order carries no membership meaning (see Pattern 3). |
| **(h)** | Collection `id` is PascalCase (the SQL table name) while `main.collections` lists snake_case filenames | Key the model by `id` (defaulting to the filename stem when absent, as Hub does); look up by SQL table name, never by filename. |
| **(i)** | The time-series dimension column has two spellings: `attribute_group.date_time.*` (SCE, BESSOperation, GNoMo) vs a plain `[[attribute]] id = "date_time"` (all 6 HTD group files, GNoMo/historical_conditions) | Absorb both into one representation at parse time, so no consumer and no renderer ever branches on the spelling. 39 of 75 groups use the nested form. |
| **(j)** | `format` carries two grammars (`{:.2f}` on values, `yyyy-MM-dd` on dates) **and** accepts a 4-key table form (`element_view` / `collection_view` / `edit` / `data`) that no file currently uses | Accept both shapes from day one — four lines. Store the string verbatim; **do not classify the grammar**. A string-only parser passes every test today and breaks on first use. This is also the one parser file that differs between hub1 and hub3, so it is the likeliest place the format moves next. |

Corpus the parser must satisfy: 13 repos, 117 files in scope (13 `main.toml`, 67 collection files, 12 `enum.toml` of which 9 non-empty), 736 attributes, 75 attribute groups, 62 vocabularies, 164 enum entries, 127 enum bindings, **0 dangling references**, 4 dead vocabularies. Realistic live reach is 6–8 models (Boost, CHain, SORA, SORA2 are empty or stub).

## Build Order

The one insight that sets the order: **`describe` / `describe_collection` / `summarize_collection` already return `std::string` through the existing C API `new_c_str` wrappers** (`src/c/database.cpp` L129/141/155). Rendering enum labels into those reports therefore reaches Julia, Dart, Python, JS **and** Lua with **zero binding work and zero ABI change**. The whole triggering defect closes behind an existing, already-bound string. The ABI break lives in a different phase entirely and must not be entangled with it.

| Order | Component | Depends on | Why that dependency is real | Ships as |
|-------|-----------|-----------|------------------------------|----------|
| **B1** | `include/quiver/ui_metadata.h` + `src/database_ui_config.cpp` + `tests/schemas/ui/` fixture corpus + `tests/test_database_ui_config.cpp` | nothing | toml++ is already `PRIVATE` on `quiver` — **zero CMake change**. Nothing else can be written or tested before a parsed model exists | — |
| **B2** | `Impl` wiring: ui-dir member set by ctor (convention `<db_dir>/ui/`), parse inside `load_schema_metadata`, honour the publish invariant | B1 | The load hook needs a parser to call | — |
| **B3** | Enum rendering in `summarize_collection` + `describe_collection` + `describe` | B2 | The renderer needs a config on `Impl` | **patch**, no ABI change, **all 5 bindings + Lua get it free** |
| **B4** | `DatabaseOptions.ui_config_path` + `ui_locale` — C++ struct, 3 factory signatures, `src/cli/main.cpp` L95-97, `quiver_database_options_t` (8 → 24 bytes), `src/c/options.cpp` aggregate init, `convert_database_options` NULL guard, then all four FFI options builders | B2 | Pointless before there is something to point at; **isolated from B3 so the defect fix does not wait on an ABI break** | **minor** bump (0.x minor = breaking) |
| **B5** | `quiver_ui_metadata_t` + the 2 C symbols + `get_ui_metadata` in C++ | B2 | Needs the parsed model; **does NOT need B4** — the convention path already supplies a config | same release as B4 |
| **B6** | Per-binding decoders: Julia `ui_metadata.jl`, Dart `ui_metadata.dart`, Python, JS `ui-metadata.ts`, Lua `ui_metadata_lua` | B5 | Each decodes a struct that must already exist | — |
| **B7** | Collection-level (`label`/`icon`/`help`/`display_order`) and `[[attribute_group]]` metadata, absorbing both dimension spellings | B1 (parse), B5 (struct idiom) | Reuses B5's converter/free pattern; the parse work is already done in B1 | — |
| **B8** | `validate_ui_config()` | B1 + `Schema` | Cross-checks the parsed model against PRAGMA-derived structure; mirrors `validate_migrations` (no out-param, throws) | — |

### What can run in parallel, and what strictly cannot

**Parallel:**
- Inside B1: the parser and the `tests/schemas/ui/` corpus are separate work items (the corpus is distilled from real files — a BESSOperation slice for the all-bare-string case, a Foresight slice for en/es/pt, an HTD slice for the plain-`date_time`-attribute spelling).
- **B4 and B5 are independent of each other** and can be built concurrently once B2 lands. They touch disjoint files even in JS (`ffi-helpers.ts` vs a new `ui-metadata.ts`). They should ship in the same release because both are native changes.
- Inside B4: the four FFI options builders (Julia `build_quiver_database_options`, Dart `_makeOptions`, Python `_make_options`, JS `makeDefaultOptions`) are four independent edits **once the C header is fixed**.
- Inside B6: all five binding decoders, once B5's header is final.
- B7 and B8 are independent of each other.

**Strictly sequential:**
- B1 → B2 → B3. No renderer without a config on `Impl`; no config on `Impl` without a parser.
- B5 → B6. A binding cannot decode a struct that does not exist; freezing the C header before any binding starts avoids five simultaneous re-edits.
- The C header edit inside B4 must land **before** any of its four binding builders, for the same reason — and **`bindings/js/src/ffi-helpers.ts` needs its own plan and a runtime assert**. It hardcodes `new Uint8Array(8)` with `setInt32(0)` / `setInt32(4)`, Bun cannot call `quiver_database_options_default` (struct-by-value, bun#6139) so JS has **no generated fallback**, and getting it wrong means C writes 16 bytes past a JS-owned buffer with no error. Callers: `bindings/js/src/database.ts` L21/39/58.
- Any release is sequential and whole: bump all five manifests (`CMakeLists.txt` is the source of truth at 0.10.6) → `publish-s3` → tag → Julia/Python/JS in parallel → **Dart published by hand**. `CHANGELOG.md` currently heads `## [0.10.4] — unreleased` against 0.10.6 — reconcile before the Bump Version workflow runs.

**Fallback property of this order:** if the milestone stalls after B3, the triggering defect is closed in every layer and nothing has been broken. That is the reason B3 precedes B4, and the reason B4 must not be folded into it.

## Anti-Patterns

### AP1: Adding UI fields to `ScalarMetadata` / `GroupMetadata`

**What people do:** the "natural" move — enum + unit + label belong on the attribute's metadata.
**Why it's wrong:** it is a native out-of-bounds write in JS (`SCALAR_METADATA_SIZE = 56` *is* the out-buffer), a silent corruption in Python's ABI-mode cdef and hand-edited Dart, and it puts an enum map on every `list_scalar_attributes` call.
**Do this instead:** Pattern 1 — a new struct, a new size constant, nothing existing disturbed.

### AP2: Regenerating `bindings/dart/lib/src/ffi/bindings.dart` with ffigen

**What people do:** run the generator because the C API changed — that is what the generator is for.
**Why it's wrong:** a full regen flips enums and breaks Hub, which pins quiverdb at `e598fb46` (v0.10.6).
**Do this instead:** hand-add the fields/symbols (around L3555), and clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` or the tests silently run the old layout.

### AP3: Directory-scanning `<db_dir>/ui/` for collection files

**What people do:** glob `*.toml`, it is simpler than following `main.collections`.
**Why it's wrong:** `SCE/database/ui/agent.toml` is a fully-formed collection file that SCE's `collections` array does not list. Hub never loads it; a globbing parser would, and Quiver would then report a collection the UI does not have.
**Do this instead:** tolerance (e) — `main.collections` is the only load list, and it doubles as the display order.

### AP4: Inferring group membership from declaration order

**What people do:** follow `toml-schema.md`, the only written spec: "attributes declared after a group belong to that group".
**Why it's wrong:** the Dart parser does not implement it; membership comes from joining the group id against `{Collection}_vector_{id}` / `_time_series_{id}` table names. 15 files interleave.
**Do this instead:** Pattern 3 — a flat per-collection attribute map, membership from `Schema`. Write against the Dart source, not the doc.

### AP5: Failing the schema load on a bad or missing `ui/` dir

**What people do:** validate strictly, because `load_schema_metadata` already throws on an invalid schema.
**Why it's wrong:** every existing `open()` on a database whose UI dir moved starts throwing. A descriptive feature becomes a breaking change for current callers.
**Do this instead:** Pattern 5 — warn, leave `ui_config` null, `has_ui_config() == false`, `open()` succeeds.

### AP6: Inserting enrichment into existing describe lines

**What people do:** put the label right after the column name, where it reads best.
**Why it's wrong:** `tests/test_database_describe.cpp` L33/48/50/80 assert exact substrings, and L84/L104 are `EXPECT_FALSE` assertions that any brace-list after a float scalar or any literal `values {` will trip. The four *binding* describe suites assert only "returns a String" and would catch nothing.
**Do this instead:** Pattern 4 — append-only, byte-identical without a config, new fixtures with exact-string assertions in the C++ and Lua suites. Leave the binding describe suites honest rather than pretending they verify format.

### AP7: Passing the locale union, or a locale parameter, across the API

**What people do:** keep both locales available "in case someone needs it".
**Why it's wrong:** a variant decoder in five bindings, a two-level C struct, and a Lua table nobody asked for — to serve zero known consumers.
**Do this instead:** Pattern 2. Resolve once, at parse time. (PROJECT.md: settled.)

### AP8: Enforcing enum domains on write

**What people do:** the vocabulary is now known, so validate it in `TypeValidator`.
**Why it's wrong:** it contradicts the settled boolean decision in root `CLAUDE.md` — `CHECK (col IN (0,1))` in the schema is where domain enforcement belongs, and it covers raw SQL too. It also weaponises unverified labels: `HydroThermalDispatch.jl/src/collections/hydro_plant.jl` declares `HasCommitment` as `YES = 0 / NO = 1` while its `enum.toml` says `[[bool]] id=0 → "Disable"`.
**Do this instead:** stay descriptive. `validate_ui_config()` (B8) reports the disagreement; it never resolves it.

## Integration Points

### External consumers (served, not built here)

| Consumer | Integration | Notes |
|----------|-------------|-------|
| **Claw** (`Claw/claw/src/core/study-config.ts`, TS/Bun) | Will consume the structured getters (B5/B6) and drop the per-attribute half of `study-config.ts` | The committed consumer. Its own comment nominates quiverdb. Its read sandbox (`src/tools/native.ts` `readRoots`) **cannot reach `database/ui`** — which is why a host-supplies-a-parsed-map design was rejected. It reaches the enum fix (B3) through `describe_data` → `quiverdb.describe()` with no code change at all |
| **Hub** (`Hub/hub1`, Flutter, 15 parser classes ~719 lines) | The de facto format owner; pins quiverdb at `e598fb46` (v0.10.6) | **Not a goal of this milestone.** Hub keeps its own parser. Quiver must match its accept-set, not replace it |
| **13 model repos' `database/ui/`** | Read-only input | Hand-maintained, hot (Foresight edited 2026-09-17), **no version key in any of the 311 files across ~30 repos**, no CI anywhere validates them. Quiver's `tests/schemas/ui/` corpus becomes the first artifact that can arbitrate |

### Internal boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| `database_ui_config.cpp` ↔ `Database::Impl` | direct C++, owned `unique_ptr` | The only toml++ TU outside `src/binary/`. `quiver_c` / `quiver_cli` / `quiver_tests` cannot see toml++ |
| `Impl` ↔ `Schema` | `Schema` is loaded first, in the same function | Group membership and column existence both come from `Schema`, which is PRAGMA-derived — Quiver reads `sqlite_master` for **names only** (`src/schema.cpp` L317) and has never parsed a byte of SQL text |
| C++ ↔ C API | out-param struct + `quiver_error_t`, single error channel `quiver_get_last_error` | Converter/free pair co-located in `src/c/database_metadata.cpp`, modelled on `convert_scalar_to_c` / `free_scalar_fields` (`src/c/database_helpers.h` L167-210) |
| C API ↔ bindings | Julia regen (`generator.bat`), Dart **hand**, Python cdef **hand**, JS symbol table **hand** | Four hand-maintained ABI mirrors; only Julia fails loudly on drift |
| Core ↔ Lua | sol2, direct C++, no C API | One `ui_metadata_lua` converter beside `scalar_metadata_lua` (`src/lua_runner.cpp` L1166-1188), through which all seven Lua metadata entry points already route. **Any new `db:` name must be added to `bindings/js/src/lua-api.ts` or `bindings/js/test/lua-api-sync.test.ts` fails** |

## Cost and Risk Profile

| Dimension | Reality |
|-----------|---------|
| Files touched | ~45 across core, C API, 5 bindings, 7 test suites. B1+B2+B3 alone is ~4 files |
| Highest-risk single edit | `bindings/js/src/ffi-helpers.ts` `makeDefaultOptions` (B4) — no generated fallback, silent heap corruption. Own plan, runtime assert |
| Lowest-risk highest-value edit | B3's three format strings — zero ABI, zero binding work, closes the triggering defect in all six layers |
| Release gate | Any native change = full ritual: 5 manifests → `publish-s3` → tag → Julia/Python/JS parallel → Dart by hand. B3 is a patch; B4/B5 are a minor |
| Standing risk with no mitigation before B8 | Quiver becomes an authoritative repeater of labels it has not checked (the `HasCommitment` inversion). A reader alone makes this class of bug *worse*. Accepted by explicit decision; B8 is the only mitigation that exists |

## Sources

- `C:/Development/Quiver/quiver3/.planning/PROJECT.md` — decided scope, key decisions, the ten tolerances
- `.../subagents/workflows/wf_92804be2-64e/journal.jsonl` — `survey:quiver-cost` (layer-by-layer build cost, JS out-buffer hazard, publish invariant, test break list, release ritual), `survey:format-owner`, `survey:key-inventory`, `verify:one-parser-possible` (CONFIRMED), `verify:no-owner`, `synthesize:milestone-brief`
- `.../subagents/workflows/wf_e5d70f00-efb/journal.jsonl` — `survey:quiver-surface` (describe internals, `kMaxDistributionCardinality`, `CSVOptions::enum_labels`, `load_schema_metadata`, the layer inventory), `survey:ddl-comments`, `survey:agent-tooling`
- Repo, read directly: `src/database_describe.cpp`, `src/database_impl.h`, `src/database_internal.h`, `src/schema.cpp`, `src/binary/binary_metadata.cpp`, `src/c/database_helpers.h`, `src/c/database_options.h`, `src/c/options.cpp`, `include/quiver/attribute_metadata.h`, `include/quiver/options.h`, `include/quiver/c/database.h`, `include/quiver/c/options.h`, `bindings/js/src/metadata.ts`, `bindings/js/src/ffi-helpers.ts`, `bindings/js/src/csv.ts`, `src/lua_runner.cpp`, `tests/test_database_describe.cpp`, `tests/test_lua_runner_describe.cpp`, `src/CMakeLists.txt`, `cmake/Dependencies.cmake`
- Format reference implementation: `C:/Development/Hub/hub1/lib/models/configuration/*.dart` + `lib/models/utils/toml_utils.dart` (**the spec**); `C:/Development/Hub/hub1/.claude/skills/psrhub-ui/references/toml-schema.md` (**known wrong** on group membership)

---
*Architecture research for: UI metadata layer threaded through Quiver's existing C++ core → C API → 5 bindings + Lua*
*Researched: 2026-09-17*
