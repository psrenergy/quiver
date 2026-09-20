# Phase 2: Enum Labels on the Value Histogram - Context

**Gathered:** 2026-09-20
**Status:** Ready for planning
**Mode:** Smart discuss (autonomous) — four grey areas proposed, all accepted as recommended

<domain>
## Phase Boundary

`summarize_collection()`'s integer value distribution renders each observed code with that code's
English enum label beside it, using the `UiMetadata` map Phase 1 already builds in
`from_migrations`. Scope is the histogram clause only — the second and last injection site in this
milestone (`src/database_describe.cpp`, inside `summarize_collection`, at the `; values {` emission
loop). `write_collection_section` is Phase 1's seam and is not touched.

Out of this phase: any change to `ScalarMetadata` / `ColumnDefinition` / the C API, any new
structured getter, and any `; label` / `; tooltip` clause on summarize's scalar lines.

</domain>

<decisions>
## Implementation Decisions

### The annotation grammar

- **D2-01: The label rides on the key, not the count** — `code SP "Label": count`. SC-1 says
  "beside the code", and the label names the code rather than its row count; `0 "User Defined
  Forecast"` is a single noun phrase nothing can rebind. Rendered forms:
  - both codes labelled: `; values {0 "User Defined Forecast": 7, 1 "Model": 5}`
  - code 1 uncovered: `; values {0 "User Defined Forecast": 7, 1: 5}`
  - no vocabulary at all: `; values {0: 7, 1: 5}` (byte-identical to today)
  Rejected: trailing `0: 7 "Label"` (keeps `code: count` contiguous, but contradicts SC-1's
  wording and sits the label adjacent to the count); parenthesized `0 ("Label"): 7` (new token
  shape, two extra bytes per entry, and parens occur inside real corpus labels).
- **D2-02: Exactly one U+0020 between code and label, and nothing between the closing quote and
  the `:`.** No new delimiter enters the grammar — D-02's quotes already bound the text. Rejected:
  `=` and parentheses, both of which invent a token Phase 1 never used.
- **D2-03: Escaping and normalization reuse Phase 1 verbatim** — `normalize_ui_text` (D-03) then
  `quote_ui_text` (D-02). One escaping rule across both reports, so a parser that handles
  `; enum {...}` handles this. Explicitly rejected: additionally stripping `,` `:` `{` `}` from
  labels (D-02 exists precisely so separators need no blocklist, and a second rule can drift from
  the first), and emitting unquoted when a label happens to contain no separator (two forms for
  one thing).
- **D2-04: Entry order is unchanged.** The existing `ORDER BY <col>` is already ascending code,
  which agrees with D-06's ascending-code order for free. No sort is added.

### Uncovered codes and suppression

- **D2-05: An observed code absent from `enum_labels` renders bare, byte-identical to today.** The
  same absent-label path covers a null `UiAttribute*`, an empty vocabulary and a gapped one — which
  is why SC-2's "per-code, not per-column" needs no separate branch. Rejected: `0 "?": 7` /
  `0 "<unknown>": 7`, which break SC-2's byte-identity and hand an agent a string it can mistake
  for a real label.
- **D2-06 (new locked decision, recorded as D-09 in the project decision log): a label that
  normalizes to empty drops the *annotation*, never the entry.** This is a deliberate divergence
  from D-06, which drops the whole entry in the `enum {}` clause. There the entry *is* vocabulary;
  here the entry is an observed row count, and dropping it destroys data. Record it as its own
  numbered decision so a later reader does not read it as an inconsistency with D-06. Rejected:
  following D-06 literally; emitting `0 "": 7` (D-03 forbids an empty quote pair).
- **D2-07: A vocabulary code with zero observed rows is not printed.** `summarize_collection`
  reports what is in the table; a zero-count entry invents data and stops `values {}` being a
  histogram. The full vocabulary, unobserved codes included, is `describe_collection`'s
  `; enum {...}` — same token grammar, different scope.
- **D2-08: D-04's restates-the-name suppression does not apply here.** D-04 compares a label to the
  *attribute name*, and there is no attribute name at entry level; D-06 already establishes that an
  enum label is never suppressed for redundancy. So `1 "1": 5` stands, quoted and parseable.

### Honesty records (two corrected roadmap premises)

- **D2-09: Success criterion 3 is vacuously satisfied and must not be cited as injection
  evidence.** Verified during discuss: `capture_describe()` is `return db.describe();`
  (`tests/test_database_lifecycle.cpp:392`) and all four fixtures are `from_schema(":memory:")`,
  whose `Impl::ui_metadata` is never populated — `ui_metadata` is loaded only by `from_migrations`.
  A summarize-only diff cannot reach those assertions: wrong constructor *and* wrong method. They
  will still pass unmodified, which satisfies SC-3 literally, but they prove nothing about this
  phase.
- **D2-10: Substring injection is NOT prevented, and the phase record says so.** `quote_ui_text`
  escapes only `\` and `"`; a corpus label spelled `Sets: 3` or `values {see manual}` renders
  verbatim *inside* the quotes. That is Phase 1's accepted D-02 posture — a **parse**-level
  guarantee, not substring immunity — and it is unchanged here. What Phase 2 widens: summarize also
  emits `  Vectors:` / `  Sets:` / `  Time Series:` headers, so a future naive header-count test
  written against `summarize_collection` on a `from_migrations` database would be breakable.
  Recorded as a known ceiling; do not add a substring blocklist to the renderer.

### Scope and housekeeping

- **D2-11: No `; label` / `; tooltip` on summarize's scalar lines.** RENDER-02 is scoped to the
  histogram. Calling `write_ui_clauses` there would fire for TEXT, REAL and primary-key scalars as
  well, lengthening every line of the statistics report to repeat `describe_collection`.
- **D2-12: The `find()` lookup lives inside the cardinality branch** — immediately before
  `out << "; values {"`, not at the top of the per-scalar loop. A collection with 200 TEXT scalars
  otherwise pays 200 pointless two-level map lookups. The read sits next to its use.
- **D2-13: `kMaxDistributionCardinality` (64) is unchanged.** It already suppresses the whole
  `; values {}` clause above 64 distinct codes, so the richest enum column renders as a bare
  `N non-null, M null` line with no labels. Recorded as a known ceiling;
  `describe_collection`'s `; enum {...}` is the fallback. Also recorded: 64 labelled entries is a
  ~2 KB single line — fine for an LLM reader, rough for a terminal. No truncation is proposed,
  because truncation needs its own ellipsis convention.
- **D2-14: Two documentation edits are part of this phase.** `CHANGELOG.md` — its
  `0.10.8 — unreleased` entry currently ends "and `summarize_collection()` does not yet render this
  metadata", which Phase 2 makes false. And the root `CLAUDE.md` Core API bullet for
  `summarize_collection`. Per the project's Self-Updating rule, also keep `src/CLAUDE.md` current.

### Claude's Discretion

- Exact test name and assertion spelling, within the coverage contract in `<specifics>`.
- Comment wording at the injection site, provided it records D2-06's divergence from D-06.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets

- `normalize_ui_text()` and `quote_ui_text()` already sit in `src/database_describe.cpp`'s
  anonymous namespace (Phase 1). No new helper is needed — the annotation is used once, so no
  extraction is warranted.
- `UiAttribute::enum_labels` is `std::map<int64_t, std::string>` (`src/ui_metadata.h`);
  `query_int_rows` yields `std::vector<std::vector<int64_t>>` (`src/database_impl.h:27`). The
  histogram's code and the map's key are both `int64_t` — exact match, no conversion.
- `impl_->ui_metadata.find(collection, attribute)` returns `const UiAttribute*`, `nullptr` when
  undescribed — the same no-op-by-construction path Phase 1 relies on.

### Established Patterns

- Phase 1's clause grammar: `"; keyword body"`, each clause carrying its own leading `"; "` so
  eliding one leaves no dangling separator (D-01).
- `std::map` iterates in ascending key order, so neither report sorts explicitly.
- Tests drive behaviour through the public `Database` API only (D-12); fixtures build
  `migrations/` and `ui/` in per-test temp dirs, never under `tests/schemas/ui/`.

### Integration Points

- `src/database_describe.cpp`, inside `summarize_collection` — the `; values {` emission loop.
  This is the only code change in the phase; there is no signature change, no new file, no header
  and no CMake edit.
- `tests/test_database_ui_metadata.cpp` — extended with the new test. **That file contains zero
  `create_element` calls today**, so nothing in it has ever emitted a `values {}` clause; the new
  test must create elements to produce a histogram at all.
- `tests/test_database_describe.cpp:80` (`SummarizeScalarsAndGroups`) cannot host this test — it
  opens via `from_schema` and can never see a sidecar.

</code_context>

<specifics>
## Specific Ideas

The test this phase owes — one `TEST_F` in `UiTempTreeFixture` (`tests/test_database_ui_metadata.cpp`):

- a migration with a non-primary-key INTEGER column;
- an `enum.toml` covering code `0` and an unobserved code `2`, but deliberately **not** code `1`;
- `create_element` calls producing rows for codes `0` and `1` (counts 2 and 1);
- one `EXPECT_TRUE` on the substring `values {0 "<label>": 2, 1: 1}` — this single string pins
  SC-1, the label's position, the quoting, and SC-2's bare-code fallback at once;
- one `EXPECT_FALSE` that code `2` appears — pins D2-07, which nothing asserts today.

`expect_reports_match` (`tests/test_database_ui_metadata.cpp:174-179`) stays unmodified: it asserts
`summarize_collection` equality between a sidecar database and a mirror with `ui/` removed, and all
four of its callers use malformed or empty sidecars *and* create no elements, so the histogram
clause never opens. It remains the SAFE-02 regression anchor. Do not add a valid-sidecar caller to
it — that case belongs in the new test.

</specifics>

<deferred>
## Deferred Ideas

- **Unobserved vocabulary codes in `summarize_collection`.** Legal-but-unused codes matter to an
  agent about to write an INSERT, but printing them makes `values {}` no longer a histogram.
  Already served by `describe_collection`'s `; enum {...}`.
- **`ui_metadata` is loaded only by `from_migrations`** (`src/database.cpp`), so
  `Database::open("study.db").summarize_collection(...)` — plausibly the most common way an agent
  inspects a study it was handed — still shows bare codes. This is Phase 1's boundary inherited,
  not a Phase 2 regression. It must **not** be fixed by hooking the load onto
  `load_schema_metadata` / `require_schema`: `migrate_up` early-returns before it on the
  open-an-existing-study path, which is the documented reason the load lives in `from_migrations`.
- **Folding `label` / `tooltip` / `enum_labels` onto `ScalarMetadata`** (META-02), and merging
  `ColumnDefinition` with `ScalarMetadata`. Both were considered and explicitly rejected for this
  milestone on 2026-09-20 — see STATE.md Accumulated Context → Decisions.
- **Raising `kMaxDistributionCardinality` for enum-bound columns**, and truncating very long
  `values {}` lines. Both need their own rationale (query cost, ellipsis convention).

</deferred>
