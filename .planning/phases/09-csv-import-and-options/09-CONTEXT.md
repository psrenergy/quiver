# Phase 9: CSV Import and Options - Context

**Gathered:** 2026-02-24
**Status:** Ready for planning

<domain>
## Phase Boundary

Align Python CSV bindings with C++ — implement `import_csv`, restructure `CSVOptions` with locale support, add round-trip tests, and update CLAUDE.md. The C++ and C API are complete. Julia and Dart bindings are fully working. This phase brings Python to parity.

</domain>

<decisions>
## Implementation Decisions

### CSVOptions API shape
- Rename `CSVExportOptions` to `CSVOptions` — remove old name entirely, no alias (WIP project, breaking changes acceptable)
- Frozen dataclass, same as current pattern
- `enum_labels` structure: `dict[str, dict[str, dict[str, int]]]` — attribute -> locale -> label -> value — matches C++ exactly
- Value direction inverted from current Python: was `int -> str`, now `str -> int` matching C++/Julia/Dart
- Update existing export tests to use new dict direction (don't rewrite from scratch)

### import_csv interface
- Signature: `import_csv(self, collection: str, group: str, path: str, *, options: CSVOptions | None = None) -> None`
- `collection`, `group`, `path` are required positional parameters
- `group` is required (pass `''` for scalar-only imports) — matches Julia/Dart
- `options` is keyword-only with `None` default
- Returns `None` (void equivalent) — errors raise exceptions
- Align `export_csv` to match same parameter pattern: `(collection, group, path, *, options=None)`

### Test strategy
- Tests live in existing CSV test file alongside export tests (same file)
- Use existing schemas from `tests/schemas/valid/` — ensures cross-binding parity
- Cover all three paths: scalar import (group=''), group import (vector/set data), enum-labeled import
- `date_time_format` testing as part of broader round-trip test (not separate)
- Round-trip pattern: export -> import -> read back -> verify

### Marshaling approach
- Rewrite `_marshal_csv_export_options` as new `_marshal_csv_options` — clean slate for 3-level dict flattening
- Single shared marshaler used by both `export_csv` and `import_csv`
- Match current CFFI allocation pattern (no context manager abstraction)
- No Python-side validation of enum_labels structure — let C API handle type mismatches per project principles

### Claude's Discretion
- Exact CFFI array allocation details
- How to organize the marshaling code within the file
- Which specific test schemas best cover scalar and group import paths
- Generator updates needed (if any) for CFFI declarations

</decisions>

<specifics>
## Specific Ideas

- Julia and Dart bindings serve as reference implementations — Python should produce identical C API calls for the same logical operations
- The C API `quiver_csv_options_t` uses grouped parallel arrays: `enum_attribute_names[]`, `enum_locale_names[]`, `enum_entry_counts[]`, `enum_labels[]`, `enum_values[]` with `enum_group_count` — marshaler must flatten the nested dict into this exact layout

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 09-csv-import-and-options*
*Context gathered: 2026-02-24*
