# Requirements: Quiver — UI Metadata in `describe`

**Defined:** 2026-09-20
**Milestone:** v0.10.8
**Core Value:** An agent calling `describe` on a PSR study database sees what an INTEGER enum column actually means — `0 = User Defined Forecast, 1 = Model` — not bare codes.

## v0.10.8 Requirements

### Sidecar Reading

- [ ] **READ-01**: Opening a database with `from_migrations` reads the `ui/` directory that sits
      beside the migrations directory, resolved with `weakly_canonical` so a trailing separator or a
      relative migrations path lands on the right directory instead of `<migrations>/ui` or the
      process CWD

- [ ] **READ-02**: Collection files are identified by shape — a non-recursive scan of `ui/*.toml`
      keeping files that carry both a top-level string `id` and an `attribute` array — so
      `main.toml`, `enum.toml`, `themes/` and `assets/` are never mistaken for one and no filename
      is ever translated into a table name

- [ ] **READ-03**: Each collection file contributes `label` and `tooltip` per `[[attribute]]` entry,
      keyed by that file's own top-level `id` and the attribute's own `id`

- [ ] **READ-04**: A localizable value is read as-is when it is a string and at its `en` key when it
      is a table, with `\n`/`\r` collapsed to a single space and UTF-8 bytes passed through
      untranscoded

- [ ] **READ-05**: `enum.toml` vocabularies are parsed and joined to attributes by the attribute's
      `enum` value, yielding a code→label map in which the code is the entry's `id` field, never its
      position

### Report Rendering

- [ ] **RENDER-01**: `describe` and `describe_collection` show an attribute's label, tooltip and enum
      labels, positioned after the attribute's name, type and flags

- [ ] **RENDER-02**: `summarize_collection` annotates each entry of an integer column's value
      histogram with that code's enum label

- [ ] **RENDER-03**: An attribute the sidecar does not describe — including one whose collection has
      no ui file, and one naming a column that does not exist in the schema — renders exactly as it
      does today

### Graceful Degradation

- [x] **SAFE-01**: A database opened from migrations with no `ui/` directory produces reports
      byte-identical to today's

- [ ] **SAFE-02**: A missing, empty, unparseable or partially-populated `ui/` never fails
      `from_migrations` — it logs a warning and the reports render as though no sidecar were present

## Future Requirements

Acknowledged, not in this milestone's roadmap.

### Validation

- **VALID-01**: `validate_ui_config()` cross-checks the sidecar against the live SQL schema and
  reports drift — dangling attribute names, enum vocabularies contradicting the model's own
  declarations, collections with no ui file

- **VALID-02**: The enum vocabulary is checked against the model's Julia `@enumx` declarations, or
  the disagreement is reported rather than silently rendered

### Richer Metadata

- **META-01**: `unit` and `format` are read and rendered, so an agent knows a column is `m³/s`
- **META-02**: A structured getter exposes label / tooltip / enum labels as data through the C API
  and all five bindings, so `claw` can drop the per-attribute half of `study-config.ts`

- **META-03**: Collection-level metadata (label, help, display order) is exposed

## Out of Scope

| Feature | Reason |
|---------|--------|
| `main.toml` parsing | Collection files self-select by shape across 67/67 corpus files. Parsing it would add the `collections` array, the `dc_line` → `DCLine` filename mapping, the orphan-file case and the listed-but-missing case, for nothing |
| The other 7 `[[attribute]]` keys — `hide`, `unit`, `format`, `tab`, `enabled_if`, `type` | Parsed and ignored for free; tomlplusplus needs no schema declaration. Adding any of them is a fourth field |
| `[[attribute_group]]` | Its ids collide with `[[attribute]]` ids in 15 real files, and it has `label` but never `tooltip`. Merging the arrays silently overwrites one label with the other |
| A `hide` filter | `describe` describes the schema, not the UI. 360 of 736 attributes carry `hide = true` and still belong in a schema report |
| Collection-level metadata, `[[card]]`, `themes/`, `[[attribute_query]]`, `[[scalar_tab]]` | Pure Hub presentation. An agent's question is about columns |
| A structured getter through the C API and bindings | `describe*` already returns `std::string`, so the report reaches all five bindings and Lua for free. A getter roughly doubles the milestone and its test surface for a consumer that does not exist yet |
| Any change to `DatabaseOptions`, `ScalarMetadata`, `GroupMetadata` | C ABI breaks whose JS failure mode is a native write past a JS-owned buffer, with no compile error |
| Locales other than English | A bare value already means "the same in every language", and 0 of 817 locale tables lack `en`. No option, no API surface |
| Reading `ui/` from `open()`, `from_schema` or `validate_migrations` | None has a migrations path to resolve a sibling from, and PSR models reach both create and load through `from_migrations` |
| `validate_ui_config()` | Deferred with the `HasCommitment` inversion explicitly on the table. See PROJECT.md → The accepted risk |
| Binding-level tests | All four binding describe suites assert only "returns a string" and say so in a comment. No binding code changes, so a binding test proves nothing |
| A committed `tests/schemas/ui/` fixture | It would become a live sibling of `tests/schemas/migrations` for every `from_migrations` call in six suites plus the Lua migrations test. Temp dirs only |

## Traceability

Mapped during roadmap creation (see .planning/ROADMAP.md).

| Requirement | Phase | Status |
|-------------|-------|--------|
| READ-01 | Phase 1 | Pending |
| READ-02 | Phase 1 | Pending |
| READ-03 | Phase 1 | Pending |
| READ-04 | Phase 1 | Pending |
| READ-05 | Phase 1 | Pending |
| RENDER-01 | Phase 1 | Pending |
| RENDER-02 | Phase 2 | Pending |
| RENDER-03 | Phase 1 | Pending |
| SAFE-01 | Phase 1 | Complete |
| SAFE-02 | Phase 1 | Pending |

**Coverage:**

- v0.10.8 requirements: 10 total
- Mapped to phases: 10
- Unmapped: 0 ✓
- Phase 1: 9 requirements (READ-01..05, RENDER-01, RENDER-03, SAFE-01, SAFE-02)
- Phase 2: 1 requirement (RENDER-02)

---
*Requirements defined: 2026-09-20*
*Last updated: 2026-09-20 after roadmap creation*
