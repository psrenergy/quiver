# Phase 2: Create Element FK Resolution - Context

**Gathered:** 2026-02-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Extend `create_element` so users can pass target labels (strings) for scalar, vector, and time series FK columns and they resolve to target IDs automatically. Set FK resolution already works from Phase 1 — this phase applies the same pattern to the remaining column types via a unified pre-resolve pass. The `resolve_fk_label` helper from Phase 1 is the core building block.

</domain>

<decisions>
## Implementation Decisions

### Pre-resolve fail strategy
- **Fail-fast on first bad label** — stop at the first unresolvable FK label anywhere across all column types, throw immediately
- **Keep existing error format** — `"Failed to resolve label 'X' to ID in table 'Y'"` with no additional column context (matches ERR-01)
- **Same non-FK INTEGER guard everywhere** — string passed to any non-FK INTEGER column (scalar, vector, set, or time series) gets a clear Quiver error, not raw SQLite STRICT mode error

### Resolution pass design
- **Dedicated first pass** — new step before the existing 4-phase insert flow: iterate all element data, resolve every FK label, store resolved values. Then the 4 insert phases use already-resolved values
- **Separate resolved map** — pre-resolve pass produces a parallel data structure with resolved values; Element input stays unchanged (immutable input)
- **Unify all paths** — move the existing set FK inline resolution (from Phase 1) into the dedicated first pass too. All 4 column types use the same pre-resolved map. Consistent and clean
- **Schema-driven discovery** — pre-resolve pass inspects schema to find FK columns automatically. User doesn't need to know which columns are FKs

### Claude's Discretion
- Exact data structure for the resolved-values map
- How the pre-resolve pass discovers and iterates FK columns (implementation of schema scanning)
- Internal method signatures and code organization

</decisions>

<specifics>
## Specific Ideas

- The pre-resolve pass should be a single method/function that can be tested independently
- Set FK resolution (Phase 1) should be migrated into the same pass for uniformity — no special path for sets

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

### Test Schema Model
- **Extend existing relations.sql** — add scalar FK, vector FK, and time series FK columns to the existing Parent/Child tables
- **Parent/Child only** — no need for a third table or multi-level FK chains
- **Include no-FK regression test** — explicit test that `create_element` with zero FK columns still works after the pre-resolve pass is added
- **Include combined all-types test** — one test exercising scalar FK + vector FK + time series FK in a single `create_element` call to validate the full pre-resolve pass end-to-end

---

*Phase: 02-create-element-fk-resolution*
*Context gathered: 2026-02-23*
