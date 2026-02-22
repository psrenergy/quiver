# Phase 3: Language Bindings - Context

**Gathered:** 2026-02-21
**Status:** Ready for planning

<domain>
## Phase Boundary

Bind the C API transaction functions (begin_transaction, commit, rollback, in_transaction) to Julia, Dart, and Lua. Each binding includes both raw function wrappers and a convenience `transaction` block with auto commit on success and rollback on exception. Naming follows CLAUDE.md cross-layer conventions. No new C++ or C API work.

</domain>

<decisions>
## Implementation Decisions

### Convenience wrapper semantics
- Block return value passes through: `result = transaction(db) do ... return 42 end` gives `result = 42`
- After rollback on exception, the original exception re-raises unchanged — bindings do not wrap or modify error messages
- Rollback failure is silently ignored (best-effort rollback) — original exception still re-raises
- No nesting guard in bindings — let C++ handle double-begin errors naturally
- All three bindings pass `db` to the block function for uniformity:
  - Julia: `transaction(db) do db ... end`
  - Dart: `db.transaction((db) { ... })`
  - Lua: `db:transaction(function(db) ... end)`
- Lua returns single value only from transaction block (no multi-return passthrough)

### Test depth per binding
- Full mirror: each binding re-tests all C API transaction scenarios (begin/commit/rollback, error cases, multi-op batches) plus binding-specific features
- Use shared schemas from `tests/schemas/valid/` — no binding-specific schemas
- Error propagation tests verify that errors are raised, but do not assert exact message strings
- Include a multi-operation batch test per binding: create + update inside transaction block, verify all data persists after commit

### Implementation order
- Single plan (03-01-PLAN.md) covering all three bindings
- Sequential implementation: Julia first (reference), then Dart, then Lua
- FFI binding regeneration (Julia/Dart generators) is an explicit tracked task in the plan
- Include a task to verify/update CLAUDE.md cross-layer table after implementation

### Lua pcall design
- `db:transaction(function(db) ... end)` — fn receives db as argument
- On error: pcall catches, rollback executes, then `error()` re-raises original message (not pcall-style return tuple)
- Consistent with Julia/Dart: all three bindings re-raise on failure rather than returning error codes

### Claude's Discretion
- Internal implementation details of each binding's transaction wrapper
- Test file organization within each binding's test suite
- Generator script modifications needed for FFI regeneration

</decisions>

<specifics>
## Specific Ideas

- Convenience wrappers should feel like language-native constructs: Julia's `do...end` blocks, Dart closures, Lua functions
- Re-raise behavior across all three bindings mirrors Quiver's "error messages defined in C++ layer" principle — bindings are transparent conduits

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-language-bindings*
*Context gathered: 2026-02-21*
