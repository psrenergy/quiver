# Phase 1: Sidecar Reader and Attribute Meaning - Context

**Gathered:** 2026-09-20
**Status:** Ready for planning

<domain>
## Phase Boundary

Parse the `ui/` TOML sidecar that sits beside a model's migrations directory, from
`from_migrations` only, and render each scalar attribute's English label, tooltip and enum
code→label list in `describe()` and `describe_collection()` — both of which are produced by the
single shared helper `write_collection_section` (`src/database_describe.cpp:56`). A database with
no `ui/`, or a broken one, must read exactly as it does today.

`summarize_collection`'s integer histogram annotation is **Phase 2** and is not in this phase's
scope, though this phase's grammar is chosen so Phase 2 needs no new vocabulary (see D-14).

Also in scope as a prerequisite for this milestone's first changelog entry: reconciling
`CHANGELOG.md`'s stale `## [0.10.7] — unreleased` heading (D-15).

</domain>

<decisions>
## Implementation Decisions

The render format (D-01..D-07) was designed by a 4-proposal / 3-judge panel run during this
discussion, because the user delegated it with "render that way that make [it] easier for an agent
like you to read". The winning proposal was unanimous (35/36/35 across an LLM-consumer lens, a
hostile-data lens, and a maintainer lens); D-03, D-05, D-07 and the D-04 spelling are grafts the
judges pulled from losing proposals. The two repo facts this phase leans on were independently
verified in the same run (see `<code_context>` → Verified Facts).

### Render Format

- **D-01: Semicolon clause suffix.** The scalar line is built exactly as today —
  `"    - " << name << " (" << TYPE << ")"`, then optional `" PRIMARY KEY"`, then optional
  `" NOT NULL"` — and then **zero to three clauses** are appended before the trailing `"\n"`.
  Each clause is `"; "` (semicolon + space) + a lowercase keyword + a space + the body. Fixed
  order, always:

  1. `label "<text>"`
  2. `enum {<code>: "<text>", <code>: "<text>", …}`
  3. `tooltip "<text>"` — emitted only when the `with_tooltip` bool is true (see D-08)

  Each clause carries its own leading `"; "`, so eliding one is simply not emitting it: there is
  never a dangling separator, never an empty `{}`, never a bare `""`. All 8 present/absent
  combinations fall out of this with no special cases. Nothing else in the report changes — no new
  indentation level, no new line, no change to the Vectors/Sets/Time Series sections.

  Rationale for choosing it over the three alternatives: it **extends the grammar the report
  already has** (`summarize_collection` already writes `; values {0: 1, 1: 1}`), so the two reports
  read as one format rather than two bolted together; keyword anchors let a reader segment fields
  without counting positions; and it was the only proposal to score ≥9 on all three judges'
  readability axis while taking zero hard-constraint violations.

  Worked examples — `describe()`:
  ```
      - id (INTEGER) PRIMARY KEY
      - priority (INTEGER)
      - hm3_initial (REAL); label "Initial Storage (hm³)"
      - initial_volume_type (INTEGER) NOT NULL; label "Initial Volume Unit"; enum {0: "Per Unit", 2: "Volume"}
      - reservoir_type (INTEGER); enum {0: "Reservoir", 1: "Run of river"}
  ```
  `describe_collection()` — same lines, plus the trailing tooltip clause:
  ```
      - initial_volume_type (INTEGER) NOT NULL; label "Initial Volume Unit"; enum {0: "Per Unit", 2: "Volume"}; tooltip "Unit in which the initial volume of the reservoir is given."
      - discount_rate (REAL); tooltip "Annual discount rate applied to future operating costs, in %."
      - reservoir_type (INTEGER); enum {0: "Reservoir", 1: "Run of river"}; tooltip "Operating mode of the plant."
  ```
  — **Reversibility:** costly — undoing it rewrites every expected-string assertion in the new C++
  test file. It breaks no published contract (the reports are opaque `std::string` through the C
  API), so the cost is test churn, not a migration.

- **D-02: Every free-text value is wrapped in ASCII double quotes, with `\` written `\\` and `"`
  written `\"`. No other escaping.** This is what makes every separator character unambiguous:
  arbitrary user text only ever appears between an unescaped opening and closing quote. Real corpus
  labels and tooltips contain commas, colons, semicolons, parentheses, brackets and braces — the
  hostile-data judge confirmed the unquoted alternatives become unparseable against that text. Enum
  labels are quoted too. Non-ASCII UTF-8 passes through byte-for-byte; **every separator character
  this format introduces is ASCII** (`;` `{` `}` `:` `,` `"`), so the report stays pure ASCII apart
  from user data, as it is today.

- **D-03: Whitespace normalization happens in the renderer, not on trust from the loader.** Map
  `\r`, `\n` and `\t` to a space, collapse runs of spaces to one, trim; an empty result is treated
  as **absent**, not emitted as `""`. The loader already collapses TOML newlines (137 of 1751
  corpus strings carry a literal one, e.g. `"Mean\nProduction\nFactor"`), but doing it again in the
  renderer is three lines and makes one-line-per-attribute a property of the writer rather than an
  upstream contract — i.e. no sidecar value can forge a report line regardless of loader behaviour.
  All three judges grafted this.

- **D-04: Redundant-label suppression via `squash`.** Define `squash(s)` = ASCII-lowercase, then
  keep only `a`-`z` and `0`-`9`. If `squash(label) == squash(name)`, the label clause is **not**
  emitted — `initial_volume_type` and `"Initial Volume Type"` both squash to `initialvolumetype`.
  Most of the 713 present labels are the attribute name title-cased, so this removes the single
  largest block of redundant tokens and loses nothing an LLM cannot reconstruct from the name.
  **Spell it as an explicit ASCII test (`c >= 'A' && c <= 'Z' ? c + 32 : c`, keep `a`-`z`/`0`-`9`),
  never `std::tolower(char)`** — passing a negative `char` to `std::tolower` is UB and 344 corpus
  strings are non-ASCII UTF-8. `squash` therefore drops non-ASCII bytes, which biases toward
  *printing* a label (`"Volume Útil"` vs `volume_util` squash differently and the label is kept);
  that is the safe direction and is deliberate — document it so it is not later "fixed".

- **D-05: Redundant-tooltip suppression, same mechanism.** Omit the tooltip clause when
  `squash(tooltip) == squash(name)` **or** `squash(tooltip) == squash(label)` — the latter compared
  against the raw sidecar label *even when the label clause was itself suppressed by D-04*.
  Tooltips average 49 chars and are present on 507 of 736 attributes, so a tooltip that merely
  restates the display name is the largest remaining redundancy after D-04. All three judges
  grafted this; the winning proposal had specified suppression for labels only.

- **D-06: The enum clause is never suppressed.** It is the one field that cannot be re-derived from
  the column name. Entries are emitted in **ascending code order**, which `std::map<int64_t,
  std::string>` iteration already gives — no sorting code. Codes print verbatim, so gapped
  (`{0: …, 2: …}`) and 1-based (`{1: …}`) vocabularies both render their real codes with no
  positional assumption. An entry whose label normalizes to empty under D-03 is dropped; if no
  entry survives, the whole clause is omitted. An empty map counts as absent.

- **D-07: Tooltip is last, which makes the `describe()` line a strict character-for-character
  prefix of the `describe_collection()` line — and that must be pinned by a test.** Dropping the
  tooltip only ever removes the final clause, so the two reports cannot drift by construction. All
  three judges noted the winning proposal claimed this property and never checked it: add one test
  asserting, for every scalar, that the `describe()` line is a strict prefix of the
  `describe_collection()` line. Three lines, and it is the cheapest possible anti-drift guarantee.

### Report Scope

- **D-08: The tooltip renders only in `describe_collection()`, never in whole-database
  `describe()`.** User's explicit decision, to bound whole-DB report growth (a real model, GNoMo,
  carries 236 attributes; PROJECT.md estimates ~15–20 KB of growth). `describe()` shows label +
  enum labels; `describe_collection()` shows label + enum labels + tooltip.

  **Consequence the ROADMAP's "one injection site" framing did not anticipate:**
  `write_collection_section` is shared by both reports, so its signature takes **two** new
  parameters, not one — the nullable UI-store pointer *and* a `bool with_tooltip`:
  ```cpp
  void write_collection_section(std::ostream& out, const Schema& schema,
                                const std::string& collection, int64_t count,
                                const UiConfig* ui, bool with_tooltip);
  ```
  Still one call site each — `describe()` at `src/database_describe.cpp:104` passes `false`,
  `describe_collection()` at `:114` passes `true` — and still no refactor. Recorded here so the
  planner does not treat the second parameter as a deviation from the roadmap.

  **Escape hatch, if whole-DB growth still bites:** dropping the *label* clause from `describe()`
  too is one more bool on the same function, not a re-specification of the format. Do not build it
  now.

### Failure Handling

- **D-09: Per-file try/catch nested inside one outer try/catch.** User's explicit decision. The
  outer catch guarantees the posture the project already settled — the UI load never fails
  `from_migrations`, it warns via `logger->warn` and yields an empty map (following
  `src/database.cpp:80-86`, and explicitly **not** copying `src/binary/binary_metadata.cpp`'s
  throwing posture). The inner per-file catch upgrades the *granularity*: one malformed
  `ui/*.toml` costs that collection its metadata, not all 67 collections'. Same cost, strictly
  better degradation. The corpus has 0 parse failures today, so this is insurance rather than an
  observed need. The outer catch still covers everything the per-file loop cannot — directory
  iteration itself, and `enum.toml`.

### Code Placement

- **D-10: New internal component at `src/ui_config.h` + `src/ui_config.cpp`**, modelled directly on
  `src/csv_read.h`/`.cpp`, which is this repo's established precedent for an internal `src/`-local
  component: no `include/quiver/` counterpart, no `QUIVER_API`, no C API symbol, no FFI binding.
  The header exposes plain `std` types only; **toml++ is included solely in the `.cpp`**, so
  `database_impl.h` and `database_describe.cpp` never see it. `ui_config.cpp` goes in
  `QUIVER_SOURCES` (`src/CMakeLists.txt`) — mandatory, since tomlplusplus is linked **PRIVATE** on
  the `quiver` target and therefore does not propagate to `quiver_c`, `quiver_cli` or
  `quiver_tests`.

- **D-11: One plain, non-`mutable` member on `Database::Impl`** (`src/database_impl.h:59`),
  populated in `from_migrations` (`src/database.cpp:242-257`) after `migrate_up` returns. It does
  not need `mutable`: the three describe readers are `const` but reach `Impl` through `impl_->`,
  and constness does not propagate through `unique_ptr::operator->`. It must **not** be hooked onto
  `load_schema_metadata` — `migrate_up` early-returns at `src/database.cpp:398-401` and `:406-409`
  before reaching it, which is exactly the path an already-migrated study takes on every open.

- **D-12: Tests go in a new `tests/test_database_ui_metadata.cpp`**, registered in
  `tests/CMakeLists.txt`, driving the parser **through the public API only** — the precedent is
  `tests/test_binary_metadata.cpp` (819 lines, zero toml++ includes), and it is forced by the same
  PRIVATE-linkage fact as D-10. A new file rather than extending `tests/test_database_describe.cpp`
  because every test here needs a temp-dir `migrations/` + `ui/` fixture pair, which that file's
  `open()` helper (`from_schema`-only) does not provide. Fixture idiom:
  `tests/test_migrations.cpp:12-32` and `:194-199`. **Nothing is committed under
  `tests/schemas/ui/`** — such a directory would become a live sibling of `tests/schemas/migrations`
  for every `from_migrations` call across six suites plus the recursive-copy Lua migrations test.

### Changelog

- **D-15: Date the stale 0.10.7 section; do not rename it.** The ROADMAP's wording ("reconcile
  `CHANGELOG.md`'s `## [0.10.7] — unreleased` heading and its compare link to `0.10.8`") reads as a
  rename, and a rename would be wrong — it would relabel already-released work. Verified: tag
  `v0.10.7` exists at `7bd1f16`, which is the very commit that wrote the entries under that
  heading, and no commit has touched `CHANGELOG.md` since. The file is **missing a section**, not
  misnumbered. Three edits, matching the file's own conventions exactly:
  1. `## [0.10.7] — unreleased` → `## [0.10.7] — 2026-09-17` (em dash U+2014 with a space either
     side, matching `## [0.10.6] — 2026-09-11` at `CHANGELOG.md:111`)
  2. `[0.10.7]: https://github.com/psrenergy/quiver/compare/v0.10.6...HEAD` →
     `...compare/v0.10.6...v0.10.7` (matching the released-link form used by `[0.10.6]`)
  3. Add `## [0.10.8] — unreleased` above the 0.10.7 section, and
     `[0.10.8]: https://github.com/psrenergy/quiver/compare/v0.10.7...HEAD` at the top of the
     newest-first link block

  `v0.10.7` is a **lightweight** tag, so its date is the commit date, 2026-09-17.

### Claude's Discretion

- The precise `UiConfig` type/field spelling and the internal map shape. Constraint from PROJECT.md:
  the enum map is a `std::map<int64_t, std::string>` named `enum_labels`, not a new type and not
  `details`.
- Whether the collection→attribute store is one nested map or two; whether `ui_config.cpp` exposes a
  free function or a small struct with an accessor.
- The one accessor that reads a localizable value (string used as-is, table read at `en`) serves all
  three fields including each `enum.toml` entry's own `label` — its exact signature is open.
- Test case names and the internal shape of the temp-dir fixture builder.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Milestone planning (read first)
- `.planning/PROJECT.md` — scope, the full Key Decisions table, parser tolerances measured against
  the real corpus, the two resolution traps, the output budget, and the accepted `HasCommitment`
  risk
- `.planning/REQUIREMENTS.md` — READ-01..05, RENDER-01, RENDER-03, SAFE-01, SAFE-02 are this
  phase's nine; the Out of Scope table is binding
- `.planning/ROADMAP.md` § Phase 1 — goal, six success criteria, and the non-negotiable
  implementation facts
- `.planning/research/SUMMARY.md` — the measured corpus numbers and the three findings that
  changed the design

### Source — injection and construction sites
- `src/database_describe.cpp` — `write_collection_section` at `:56`, its scalar loop at `:62-72`
  (append after the flags at `:68-70`, before the `"\n"` at `:71`); callers at `:104`
  (`describe()`) and `:114` (`describe_collection()`); `summarize_collection` at `:118-183` is
  **Phase 2**, do not touch it here
- `src/database.cpp` — `from_migrations` at `:242-257` (the UI read goes here, after `migrate_up`);
  the `migrate_up` early returns at `:398-401` and `:406-409` that make schema-load hooking wrong;
  the warn-and-continue logging precedent at `:80-86`
- `src/database_impl.h` — `Database::Impl` at `:59`, where the UI store member lands
- `src/CMakeLists.txt` — `QUIVER_SOURCES` at `:2-39`; tomlplusplus is PRIVATE at `:81`

### Source — precedents to follow (and one not to)
- `src/csv_read.h` / `src/csv_read.cpp` — the template for an internal `src/`-local component with
  no public header, no C API and no binding; its header comment states the convention explicitly
- `src/binary/binary_metadata.cpp` — the only existing toml++ sidecar reader, and an **anti**-model
  for error handling: it throws on `toml::parse_error` and on `std::bad_optional_access` from a
  bare `.value()`, with no test for either
- `src/lua_runner.cpp` → `resolve_sandboxed_path` — already uses the `fs::weakly_canonical` call
  this phase needs

### Tests
- `tests/test_migrations.cpp:12-32`, `:194-199` — the temp-dir fixture idiom to copy
- `tests/test_binary_metadata.cpp` — precedent for exercising a toml++ parser through the public
  API only
- `tests/test_database_lifecycle.cpp:383-500` — the existing describe assertions that must keep
  passing unmodified (success criterion 6); `:463-485` pins the literal prefix `"    - <name> "`
  including its trailing space
- `tests/test_database_describe.cpp` — the other existing describe suite; `from_schema`-only, so
  invisible to this feature
- `tests/CLAUDE.md` — suite conventions
- `tests/CMakeLists.txt` — where the new test file is registered

### Corpus (read-only ground truth, outside this repo)
- `C:/Development/HydroThermalDispatch/HydroThermalDispatch.jl/database/ui/` — representative
  sidecar: `hydro_plant.toml` shows `id`/`label.en`/`tooltip.en`/`type`/`enum` and the colliding
  `[[attribute_group]]`; `enum.toml` shows the `[[vocab]]` + `id` + `label.en` shape and the gapped
  `initial_volume_type` `[0, 2]`
- `C:/Development/HydroThermalDispatch/HydroThermalDispatch.jl/src/collections/hydro_plant.jl` —
  the `HasCommitment` declaration that `enum.toml` contradicts (the accepted risk)

### Repo conventions
- `CLAUDE.md` (root) — error message patterns, changelog rule, self-updating rule
- `src/CLAUDE.md` — core internals; update it with this change
- `CHANGELOG.md` — the file D-15 reconciles

</canonical_refs>

<code_context>
## Existing Code Insights

### Verified Facts (checked during this discussion, not assumed)

- **SAFE-01 is free, not engineered — CONFIRMED exhaustively.** No test anywhere in the repo calls
  `describe` / `describe_collection` / `summarize_collection` on a database built by
  `from_migrations`. Every describe/summarize test in all six suites routes through
  `from_schema` / `fromSchema`: `tests/test_database_describe.cpp:11`,
  `tests/test_database_lifecycle.cpp:385/397/419/440/465/492`,
  `tests/test_c_api_database_lifecycle.cpp:510`, `tests/test_c_api_database_metadata.cpp:164`,
  `tests/test_lua_runner_describe.cpp:5`, `tests/test_lua_runner_query.cpp:138`,
  `bindings/julia/test/test_database_describe.jl:12`,
  `bindings/julia/test/test_database_lifecycle.jl:116`,
  `bindings/dart/test/describe_test.dart:11`,
  `bindings/dart/test/database_lifecycle_test.dart:222`,
  `bindings/python/tests/conftest.py:50`, `bindings/python/tests/test_database_metadata.py:208`,
  `bindings/js/test/database-describe.test.ts:22`. Every `from_migrations` call site in tests was
  inventoried and each is describe-free. **Watch item:** `tests/test_database_lifecycle.cpp` mixes
  both constructors in one file, so a future author adding a describe assertion beside the
  migration tests would be the first to hit the new sidecar path.
- **The CHANGELOG section is unmixed — CONFIRMED.** `git log --oneline v0.10.7..HEAD --
  CHANGELOG.md` is empty and `git log --oneline v0.10.6..v0.10.7 -- CHANGELOG.md` yields only
  `7bd1f16`, so the 0.10.7 section is exactly one tagged commit's work and needs no splitting.
  `CMakeLists.txt:4` is already at `0.10.8` (bumped in `6d2881e`, after the tag).

### Reusable Assets
- `src/csv_read.h`/`.cpp` — structural template for D-10 (internal component, dependency confined
  to the `.cpp`, header comment documenting why it has no public counterpart)
- `src/lua_runner.cpp`'s `resolve_sandboxed_path` — the `fs::weakly_canonical` idiom for D-16
- `tests/test_migrations.cpp:12-32`, `:194-199` — temp-dir migrations-tree builder to extend with a
  sibling `ui/`
- `src/database.cpp:80-86` — the warn-and-continue logging shape D-09 follows
- `std::map<int64_t, std::string>`'s iteration order — already ascending, so D-06 needs no sort

### Established Patterns
- **tomlplusplus is PRIVATE on `quiver`** (`src/CMakeLists.txt:81`) and does not propagate. This is
  the single constraint that forces D-10 (parser is a `.cpp` in `QUIVER_SOURCES`) and D-12 (tests
  drive it through the public API).
- **Error messages follow three patterns** (root `CLAUDE.md`), but this phase throws nothing — the
  whole UI path is warn-and-degrade per D-09.
- **The UI is a partial overlay on the schema, never the reverse.** Iterate `table_def->column_order`
  and look the UI up by name; 15 of 736 corpus attributes name a column that does not exist.
- **Collection lookup is case-sensitive** — `Schema` keys tables in a `std::map`
  (`include/quiver/schema.h:104`) while SQLite matches names case-insensitively, so a ui `id` whose
  casing differs from the `CREATE TABLE` spelling matches nothing, silently. Accepted, no
  diagnostic.

### Integration Points
- `Database::from_migrations` (`src/database.cpp:242-257`) — the only construction site; reachable
  as `db.impl_->` because `from_migrations` is a static member holding the `Database` by value
- `Database::Impl` (`src/database_impl.h:59`) — the store's home
- `write_collection_section` (`src/database_describe.cpp:56`) and its two callers at `:104`/`:114`
- `src/CMakeLists.txt` `QUIVER_SOURCES` and `tests/CMakeLists.txt`

### Remaining Constraints (unchanged, restated because they gate acceptance)
- **D-16: path resolution is `fs::weakly_canonical(migrations_path).parent_path() / "ui"`.** Raw
  `parent_path()` was compiled and proven wrong: a trailing separator yields `<migrations>/ui`
  (never exists) and a bare relative path yields `./ui` against the process CWD. Trailing-slash
  migrations paths already appear in this repo's tests
  (`tests/test_c_api_database_lifecycle.cpp:229,242`, `tests/test_database_lifecycle.cpp:265`).
- **D-17: collection files self-select by shape** — a non-recursive scan of `ui/*.toml` keeping
  files carrying both a top-level string `id` and an `attribute` array. No `main.toml` parsing, no
  filename→table mapping. True for 67/67 collection files and false for every `main.toml`,
  `enum.toml` and theme.
- **D-18: read `[[attribute]]` only, never `[[attribute_group]]`** — 15 real files reuse one id for
  both, and merging silently overwrites a label.
- **D-19: the enum join key is the attribute's `enum` value, not its `id`** — 46 attributes share
  the `bool` vocabulary.
- **D-20: hidden attributes still render** — 360 of 736 carry `hide = true`; `describe` describes
  the schema, not the UI.
- The injected text must never contain `Vectors:`, `Sets:`, `Time Series:` or `values {`, and must
  never disturb the pinned `"    - <name> "` prefix. D-01 satisfies all four by construction: it
  emits `enum {`, never `values {`, and appends strictly after the flags.

</code_context>

<specifics>
## Specific Ideas

- The user's one instruction on format was "render that way that make [it] easier for an agent like
  you to read" — the primary consumer is an LLM, not a human, and readability was judged on that
  basis (can a model segment name/type/flags/label/tooltip/enum unambiguously at speed inside a
  20 KB report), not on terminal aesthetics. A multi-line layout was designed and fairly judged, and
  lost: it took a hard-constraint violation for moving text after the header's newline, and grew
  GNoMo's reports by +270 / +433 lines.
- The escape hatch for whole-DB size, if D-08 turns out not to be enough, is dropping the *label*
  clause from `describe()` — one more bool on the same function. Explicitly not built now.

</specifics>

<deferred>
## Deferred Ideas

- **Phase 2 histogram spelling (not this phase, but decided now so the grammars cannot diverge):**
  emit the enum as its own clause on the `summarize_collection` line, reusing D-01's grammar
  verbatim and leaving the existing histogram untouched —
  `    - priority: 2 non-null, 1 null; values {0: 8, 1: 4}; enum {0: "No", 1: "Yes"}`. This was the
  maintainer judge's explicit recommendation over both interleaved alternatives the designers
  proposed (`values {0: 8 "Per Unit"}` juxtaposes count and label with no separator;
  `values {0=No: 8}` packs two key-value glyphs into nine characters). Zero new vocabulary, and the
  code→label mapping has exactly one spelling in the codebase.
- **Latent defect in the existing negative assertions, deliberately NOT fixed in this phase.**
  `tests/test_database_lifecycle.cpp` (~`:403-410`, `:424-431`, `:445-452`, `:494-500`) matches bare
  `Vectors:` / `Sets:` / `Time Series:` anywhere in the output rather than anchoring on
  `"\n  Vectors:\n"`. All three judges flagged it. It **cannot** fire in this phase — those tests
  use `from_schema` and no UI text can reach them — and ROADMAP success criterion 6 requires those
  assertions to pass *unmodified*, so anchoring them here would contradict the criterion for no
  gain. Recorded so a future phase that feeds sidecar prose into those tests fixes it first.
- `validate_ui_config()` (VALID-01) and the `@enumx` cross-check (VALID-02) — deferred at
  requirements time, with the `HasCommitment` inversion accepted on the table. Rendering an
  unvalidated `enum.toml` hands 2-of-8 known-inverted mechanically-checkable attributes to an LLM
  as fact.
- `unit` / `format` rendering (META-01), a structured getter through the C API and bindings
  (META-02), collection-level metadata (META-03) — all deferred at requirements time.
- Locales other than English, a `hide` filter, `[[attribute_group]]`, `main.toml` parsing,
  `[[card]]`/`themes/` — all in the REQUIREMENTS.md Out of Scope table, none revisited here.

</deferred>

---

*Phase: 1-Sidecar Reader and Attribute Meaning*
*Context gathered: 2026-09-20*
