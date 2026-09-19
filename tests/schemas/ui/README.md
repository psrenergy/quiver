# `tests/schemas/ui/` — UI sidecar fixture corpus

One directory per parser tolerance. Every directory holds `schema.sql` (the SQL schema the TOML
sidecar describes) plus a `ui/` subdirectory (the hand-written PSR-shaped sidecar), except
`no_ui_dir`, which deliberately has no `ui/` at all, and `no_main_toml`, whose `ui/` exists but
deliberately has no `main.toml` inside it. Every file is a hand-written miniature — none
reproduces a real model repo wholesale — so a failing fixture names the single broken rule
(D-27, `.planning/phases/01-enum-labels-in-describe/01-CONTEXT.md`).

Authored across two plans: `enum_basic`, `malformed`, `no_ui_dir` by 01-01 (the tracer slice);
the next eight by 01-02; `no_main_toml` added by the WR-01 review fix (a `ui/` directory missing
`main.toml` must be treated as malformed, not as a valid empty sidecar). This file is mirrored by
`tests/test_database_ui_corpus.cpp`'s `ReadmeNamesEveryFixture` (every directory below must
appear here) and `FixtureLiteralsArePinned` (the `## Rendered literals` table below must match
the fixture files on disk byte-for-byte).

## Directory → rule map

| Directory | Rule(s) pinned | Requirement(s) |
|-----------|-----------------|----------------|
| `enum_basic` | The tracer slice's canonical end-to-end shape: a bare-string `bool` vocabulary bound to one attribute, plus `unit`/`hide`/empty-label scalar lines | DESC-01, DESC-04, PARSE-02 |
| `malformed` | A single broken collection file fails the whole config — nothing partial publishes, even with a valid `enum.toml` beside it | PARSE-12, DESC-05 |
| `no_ui_dir` | No `ui/` directory at all — `describe()`/`describe_collection()`/`summarize_collection()` stay byte-identical to the no-sidecar baseline | DESC-05, PARSE-11 |
| `no_main_toml` | A `ui/` directory that exists and holds `enum.toml` and a collection file, but no `main.toml` — treated as malformed (warns, publishes nothing), not as a valid empty sidecar | WR-01, D-24, D-25 |
| `bess_like` | CORPUS-01 (BESSOperation `storage.toml`): all-bare-string vocabulary labels; gapped, non-zero-based, negative and int64-max codes; `degradation` as both an `[[attribute]]` id and an `[[attribute_group]]` id with **different** labels; interleaved `[[attribute]]`/`[[attribute_group]]` blocks | CORPUS-01, PARSE-03, PARSE-07, PARSE-08 |
| `foresight_like` | CORPUS-01 (Foresight `enum.toml`): mixed dotted (`label.en`/`label.es`/`label.pt`) and bare-string entries within one vocabulary; an `es`+`pt`-only entry (no `en`) exercising the first-key-in-map-order fallback leg; two labels carrying non-ASCII UTF-8 bytes that both resolve at locale `en` (the corpus's encoding fixture). The vocabulary is bound with `enum = "model"` on `economic_driver.toml`'s `forecast_model` attribute — without that binding nothing renders it. | CORPUS-01, PARSE-04 |
| `htd_like` | CORPUS-01 (HydroThermalDispatch `hydro_plant.toml`): PascalCase `id` over a snake_case filename, a plain `[[attribute]] id = "date_time"` block, a `bool` vocabulary spelled `Disable`/`Enable` (deliberately different from `enum_basic`'s `Disabled`/`Enabled`); `thermal_plant.toml` is listed in `main.collections` but declares no top-level `id` — skipped with a debug log, not fatal | CORPUS-01, PARSE-10 |
| `no_enum` | No `enum.toml` and no `themes/` at all — an attribute bound to a vocabulary that does not exist anywhere must not resolve and must not throw | PARSE-09 |
| `empty_enum` | `enum.toml` exists and is exactly zero bytes — same non-resolving, non-throwing behavior as `no_enum`, the other half of PARSE-09 | PARSE-09 |
| `unknown_keys` | Unknown keys at all three levels (main, collection, attribute — including the real GNoMo `conditions` key and Hub's unused `tab`/`type`/`read_only`/`enabled_if`) are ignored; every *known* key in the same file (`label`, `unit`) still reads correctly | PARSE-05 |
| `format_table` | `format`'s 4-key table form (`element_view`/`collection_view`/`edit`/`data`, each independent), the plain-string form (one value fans out to all four), a table declaring only `data`, and a collection with zero `[[attribute]]` blocks | PARSE-06 |
| `orphan_collection` | A fully-formed `agent.toml` collection file that `main.collections` does not list is never loaded — reproduces SCE's real orphan `agent.toml` | PARSE-01 |

## Documented non-requirements

- **Duplicate vocabulary id within one `enum.toml` entry.** Hub's `EnumerationConfiguration.fromTOML`
  throws on a duplicate `id` within one vocabulary. PARSE-03 is silent about it, and no real corpus
  file read for this milestone (BESSOperation, Foresight, HydroThermalDispatch, SCE, GNoMo,
  CarbSteeler) exercises it. Quiver's behaviour: every declared entry is kept in declaration order,
  and the code→label lookup takes the first match — recorded here so the next reader does not
  mistake the absence of a fixture for an oversight (01-CONTEXT.md Assumption A1, 01-RESEARCH.md
  Pitfall 4).

## Rendered literals

Reproduced verbatim from `01-02-PLAN.md`'s `## Fixture literals (authoritative)` table — this is
the **only other place** any of these strings is allowed to be defined. Plans 01-03, 01-04, 01-05
and 01-06 quote these exact bytes; they do not invent, paraphrase or re-derive one. Rows marked
*(01-01)* are authored by the tracer plan and catalogued here for completeness.

| # | Fixture | Rendered literal (exact bytes) | Asserted by |
|---|---------|--------------------------------|-------------|
| L1 | `enum_basic` *(01-01)* | `values {0: 8 (Disabled), 1: 4 (Enabled)}` | 01-01, 01-05 C API + Lua, 01-06 all four bindings |
| L2 | `enum_basic` *(01-01)* | `2: 1 (undeclared)` | 01-01, 01-03 |
| L3 | `enum_basic` *(01-01)* | `enum bool {0: Disabled, 1: Enabled}` | 01-03, 01-05, 01-06 |
| L4 | `enum_basic` *(01-01)* | `- max_generation (REAL) [MW] — "Maximum Generation"` | 01-03, 01-05 C API |
| L5 | `enum_basic` *(01-01)* | `- internal_code (INTEGER) [hidden] — "Internal Code"` | 01-03, 01-05 C API |
| L6 | `enum_basic` *(01-01)* | `Storage Units` (collection label) | 01-03, 01-05 Lua |
| L7 | `enum_basic` *(01-01)* | `notes` renders its line with no `—` clause (empty declared label, D-12) | 01-03 task 2 |
| L8 | `foresight_like` | `enum model {2: Local Linear Trend, 3: ARIMA, 5: Seasonal Naïve, 4: Regresión Lineal}` | 01-04 `MixedLocaleFormsResolveInOneVocabulary` |
| L9 | `foresight_like` | `Seasonal Naïve` — bytes `Seasonal Na\xC3\xAFve`, the **canonical accented literal** | 01-04, 01-05 C API + Lua, 01-06 all four bindings |
| L10 | `foresight_like` | `Regresión Lineal` — bytes `Regresi\xC3\xB3n Lineal`, the fallback-leg accented literal | 01-04 |
| L11 | `bess_like` | `Degradation Rate` on the `degradation` **scalar** line, and `Degradation Curve` **not** on it | 01-04 `AttributeAndGroupIdsAreSeparateNamespaces` |
| L12 | `bess_like` | `Installed Capacity` and `Rated Cycle Count` (the two post-group-block attributes) | 01-04 `AttributesAfterAGroupBlockAreStillParsed` |
| L13 | `htd_like` | `Hydro Plants`, `Measurement Date`, `enum bool {0: Disable, 1: Enable}` | 01-04 `PascalCaseIdKeysTheConfig` |
| L14 | `htd_like` | `Thermal Plants` **never appears** in any report (the no-`id` file is skipped) | 01-04 `MissingCollectionIdIsSkippedNotFatal` |
| L15 | `no_enum` / `empty_enum` | `enum bool (undeclared vocabulary)` | 01-03, 01-04 |
| L16 | `orphan_collection` | the `Storage` collection label renders; the `Agent` collection line carries none | 01-04 `UnlistedCollectionFileIsNeverLoaded` |

L8's entry order is the fixture's TOML declaration order, not code order — that is what makes
DESC-03's "declaration order, never sorted-by-code" observable.
