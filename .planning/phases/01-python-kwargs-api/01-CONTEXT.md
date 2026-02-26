# Phase 1: Python kwargs API - Context

**Gathered:** 2026-02-26
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace Python's Element builder pattern with kwargs-based `create_element` and `update_element` so the Python binding feels native, matching Julia/Dart conventions. Remove Element from the public API. Update all tests and CLAUDE.md docs.

</domain>

<decisions>
## Implementation Decisions

### Kwargs call signature
- `db.create_element("Collection", label="Item1", some_integer=42)` — collection is positional, all attributes as kwargs
- `db.update_element("Collection", id, label="Updated")` — collection and id positional, updated attributes as kwargs
- Lists for vectors/sets: `tags=["a", "b"]`, `values=[1, 2, 3]` — type inferred from first element, same as Element.set() today
- Nulls via `value=None` — natural Python pattern
- Time series columns as parallel kwargs arrays: `date_time=["2024-01-01"], value=[1.0]` — same as current Element behavior
- kwargs-only, no dict positional arg — users can do `**my_dict` unpacking if needed

### Element class fate
- Remove from `__init__.py` exports and `__all__` — Element is no longer part of the public API
- Keep `element.py` as internal implementation detail — `create_element`/`update_element` build an Element under the hood (same pattern as Julia)
- Keep `test_element.py` — update imports from `from quiverdb import Element` to `from quiverdb.element import Element`

### Test migration
- Mechanical 1:1 swap: `Element().set("label", "x").set("value", 42)` becomes `label="x", value=42`
- Same test names, same structure, same assertions — no reorganization
- Add explicit test that `from quiverdb import Element` raises `ImportError` (success criterion #3)
- Add one test verifying `**dict` unpacking works with `create_element`

### Claude's Discretion
- Whether to trim Element's public API (remove `__repr__`, `clear`, etc.) since it's internal-only now
- Internal implementation details of the kwargs-to-Element conversion
- Any type hint annotations on the kwargs parameters

</decisions>

<specifics>
## Specific Ideas

- Julia's pattern is the direct reference: kwargs overload creates Element internally, calls Element-based function, destroys Element in `finally` block
- `**dict` unpacking is the documented alternative to a dict positional arg (per REQUIREMENTS out-of-scope note)

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-python-kwargs-api*
*Context gathered: 2026-02-26*
