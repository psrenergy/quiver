# Requirements: Quiver — UI Metadata Layer

**Defined:** 2026-09-17
**Core Value:** An agent calling Quiver's `describe` on a PSR model database sees what an INTEGER column actually **means** — `values {0: 8 (Disabled), 1: 4 (Enabled)}`, not bare codes.

## v1 Requirements

Requirements for this milestone. Each maps to exactly one roadmap phase.

### Parser (PARSE)

The C++ reader for the PSR `database/ui/` TOML sidecar. Written against Hub's Dart parser
behaviour (`C:/Development/Hub/hub1/lib/models/configuration/*.dart`), **not** against
`toml-schema.md`, which is already wrong about group membership.

- [x] **PARSE-01**: Quiver parses `main.toml` and loads collection files **only** from its `collections` array, never by scanning the directory (SCE ships a fully-formed orphan `agent.toml` that must stay unloaded)
- [x] **PARSE-02**: Quiver parses each collection TOML's `[[attribute]]` entries, keyed by the attribute's `id` (the SQL column name), into a flat per-collection map
- [x] **PARSE-03**: Quiver parses `enum.toml` into named vocabularies, each an ordered list of `{id: int64, label}` entries, accepting arbitrary and gapped integer ids
- [x] **PARSE-04**: Quiver resolves a localizable field that is either a bare string or a `{locale: string}` table, mixed within the same file, to a single string using Hub's fallback chain (exact locale → `en` → first key)
- [x] **PARSE-05**: Quiver ignores unknown keys rather than rejecting them, so a TOML written for a newer Hub still loads
- [x] **PARSE-06**: Quiver accepts `format` as either a string or the 4-key table form (`element_view` / `collection_view` / `edit` / `data`), even though no file in the corpus uses the table form today
- [x] **PARSE-07**: Quiver treats attribute ids and `[[attribute_group]]` ids as separate namespaces (`BESSOperation/storage.toml` uses `degradation` for both)
- [x] **PARSE-08**: Quiver accepts `[[attribute]]` and `[[attribute_group]]` interleaved in any order and flattens them correctly (15 files in the corpus interleave)
- [x] **PARSE-09**: Quiver tolerates an absent or zero-byte `enum.toml` and an absent `themes/` directory without error
- [x] **PARSE-10**: Quiver maps a collection TOML's PascalCase `id` (the SQL table name) to the snake_case filename listed in `main.collections`
- [x] **PARSE-11**: A missing or malformed `ui/` directory degrades silently — a warning is logged, `open()` still succeeds, and `has_ui_config()` returns false
- [x] **PARSE-12**: A parsed UI config is published only after it validates, honouring `load_schema_metadata`'s publish-nothing-until-valid invariant

### Describe Rendering (DESC)

The agent-facing surface. Reaches all five bindings and Lua with no ABI change, because
`describe*` already returns a `std::string` through the existing C API string wrappers.

- [x] **DESC-01**: `summarize_collection` renders enum labels beside the codes in its value histogram — `values {0: 8 (Disabled), 1: 4 (Enabled)}`
- [x] **DESC-02**: `describe_collection` renders each scalar's UI label and unit after its type — `- max_generation (REAL) [MW] — "Maximum Generation"`
- [x] **DESC-03**: `describe_collection` names an enum attribute's vocabulary and its full value list, including codes with zero rows in the data
- [x] **DESC-04**: `describe` renders each collection's UI label and a header line naming the loaded UI config path and resolved locale
- [x] **DESC-05**: With no UI config present, the output of `describe`, `describe_collection` and `summarize_collection` is **byte-identical** to the current output
- [x] **DESC-06**: A `hide = true` attribute still appears in describe output, tagged `[hidden]` — hiding is a GUI affordance, and an agent reading the schema wants it
- [x] **DESC-07**: The enum rendering is exercised from Lua and from all five bindings by tests that assert on exact strings, not merely that a String was returned

### Config Path and Locale (OPT)

The ABI-breaking phase. Isolated deliberately.

- [ ] **OPT-01**: `DatabaseOptions` accepts an explicit UI config directory path, overriding the `<db_dir>/ui/` convention
- [ ] **OPT-02**: `DatabaseOptions` accepts a UI locale, defaulting to `"en"`
- [ ] **OPT-03**: Both are exposed as optional parameters on `open`, `from_schema` and `from_migrations` in all five bindings, per the existing `read_only` / `console_level` pattern
- [ ] **OPT-04**: `has_ui_config()` reports whether a UI config was successfully loaded, in every layer
- [ ] **OPT-05**: `bindings/js/src/ffi-helpers.ts` `makeDefaultOptions` allocates the correct buffer size for the grown options struct, with named offset constants rather than inline literals
- [ ] **OPT-06**: Python's CFFI cdef, Dart's hand-edited `bindings.dart`, and Julia's regenerated `c_api.jl` all reflect the new options layout, and Dart's `.dart_tool/hooks_runner/` and `.dart_tool/lib/` caches are cleared so tests do not silently run the old layout

### Layout Safety (SAFE)

Turns the silent-corruption failure class into a loud startup error. Protects work beyond
this milestone.

- [ ] **SAFE-01**: The C API exposes size accessors returning the native `sizeof` for each FFI struct a binding allocates a buffer for
- [ ] **SAFE-02**: Each binding asserts its hardcoded struct size against the native value at load time and fails loudly on mismatch
- [ ] **SAFE-03**: The assertion covers the pre-existing hazards as well as the new struct — the options struct, `quiver_scalar_metadata_t` (JS `SCALAR_METADATA_SIZE = 56`), and `quiver_group_metadata_t` (JS `GROUP_METADATA_SIZE = 32`)

### Structured Metadata Getter (META)

What claw consumes, replacing the per-attribute half of `study-config.ts`.

- [ ] **META-01**: A new C++ type carries a scalar attribute's UI metadata: label, tooltip, unit, format, hidden flag, enum vocabulary name, and enum values
- [ ] **META-02**: A public C++ getter returns that metadata for a `(collection, attribute)` pair, returning a default-constructed value for an unconfigured attribute rather than throwing
- [ ] **META-03**: The metadata crosses the C API as its **own** struct with its own free function — never as new fields on `quiver_scalar_metadata_t` or `quiver_group_metadata_t`
- [ ] **META-04**: The getter is bound in all five bindings and in Lua, named per the cross-layer convention
- [ ] **META-05**: A public method lists the loaded vocabularies, and another returns one vocabulary's entries by name, throwing Pattern 2 on an unknown name
- [ ] **META-06**: Adding any new `db:` method keeps `bindings/js/src/lua-api.ts` in sync so `lua-api-sync.test.ts` passes

### Collection and Group Metadata (GROUP)

- [ ] **GROUP-01**: A public getter returns a collection's UI label, icon and help text
- [ ] **GROUP-02**: The getter reports the collection's display order — its index in `main.collections`
- [ ] **GROUP-03**: `[[attribute_group]]` metadata is exposed, with group membership recovered by joining the group's `id` against the `{Collection}_vector_{id}` / `_time_series_{id}` table names in SQLite
- [ ] **GROUP-04**: Both spellings of the time-series dimension column are absorbed — nested `attribute_group.date_time.*` (SCE, BESSOperation, GNoMo) and a plain `[[attribute]] id = "date_time"` (all 6 HTD group files, GNoMo/historical_conditions)
- [ ] **GROUP-05**: Collection and group metadata are bound through the C API to all five bindings and Lua

### Validation (VALID)

Advisory, ships last. See the accepted risk in PROJECT.md.

- [ ] **VALID-01**: `validate_ui_config()` cross-checks the loaded sidecar against the live SQL schema and reports drift, mirroring `validate_migrations` — no out-parameter, throws on failure
- [ ] **VALID-02**: Validation reports an attribute bound to a vocabulary that `enum.toml` does not declare
- [ ] **VALID-03**: Validation reports a TOML attribute whose column does not exist in the SQL schema
- [ ] **VALID-04**: Validation reports a SQL table absent from `main.collections` (HTD's `Interconnection` is entirely invisible today)
- [ ] **VALID-05**: Validation reports a collection TOML present on disk but not listed in `main.collections` (SCE's orphan `agent.toml`)
- [ ] **VALID-06**: Validation reports vocabularies declared but bound by nothing (4 dead vocabularies in the corpus)
- [ ] **VALID-07**: `validate_ui_config()` is bound in every layer including Lua, db-scoped and sandboxed per the existing Lua file-operation policy
- [ ] **VALID-08**: Running the validator against the real model repos produces a drift report; the findings are recorded, not auto-fixed

### Test Corpus (CORPUS)

The corpus is worth more than the parser — it is the contract.

- [x] **CORPUS-01**: `tests/schemas/ui/` holds fixtures distilled from real model repos, covering the all-bare-string case (BESSOperation), the mixed en/es/pt case (Foresight), and the plain-`date_time`-attribute spelling (HydroThermalDispatch)
- [x] **CORPUS-02**: Fixtures cover each parser tolerance individually: gapped enum ids, absent `enum.toml`, unknown keys, the `format` table form, interleaved attribute/group entries, the orphan collection file, and the id-namespace collision
- [x] **CORPUS-03**: Fixtures live under `tests/schemas/` and are referenced by every binding suite, never copied into one

## v2 Requirements

Acknowledged, deferred, not in this roadmap.

### Format Governance (GOV)

- **GOV-01**: A version field in the `database/ui` TOML format, so a second implementation can detect a format it does not understand
- **GOV-02**: A CI check in the model repos that runs `validate_ui_config()` against each repo's own schema
- **GOV-03**: A named arbiter for disagreements between Hub's Dart parser and Quiver's C++ parser

### Vocabulary Cleanup (VOCAB)

- **VOCAB-01**: Standardize the boolean vocabulary — `bool` / `yes_no` / `yes_or_no` currently fork three ways with three label pairs; CarbSteeler and HTD each ship two, one dead in each
- **VOCAB-02**: Reconcile the `HasCommitment` inversion and any other disagreement the validator surfaces between Julia `@enumx` declarations and `enum.toml`

## Out of Scope

| Feature | Reason |
|---------|--------|
| `[[card]]` dashboard queries | 68 SQL strings; an agent gets strictly more by running the SQL itself. Pure Hub presentation. |
| `themes/*.toml` | A flat ARGB map feeding a Flutter `ColorScheme`. Zero agent or model-code value. |
| `[[attribute_query]]` | Hub view affordance, 6 files. |
| `[[scalar_tab]]` | Hub view affordance, 2 files. |
| `[[card]].conditions` | 1 occurrence in the entire corpus, read by no parser. Do not implement keys nobody reads. |
| `[[enum item]].hide` | Legal per Hub's Dart parser; **zero** models use it. |
| String-keyed enum vocabularies | HTD stores 5 closed vocabularies as TEXT. `enum.toml` cannot declare them, so support means extending the format itself. `UIEnumEntry.code` stays `int64_t`. User: "don't care about HTD now". |
| Write-side TOML scaffolding | Generating a collection TOML when a migration adds a table. A reader fixes consumption, not drift. Doubles the milestone. |
| Enforcing enum domains on write | Contradicts the settled boolean decision: `CHECK (col IN (0,1))` in the schema is where domain enforcement belongs. This milestone is descriptive. |
| Two locales live at once | Locale resolves once at parse time. The `string \| table` union never reaches the C API or any binding — the single biggest available simplification. |
| Hub consuming Quiver's parser | Not a goal. Claw is the committed consumer; Hub keeping its Dart parser is fine. |
| Parsing DDL comments | Prose, three formats, Portuguese, missing 7 of 25 columns, and the richest instance is dropped by `sqlite_master`. Quiver has never parsed SQL text and will not start. |
| Subsuming `CSVOptions::enum_labels` | The existing CSV enum map is keyed by bare attribute name and caller-supplied; unifying it is a separate breaking change with no shipped caller to benefit. |

## Traceability

Which phases cover which requirements. Populated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| PARSE-01 | Phase 1 | Complete |
| PARSE-02 | Phase 1 | Complete |
| PARSE-03 | Phase 1 | Complete |
| PARSE-04 | Phase 1 | Complete |
| PARSE-05 | Phase 1 | Complete |
| PARSE-06 | Phase 1 | Complete |
| PARSE-07 | Phase 1 | Complete |
| PARSE-08 | Phase 1 | Complete |
| PARSE-09 | Phase 1 | Complete |
| PARSE-10 | Phase 1 | Complete |
| PARSE-11 | Phase 1 | Complete |
| PARSE-12 | Phase 1 | Complete |
| DESC-01 | Phase 1 | Complete |
| DESC-02 | Phase 1 | Complete |
| DESC-03 | Phase 1 | Complete |
| DESC-04 | Phase 1 | Complete |
| DESC-05 | Phase 1 | Complete |
| DESC-06 | Phase 1 | Complete |
| DESC-07 | Phase 1 | Complete |
| CORPUS-01 | Phase 1 | Complete |
| CORPUS-02 | Phase 1 | Complete |
| CORPUS-03 | Phase 1 | Complete |
| OPT-01 | Phase 2 | Pending |
| OPT-02 | Phase 2 | Pending |
| OPT-03 | Phase 2 | Pending |
| OPT-04 | Phase 2 | Pending |
| OPT-05 | Phase 2 | Pending |
| OPT-06 | Phase 2 | Pending |
| SAFE-01 | Phase 2 | Pending |
| SAFE-02 | Phase 2 | Pending |
| SAFE-03 | Phase 2 | Pending |
| META-01 | Phase 3 | Pending |
| META-02 | Phase 3 | Pending |
| META-03 | Phase 3 | Pending |
| META-04 | Phase 3 | Pending |
| META-05 | Phase 3 | Pending |
| META-06 | Phase 3 | Pending |
| GROUP-01 | Phase 4 | Pending |
| GROUP-02 | Phase 4 | Pending |
| GROUP-03 | Phase 4 | Pending |
| GROUP-04 | Phase 4 | Pending |
| GROUP-05 | Phase 4 | Pending |
| VALID-01 | Phase 5 | Pending |
| VALID-02 | Phase 5 | Pending |
| VALID-03 | Phase 5 | Pending |
| VALID-04 | Phase 5 | Pending |
| VALID-05 | Phase 5 | Pending |
| VALID-06 | Phase 5 | Pending |
| VALID-07 | Phase 5 | Pending |
| VALID-08 | Phase 5 | Pending |

**Coverage:**

- v1 requirements: 50 total
- Mapped to phases: 50 ✓
- Unmapped: 0

**By phase:**

| Phase | Name | Requirements | Count |
|-------|------|--------------|-------|
| 1 | Enum Labels in Describe | PARSE-01…12, DESC-01…07, CORPUS-01…03 | 22 |
| 2 | Config Path, Locale and Struct-Size Safety | OPT-01…06, SAFE-01…03 | 9 |
| 3 | Structured Attribute Metadata | META-01…06 | 6 |
| 4 | Collection and Attribute-Group Metadata | GROUP-01…05 | 5 |
| 5 | `validate_ui_config()` and Milestone Release | VALID-01…08 | 8 |

---
*Requirements defined: 2026-09-17*
*Last updated: 2026-09-17 after roadmap creation*
