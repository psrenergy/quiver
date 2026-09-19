# Phase 1: Enum Labels in Describe - Context

**Gathered:** 2026-09-19
**Status:** Ready for planning

<domain>
## Phase Boundary

A C++ reader for the PSR `database/ui/` TOML sidecar, a lazily-loaded member on
`Database::Impl`, and append-only rendering in the three `describe*` text reports — reaching
C++, the C API, Julia, Dart, Python, JS and Lua **for free**, because `describe*` already
returns a `std::string` through existing C API string wrappers.

Phase 1 adds **zero public surface**: no public header change, no new C symbol, no file under
`bindings/src`. It ships as a **patch**.

Everything about the *structured getters* decided below is recorded for Phases 3–5. It is
context, not Phase 1 work.

</domain>

<decisions>
## Implementation Decisions

### Render format (the deliverable)

- **D-01:** Enum vocabulary and its **full declared value list** render **inline on the scalar
  line** in `describe_collection`, not as an indented sub-line or a trailing `Enums:` block.
  — **Reversibility:** costly — seven test suites assert these strings byte-for-byte.
- **D-02:** Scalar line token order is **append-only**:
  `- name (TYPE) [PRIMARY KEY|NOT NULL] [unit] [hidden] — "Label" enum <vocab> {code: label, …}`.
  Today's line stays a literal **prefix**, so existing substring `contains` assertions in
  `tests/test_database_describe.cpp` hold unchanged. Every new clause is guarded on non-empty,
  so DESC-05 (byte-identical with no sidecar) holds **by construction**, not by care.
- **D-03:** `describe()` uses the **same shared renderer** — `write_collection_section` is not
  branched. The whole-DB report shows label, unit, `[hidden]` and the inline vocabulary for
  every scalar. One call gives an agent everything, which is the triggering use case.
- **D-04:** A data code the vocabulary does not declare is **marked explicitly**:
  `values {0: 8 (Disabled), 2: 1 (undeclared)}`. An agent must be able to distinguish
  "this vocabulary has no label for 2" from "this column has no vocabulary at all" — this is
  the read-side half of the accepted-risk posture.
- **D-05:** The header line is `UI config: <path> (locale: en)` and appears in **all three**
  reports. In `describe()` it follows `Version:`; in `describe_collection` /
  `summarize_collection` it becomes line 1. Absent entirely when no sidecar loaded.
- **D-06:** Locale is hardcoded `"en"` in Phase 1 (`DatabaseOptions.ui_locale` is OPT-02,
  Phase 2). The header still prints it, so the line's shape does not change in Phase 2.
- **D-07:** A **configured** attribute with no declared label prints **today's line unchanged**.
  The attribute id is never synthesised into `label` — see D-12.

### UI metadata design (recorded for Phase 3; NOT built in Phase 1)

Reached via an 8-agent design panel with adversarial review. Four independent designs plus an
incumbent were scored; the incumbent (UI folded onto `ScalarMetadata`) placed last and was
rejected on verified code, not taste.

- **D-08:** UI metadata is its **own type**. It is **never** a field on `ScalarMetadata` or
  `quiver_scalar_metadata_t`. — **Reversibility:** one-way — reversing means re-editing five
  hand-mirrored FFI layouts plus a Lua converter. Three verified reasons:
  1. `GroupMetadata::value_columns` is `std::vector<ScalarMetadata>`
     (`include/quiver/attribute_metadata.h:27`), so a `ui` member lands on every vector/set/
     time-series **value column**, where the sidecar has nothing to say — it decorates *groups*
     via `[[attribute_group]]`, not group columns. Four of five producers could never fill it.
  2. `scalar_metadata_from_column` / `scalar_metadata_with_fk` (`src/database_internal.h:167,181`)
     are pure PRAGMA projections. Filling `ui` needs a `UIConfig*` **and** a collection name they
     cannot derive — `TableDefinition::name` for a group is `Items_vector_values`, and
     `src/CLAUDE.md` forbids hand-rolling the prefix split.
  3. `describe_collection` would never even see it: `write_collection_section`
     (`src/database_describe.cpp:62-70`) iterates `TableDefinition::column_order` and
     `ColumnDefinition` directly, not `ScalarMetadata`.
- **D-09:** Public model is **two types, eleven fields**:
  ```cpp
  struct QUIVER_API UIMetadata {   // ONE record answers for collection, attribute AND group
      bool configured = false;
      std::string label, tooltip, unit, format, icon;
      bool hidden = false;
      std::string vocabulary;      // unresolved, see D-10
      int64_t display_order = -1;  // collections only; -1 for attribute/group
  };
  struct QUIVER_API UIEnumEntry { int64_t code = 0; std::string label; };
  ```
  One record for three levels (Hub's three Dart classes are ~80% identical) makes Phase 4
  **two getters, zero new structs, zero new decoders**.
- **D-10:** `vocabulary` stays an **unresolved string**. Resolving it at parse time makes
  VALID-02's drift *unrepresentable* and deletes a finding Phase 5 exists to report. Entries come
  from `get_ui_vocabulary(name)`. — **Reversibility:** one-way once five binding decoders exist.
- **D-11:** Vocabularies cross the C API as **parallel arrays** (`int64_t** out_codes`,
  `char*** out_labels`, `size_t* out_count`), freed by the existing
  `quiver_database_free_integer_array` / `free_string_array`. Net new free functions for the whole
  feature: **one**. Precedent: `quiver_database_read_time_series_files`
  (`include/quiver/c/database.h:514-528`) already crosses a map-of-strings this way in all six layers.
- **D-12:** `label` is a **non-optional `std::string`**; empty means *the sidecar declares none*.
  Hub's id-fallback (`attribute_configuration.dart`) is **not** copied — Hub is a renderer, Quiver
  is a reader. Back-filling would make "author declared this label" and "author declared nothing"
  the same bytes in six layers, permanently, in the milestone whose accepted risk is already
  *"Quiver authoritatively repeats labels nothing has checked."* `configured` is the discriminator,
  never `label.empty()`.
- **D-13:** Absence is spelled **empty string**, not a nullable pointer, for all six string
  fields — keeps each of five hand-written binding decoders a straight-line read with no null
  branches, and sidesteps Julia's nullability-aware return-type rule.
- **D-14:** `format` is a **single verbatim string** carrying both grammars (`{:.2f}`,
  `yyyy-MM-dd`) and both shapes. Quiver classifies neither.
- **D-15:** `quiver_ui_metadata_t` is laid out **hole-free at 64 bytes** (six pointers, one
  `int64_t`, two `int`s) because `bindings/js/src/metadata.ts` decodes by literal offset by hand.
- **D-16:** `quiver_scalar_metadata_t` and `quiver_group_metadata_t` byte layouts are **not
  touched in this milestone — in either direction**. A **shrink is the dangerous direction**:
  `SCALAR_METADATA_SIZE` is the array stride in `listMetadata` and the nested stride inside
  `readGroupMetadataAt` (`bindings/js/src/metadata.ts:62,153`), so a stale `56` over a `48n` C
  allocation misaligns every element after the first, runs `8n` bytes past the block, and hands
  wild pointers to `new CString(...)`.
- **D-17:** `is_foreign_key` + `references_collection` + `references_column` **should** collapse
  to `std::optional<ForeignKeyRef>` — the flag is fully derivable and
  `src/database_internal.h:181-191` is the only writer. But it ships as its **own PR, after
  Phase 2's SAFE-01..03 size accessors land**, because it shrinks the struct (see D-16) and buys
  this milestone zero capability. Recorded so "deferred" does not become "dropped".

### Parser and lifecycle

- **D-18:** Parser output type `UIConfigSet` lives in **`src/ui_config.h` — private, never under
  `include/`, never `QUIVER_API`**. It aggregates `std::map` members; exposing it would put
  `std::map` in the ABI and create a second route to the same data.
- **D-19:** It carries a `from_directory(dir, locale)` factory mirroring
  `BinaryMetadata::from_toml_file` / `from_toml_content`
  (`src/binary/binary_metadata.cpp:220-232`) so content-level parsing is testable without a
  directory. This is what lets `quiver_tests` exercise the parser while `tomlplusplus` stays
  `PRIVATE` on the `quiver` target.
- **D-20:** Two `Impl` members beside `schema`: `mutable std::optional<UIConfigSet> ui_config`
  and `mutable bool ui_load_attempted`. The separate bool is required because **absence is the
  normal case** — without it every `describe()` on a sidecar-less database re-walks the directory.
- **D-21:** `require_ui_config()` is **separate from `require_schema()`**. `load_schema_metadata`'s
  all-or-nothing contract (`src/database_impl.h:342`) must not become "…or the TOML was malformed".
- **D-22:** `has_ui_config()` is **C++-only in Phase 1** (OPT-04 takes it to every layer in
  Phase 2) — Phase 1 forbids new C symbols.
- **D-23:** `UIConfigSet` records `unlisted_files` at parse time — the only moment the directory
  listing and the `main.collections` list are both in hand. PARSE-01 forbids **loading** by
  directory scan, not **listing** the directory; different acts. This is what makes VALID-05
  (SCE's orphan `agent.toml`) reachable later, and it is free because the type is private.

### Degradation and logging

- **D-24:** **Absent `ui/` logs at `debug`; a malformed one logs at `warn`.** PARSE-11 conflates
  them. Absence is the expected state for every non-PSR quiver database, and `console_level`
  defaults to `Info`, so warning there would put noise in front of every existing user.
  — *Amends PARSE-11's wording.*
- **D-25:** A single broken collection file **fails the whole config** — nothing partial is
  published (PARSE-12, mirroring `load_schema_metadata`). `require_ui_config` catches, logs, and
  publishes nothing.
- **D-26:** Unknown keys are ignored and logged at **debug, once per file** — not per key. The
  corpus has 736 attributes; per-key logging would be a wall of text for one real occurrence
  (`conditions` in `GNoMo/floating_storage_unit.toml`).

### Fixtures and test harness

- **D-27:** Fixtures are **hand-written miniatures, one tolerance each** — gapped ids, absent /
  zero-byte `enum.toml`, unknown key, `format` table form, interleaved `[[attribute]]` /
  `[[attribute_group]]`, orphan collection file, `degradation` as both an attribute id and a group
  id. A failing fixture names the broken rule. Three of the miniatures additionally carry the
  CORPUS-01 cases: all-bare-string (BESSOperation), mixed en/es/pt (Foresight), plain
  `[[attribute]] id = "date_time"` (HydroThermalDispatch).
- **D-28:** **The database is built inside the fixture directory**, not copied to a temp dir.
  Phase 1 has no config-path option (OPT-01 is Phase 2), so the sidecar must physically sit at
  `<db_dir>/ui/`. Building in place keeps one fixture source with no copy step and satisfies
  CORPUS-03 (referenced, never copied into a binding).
- **D-29:** Each suite uses its **own database filename** inside the shared fixture directory
  (`cpp.sqlite`, `capi.sqlite`, `julia.sqlite`, `dart.sqlite`, `python.sqlite`, `js.sqlite`,
  `lua.sqlite`), all gitignored — so seven suites cannot collide even when run concurrently.
  *Consequence accepted:* generated files land under `tests/schemas/`, needing gitignore discipline.
- **D-30:** The four binding describe suites currently assert only "returns a String". DESC-07
  forces new **exact-string** enum assertions; the explicit call the roadmap demands is made in
  D-31 below.

### Claude's Discretion

- Exact wording of the `(undeclared)` marker and the `enum <vocab> {…}` separator, within D-01/D-04.
- Whether `describe()`'s header line is separated from `Version:` by a blank line.
- Internal layout of `src/ui_config.cpp` (one file vs. parser + validator split).
- **D-31:** Whether the four weak binding describe assertions are strengthened, annotated, or
  deleted alongside the new exact-string tests — *not settled in discussion*. The roadmap requires
  an explicit written call; planner must make one and record it, and must not count those suites
  as five-layer coverage until then.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### The format's actual specification (NOT the written spec)
- `C:/Development/Hub/hub1/lib/models/configuration/attribute_configuration.dart` — the real
  per-attribute field set. Reads **11 keys**: `id, hide, format, tab, label, unit, tooltip, type,
  enum, read_only, enabled_if`. `label` defaults to the attribute `id`; `hide` defaults to `false`
  via `map["hide"] ?? false`. v1 consumes 7; `tab`, `type`, `read_only`, `enabled_if` are unused.
- `C:/Development/Hub/hub1/lib/models/utils/localization_string.dart` — PARSE-04's fallback chain
  verbatim: a **bare string wins outright and ignores locale**; otherwise exact locale → `"en"` →
  first key → **throws**. Quiver must not throw there (PARSE-05/11) — pick a different ending.
- `C:/Development/Hub/hub1/lib/models/configuration/format_configuration.dart` — PARSE-06. The
  string form assigns the **same** string to all four of `element_view`/`collection_view`/`edit`/
  `data`; the table form sets each independently with missing keys null.
- `C:/Development/Hub/hub1/lib/models/configuration/collection_configuration.dart` and
  `attribute_group_configuration.dart` — read before Phase 4; D-09's one-record claim rests on
  their field sets coinciding with the attribute one.
- ⚠ `Hub/hub1/.claude/skills/psrhub-ui/references/toml-schema.md` — **known wrong** about group
  membership. Do not write the parser against it.

### The committed consumer
- `C:/Development/Claw/claw/src/core/study-config.ts` — **68 lines.** Reads only `main.model`,
  `main.processes`, and each collection TOML's `id`. Its header states per-attribute semantics are
  *intentionally not read here*.
- `C:/Development/Claw/claw/src/tools/describe-data.ts:39` — `report = db.describe()`. claw's
  actual consumption path is the **text report**.
- `C:/Development/Claw/claw/src/prompt.ts:131` — the enumeration promise, made about `describe_data`.

### Code this phase modifies or mirrors
- `src/database_describe.cpp` — the three reports. `write_collection_section:62-70` (scalar loop,
  reads `ColumnDefinition` directly) and `summarize_collection:141-160` (the histogram, the
  milestone in miniature). `kMaxDistributionCardinality = 64`.
- `src/database_impl.h:59-80, 342-348` — `Impl`, `require_schema`, `load_schema_metadata`'s
  publish-nothing-until-valid rule. D-20/D-21 mirror this shape.
- `src/binary/binary_metadata.cpp:220-232` — the `from_toml_file` / `from_toml_content` split to copy.
- `tests/test_database_describe.cpp` — existing assertions are substring `contains`, which is why
  D-02 works. The one brittle negative:
  `EXPECT_FALSE(contains(report, "some_float: 2 non-null, 1 null; values"))`.
- `include/quiver/schema.h:92` / `src/schema.cpp:265` — `Schema::find_all_tables_for_column`,
  which searches the collection table **and** its group tables. Phase 5's VALID-03 must use this;
  a plain `has_column` false-positives on every group column.

### Layout hazards (read before any FFI edit)
- `include/quiver/attribute_metadata.h:24-28` — `GroupMetadata::value_columns` is
  `std::vector<ScalarMetadata>`. This is why D-08 exists.
- `bindings/js/src/metadata.ts:30-31, 62, 81, 153` — `SCALAR_METADATA_SIZE = 56` /
  `GROUP_METADATA_SIZE = 32` used as **both** out-buffer and array stride.
- `src/c/database_helpers.h:167-210` — `convert_scalar_to_c` (8 named assignments) /
  `free_scalar_fields` (4 `delete[]`). The only bridge between the C++ and C types.
- `include/quiver/c/database.h:319-336` — `quiver_scalar_metadata_t` (56 B) /
  `quiver_group_metadata_t` (32 B).
- `include/quiver/c/database.h:514-528` — `read_time_series_files`: the parallel-string-array
  precedent D-11 copies.

### Project rules
- `CLAUDE.md` (root) — error message patterns, cross-layer naming, design decisions.
- `src/CLAUDE.md` — core internals; the group-table prefix-split prohibition behind D-08(2).
- `tests/CLAUDE.md` — suite layout and the shared-schema rule behind D-28.
- `.planning/PROJECT.md`, `.planning/REQUIREMENTS.md`, `.planning/ROADMAP.md`.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `BinaryMetadata::from_toml_file` / `from_toml_content` — the split that keeps `tomlplusplus`
  `PRIVATE` while staying testable. Copy it exactly.
- `quiver_database_read_time_series_files` + `free_integer_array` / `free_string_array` — a
  map-of-strings and a parallel-array pair that already cross all six layers with **no struct, no
  size constant, no offset decoding**. D-11 rides on this.
- `Impl::require_schema` / `load_schema_metadata` — the lazy, publish-nothing-until-valid shape
  that D-20/D-21 mirror.
- `Schema::find_all_tables_for_column` — already searches collection + group tables.
- `scalar_metadata_lua` (`src/lua_runner.cpp:1166-1189`) — the Lua converter shape to copy.

### Established Patterns
- `describe*` returns `std::string` through existing C API string wrappers — **this is why
  Phase 1 needs no new C symbol and reaches six layers for free.**
- Existing describe assertions are substring `contains`, which makes append-only rendering safe.
- `spdlog` through the per-database `Impl::logger`, never the `spdlog::` globals.
- Error patterns: Pattern 1 precondition, Pattern 2 not-found, Pattern 3 operation-failure.

### Integration Points
- `src/database_describe.cpp` — three guarded, append-only render sites.
- `src/database_impl.h` — two new members + `require_ui_config()`.
- `src/CMakeLists.txt` — `ui_config.cpp` added to the `quiver` target. **No** new dependency;
  `tomlplusplus` is already linked `PRIVATE`.
- `tests/schemas/ui/` — new fixture tree; `tests/CMakeLists.txt` for the new test file.

</code_context>

<specifics>
## Specific Ideas

- The canonical output the whole milestone is judged by:
  `values {0: 8 (Disabled), 1: 4 (Enabled)}` — code **and** label, never label alone.
- Worked example the user asked for, carried forward as the shape to build toward
  (`HydroPlant.has_commitment` → vocabulary `bool`): the attribute record carries
  `vocabulary = "bool"`, and `get_ui_vocabulary("bool")` returns
  `[{0, "Disable"}, {1, "Enable"}]`.
- User's own objection that drove D-10: holding `enum_name` **and** `enum_values` in one record
  makes illegal states representable (named-but-empty = VALID-02's drift; unset-but-populated =
  nonsense) and denormalises a shared entity — the exact "four unsynced places" disease
  PROJECT.md says this milestone exists to cure.

</specifics>

<deferred>
## Deferred Ideas

- **`is_foreign_key` → `optional<ForeignKeyRef>`** (D-17) — own PR, after Phase 2's SAFE-01..03.
- **Correcting the 56/32 constraint text** in `PROJECT.md` and `.claude/CLAUDE.md` — it names the
  **C++** types (`ScalarMetadata`/`GroupMetadata`) while describing the **C** ones. Verified:
  no binding can see the C++ type; `sizeof(quiver::ScalarMetadata)` is already ~200 bytes, so 56
  was never it. As written it will block a safe change and teach the next reader the two structs
  are one. Roadmap amendment, per the user's "CONTEXT.md now, amendments separately".
- **Recording the `libquiver`/`libquiver_c` ABI hazard** — they are separate `SHARED` targets
  (`src/CMakeLists.txt:42,114,149`) and `ScalarMetadata` crosses between them **by value**. A
  hand-assembled mixed native pair corrupts silently. Currently written down nowhere.
- **PROJECT.md's "committed consumer" claim** — it attributes a "per-attribute half of
  `study-config.ts`" to Phase 3; that code does not exist. The file's own comment endorses
  **Phase 1**. User confirmed Phase 3 stays (a consumer exists the source does not yet show), so
  the *sequencing* is unchanged — but the stated justification needs correcting.
- **Static `validate_ui_config(ui_directory, schema_path)` overload** — GOV-02 (model-repo CI) is
  already written down as v2, `validate_migrations` is static precisely so CI can call it on a
  path, and three of five drift checks need no database. ~5 extra lines. Left out as YAGNI;
  revisit at Phase 5.
- **What claw could actually drop** — `loadStudyConfig`: model name, `processes` query, collection
  list and order. That is collection-level (GROUP-01/02, Phase 4) plus a `main.model` /
  `main.processes` getter **nobody has specified**. Worth raising before Phase 4 planning.
- **Refusals recorded so they are not re-proposed:** a fourth/fifth/sixth struct for Phase 4's
  collection and group records (the field sets coincide — D-09); a JSON string as the typed
  metadata surface (Julia and Lua have no parser); SQLite temp/ATTACHed relations as the
  *consumer-facing read* surface (`execute` is private at `include/quiver/database.h:266` and
  `query_*` returns one cell, so one 12-entry vocabulary is 12 round trips — note its **validator**
  SQL was the strongest artifact the panel produced and is worth revisiting at Phase 5); an outer
  `std::optional<UIMetadata>` on the getters (the FFI flattens it away); a structured finding type
  for `validate_ui_config` (`validate_migrations` established throw-a-string); a
  `resolve_sandboxed_path` gate on the new methods (none takes a path — VALID-07 is satisfied
  vacuously).

</deferred>

---

*Phase: 1-Enum Labels in Describe*
*Context gathered: 2026-09-19*
