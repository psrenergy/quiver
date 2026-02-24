# Phase 4: Queries and Relations - Context

**Gathered:** 2026-02-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Parameterized SQL queries returning correctly typed Python results with safe parameter marshaling. Wraps the 6 existing C API query functions (3 simple, 3 parameterized) into unified Python methods. Also includes `query_date_time` convenience wrapper. Relations (read/update) were completed in Phases 2-3.

</domain>

<decisions>
## Implementation Decisions

### Method API shape
- Unified methods with optional keyword-only params: `query_string(sql, *, params=None)`, `query_integer(sql, *, params=None)`, `query_float(sql, *, params=None)`
- Three core methods total, each routing to simple or `_params` C API call based on whether params is provided
- Include `query_date_time(sql, *, params=None)` in this phase (pulled forward from CONV-04) — wraps `query_string` + `datetime.fromisoformat` parsing
- Four methods total: `query_string`, `query_integer`, `query_float`, `query_date_time`

### Parameter passing
- Plain list with type inference: `params=[42, 3.14, "hello", None]`
- Type mapping: `int` → INTEGER, `float` → FLOAT, `str` → STRING, `None` → NULL
- `bool` accepted as integer (True→1, False→0) since bool is a subclass of int in Python
- Unsupported types (dict, datetime, etc.) raise `TypeError` immediately before calling C API, with message: "Unsupported parameter type: {type}. Expected int, float, str, or None."
- `params=[]` (empty list) treated same as `params=None` — routes to simple C API call, no marshaling

### Return types and no-rows behavior
- All query methods return `Optional[T]`: `str | None`, `int | None`, `float | None`, `datetime | None`
- No rows → return `None` (consistent with Julia/Dart and existing Python by-id reads)
- `query_float` always returns `float` (never coerces to int), type-consistent with what was asked for
- `query_date_time`: no rows → `None`; unparseable date string → raise `ValueError` (distinguishes "no data" from "bad data")

### Claude's Discretion
- CFFI keepalive strategy for preventing GC of param buffers during C API call
- Internal `_marshal_params` helper implementation details
- Whether to use `ffi.new` arrays or manual buffer management for void** dispatch

</decisions>

<specifics>
## Specific Ideas

- Keyword-only `params` argument prevents accidental misuse (can't pass list as positional arg)
- API feel should match the rest of the Python binding — clean, minimal, no surprises
- `query_date_time` uses Python stdlib `datetime.fromisoformat` — no external dependencies

</specifics>

<deferred>
## Deferred Ideas

- Remaining CONV-04 DateTime helpers (read_scalar_date_time_by_id, read_vector_date_time_by_id, read_set_date_time_by_id) — Phase 6
- query_date_time_params was subsumed into the unified query_date_time method via the params kwarg

</deferred>

---

*Phase: 04-queries-and-relations*
*Context gathered: 2026-02-23*
