# Phase 2: Binding Cleanup - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Remove dead code across all language bindings (Julia, Dart, Python) to restore homogeneity. Audit each binding for unused functions, stale error helpers, and dead exports. Keep all actively-used code including helper_maps.jl.

</domain>

<decisions>
## Implementation Decisions

### Deletion scope
- Delete `quiver_database_sqlite_error` from Julia `exceptions.jl` (dead code — defined but never called)
- **Keep** `helper_maps.jl` and `test_helper_maps.jl` — user actively calls `scalar_relation_map`/`set_relation_map` from application code
- Do NOT export `scalar_relation_map`/`set_relation_map` — keep current access pattern via `Quiver.scalar_relation_map`
- Light audit across ALL bindings (Julia, Dart, Python) for additional dead code
- Any dead code found in any binding gets cleaned up in this phase

### exceptions.jl cleanup
- Delete `quiver_database_sqlite_error` one-liner (line 5)
- Review `check()` function for potential improvements during implementation
- Keep the `@warn` fallback guard for empty error messages — user wants the defensive behavior
- Check Dart and Python exception modules for similar dead `sqlite_error` patterns

### helper_maps.jl
- Leave completely as-is — no changes to functions, docstrings, or exports
- Julia-only convenience — no cross-binding parity needed for Dart/Python

### Claude's Discretion
- Specific improvements to `check()` function if warranted during review
- Which dead code items found during the cross-binding audit to include vs defer
- Order of operations for cleanup across bindings

</decisions>

<specifics>
## Specific Ideas

- User actively uses `scalar_relation_map` and `set_relation_map` from their application code — these must not be removed
- Phase was originally scoped as "Julia Cleanup" but expanded to all bindings per user preference

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `exceptions.jl`: Contains `DatabaseException` struct, `quiver_database_sqlite_error` (dead), and `check()` function
- `helper_maps.jl`: Contains `scalar_relation_map` and `set_relation_map` — actively used, must stay
- `Quiver.jl`: Module entry point with all includes and exports

### Established Patterns
- Julia exceptions: `DatabaseException` struct + `check()` function pattern
- Dart exceptions: `QuiverException` in `exceptions.dart`
- Python exceptions: `QuiverError` in `exceptions.py`
- All bindings follow same file organization: `database_*.{ext}` per domain

### Integration Points
- `Quiver.jl` line 22: `include("helper_maps.jl")` — stays
- `exceptions.jl` line 5: `quiver_database_sqlite_error` — remove this line only
- Dart `exceptions.dart` and Python `exceptions.py` — check for similar dead code

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 02-binding-cleanup*
*Context gathered: 2026-03-01*
