# Phase 4: Queries and Relations - Research

**Researched:** 2026-02-23
**Domain:** Python CFFI ABI-mode bindings for parameterized SQL query operations with void** typed-parameter marshaling
**Confidence:** HIGH

## Summary

Phase 4 wraps the 6 existing C API query functions (3 simple + 3 parameterized) into 4 unified Python methods: `query_string`, `query_integer`, `query_float`, and `query_date_time`. Each method uses a keyword-only `params` argument to route to either the simple or `_params` C API variant. Relations were already completed in Phases 2-3, so this phase focuses entirely on query operations.

The primary technical challenge is marshaling Python parameter lists into the C API's `const int* param_types` + `const void* const* param_values` parallel arrays. Each parameter must be allocated as the correct typed pointer (`int64_t*`, `double*`, or `char*`) and cast to `void*`, while keeping all backing buffers alive until the C API call completes. CFFI's `ffi.new` and `ffi.cast` provide the necessary tools, and the Julia/Dart implementations serve as verified reference patterns.

The CFFI declarations for all 6 query functions are NOT yet in `_c_api.py` and must be added. The `quiver_element_free_string` function (needed to free query_string results) is already declared.

**Primary recommendation:** Add 6 CFFI cdef declarations, implement a `_marshal_params` helper that builds keepalive-safe parallel arrays, then add 4 unified query methods to the Database class using the same try/finally pattern established in prior phases.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Unified methods with optional keyword-only params: `query_string(sql, *, params=None)`, `query_integer(sql, *, params=None)`, `query_float(sql, *, params=None)`
- Three core methods total, each routing to simple or `_params` C API call based on whether params is provided
- Include `query_date_time(sql, *, params=None)` in this phase (pulled forward from CONV-04) -- wraps `query_string` + `datetime.fromisoformat` parsing
- Four methods total: `query_string`, `query_integer`, `query_float`, `query_date_time`
- Plain list with type inference: `params=[42, 3.14, "hello", None]`
- Type mapping: `int` -> INTEGER, `float` -> FLOAT, `str` -> STRING, `None` -> NULL
- `bool` accepted as integer (True->1, False->0) since bool is a subclass of int in Python
- Unsupported types (dict, datetime, etc.) raise `TypeError` immediately before calling C API, with message: "Unsupported parameter type: {type}. Expected int, float, str, or None."
- `params=[]` (empty list) treated same as `params=None` -- routes to simple C API call, no marshaling
- All query methods return `Optional[T]`: `str | None`, `int | None`, `float | None`, `datetime | None`
- No rows -> return `None` (consistent with Julia/Dart and existing Python by-id reads)
- `query_float` always returns `float` (never coerces to int), type-consistent with what was asked for
- `query_date_time`: no rows -> `None`; unparseable date string -> raise `ValueError` (distinguishes "no data" from "bad data")

### Claude's Discretion
- CFFI keepalive strategy for preventing GC of param buffers during C API call
- Internal `_marshal_params` helper implementation details
- Whether to use `ffi.new` arrays or manual buffer management for void** dispatch

### Deferred Ideas (OUT OF SCOPE)
- Remaining CONV-04 DateTime helpers (read_scalar_date_time_by_id, read_vector_date_time_by_id, read_set_date_time_by_id) -- Phase 6
- query_date_time_params was subsumed into the unified query_date_time method via the params kwarg
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| QUERY-01 | query_string, query_integer, query_float for simple SQL queries | CFFI declarations for 3 simple query functions verified in C API header; Python method patterns match existing by-id reads (out_value + out_has_value pattern) |
| QUERY-02 | query_string_params, query_integer_params, query_float_params with typed parameter marshaling and keepalive | CFFI declarations for 3 parameterized query functions verified; void** marshaling pattern documented with keepalive strategy; Julia/Dart reference implementations analyzed |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| cffi | (project version) | CFFI ABI-mode FFI for calling C API | Already used by all prior phases; only FFI library in project |
| datetime (stdlib) | Python 3.13+ | `datetime.fromisoformat` for query_date_time | Zero-dependency; handles ISO 8601 strings natively since Python 3.7+ |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| pytest | (project version) | Test framework | All test files use pytest class-based pattern from conftest.py |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `datetime.fromisoformat` | `dateutil.parser.parse` | dateutil handles more formats but adds dependency; fromisoformat is sufficient for ISO strings from SQLite |

**Installation:** No new dependencies needed. CFFI and pytest already in project.

## Architecture Patterns

### Recommended Project Structure
No new files beyond what's modified:
```
bindings/python/src/quiver/
    _c_api.py              # ADD: 6 query CFFI declarations
    database.py            # ADD: 4 query methods + _marshal_params helper
bindings/python/tests/
    test_database_query.py # NEW: query test file
```

### Pattern 1: Unified Method with Optional Params (Locked Decision)
**What:** Each query method accepts `params=None` as keyword-only argument; routes to simple or `_params` C API call.
**When to use:** For all 4 query methods.
**Example:**
```python
# Source: Pattern derived from CONTEXT.md locked decision + C API header analysis
def query_string(self, sql: str, *, params: list | None = None) -> str | None:
    self._ensure_open()
    lib = get_lib()

    if params is not None and len(params) > 0:
        # Route to parameterized C API call
        keepalive, c_types, c_values = _marshal_params(params)
        out_value = ffi.new("char**")
        out_has = ffi.new("int*")
        check(lib.quiver_database_query_string_params(
            self._ptr, sql.encode("utf-8"),
            c_types, c_values, len(params),
            out_value, out_has,
        ))
    else:
        # Route to simple C API call
        out_value = ffi.new("char**")
        out_has = ffi.new("int*")
        check(lib.quiver_database_query_string(
            self._ptr, sql.encode("utf-8"),
            out_value, out_has,
        ))

    if out_has[0] == 0 or out_value[0] == ffi.NULL:
        return None
    try:
        return ffi.string(out_value[0]).decode("utf-8")
    finally:
        lib.quiver_element_free_string(out_value[0])
```

### Pattern 2: Parameter Marshaling with Keepalive (Claude's Discretion)
**What:** Convert Python `list` of mixed-type params into parallel C arrays `int* param_types` and `void** param_values`, keeping typed backing buffers alive.
**When to use:** For all parameterized query calls.

**Recommended implementation:** Use a module-level `_marshal_params` helper that returns a tuple of `(keepalive_list, c_types_array, c_values_array)`. The keepalive list holds references to all `ffi.new` allocations so Python's GC cannot collect them before the C API call completes.

```python
# Source: Derived from Julia marshal_params + Dart _marshalParams + CFFI docs
def _marshal_params(params: list) -> tuple:
    """Marshal Python params into C API parallel arrays.

    Returns (keepalive, c_types, c_values) where keepalive must remain
    referenced until the C API call completes.
    """
    n = len(params)
    c_types = ffi.new("int[]", n)
    c_values = ffi.new("void*[]", n)
    keepalive = []  # prevent GC of typed buffers

    for i, p in enumerate(params):
        if p is None:
            c_types[i] = 4  # QUIVER_DATA_TYPE_NULL
            c_values[i] = ffi.NULL
        elif isinstance(p, int):  # bool is subclass of int, handled here
            c_types[i] = 0  # QUIVER_DATA_TYPE_INTEGER
            buf = ffi.new("int64_t*", p)
            keepalive.append(buf)
            c_values[i] = ffi.cast("void*", buf)
        elif isinstance(p, float):
            c_types[i] = 1  # QUIVER_DATA_TYPE_FLOAT
            buf = ffi.new("double*", p)
            keepalive.append(buf)
            c_values[i] = ffi.cast("void*", buf)
        elif isinstance(p, str):
            c_types[i] = 2  # QUIVER_DATA_TYPE_STRING
            buf = p.encode("utf-8")
            c_buf = ffi.new("char[]", buf)
            keepalive.append(c_buf)
            c_values[i] = ffi.cast("void*", c_buf)
        else:
            raise TypeError(
                f"Unsupported parameter type: {type(p).__name__}. "
                "Expected int, float, str, or None."
            )

    return keepalive, c_types, c_values
```

**Key design decisions:**
1. **`keepalive` as list:** Each `ffi.new` allocation is appended to a list that the caller holds in scope. This prevents GC while the C API call is in flight.
2. **`isinstance(p, int)` catches `bool`:** Since `bool` is a subclass of `int` in Python, `isinstance(True, int)` returns `True`. The `int` check must come before any hypothetical `bool` check. Values `True`/`False` naturally convert to `1`/`0` when passed to `ffi.new("int64_t*", p)`.
3. **`ffi.cast("void*", buf)`:** Converts typed pointers to `void*` for the `c_values` array. CFFI handles this cast automatically per its docs.
4. **String encoding:** Encode to `bytes` first, then `ffi.new("char[]", buf)` allocates a null-terminated C string. The `char[]` buffer is kept alive in the keepalive list.

### Pattern 3: query_date_time Wrapper
**What:** Wraps `query_string` and parses the result via `datetime.fromisoformat`.
**When to use:** For date/time queries returning ISO 8601 strings from SQLite.
```python
from datetime import datetime

def query_date_time(self, sql: str, *, params: list | None = None) -> datetime | None:
    result = self.query_string(sql, params=params)
    if result is None:
        return None
    return datetime.fromisoformat(result)
```

### Anti-Patterns to Avoid
- **Allocating typed buffers inside a comprehension without keepalive:** The `ffi.new` objects would be GC'd immediately, leaving dangling pointers in `c_values`. Always store references.
- **Using `ffi.buffer` or `ffi.from_buffer` for small scalar params:** Unnecessary complexity; `ffi.new` is the right tool for individual scalar pointers.
- **Checking `isinstance(p, bool)` separately:** Since `bool` is a subclass of `int`, the `isinstance(p, int)` check already handles booleans. Adding a separate bool branch is redundant.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| ISO 8601 date parsing | Custom date string parser | `datetime.fromisoformat` | Handles all SQLite date formats; stdlib, no dependencies |
| Parameter type dispatch | Manual if/elif with C type constants | Single `_marshal_params` helper | Centralizes type mapping; matches Julia `marshal_params` and Dart `_marshalParams` pattern |
| Void pointer casting | Pointer arithmetic or `ffi.buffer` | `ffi.cast("void*", typed_ptr)` | CFFI native cast; safe, documented, no manual address manipulation |

**Key insight:** The C API already defines the `convert_params` pattern in `src/c/database_query.cpp`. The Python `_marshal_params` is just its mirror image: converting from Python types to the same parallel-array format that `convert_params` reads.

## Common Pitfalls

### Pitfall 1: Premature GC of Parameter Buffers
**What goes wrong:** CFFI `ffi.new` objects are garbage-collected when no Python reference exists. If typed buffers (e.g., `ffi.new("int64_t*", 42)`) are created in a loop but not stored, they may be freed before the C API reads them, causing segfaults or wrong values.
**Why it happens:** Python's GC can collect unreferenced objects at any point. In CFFI ABI mode, there's no arena allocator -- each `ffi.new` is independently tracked.
**How to avoid:** Collect all `ffi.new` allocations in a `keepalive` list that persists in scope throughout the C API call. Return the keepalive list from `_marshal_params` and hold it in the calling method.
**Warning signs:** Intermittent segfaults or wrong query results, especially with multiple parameters. Symptoms may be non-reproducible due to GC timing.

### Pitfall 2: Bool Treated as Separate Type
**What goes wrong:** If `_marshal_params` checks `isinstance(p, bool)` before `isinstance(p, int)`, it works. But if it does NOT check bool at all, someone might later add a check that raises TypeError for bools.
**Why it happens:** `bool` is a subclass of `int` in Python, but `type(True) is int` returns `False`. Using `type()` instead of `isinstance()` would miss bools.
**How to avoid:** Always use `isinstance(p, int)` which catches bool. The CONTEXT.md explicitly states "bool accepted as integer (True->1, False->0)". Use `isinstance` not `type()`.
**Warning signs:** `TypeError` when passing `True`/`False` as query parameters.

### Pitfall 3: Forgetting to Free query_string Results
**What goes wrong:** The C API allocates a `char*` for query_string/query_string_params results via `strdup_safe`. If not freed, memory leaks occur.
**Why it happens:** Unlike integer/float results which are stack-allocated out-params, string results are heap-allocated and returned via `char**`.
**How to avoid:** Use try/finally to call `lib.quiver_element_free_string(out_value[0])` after extracting the Python string, identical to `read_scalar_string_by_id` pattern already in codebase.
**Warning signs:** Memory leaks in long-running processes. Not visible in short test runs.

### Pitfall 4: Missing CFFI Declarations
**What goes wrong:** Calling a C function not declared in `ffi.cdef` raises `AttributeError` at runtime.
**Why it happens:** CFFI ABI mode requires explicit cdef declarations for every function. The 6 query functions are not yet declared.
**How to avoid:** Add all 6 declarations to `_c_api.py` before implementing the Python methods. Verify exact signatures match `include/quiver/c/database.h`.
**Warning signs:** `AttributeError: function 'quiver_database_query_string' not found` at runtime.

### Pitfall 5: Empty Params List Not Short-Circuiting
**What goes wrong:** Passing `params=[]` to the `_params` C API variant with `param_count=0` works but is unnecessary overhead; the C API validates `param_count > 0 && (!param_types || !param_values)`.
**Why it happens:** Not checking for empty list before marshaling.
**How to avoid:** CONTEXT.md explicitly says `params=[]` should be treated same as `params=None` -- route to simple C API call. Check `if params is not None and len(params) > 0` before calling `_marshal_params`.
**Warning signs:** Unnecessary overhead, no functional issue.

## Code Examples

Verified patterns from project source code:

### CFFI Declarations to Add
```python
# Source: include/quiver/c/database.h lines 437-478
# Add to _c_api.py cdef block:

    // Query methods - simple
    quiver_error_t quiver_database_query_string(quiver_database_t* db,
        const char* sql, char** out_value, int* out_has_value);
    quiver_error_t quiver_database_query_integer(quiver_database_t* db,
        const char* sql, int64_t* out_value, int* out_has_value);
    quiver_error_t quiver_database_query_float(quiver_database_t* db,
        const char* sql, double* out_value, int* out_has_value);

    // Query methods - parameterized
    quiver_error_t quiver_database_query_string_params(quiver_database_t* db,
        const char* sql, const int* param_types, const void* const* param_values,
        size_t param_count, char** out_value, int* out_has_value);
    quiver_error_t quiver_database_query_integer_params(quiver_database_t* db,
        const char* sql, const int* param_types, const void* const* param_values,
        size_t param_count, int64_t* out_value, int* out_has_value);
    quiver_error_t quiver_database_query_float_params(quiver_database_t* db,
        const char* sql, const int* param_types, const void* const* param_values,
        size_t param_count, double* out_value, int* out_has_value);
```

**CFFI Note on `const void* const*`:** CFFI may not parse `const void* const*` directly. If it fails, use `void**` in the cdef (CFFI ignores const qualifiers in ABI mode -- they are hints for the C compiler, not the ABI). Verify this during implementation. The existing `const char* const*` declarations in `_c_api.py` (for update_vector_strings etc.) already work, so `const void* const*` likely works too.

### query_integer Full Implementation
```python
# Source: Pattern from read_scalar_integer_by_id + C API query signature
def query_integer(self, sql: str, *, params: list | None = None) -> int | None:
    """Execute SQL and return the first column of the first row as int, or None."""
    self._ensure_open()
    lib = get_lib()
    out_value = ffi.new("int64_t*")
    out_has = ffi.new("int*")

    if params is not None and len(params) > 0:
        keepalive, c_types, c_values = _marshal_params(params)
        check(lib.quiver_database_query_integer_params(
            self._ptr, sql.encode("utf-8"),
            c_types, c_values, len(params),
            out_value, out_has,
        ))
    else:
        check(lib.quiver_database_query_integer(
            self._ptr, sql.encode("utf-8"),
            out_value, out_has,
        ))

    if out_has[0] == 0:
        return None
    return out_value[0]
```

### query_float Full Implementation
```python
# Source: Pattern from read_scalar_float_by_id + C API query signature
def query_float(self, sql: str, *, params: list | None = None) -> float | None:
    """Execute SQL and return the first column of the first row as float, or None."""
    self._ensure_open()
    lib = get_lib()
    out_value = ffi.new("double*")
    out_has = ffi.new("int*")

    if params is not None and len(params) > 0:
        keepalive, c_types, c_values = _marshal_params(params)
        check(lib.quiver_database_query_float_params(
            self._ptr, sql.encode("utf-8"),
            c_types, c_values, len(params),
            out_value, out_has,
        ))
    else:
        check(lib.quiver_database_query_float(
            self._ptr, sql.encode("utf-8"),
            out_value, out_has,
        ))

    if out_has[0] == 0:
        return None
    return out_value[0]
```

### Test Pattern (from Julia/Dart cross-reference)
```python
# Source: bindings/julia/test/test_database_query.jl test structure
class TestQueryString:
    def test_query_string_returns_value(self, db):
        db.create_element("Configuration", Element().set("label", "Test").set("string_attribute", "hello"))
        result = db.query_string("SELECT string_attribute FROM Configuration WHERE label = 'Test'")
        assert result == "hello"

    def test_query_string_no_rows_returns_none(self, db):
        result = db.query_string("SELECT string_attribute FROM Configuration WHERE 1 = 0")
        assert result is None

    def test_query_string_with_params(self, db):
        db.create_element("Configuration", Element().set("label", "Test").set("string_attribute", "hello"))
        result = db.query_string("SELECT string_attribute FROM Configuration WHERE label = ?", params=["Test"])
        assert result == "hello"

    def test_query_string_with_none_param(self, db):
        db.create_element("Configuration", Element().set("label", "Test"))
        result = db.query_integer("SELECT COUNT(*) FROM Configuration WHERE ? IS NULL", params=[None])
        assert result == 1
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Separate `query_string` and `query_string_params` methods | Unified `query_string(sql, *, params=None)` | Phase 4 design decision | Cleaner API: 4 methods instead of 8; matches Python conventions |

**Deprecated/outdated:**
- Nothing deprecated in this domain. The C API's separate simple/params functions are stable and correct; the Python unification is purely a convenience wrapper.

## Open Questions

1. **CFFI parsing of `const void* const*`**
   - What we know: CFFI typically ignores `const` qualifiers. The existing `const char* const*` declarations work fine in `_c_api.py`.
   - What's unclear: Whether `const void* const*` specifically works or needs to be simplified to `void**`.
   - Recommendation: Try `const void* const*` first in cdef. If CFFI parser rejects it, fall back to `void**`. Both are ABI-compatible.

2. **`_marshal_params` placement: module-level vs Database method**
   - What we know: `_parse_scalar_metadata` and `_parse_group_metadata` are module-level helpers. Julia's `marshal_params` is module-level.
   - What's unclear: Whether `_marshal_params` should be a module-level function or a private method on Database.
   - Recommendation: Module-level function (private, prefixed with `_`), consistent with `_parse_scalar_metadata` pattern. It has no dependency on Database state.

## Sources

### Primary (HIGH confidence)
- `include/quiver/c/database.h` lines 437-478 -- C API query function signatures (6 functions)
- `src/c/database_query.cpp` -- C implementation with `convert_params` helper showing exact type dispatch
- `bindings/julia/src/database_query.jl` -- Julia `marshal_params` reference implementation with keepalive pattern
- `bindings/dart/lib/src/database_query.dart` + `database.dart` -- Dart `_marshalParams` with arena-based allocation
- `bindings/python/src/quiver/database.py` -- Existing Python binding patterns (read_scalar_*_by_id for out_value+out_has pattern)
- `bindings/python/src/quiver/_c_api.py` -- Current CFFI declarations (shows what to add)
- [CFFI Using ffi/lib docs](https://cffi.readthedocs.io/en/latest/using.html) -- ffi.new, ffi.cast, void pointer handling

### Secondary (MEDIUM confidence)
- `bindings/julia/test/test_database_query.jl` -- Test structure reference (12 test cases)
- `tests/test_database_query.cpp` -- C++ test cases showing param patterns including NULL

### Tertiary (LOW confidence)
- None. All findings verified against project source code.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - No new libraries; CFFI already in use, datetime is stdlib
- Architecture: HIGH - Pattern directly mirrors existing code (read_scalar_*_by_id, Julia/Dart marshal_params)
- Pitfalls: HIGH - GC/keepalive issue is well-documented in CFFI and verified by Julia's explicit keepalive pattern

**Research date:** 2026-02-23
**Valid until:** 2026-03-23 (stable domain; C API and CFFI are both stable)
