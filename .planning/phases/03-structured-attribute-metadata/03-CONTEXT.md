# Phase 3: Structured Attribute Metadata - Context

**Gathered:** 2026-09-19
**Status:** Ready for planning
**Mode:** Smart discuss — five parallel code scouts grounded the forks; four put to the user, two defaulted with rationale

<domain>
## Phase Boundary

Make the PSR `ui/` sidecar data that Phase 1 already parses and renders as text available as
**structured data**: a public C++ getter returning one scalar attribute's UI record (label,
tooltip, unit, format, icon, hidden, vocabulary name), plus a vocabulary lister and a by-name
vocabulary fetcher — crossing the C API as **one new struct** with its own size accessor and free
function, and bound in Julia, Dart, Python, JS and Lua.

Requirements: META-01 … META-06.

Because `UIMetadata` and `UIEnumEntry` already exist privately with exactly the intended field set
(`src/ui_config.h:21-45`), the C++ half is **a header move plus two getters, not a design**. The
weight of the phase is the seven marshalling layers.

In scope: the public header move, three public getters, the new C struct + its size accessor + its
free function, the vocabulary parallel-array pair, five binding decoders, the Lua converter, and
the `lua-api.ts` sync edit.

Out of scope: collection- and group-level metadata (Phase 4 — it reuses this same record type and
adds **zero** new structs), `validate_ui_config()` (Phase 5), any change to
`quiver_scalar_metadata_t` / `quiver_group_metadata_t` in **either** direction (D-16), and any work
inside the claw repo.

</domain>

<premise_corrections>
## Two Stale Premises in the ROADMAP (correct before planning)

Both were established by code inspection during this discussion. Neither changes what Phase 3
builds; both change what "done" means, and the first makes the ROADMAP goal line factually wrong.

- **P-01: claw has no per-attribute half to drop.** The ROADMAP Phase 3 goal reads
  "`Claw/claw/src/core/study-config.ts` can drop its per-attribute half". That file is 68 lines
  reading only `main.toml` plus each collection file's `id`, and its header comment states that
  per-attribute semantics are *deliberately* left to quiverdb (`study-config.ts:1-11`). claw also
  parses nothing out of `describe()` — `describe-data.ts:41` returns the raw report string straight
  to the LLM. **Phase 3 deletes no parser anywhere; it adds a capability claw has no code path
  for yet.** Phase 1's own canonical-refs section already recorded this correctly; only the
  ROADMAP goal line drifted. (The path is also misspelled — see P-03.)
- **P-02: claw cannot consume Phase 3 until it passes Phase 2's `uiConfigDir`.** It opens
  databases with no options (`C:/Development/Claw/claw1/src/core/db.ts:12-14`), and the
  `<db_dir>/ui/` convention cannot resolve for a deployed model: the study is copied into
  `<studyDir>/.claw/<db>/<session>/original/` while the sidecar lives at
  `<installRoot>/model/bin/database/ui`. That is **claw-repo work with no Quiver requirement
  behind it** — do not plan it here, but do not call the milestone's core value delivered without it.
- **P-03: the ROADMAP's claw path is wrong.** It writes `Claw/claw/src/core/study-config.ts`;
  the real directory is `claw1` — `C:/Development/Claw/claw1/src/core/study-config.ts`. A grep for
  the ROADMAP spelling finds nothing. Phase 1's canonical refs carry the same typo.

</premise_corrections>

<decisions>
## Implementation Decisions

Phase 1's discussion ran an 8-agent design panel whose output D-08 … D-17 is **binding on this
phase** and is not relitigated here. Read
`.planning/phases/01-enum-labels-in-describe/01-CONTEXT.md` before planning. The decisions below
are only what that panel left open, plus the corrections this discussion found.

### Carried forward from Phase 1 — locked, do not revisit

- **D-08** metadata is its own type, never a field on `ScalarMetadata` (= META-03).
- **D-09** two types, eleven fields; one record answers for collection, attribute **and** group.
- **D-10** `vocabulary` stays an **unresolved string**; entries come from a separate getter.
- **D-11** vocabularies cross as parallel arrays (`int64_t**`, `char***`, `size_t*`).
- **D-12** `label` non-optional; empty ≠ unconfigured; `configured` is the discriminator.
- **D-13** absence is spelled empty string, not a nullable pointer.
- **D-14** `format` is a single verbatim string (but see D-32 — this discussion resolved what
  "verbatim" can honestly mean).
- **D-15** `quiver_ui_metadata_t` is hole-free at 64 bytes.
- **D-16** `quiver_scalar_metadata_t` / `quiver_group_metadata_t` untouched in either direction.

### The public type

- **D-30:** The C++ half is a **header move, not a redesign**. `UIMetadata` and `UIEnumEntry`
  (`src/ui_config.h:21-45`) already carry the exact intended field set, contain only
  `std::string` / `int64_t` / `bool` (no toml++, no `std::map`), and move under `include/quiver/`
  with `QUIVER_API`. `UIConfigSet` **stays private** — it aggregates `std::map` members (D-18).
  The file's own header comment states this intent. There is no name collision to resolve: the
  private types move out, they are not duplicated.
  — **Reversibility:** one-way — reversing means un-publishing a `QUIVER_API` type after five
  hand-mirrored FFI decoders exist.
- **D-31:** `icon` and `display_order` ship in the Phase 3 struct even though only Phase 4
  populates `display_order` (always `-1` on an attribute record, `src/ui_config.cpp:320`). This is
  D-09's whole point — it makes Phase 4 two getters and **zero** new structs. Do not trim the
  record to Phase 3's needs.

### Format (user decision)

- **D-32:** The record carries **one collapsed string**, as D-14 specifies. The 4-key table form
  (`data` / `element_view` / `collection_view` / `edit`) is collapsed at parse time to the
  first present key and the other three are discarded irrecoverably
  (`resolve_format_table`, `src/ui_config.cpp:88-97`). The struct stays hole-free at 64 bytes.
  — **Reversibility:** one-way — adding fields after the header freeze is an ABI break across five
  hand-written decoders plus the four-struct size gate, which is the precise failure the freeze
  exists to prevent.
  — **Consequence accepted, and it must be written down rather than glossed:** three of four
  author-declared format strings are unreachable through **any** Quiver surface. On the
  `format_table` fixture, `capacity` declares `0.0`/`0.00`/`0.000`/`0.0000` and only `0.0000`
  survives. A consumer asking for `capacity`'s edit format gets the `data` key with no signal that
  anything else was declared.
- **D-33:** **ROADMAP criterion 3 must be amended before it is used as an acceptance test.** It
  currently reads that `format` "round-trips **verbatim** in both grammars … and both shapes". Under
  D-32 that is not deliverable for the table shape. Amend to "the winning key's string round-trips
  verbatim" so the criterion tests what the parser can actually do. Planner: do not write a test
  asserting four-key round-trip; it cannot pass.

### Surface shape (user decisions)

- **D-34:** **Singular getter only**, exactly as META-02 is written. No collection-wide listing in
  Phase 3. Adding a plural later is purely additive — one function, one free, one method per
  binding, no ABI break and no re-edit of the singular decoder.
  — **Consequence accepted:** claw's only structural-metadata idiom is list-then-find
  (`C:/Development/Claw/claw1/src/tools/read-data.ts:222` — it never calls the singular
  `getScalarMetadata`), so the real consumer's first use of this feature will be a shape no test
  exercises. If the plural is ever added, that is the reason.
- **D-35:** The getter is named **`get_attribute_ui_metadata(collection, attribute)`**, fixing
  Phase 4's siblings as `get_collection_ui_metadata` / `get_group_ui_metadata`. The project forbids
  overloads and type dispatch, so with one record serving three levels the distinction must live in
  the name; a symmetric trio now saves Phase 4 from choosing under pressure.
  — **Reversibility:** one-way — the name is hand-mirrored in C, Julia, Dart, Python, JS, Lua and
  the `lua-api.ts` reference, whose sync test matches the literal token with a word boundary, so a
  rename fails in two directions at once.
  — Companions, per D-10 and META-05: `list_ui_vocabularies()` and `get_ui_vocabulary(name)`.
- **D-36:** **The getter validates against the live SQL schema** — `require_collection` plus a
  column check, Pattern 2 on a miss — matching `get_scalar_metadata`
  (`src/database_metadata.cpp:6-16`). META-02's "rather than throwing" governs an **unconfigured**
  attribute, not a **nonexistent** one: the default-constructed record is returned only for a real
  column the sidecar does not configure.
  — **Reversibility:** one-way — the error contract is mirrored by hand in seven layers plus each
  layer's error-channel test.
  — **Consequence accepted:** this is the first UI-path surface in the milestone that is loud
  rather than degrading (`has_ui_config` never throws, `find_attribute` returns `nullptr`,
  `require_ui_config` swallows everything), and it couples a UI-metadata read to `require_schema`,
  so the getter does not work on the half-migrated or non-quiver databases the lazy-schema design
  keeps usable. Chosen anyway, because without the check a misspelled column returns an all-empty
  record indistinguishable from "this column has no UI metadata" — the exact ambiguity D-12
  refused to create at the field level.

### Marshalling (Claude's call — overridable)

- **D-37:** The vocabulary getter ships a **dedicated** combined free,
  `quiver_database_free_ui_vocabulary(int64_t* codes, char** labels, size_t count)`, rather than
  asking callers to compose `free_integer_array` + `free_string_array`. D-11 budgeted "net new free
  functions: one" and said reuse the generics — but the precedent D-11 *itself cites*,
  `read_time_series_files`, ships its own combined free
  (`include/quiver/c/database.h:538`) instead of making five hand-written decoders remember to call
  two and hold `count` alive for the second. A decoder that frees only one array leaks silently
  with nothing to catch it. Siding with the cited evidence over the budget line; criterion 4's "its
  own free function" then reads literally for both new surfaces.
- **D-38:** In Lua, an **empty text field arrives as `nil`, not `""`**. `""` is truthy in Lua, so
  the idiomatic `if md.unit then print(md.unit) end` would print a blank unit for every attribute
  in the database — the precise trap `src/lua_runner.cpp:772-781` documents avoiding for
  `db:read_csv`'s header. `scalar_metadata_lua` (`:1660-1675`) already maps every absent optional to
  `sol::lua_nil`, removing the key, so a variable key set is already the norm for this record
  family. `configured` is always present as the discriminator.
  — **Consequence accepted:** this makes Lua the one layer where D-13's empty-string-means-absent
  does not hold — a sixth entry on the documented per-binding divergence list. Record it in root
  `CLAUDE.md` alongside the other five.
- **D-39:** A vocabulary in Lua is a **1-based array of `{code = …, label = …}` record tables**.
  A code-keyed map is ruled out by the JSON return encoder, not taste: a table keyed `1..n` encodes
  as a JSON array and silently drops the codes, and anything else encodes as a lexicographically
  sorted object (`"10"` before `"2"`), so the shape would vary with the data
  (`src/lua_runner.cpp:137-166`, `:173-184`). Closest precedent: `dimension_to_lua` (`:1077-1092`).

### Already settled by existing policy — no decision needed

- **D-40:** `quiver_ui_metadata_t` joins the load-time struct-size gate as the **fifth** struct,
  appended in the existing fixed order (options, scalar metadata, group metadata, csv options, then
  this), in all four bindings with no reordering. `src/c/CLAUDE.md:98-110` already states a new
  hand-allocated struct "joins this list by default … that is not a decision to revisit per
  struct." Each gate's hardcoded name list and its test's expected count go from four to five.
- **D-41:** The getter **cannot throw because the sidecar is absent or malformed**.
  `require_ui_config` (`src/ui_config.cpp:369-417`) swallows every failure and publishes nothing, so
  `:memory:`, an absent directory and a parse throw all end with the cache empty and
  `has_ui_config() == false`. Any getter calling it inherits a never-throws-from-loading contract
  for free. (This is orthogonal to D-36, which throws on a bad *column name*, not a bad sidecar.)
- **D-42:** The single-record getter is **caller-allocated** with `char*` fields allocated by C with
  `quiver::string::new_c_str` (`new char[]`, `src/utils/string.h:16-21` — the house idiom per
  `src/c/CLAUDE.md` "String Handling"), released with `delete[]` and nulled by the matching free —
  the `get_scalar_metadata` / `free_scalar_metadata` pair
  (`include/quiver/c/database.h:349-352,370`). This fixes the allocation idiom in every binding:
  Julia `Ref`, Dart `arena<T>()`, Python `ffi.new("T*")`, JS `new Uint8Array(SIZE)`.
- **D-43:** No new dependency and no CMake work. tomlplusplus is already `PRIVATE` on `quiver` and
  the new public types add no dependency to the public interface. Julia's generator auto-discovers
  a new header under `include/quiver/c/`; Python's generator needs **one line** in its 5-entry
  `HEADERS` list; Dart's `bindings.dart` is hand-edited by standing milestone constraint; JS's
  symbol table is hand-written regardless.

### Claude's Discretion

- Whether the three getters live in a new `src/database_ui_metadata.cpp` or join an existing
  `database_metadata.cpp`.
- Exact header filename for the moved public types (`include/quiver/ui_metadata.h` suggested —
  **not** `attribute_metadata.h`, which criterion 4 requires untouched).
- Whether `list_ui_vocabularies` returns sorted or map-order names (state the choice in a test).
- Wave decomposition, subject to the freeze-the-header-first note below.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Binding prior decisions (read first — they are not repeated here)
- `.planning/phases/01-enum-labels-in-describe/01-CONTEXT.md` §D-08 … D-17 — the 8-agent design
  panel that settled this phase's type design. **Binding.** Its canonical-refs section also carries
  the Hub format specification links below.
- `.planning/phases/02-config-path-locale-and-struct-size-safety/02-CONTEXT.md` §D-07 … D-16 — the
  struct-size gate this phase's struct joins, and the per-binding generator discipline (D-16) that
  still applies verbatim.

### The supply side — what Phase 1 already parses
- `src/ui_config.h:21-45` — `UIMetadata` / `UIEnumEntry` with the exact field set to publish; the
  header comment states Phase 3 is "a header move, not a redesign". `:16-19` keeps `UIConfigSet`
  private.
- `src/ui_config.cpp:88-97` — `resolve_format_table`, first-present-wins (the D-32 collapse).
- `src/ui_config.cpp:320` — `display_order` set for collections only.
- `src/ui_config.cpp:344-355` — `find_attribute` returns `nullptr`, never throws.
- `src/ui_config.cpp:369-417` — `require_ui_config` swallows everything; assignment is the last
  statement in the `try` (`:410-416`).
- `src/database_describe.cpp:61-89` — the only current consumer; reads **only** `unit`, `hidden`,
  `label`, `vocabulary`. `:75-88` already resolves a vocabulary by second lookup and prints
  `(undeclared vocabulary)` on a miss.

### Shapes to copy rather than invent
- `include/quiver/c/database.h:349-352,:370` + `src/c/database_metadata.cpp:20-33,:67-76` —
  `get_scalar_metadata` / `free_scalar_metadata`: the caller-allocated out-struct idiom (D-42).
- `src/c/database_helpers.h:167-210` — `convert_scalar_to_c` / `free_scalar_fields`: the
  converter/free pair.
- `include/quiver/c/database.h:527-531,:538` + `src/c/database_time_series.cpp:463-476` —
  `read_time_series_files`: the two-parallel-arrays precedent **and** its dedicated combined free
  (the evidence behind D-37). Already decoded in all four bindings:
  `bindings/js/src/time-series.ts:385-418`, `bindings/julia/src/database_read.jl:747-775`,
  `bindings/python/src/quiverdb/database.py:1890`, `bindings/dart/lib/src/database_read.dart:1348`.
- `src/database_metadata.cpp:6-16` — `get_scalar_metadata`'s validation contract (D-36).
- `src/lua_runner.cpp:1660-1675` — `scalar_metadata_lua`, the Lua converter precedent (D-38).
- `src/lua_runner.cpp:1077-1092` — `dimension_to_lua`, the ordered-record-array precedent (D-39).

### The struct-size gate this struct joins
- `src/c/CLAUDE.md:98-110` — the promoted "every hand-allocated struct joins by default" rule.
- `bindings/js/src/loader.ts:338-343,413-443`; `bindings/python/src/quiverdb/_loader.py:22-27`;
  `bindings/julia/generator/prologue.jl:126-133`;
  `bindings/dart/lib/src/ffi/library_loader.dart:94-99`.
- Tests whose expected count goes four → five: `bindings/js/test/struct-sizes.test.ts:26-47`,
  `bindings/julia/test/test_struct_sizes.jl:23-38`,
  `bindings/python/tests/test_struct_sizes.py:23-44`,
  `bindings/dart/test/struct_sizes_test.dart:23-40`.

### The Lua sync gate (criterion 5 / META-06)
- `bindings/js/test/lua-api-sync.test.ts:19-21,29-47,51-53,82-91` — matching is
  **word-boundary-suffixed**, so `db:get_ui_vocabulary` is not satisfied by
  `db:list_ui_vocabularies`; each name needs its own literal token. The reverse check also fails if
  any `db:` token in new prose is not a real bound name.
- Register through the regex-detected route: `bind.set_function("<name>", &<name>_lua);` with the
  literal receiver `bind`, in the **same commit** as the `lua-api.ts` token.
- Root `CLAUDE.md` "Do Not Fix" — relocating `LUA_DB_API_REFERENCE` is forbidden.

### The consumer
- `C:/Development/Claw/claw1/src/core/study-config.ts:1-11` — 68 lines; the P-01 evidence.
- `C:/Development/Claw/claw1/src/tools/read-data.ts:222,240-243` — `listScalarAttributes(...).find(...)`,
  claw's actual list-then-find idiom (the D-34 counter-argument).
- `C:/Development/Claw/claw1/src/tools/describe-data.ts:41` — returns the raw report to the LLM.
- `C:/Development/Claw/claw1/src/core/db.ts:12-14` — opens with no options (the P-02 evidence).
- `C:/Development/Claw/claw1/test/prompt.test.ts:38-46` — prompt budget raised 50k → 60k at ~54.4k
  actual, recorded as "a DEFERRAL, not a verdict" naming quiverdb's reference as the cause.
  **≈5.6k characters of headroom** for three new Lua surfaces plus their documentation.

### The format specification (Hub, the de facto spec)
- `C:/Development/Hub/hub1/lib/models/configuration/format_configuration.dart` — the string form
  assigns the same string to all four keys; the table form sets each independently.
- `C:/Development/Hub/hub1/lib/models/configuration/attribute_configuration.dart` — the real
  11-key per-attribute field set. Note `icon` is **not** among them; it is collection-level.

### Fixtures
- `tests/schemas/ui/format_table/ui/storage.toml` — `capacity` declares all four format keys; only
  `0.0000` survives (the D-32 consequence, made observable for the first time in this phase).
- `tests/schemas/ui/enum_basic/ui/storage.toml` — `notes` has `label = ""`, pinning D-12's
  declared-blank vs undeclared distinction.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **`UIMetadata` / `UIEnumEntry` already exist** with the right fields — the C++ work is a move.
- **`read_time_series_files`'s decoder** in all four bindings is the same two-parallel-arrays shape
  the vocabulary getter needs, so **no binding learns a new pattern**.
- **`get_scalar_metadata` / `free_scalar_metadata`** fixes the single-record allocation idiom.
- **The four-struct load-time gate** extends to five mechanically; Phase 2 built the mechanism.

### Established Patterns
- Freeze the C header before any binding decoder starts (ROADMAP note) — the five decoders
  parallelize only after it is frozen.
- Generator discipline differs per binding and is non-negotiable (02-CONTEXT D-16): Julia
  regenerated, Dart hand-edited, Python cdef hand-edited, JS symbol table hand-written.
- Dart's suite must clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` (02-CONTEXT D-15) —
  already wired into `test.bat`.

### Integration Points
- `Impl::require_ui_config()` is the single supply point; all three getters read through it.
- `src/database_describe.cpp` is an existing consumer of the same data and must keep its output
  byte-identical — this phase adds getters, it does not touch rendering.

</code_context>

<specifics>
## Specific Ideas

- The user accepted all four recommended options without amendment, and accepted both
  Claude's-call defaults (D-37, D-38) implicitly by not objecting. If either default is wrong,
  D-37 and D-38 are the two cheapest decisions in the phase to flip — both are pre-freeze.

</specifics>

<deferred>
## Deferred Ideas

- **A collection-wide plural getter** (`list_attribute_ui_metadata`) — deferred by D-34. Purely
  additive later. The trigger to revisit: claw's list-then-find idiom making N×M round trips
  measurably slow, or claw's first real integration finding the singular shape awkward.
- **Four-key format access** — foreclosed for this milestone by D-32's ABI-freeze argument. If a
  consumer ever needs `edit` separately from `data`, it is a new struct or a new getter, not a
  field addition.
- **D-17 (Phase 1) remains open:** collapsing `is_foreign_key` + `references_collection` +
  `references_column` into `std::optional<ForeignKeyRef>`. Its stated precondition — "after Phase 2's
  SAFE-01..03 size accessors land" — is **now met**, but it shrinks `quiver_scalar_metadata_t`,
  which D-16 forbids for this milestone. Still its own PR, after v1.
- **claw-side work to consume any of this** (pass `uiConfigDir`, call the new getters) — P-02.
  Belongs in the claw repo, not this roadmap.

### Doc drift found while scouting (fix opportunistically, not in scope)
- `src/c/CLAUDE.md:100-105` claims native `static_assert`s pin `quiver_scalar_metadata_t` at 56 and
  `quiver_group_metadata_t` at 32. **There are none** — the only six live in `src/c/options.cpp:9-14`.
  Those two sizes are enforced solely by the four runtime binding gates. (This is also *why*
  criterion 4's "untouched" guarantee holds: nothing in the native build would fail if they moved.)
- `REQUIREMENTS.md` PARSE-03 calls `UIEnumEntry`'s field `id`; the struct and ROADMAP criterion 1
  call it `code`. **`code` is what every layer should use.**
- ROADMAP criterion 1 reads as one record carrying the vocabulary's entries inline, drifting from
  D-10. Amend to "the two getters together" so it does not read as mandating the inlining D-10
  rejected.
- A zero-byte `enum.toml` and a declared-but-empty vocabulary are **different states**: the
  zero-byte case leaves the map empty so every name is a Pattern 2 miss, while a declared array with
  zero rows is a present key with an empty vector. The `empty_enum` fixture is the zero-byte case,
  so it exercises the throw path only — **nothing in the corpus exercises the empty-list path.**

</deferred>

---

*Phase: 3-Structured Attribute Metadata*
*Context gathered: 2026-09-19*
