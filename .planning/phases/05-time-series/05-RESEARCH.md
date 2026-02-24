# Phase 5: Time Series - Research

**Researched:** 2026-02-23
**Domain:** Python CFFI bindings for multi-column time series read/write and time series file operations
**Confidence:** HIGH

## Summary

Phase 5 wraps the existing C API time series functions into Python methods on the `Database` class. The C API is fully implemented and stable -- no C++ or C layer changes are needed. The core challenge is marshaling columnar `void**` data across the CFFI boundary: reading typed column arrays back into Python dicts, and converting Python dicts into columnar typed arrays for writes.

The existing Python binding patterns from Phases 1-4 provide all necessary infrastructure: CFFI ABI-mode declarations, `check()` error handling, `ffi.new`/`ffi.cast` for memory allocation, and `try/finally` for C-allocated memory cleanup. The `_marshal_params` function from Phase 4 demonstrates the exact void-pointer-dispatch-by-type pattern needed here, just applied to N columns of M rows instead of N scalar parameters.

**Primary recommendation:** Follow the established Phase 4 void** marshaling pattern. Use `get_time_series_metadata` to determine column types on read (for casting void pointers). For update, infer types from Python values (int/float/str) and validate strictly per CONTEXT.md decisions. Return `list[dict]` for read and accept `list[dict]` for update, converting between row-oriented Python dicts and columnar C API arrays within the binding layer.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- List of dicts for both read return and update input: `[{'date_time': '2024-01-01', 'value': 3.14}, ...]`
- Dict keys match exact SQL column names from the schema (no translation)
- Key insertion order preserved as returned by C API (dimension column first, then value columns in schema order)
- For update, all columns present in the schema are required in each row dict -- error if a key is missing
- INTEGER columns map to Python `int`
- FLOAT columns map to Python `float`
- STRING and DATE_TIME columns map to Python `str`
- Consistent with Phase 4 query return type conventions
- For update input, strict types only: `int` for INTEGER, `float` for FLOAT, `str` for STRING/DATE_TIME -- error on wrong type, no coercion
- `update_time_series_group(collection, id, group, [])` clears all rows -- no separate clear method
- `read_time_series_group` returns `[]` for an element with no rows
- Both read and update raise an error if the element ID doesn't exist (distinguishes "no rows" from "no element")
- `has_time_series_files(collection)` returns plain `bool`
- `list_time_series_files_columns(collection)` returns `list[str]`
- `read_time_series_files(collection)` returns `dict[str, str|None]` (column name to path, None if unset)
- `update_time_series_files(collection, data)` accepts same dict shape as read returns -- symmetric API

### Claude's Discretion
- DATE_TIME column handling: whether to return as plain `str` or auto-parse to `datetime.datetime` (decide based on Phase 4 patterns and other binding behavior)
- Internal CFFI marshaling approach for void** column dispatch
- Error message formatting for validation failures (type mismatch, missing keys)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| TS-01 | `read_time_series_group` with void** column dispatch by type | C API signature fully documented; Julia/Dart reference implementations studied; CFFI void** casting pattern established via `_marshal_params`; metadata-driven type dispatch pattern identified |
| TS-02 | `update_time_series_group` with columnar typed-array marshaling (including clear via empty arrays) | C API accepts columnar `void* const*` with type tags; row-to-column transposition needed in Python; clear path via `column_count=0, row_count=0` with NULL arrays; strict type validation per user decision |
| TS-03 | `has_time_series_files`, `list_time_series_files_columns`, `read_time_series_files`, `update_time_series_files` | C API uses simple parallel arrays of strings; straightforward CFFI marshaling; patterns identical to existing scalar reads/string arrays |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| CFFI | (existing) | Python-C bridge via ABI mode | Already used by project; no compiler needed at install time |
| quiver._c_api | (existing) | `ffi` and `get_lib()` singleton | Established pattern from Phase 1 |
| quiver._helpers | (existing) | `check()`, `decode_string`, `decode_string_or_none` | Error handling and string marshaling |

### Supporting
No new libraries needed. All infrastructure is in place from Phases 1-4.

### Alternatives Considered
None -- the stack is fixed by project architecture.

## Architecture Patterns

### Recommended File Structure
No new files needed. All changes go into existing files:
```
bindings/python/src/quiver/
  _c_api.py            # Add CFFI cdef declarations for 8 time series C API functions + 2 free functions
  database.py          # Add 6 methods: read_time_series_group, update_time_series_group,
                       #   has_time_series_files, list_time_series_files_columns,
                       #   read_time_series_files, update_time_series_files
bindings/python/tests/
  conftest.py          # Add mixed_time_series_db fixture
  test_database_time_series.py  # New test file (one per functional area pattern)
```

### Pattern 1: Columnar void** Read Dispatch
**What:** Read time series data from C API (columnar typed arrays) and convert to list of row dicts.
**When to use:** `read_time_series_group`
**Implementation approach:**

1. Call `quiver_database_read_time_series_group` which returns parallel arrays: `column_names[]`, `column_types[]`, `column_data[]` (void**), plus column_count and row_count
2. If column_count == 0 or row_count == 0, return `[]`
3. For each column, cast `column_data[c]` based on `column_types[c]`:
   - `QUIVER_DATA_TYPE_INTEGER` (0): cast to `int64_t*`, read as Python `int`
   - `QUIVER_DATA_TYPE_FLOAT` (1): cast to `double*`, read as Python `float`
   - `QUIVER_DATA_TYPE_STRING` (2) or `QUIVER_DATA_TYPE_DATE_TIME` (3): cast to `char**`, read as Python `str`
4. Transpose from columnar to row-oriented: build list of dicts
5. Free C-allocated memory via `quiver_database_free_time_series_data` in a `finally` block

**Key CFFI patterns:**
```python
# Cast void* to typed pointer for reading
col_ptr = out_column_data[0][c]
if col_type == 0:  # INTEGER
    typed_ptr = ffi.cast("int64_t*", col_ptr)
    values = [typed_ptr[r] for r in range(row_count)]
elif col_type == 1:  # FLOAT
    typed_ptr = ffi.cast("double*", col_ptr)
    values = [typed_ptr[r] for r in range(row_count)]
else:  # STRING or DATE_TIME
    typed_ptr = ffi.cast("char**", col_ptr)
    values = [ffi.string(typed_ptr[r]).decode("utf-8") for r in range(row_count)]
```

### Pattern 2: Columnar void** Write Marshaling
**What:** Convert list of row dicts to columnar typed arrays for C API update.
**When to use:** `update_time_series_group`
**Implementation approach:**

1. If rows is empty `[]`, call C API with `column_count=0, row_count=0` and NULL arrays (clear operation)
2. Get metadata via `get_time_series_metadata` to know expected column names and types
3. Validate: all rows have all required columns, types are strict (int for INTEGER, float for FLOAT, str for STRING/DATE_TIME)
4. Transpose from row-oriented to columnar: for each column, extract all values into a typed array
5. Build parallel arrays: `column_names[]`, `column_types[]`, `column_data[]` (void**)
6. Call C API with columnar data
7. Keep all CFFI-allocated buffers alive until C API call completes (keepalive list pattern from `_marshal_params`)

**Key CFFI patterns:**
```python
keepalive = []
c_col_data = ffi.new("void*[]", col_count)

for c, (col_name, col_type) in enumerate(columns):
    if col_type == 0:  # INTEGER
        arr = ffi.new("int64_t[]", [row[col_name] for row in rows])
        keepalive.append(arr)
        c_col_data[c] = ffi.cast("void*", arr)
    elif col_type == 1:  # FLOAT
        arr = ffi.new("double[]", [row[col_name] for row in rows])
        keepalive.append(arr)
        c_col_data[c] = ffi.cast("void*", arr)
    else:  # STRING/DATE_TIME
        encoded = [row[col_name].encode("utf-8") for row in rows]
        c_strs = [ffi.new("char[]", e) for e in encoded]
        keepalive.extend(c_strs)
        c_arr = ffi.new("char*[]", c_strs)
        keepalive.append(c_arr)
        c_col_data[c] = ffi.cast("void*", c_arr)
```

### Pattern 3: Time Series Files (Parallel String Arrays)
**What:** Simple parallel string arrays for file path references.
**When to use:** `read_time_series_files`, `update_time_series_files`, `list_time_series_files_columns`, `has_time_series_files`
**Implementation approach:**

These are simpler than the time series group operations. They use the same patterns as existing string array reads (e.g., `read_scalar_strings`):
- `has_time_series_files`: Single `int*` out-parameter, return `bool(out[0])`
- `list_time_series_files_columns`: String array out, decode and free
- `read_time_series_files`: Two parallel string arrays (columns, paths), build dict with None for NULL paths
- `update_time_series_files`: Build parallel string arrays from dict, pass NULL for None values

### Anti-Patterns to Avoid
- **Returning columnar data directly:** User decided on list-of-dicts, not dict-of-lists. Julia/Dart return dict-of-lists, but Python phase must return list-of-dicts per CONTEXT.md decision.
- **Auto-coercing types on update:** User explicitly decided NO coercion. If schema expects FLOAT and user passes int, raise an error. This differs from Julia which auto-coerces Int to Float.
- **Adding datetime auto-parsing for DATE_TIME dimension column:** Julia and Dart auto-parse the dimension column to DateTime on read. For Python, given the Phase 4 pattern where `query_date_time` is a separate convenience method and all other string-type operations return raw strings, the recommendation is to return plain `str` for all columns including DATE_TIME. This keeps the core API consistent. A future `read_time_series_group_datetime` convenience method could be added in Phase 6 (CONV-04) if needed.

## Discretion Recommendations

### DATE_TIME Handling: Return as plain `str`
**Confidence:** HIGH
**Rationale:** Phase 4 established the pattern: `query_string` returns `str`, and `query_date_time` is a separate convenience wrapper that parses to `datetime.datetime`. The same principle should apply here. Returning raw ISO strings from `read_time_series_group` keeps the core API predictable and type-stable. Users who need `datetime` objects can parse them explicitly with `datetime.fromisoformat()`, or a future CONV-04 convenience method can be added.

This also avoids the question of which columns to parse -- only the dimension column? All STRING columns? Keeping everything as `str` is unambiguous.

### Internal CFFI Marshaling: Module-level helper functions
**Confidence:** HIGH
**Rationale:** Following the `_marshal_params` pattern from Phase 4, create module-level helper functions:
- `_marshal_time_series_columns(rows, metadata)` -- converts list-of-dicts to columnar CFFI arrays
- The read side can be inlined in the method since it only flows one direction

### Error Messages for Validation Failures
**Confidence:** HIGH
**Rationale:** Follow the C++ error message patterns. Since these are Python-side validation errors (not C API errors), use `TypeError` for type mismatches and `ValueError` for missing keys. Examples:
- `TypeError("Column 'temperature' expects float, got int")` for type mismatch
- `ValueError("Row 0 is missing column 'humidity'")` for missing keys
- `ValueError("All rows must have the same keys")` for inconsistent rows

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| void** type dispatch | Custom C extension | CFFI `ffi.cast()` | CFFI handles pointer casting natively in ABI mode |
| Column type lookup | Hard-coded column names | `get_time_series_metadata()` | Schema-driven; already implemented in Phase 2 |
| Memory cleanup | Manual tracking | `try/finally` with free functions | Established pattern throughout codebase |
| Row-column transposition | numpy/pandas | Plain Python loops | No external dependencies; data volumes are small |

**Key insight:** All the hard work (SQL execution, type marshaling, memory management) is done by the C API. The Python binding just needs to correctly shuttle data across the CFFI boundary using established patterns.

## Common Pitfalls

### Pitfall 1: CFFI Garbage Collection of Temporary Buffers
**What goes wrong:** CFFI-allocated buffers get garbage collected before the C API call reads them, causing segfaults or corrupted data.
**Why it happens:** Python's garbage collector can free `ffi.new()` objects if no Python reference exists.
**How to avoid:** Use a `keepalive` list (same pattern as `_marshal_params` in Phase 4). Append every `ffi.new()` result to the list. Keep the list alive until after the C API call returns.
**Warning signs:** Intermittent segfaults or corrupted values that only appear under load.

### Pitfall 2: Forgetting to Free C-Allocated Read Results
**What goes wrong:** Memory leak from not calling `quiver_database_free_time_series_data` or `quiver_database_free_time_series_files` after reads.
**Why it happens:** The C API allocates memory for read results that the caller must free.
**How to avoid:** Always use `try/finally` pattern: read into Python data structures in the `try` block, call the free function in `finally`.
**Warning signs:** Growing memory usage in long-running processes.

### Pitfall 3: void* Casting Direction Mismatch
**What goes wrong:** Casting `void*` to the wrong typed pointer (e.g., casting INTEGER column data to `double*`).
**Why it happens:** Column type array and column data array are separate -- easy to index them differently.
**How to avoid:** Always read `column_types[c]` and `column_data[c]` together using the same index `c`. The C API guarantees they correspond 1:1.
**Warning signs:** Wrong numeric values, crashes on string columns.

### Pitfall 4: Row-vs-Column Data Format Mismatch
**What goes wrong:** The C API uses columnar format (each column is a contiguous array), but the Python API returns row-oriented dicts. Transposition errors corrupt data.
**Why it happens:** Index confusion: `column_data[col][row]` in C vs `rows[row][col_name]` in Python.
**How to avoid:** Clear variable naming: `for r in range(row_count): for c in range(col_count)`.
**Warning signs:** Values appearing in wrong columns.

### Pitfall 5: CFFI const qualifiers in cdef
**What goes wrong:** CFFI ABI mode ignores `const` qualifiers, but using `const void* const*` in cdef may cause parse issues.
**Why it happens:** CFFI's C parser is not a full C compiler.
**How to avoid:** Use `void**` instead of `const void* const*` in the cdef declaration. Per Phase 4 decision [04-01], this is ABI-compatible since CFFI ignores const qualifiers. The existing `_declarations.py` already uses `const void* const*` which works, but the hand-written `_c_api.py` should use `void**` for the parameterized variant (as done in Phase 4).
**Warning signs:** CFFI parse errors at module import time.

### Pitfall 6: Empty Read Returns
**What goes wrong:** C API returns column_count=0 and row_count=0 for elements with no time series data, returning NULL pointers. Attempting to dereference them crashes.
**Why it happens:** Not checking for the empty case before accessing arrays.
**How to avoid:** Check `column_count == 0 or row_count == 0` immediately after the C API call and return `[]` early.
**Warning signs:** Segfault when reading time series for an element with no data.

## Code Examples

### CFFI Declarations Needed (add to _c_api.py)
```python
# Time series group read/update
quiver_error_t quiver_database_read_time_series_group(quiver_database_t* db,
    const char* collection, const char* group, int64_t id,
    char*** out_column_names, int** out_column_types,
    void*** out_column_data, size_t* out_column_count, size_t* out_row_count);

quiver_error_t quiver_database_update_time_series_group(quiver_database_t* db,
    const char* collection, const char* group, int64_t id,
    const char* const* column_names, const int* column_types,
    void** column_data, size_t column_count, size_t row_count);

quiver_error_t quiver_database_free_time_series_data(char** column_names,
    int* column_types, void** column_data,
    size_t column_count, size_t row_count);

# Time series files
quiver_error_t quiver_database_has_time_series_files(quiver_database_t* db,
    const char* collection, int* out_result);

quiver_error_t quiver_database_list_time_series_files_columns(quiver_database_t* db,
    const char* collection, char*** out_columns, size_t* out_count);

quiver_error_t quiver_database_read_time_series_files(quiver_database_t* db,
    const char* collection, char*** out_columns, char*** out_paths, size_t* out_count);

quiver_error_t quiver_database_update_time_series_files(quiver_database_t* db,
    const char* collection, const char* const* columns, const char* const* paths,
    size_t count);

quiver_error_t quiver_database_free_time_series_files(char** columns, char** paths, size_t count);
```

Note: For `update_time_series_group`, use `void**` not `const void* const*` for `column_data` in the cdef (consistent with Phase 4 decision about CFFI ABI-mode and const qualifiers).

### read_time_series_group Pattern
```python
def read_time_series_group(self, collection: str, group: str, id: int) -> list[dict]:
    self._ensure_open()
    lib = get_lib()
    out_names = ffi.new("char***")
    out_types = ffi.new("int**")
    out_data = ffi.new("void***")
    out_col_count = ffi.new("size_t*")
    out_row_count = ffi.new("size_t*")
    check(lib.quiver_database_read_time_series_group(
        self._ptr, collection.encode("utf-8"), group.encode("utf-8"), id,
        out_names, out_types, out_data, out_col_count, out_row_count,
    ))
    col_count = out_col_count[0]
    row_count = out_row_count[0]
    if col_count == 0 or row_count == 0:
        return []
    try:
        # Build column info
        columns = []
        for c in range(col_count):
            name = ffi.string(out_names[0][c]).decode("utf-8")
            ctype = out_types[0][c]
            columns.append((name, ctype))

        # Transpose columnar to row dicts
        rows = []
        for r in range(row_count):
            row = {}
            for c, (name, ctype) in enumerate(columns):
                if ctype == 0:  # INTEGER
                    row[name] = ffi.cast("int64_t*", out_data[0][c])[r]
                elif ctype == 1:  # FLOAT
                    row[name] = ffi.cast("double*", out_data[0][c])[r]
                else:  # STRING or DATE_TIME
                    row[name] = ffi.string(ffi.cast("char**", out_data[0][c])[r]).decode("utf-8")
            rows.append(row)
        return rows
    finally:
        lib.quiver_database_free_time_series_data(
            out_names[0], out_types[0], out_data[0], col_count, row_count)
```

### update_time_series_group Pattern
```python
def update_time_series_group(self, collection: str, id: int, group: str,
                             rows: list[dict]) -> None:
    self._ensure_open()
    lib = get_lib()
    c_collection = collection.encode("utf-8")
    c_group = group.encode("utf-8")

    if not rows:
        # Clear operation
        check(lib.quiver_database_update_time_series_group(
            self._ptr, c_collection, c_group, id,
            ffi.NULL, ffi.NULL, ffi.NULL, 0, 0))
        return

    # Get metadata for column schema
    metadata = self.get_time_series_metadata(collection, group)
    # ... validate rows, build columnar arrays, call C API
```

### Test Fixtures Needed
```python
# conftest.py addition
@pytest.fixture
def mixed_time_series_schema_path(schemas_path: Path) -> Path:
    return schemas_path / "valid" / "mixed_time_series.sql"

@pytest.fixture
def mixed_time_series_db(mixed_time_series_schema_path: Path, tmp_path: Path):
    database = Database.from_schema(
        str(tmp_path / "mixed_ts.db"), str(mixed_time_series_schema_path))
    yield database
    database.close()
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| N/A | void** columnar dispatch | Established in C API | Standard pattern across all bindings |

No deprecated or outdated approaches relevant to this phase.

## Open Questions

1. **Parameter order for `update_time_series_group`**
   - What we know: CONTEXT.md says `update_time_series_group(collection, id, group, rows)`. Julia uses `(collection, group, id; kwargs...)`. Dart uses `(collection, group, id, data)`. C API uses `(collection, group, id, ...)`.
   - What's unclear: The CONTEXT.md signature has `id` before `group`, but C API and other bindings have `group` before `id`.
   - Recommendation: Follow C API parameter order: `(collection, group, id, rows)` -- this matches Julia/Dart and keeps cross-binding consistency. The success criteria in the phase description also uses this order: `read_time_series_group(collection, id, group)` -- but this conflicts with the C API. Verify against the naming convention table which says Python uses same snake_case name as C++. C++ signature is `read_time_series_group(collection, group, id)`.
   - **Resolution:** Use C++/C API order: `(collection, group, id, ...)` for both read and update. This matches all other bindings.

2. **Validation: should Python validate row count consistency or let C API handle it?**
   - What we know: Julia validates vector lengths match before calling C API. Dart also validates. C API validates column_count vs schema but not intra-column row count consistency (each column array is assumed to be row_count long).
   - Recommendation: Validate in Python (all rows have same keys, all values have correct types) before marshaling. This gives better error messages than C API errors would.

## Sources

### Primary (HIGH confidence)
- `include/quiver/c/database.h` -- C API function signatures for all time series operations
- `src/c/database_time_series.cpp` -- C API implementation showing exact behavior, memory allocation patterns, and error handling
- `bindings/python/src/quiver/database.py` -- Existing Python binding patterns (Phase 1-4)
- `bindings/python/src/quiver/_c_api.py` -- Current CFFI cdef declarations
- `bindings/python/src/quiver/_declarations.py` -- Generator output showing expected cdef syntax
- `bindings/julia/src/database_read.jl` (lines 549-637) -- Julia time series read implementation
- `bindings/julia/src/database_update.jl` (lines 138-276) -- Julia time series update implementation
- `bindings/julia/test/test_database_time_series.jl` -- Julia test suite (477 lines, comprehensive coverage)
- `bindings/dart/lib/src/database_read.dart` (lines 895-970) -- Dart time series read implementation
- `bindings/dart/lib/src/database_update.dart` (lines 275-407) -- Dart time series update implementation
- `tests/schemas/valid/collections.sql` -- Schema with single-column time series + files table
- `tests/schemas/valid/mixed_time_series.sql` -- Schema with multi-column mixed-type time series (REAL, INTEGER, TEXT)

### Secondary (MEDIUM confidence)
- CLAUDE.md project instructions -- Architecture patterns, naming conventions, schema conventions

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- No new libraries; all infrastructure exists from Phases 1-4
- Architecture: HIGH -- C API is fully implemented and tested; Julia/Dart provide complete reference implementations; CFFI void** patterns proven in Phase 4
- Pitfalls: HIGH -- All identified from direct code analysis of existing implementations and C API contracts

**Research date:** 2026-02-23
**Valid until:** Indefinite (C API is stable; no external dependencies to version)
