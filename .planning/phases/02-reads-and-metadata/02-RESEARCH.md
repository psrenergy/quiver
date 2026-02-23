# Phase 2: Reads and Metadata - Research

**Researched:** 2026-02-23
**Domain:** Python CFFI bindings for C API read operations and metadata queries
**Confidence:** HIGH

## Summary

Phase 2 wraps 30+ C API read functions and 10+ metadata functions into Python methods on the `Database` class, plus introduces two new dataclass types (`ScalarMetadata`, `GroupMetadata`). The core challenge is correct memory management across CFFI boundaries -- every C-allocated array/string must be freed via the matching `quiver_database_free_*` function, using `try/finally` to guarantee cleanup even when marshaling fails.

The existing Phase 1 codebase establishes clear patterns: `_c_api.py` holds CFFI declarations, `_helpers.py` holds `check()` / `decode_string()` / `decode_string_or_none()`, and `database.py` holds all `Database` methods. The Julia and Dart bindings provide exact reference implementations for every method in this phase. The _declarations.py already contains all required C API declarations -- Phase 2 needs to add the read/metadata/free function declarations to `_c_api.py` (the hand-written cdef used at runtime).

**Primary recommendation:** Follow the existing Phase 1 pattern exactly. Add CFFI declarations for all read/free/metadata C API functions. Implement read methods with `try/finally` free patterns. Create `ScalarMetadata` and `GroupMetadata` as frozen dataclasses in a new `metadata.py` file. Test against the same schemas used by Julia/Dart.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Set reads (`read_set_*`) return `list`, not Python `set` -- consistent with vector reads and other bindings
- Bulk scalar reads return flat `list[int]`, `list[float]`, `list[str | None]` -- no ID keying
- Bulk vector/set reads return `list[list[int]]` etc. -- list of lists, matching Julia/Dart pattern
- `read_element_ids()` returns `list[int]`
- `read_scalar_relation()` returns `int | None` -- nullable foreign keys handled with None
- Bulk string reads with NULLs: `None` in list preserving positional alignment (`list[str | None]`)
- Field names match C struct exactly: `attribute_name`, `data_type`, `is_nullable`, `is_foreign_key`, `references_collection`, `references_column`
- GroupMetadata.value_columns is `list[ScalarMetadata]` -- nested composition, not flat dicts
- `_by_id` reads: return `None` for NULL attribute values (not raise)
- Missing element IDs: pass through C API behavior (don't add Python-side logic)
- Empty vector/set for existing element: return empty `list[]` (not None)
- Empty string `""` is distinct from NULL -- `""` for empty, `None` for NULL
- Consistency with Julia/Dart bindings is the guiding principle
- Phase 1 patterns (database.py, _helpers.py, check() function) serve as the foundation

### Claude's Discretion
- `list_scalar_attributes()` / `list_vector_groups()` return type (names-only `list[str]` or metadata objects)
- ScalarMetadata/GroupMetadata frozen vs mutable
- Whether ScalarMetadata/GroupMetadata are exported from `quiver.__init__`

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope

### Test Organization (Locked)
- Read tests split by data type: `test_database_read_scalar.py`, `test_database_read_vector.py`, `test_database_read_set.py`
- Metadata tests in one combined file: `test_database_metadata.py`
- Element ID reads and relation reads co-located in the relevant read test files (not separate files)
- Test schemas reused from `tests/schemas/valid/` (shared with C++/Julia/Dart, established in Phase 1)
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| READ-01 | `read_scalar_integers/floats/strings` for bulk reads with proper try/finally free | C API signatures fully mapped; Julia/Dart show exact free pattern; `quiver_database_free_integer_array`, `quiver_database_free_float_array`, `quiver_database_free_string_array` are the matching free functions |
| READ-02 | `read_scalar_integer/float/string_by_id` for single-element reads | C API uses `out_value` + `out_has_value` pattern; return `None` when `out_has_value == 0`; string variant needs `quiver_element_free_string` for cleanup |
| READ-03 | `read_vector_integers/floats/strings` for bulk vector reads with nested array marshaling | C API returns triple-pointer + sizes array + count; free with `quiver_database_free_integer_vectors` / `float_vectors` / `string_vectors`; Python must iterate outer array and build list of lists |
| READ-04 | `read_vector_integers/floats/strings_by_id` for single-element vector reads | Same as bulk scalar read pattern (`out_values` + `out_count`); free with `quiver_database_free_integer_array` / `float_array` / `string_array` |
| READ-05 | `read_set_integers/floats/strings` for bulk set reads | Identical structure to vector bulk reads; same triple-pointer pattern; same free functions (`free_integer_vectors`, etc.) |
| READ-06 | `read_set_integers/floats/strings_by_id` for single-element set reads | Same as vector by-id pattern; same free functions |
| READ-07 | `read_element_ids` for collection element ID listing | Simple `int64_t**` + `size_t*` pattern; free with `quiver_database_free_integer_array` |
| READ-08 | `read_scalar_relation` for relation reads | Returns `char***` + `size_t*` (string array); free with `quiver_database_free_string_array`; Julia handles NULL entries and empty strings as `None` |
| META-01 | `get_scalar_metadata` returning ScalarMetadata dataclass | C API fills `quiver_scalar_metadata_t` struct; free with `quiver_database_free_scalar_metadata`; Python must read struct fields and construct dataclass before freeing |
| META-02 | `get_vector_metadata`, `get_set_metadata`, `get_time_series_metadata` returning GroupMetadata | C API fills `quiver_group_metadata_t` struct with nested `value_columns` array; free with `quiver_database_free_group_metadata`; must iterate `value_columns` array via pointer arithmetic |
| META-03 | `list_scalar_attributes`, `list_vector_groups`, `list_set_groups`, `list_time_series_groups` | Returns arrays of metadata structs; free with `quiver_database_free_scalar_metadata_array` / `free_group_metadata_array`; must parse each element before freeing |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| cffi | (already installed) | ABI-mode FFI to call C API functions | Established in Phase 1; no additional dependencies |
| dataclasses | stdlib | ScalarMetadata / GroupMetadata types | Standard Python; frozen=True gives immutability + __eq__ + __hash__ for free |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| pytest | (already installed) | Test framework | All test files |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| @dataclass(frozen=True) | NamedTuple | NamedTuple is also immutable but less extensible; dataclass is more Pythonic and allows field documentation |
| @dataclass(frozen=True) | plain dict | Dicts lose type safety and IDE autocomplete; user decisions specify dataclasses |

## Architecture Patterns

### File Organization
```
bindings/python/src/quiver/
  _c_api.py           # Add read/metadata/free CFFI declarations (extend existing cdef)
  _helpers.py          # Existing: check(), decode_string(), decode_string_or_none()
  database.py          # Add all read/metadata methods to Database class
  metadata.py          # NEW: ScalarMetadata and GroupMetadata dataclasses
  __init__.py          # Export ScalarMetadata, GroupMetadata

bindings/python/tests/
  conftest.py                   # Add fixtures for collections.sql and relations.sql schemas
  test_database_read_scalar.py  # NEW: scalar reads + element IDs + relation reads
  test_database_read_vector.py  # NEW: vector reads (bulk + by-id)
  test_database_read_set.py     # NEW: set reads (bulk + by-id)
  test_database_metadata.py     # NEW: metadata get/list operations
```

### Pattern 1: Bulk Numeric Read (try/finally free)
**What:** Read a flat array of typed values from C, copy to Python list, free C memory.
**When to use:** `read_scalar_integers`, `read_scalar_floats`, `read_element_ids`
**Example:**
```python
def read_scalar_integers(self, collection: str, attribute: str) -> list[int]:
    self._ensure_open()
    lib = get_lib()
    out_values = ffi.new("int64_t**")
    out_count = ffi.new("size_t*")
    check(lib.quiver_database_read_scalar_integers(
        self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
        out_values, out_count,
    ))
    count = out_count[0]
    if count == 0 or out_values[0] == ffi.NULL:
        return []
    try:
        return [out_values[0][i] for i in range(count)]
    finally:
        lib.quiver_database_free_integer_array(out_values[0])
```

### Pattern 2: Bulk String Read (nullable entries)
**What:** Read a string array, handling NULL entries as None.
**When to use:** `read_scalar_strings`, `read_scalar_relation`
**Example:**
```python
def read_scalar_strings(self, collection: str, attribute: str) -> list[str | None]:
    self._ensure_open()
    lib = get_lib()
    out_values = ffi.new("char***")
    out_count = ffi.new("size_t*")
    check(lib.quiver_database_read_scalar_strings(
        self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
        out_values, out_count,
    ))
    count = out_count[0]
    if count == 0 or out_values[0] == ffi.NULL:
        return []
    try:
        result = []
        for i in range(count):
            ptr = out_values[0][i]
            if ptr == ffi.NULL:
                result.append(None)
            else:
                result.append(ffi.string(ptr).decode("utf-8"))
        return result
    finally:
        lib.quiver_database_free_string_array(out_values[0], count)
```

### Pattern 3: By-ID Read (optional return)
**What:** Read a single value by element ID, returning None if absent.
**When to use:** All `_by_id` scalar reads
**Example:**
```python
def read_scalar_integer_by_id(self, collection: str, attribute: str, id: int) -> int | None:
    self._ensure_open()
    lib = get_lib()
    out_value = ffi.new("int64_t*")
    out_has = ffi.new("int*")
    check(lib.quiver_database_read_scalar_integer_by_id(
        self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
        id, out_value, out_has,
    ))
    if out_has[0] == 0:
        return None
    return out_value[0]
```

### Pattern 4: Bulk Nested Array Read (vector/set)
**What:** Read a nested array (array of arrays) from C via triple-pointer + sizes.
**When to use:** `read_vector_integers`, `read_set_strings`, etc.
**Example:**
```python
def read_vector_integers(self, collection: str, attribute: str) -> list[list[int]]:
    self._ensure_open()
    lib = get_lib()
    out_vectors = ffi.new("int64_t***")
    out_sizes = ffi.new("size_t**")
    out_count = ffi.new("size_t*")
    check(lib.quiver_database_read_vector_integers(
        self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
        out_vectors, out_sizes, out_count,
    ))
    count = out_count[0]
    if count == 0 or out_vectors[0] == ffi.NULL:
        return []
    try:
        result = []
        for i in range(count):
            size = out_sizes[0][i]
            if out_vectors[0][i] == ffi.NULL or size == 0:
                result.append([])
            else:
                result.append([out_vectors[0][i][j] for j in range(size)])
        return result
    finally:
        lib.quiver_database_free_integer_vectors(out_vectors[0], out_sizes[0], count)
```

### Pattern 5: Metadata Struct Parsing
**What:** Allocate a C struct, call metadata function, parse fields into dataclass, free.
**When to use:** `get_scalar_metadata`, `get_vector_metadata`, etc.
**Example:**
```python
def get_scalar_metadata(self, collection: str, attribute: str) -> ScalarMetadata:
    self._ensure_open()
    lib = get_lib()
    out = ffi.new("quiver_scalar_metadata_t*")
    check(lib.quiver_database_get_scalar_metadata(
        self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"), out,
    ))
    try:
        return _parse_scalar_metadata(out)
    finally:
        lib.quiver_database_free_scalar_metadata(out)
```

### Pattern 6: Metadata Array Parsing (list operations)
**What:** Read an array of metadata structs, parse each into dataclass, free the array.
**When to use:** `list_scalar_attributes`, `list_vector_groups`, etc.
**Example:**
```python
def list_scalar_attributes(self, collection: str) -> list[ScalarMetadata]:
    self._ensure_open()
    lib = get_lib()
    out_metadata = ffi.new("quiver_scalar_metadata_t**")
    out_count = ffi.new("size_t*")
    check(lib.quiver_database_list_scalar_attributes(
        self._ptr, collection.encode("utf-8"), out_metadata, out_count,
    ))
    count = out_count[0]
    if count == 0 or out_metadata[0] == ffi.NULL:
        return []
    try:
        return [_parse_scalar_metadata(out_metadata[0] + i) for i in range(count)]
    finally:
        lib.quiver_database_free_scalar_metadata_array(out_metadata[0], count)
```

### Anti-Patterns to Avoid
- **Freeing before copying:** Never let `try` block exit before all Python values are copied from C buffers. The `try/finally` must wrap only the marshaling-to-Python step; the C API call itself should happen before the `try`.
- **Missing free on error path:** If the C API call succeeds but Python marshaling raises (e.g., decode error), the `finally` must still call the free function.
- **Encoding strings inline:** Use `.encode("utf-8")` consistently. Do NOT use `ffi.new("char[]", s)` for input strings -- CFFI accepts plain `bytes` for `const char*` parameters.
- **Forgetting _ensure_open():** Every public Database method must call `self._ensure_open()` first.

## Discretion Recommendations

### list_scalar_attributes / list_vector_groups return type
**Recommendation: Return full metadata objects** (`list[ScalarMetadata]` / `list[GroupMetadata]`).

Rationale:
1. The C API already returns full metadata arrays, not just names. Discarding the metadata to return only names wastes information.
2. Julia returns `ScalarMetadata[]` / `GroupMetadata[]` from `list_scalar_attributes` / `list_vector_groups`. Dart returns full record types too.
3. Callers who only want names can do `[m.name for m in db.list_scalar_attributes("Foo")]`.
4. The `read_all_scalars_by_id` convenience method (future Phase 6) needs the `data_type` field from the returned metadata to dispatch typed reads -- returning full metadata avoids a second API call.

**Confidence:** HIGH -- this matches both bindings exactly.

### ScalarMetadata / GroupMetadata: frozen vs mutable
**Recommendation: Use `@dataclass(frozen=True)`.**

Rationale:
1. Metadata is read-only data from the database schema. Mutation makes no sense.
2. Frozen dataclasses get `__hash__` and `__eq__` for free, making them usable in sets/dicts.
3. Signals immutability to callers, matching the const-ness of the underlying data.

**Confidence:** HIGH -- standard Python practice for value objects.

### Export from quiver.__init__
**Recommendation: Export both `ScalarMetadata` and `GroupMetadata` from `quiver.__init__`.**

Rationale:
1. Users need these types for type annotations and isinstance checks.
2. `from quiver import ScalarMetadata, GroupMetadata` is the expected import pattern.
3. Julia exports these types from the `Quiver` module. Dart includes them in the public API.

**Confidence:** HIGH.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Immutable value types | Manual `__eq__`/`__hash__`/`__repr__` | `@dataclass(frozen=True)` | stdlib handles it correctly |
| C memory deallocation | Custom ref counting or garbage collector hooks | `try/finally` with C API free functions | Matches Julia/Dart pattern; deterministic cleanup |
| C struct field access | Manual `ffi.cast` + offset arithmetic | `ffi.new("struct_type*")` + field access via `.field_name` | CFFI provides typed access to struct fields |
| String encode/decode | Custom helpers | `_helpers.py` `decode_string()` / `decode_string_or_none()` | Already established in Phase 1 |

**Key insight:** Every C API function has a paired free function. The Python binding's only job is to call the API, copy data to Python objects, then free. No Python-side caching, lazy loading, or smart pointers.

## Common Pitfalls

### Pitfall 1: CFFI Pointer Arithmetic for Struct Arrays
**What goes wrong:** CFFI `ffi.new("struct_type**")` returns a pointer-to-pointer. Accessing elements in a returned array of structs requires correct pointer arithmetic. `out_metadata[0][i]` works for CFFI-managed memory, but for C-allocated arrays returned via out-parameters, you may need to use `out_metadata[0] + i` to index.
**Why it happens:** CFFI treats pointer types as indexable, but the behavior differs for heap-allocated arrays returned by C.
**How to avoid:** For arrays of structs returned via `struct_type** out`, use `out[0][i]` which is valid CFFI syntax for indexing into the pointed-to array. Test with a known schema to verify field values are correct.
**Warning signs:** Getting garbage values or segfaults when reading the second element of an array.

### Pitfall 2: String Read Memory Leak via Early Return
**What goes wrong:** `read_scalar_string_by_id` returns a C-allocated string (via `char** out_value`). If the has_value check passes but the string decode raises, the C string leaks.
**Why it happens:** The free function (`quiver_element_free_string`) is not called on the error path.
**How to avoid:** Structure as: check C API call -> if has_value, wrap decode+free in try/finally -> return result.
**Warning signs:** Memory growth in long-running test suites with string reads.

### Pitfall 3: read_scalar_relation NULL vs Empty String
**What goes wrong:** `read_scalar_relation` returns string labels for related elements. Per the Julia binding, NULL pointers AND empty strings both map to `None` (the relation is unset). If Python only checks for NULL, empty-string relations would incorrectly return `""`.
**Why it happens:** The C API uses `copy_strings_to_c` which converts C++ empty strings to allocated empty C strings (not NULL). Julia explicitly handles this: `isempty(s) ? nothing : s`.
**How to avoid:** In `read_scalar_relation`, check both `ptr == ffi.NULL` and `ffi.string(ptr) == b""` before appending to result list. Map both to `None`.
**Warning signs:** Getting `""` instead of `None` for elements without set relations.

### Pitfall 4: Bulk Vector/Set String Arrays (Quadruple Pointer)
**What goes wrong:** `read_vector_strings` and `read_set_strings` return `char**** out_vectors` (a pointer to an array of arrays of strings). This four-level pointer indirection is easy to get wrong in CFFI.
**Why it happens:** The C API represents `vector<vector<string>>` as `char***` outer array, where each element is a `char**` inner array of strings.
**How to avoid:** Declare in CFFI cdef as `char**** out_vectors`. Access as: `out_vectors[0]` = the `char***`, `out_vectors[0][i]` = the `char**` for element i, `out_vectors[0][i][j]` = the `char*` for string j in element i. Free with `quiver_database_free_string_vectors(out_vectors[0], out_sizes[0], count)`.
**Warning signs:** Segfaults or incorrect string values from nested vector/set reads.

### Pitfall 5: GroupMetadata value_columns Nested Array
**What goes wrong:** `quiver_group_metadata_t` has a `value_columns` field that is a pointer to an array of `quiver_scalar_metadata_t` structs. Accessing nested struct arrays through CFFI requires correct indexing.
**Why it happens:** CFFI allows `metadata.value_columns[i]` to index into the pointed-to array of structs. But the struct size must be known to CFFI for correct offset calculation.
**How to avoid:** Ensure the `quiver_scalar_metadata_t` and `quiver_group_metadata_t` struct definitions are in the CFFI cdef. Then `metadata.value_columns[i]` will correctly access the i-th struct. Parse all value_columns before calling the free function.
**Warning signs:** Incorrect metadata for the second or third value column in a multi-column group.

### Pitfall 6: CFFI cdef for _Bool type
**What goes wrong:** The generated `_declarations.py` uses `_Bool` for `quiver_database_in_transaction`'s out parameter. CFFI may not recognize `_Bool` on all platforms.
**Why it happens:** C99 `_Bool` is mapped differently by different CFFI backends.
**How to avoid:** In the hand-written `_c_api.py` cdef, use `int*` instead of `_Bool*` for boolean out-parameters (as already done for `quiver_database_is_healthy`). The C API actually uses `bool*` (from `<stdbool.h>`) but CFFI ABI mode works fine with `int*` since both are the same size on all target platforms for simple flag values.
**Warning signs:** CFFI parse error on `_Bool` type.

## Code Examples

### CFFI Declarations to Add (read operations)
```c
// In _c_api.py ffi.cdef() -- add after existing declarations

// Read scalar attributes
quiver_error_t quiver_database_read_scalar_integers(quiver_database_t* db,
    const char* collection, const char* attribute,
    int64_t** out_values, size_t* out_count);
quiver_error_t quiver_database_read_scalar_floats(quiver_database_t* db,
    const char* collection, const char* attribute,
    double** out_values, size_t* out_count);
quiver_error_t quiver_database_read_scalar_strings(quiver_database_t* db,
    const char* collection, const char* attribute,
    char*** out_values, size_t* out_count);

// Read scalar by ID
quiver_error_t quiver_database_read_scalar_integer_by_id(quiver_database_t* db,
    const char* collection, const char* attribute, int64_t id,
    int64_t* out_value, int* out_has_value);
quiver_error_t quiver_database_read_scalar_float_by_id(quiver_database_t* db,
    const char* collection, const char* attribute, int64_t id,
    double* out_value, int* out_has_value);
quiver_error_t quiver_database_read_scalar_string_by_id(quiver_database_t* db,
    const char* collection, const char* attribute, int64_t id,
    char** out_value, int* out_has_value);

// Read vector attributes (bulk)
quiver_error_t quiver_database_read_vector_integers(quiver_database_t* db,
    const char* collection, const char* attribute,
    int64_t*** out_vectors, size_t** out_sizes, size_t* out_count);
quiver_error_t quiver_database_read_vector_floats(quiver_database_t* db,
    const char* collection, const char* attribute,
    double*** out_vectors, size_t** out_sizes, size_t* out_count);
quiver_error_t quiver_database_read_vector_strings(quiver_database_t* db,
    const char* collection, const char* attribute,
    char**** out_vectors, size_t** out_sizes, size_t* out_count);

// Read vector by ID
quiver_error_t quiver_database_read_vector_integers_by_id(quiver_database_t* db,
    const char* collection, const char* attribute, int64_t id,
    int64_t** out_values, size_t* out_count);
quiver_error_t quiver_database_read_vector_floats_by_id(quiver_database_t* db,
    const char* collection, const char* attribute, int64_t id,
    double** out_values, size_t* out_count);
quiver_error_t quiver_database_read_vector_strings_by_id(quiver_database_t* db,
    const char* collection, const char* attribute, int64_t id,
    char*** out_values, size_t* out_count);

// Read set attributes (bulk) -- same structure as vectors
quiver_error_t quiver_database_read_set_integers(quiver_database_t* db,
    const char* collection, const char* attribute,
    int64_t*** out_sets, size_t** out_sizes, size_t* out_count);
quiver_error_t quiver_database_read_set_floats(quiver_database_t* db,
    const char* collection, const char* attribute,
    double*** out_sets, size_t** out_sizes, size_t* out_count);
quiver_error_t quiver_database_read_set_strings(quiver_database_t* db,
    const char* collection, const char* attribute,
    char**** out_sets, size_t** out_sizes, size_t* out_count);

// Read set by ID
quiver_error_t quiver_database_read_set_integers_by_id(quiver_database_t* db,
    const char* collection, const char* attribute, int64_t id,
    int64_t** out_values, size_t* out_count);
quiver_error_t quiver_database_read_set_floats_by_id(quiver_database_t* db,
    const char* collection, const char* attribute, int64_t id,
    double** out_values, size_t* out_count);
quiver_error_t quiver_database_read_set_strings_by_id(quiver_database_t* db,
    const char* collection, const char* attribute, int64_t id,
    char*** out_values, size_t* out_count);

// Read element IDs
quiver_error_t quiver_database_read_element_ids(quiver_database_t* db,
    const char* collection, int64_t** out_ids, size_t* out_count);

// Read scalar relation
quiver_error_t quiver_database_read_scalar_relation(quiver_database_t* db,
    const char* collection, const char* attribute,
    char*** out_values, size_t* out_count);

// Free functions for read results
quiver_error_t quiver_database_free_integer_array(int64_t* values);
quiver_error_t quiver_database_free_float_array(double* values);
quiver_error_t quiver_database_free_string_array(char** values, size_t count);
quiver_error_t quiver_database_free_integer_vectors(int64_t** vectors, size_t* sizes, size_t count);
quiver_error_t quiver_database_free_float_vectors(double** vectors, size_t* sizes, size_t count);
quiver_error_t quiver_database_free_string_vectors(char*** vectors, size_t* sizes, size_t count);

// Metadata types
typedef struct {
    const char* name;
    quiver_data_type_t data_type;
    int not_null;
    int primary_key;
    const char* default_value;
    int is_foreign_key;
    const char* references_collection;
    const char* references_column;
} quiver_scalar_metadata_t;

typedef struct {
    const char* group_name;
    const char* dimension_column;
    quiver_scalar_metadata_t* value_columns;
    size_t value_column_count;
} quiver_group_metadata_t;

// Metadata queries
quiver_error_t quiver_database_get_scalar_metadata(quiver_database_t* db,
    const char* collection, const char* attribute,
    quiver_scalar_metadata_t* out_metadata);
quiver_error_t quiver_database_get_vector_metadata(quiver_database_t* db,
    const char* collection, const char* group_name,
    quiver_group_metadata_t* out_metadata);
quiver_error_t quiver_database_get_set_metadata(quiver_database_t* db,
    const char* collection, const char* group_name,
    quiver_group_metadata_t* out_metadata);
quiver_error_t quiver_database_get_time_series_metadata(quiver_database_t* db,
    const char* collection, const char* group_name,
    quiver_group_metadata_t* out_metadata);

// Metadata free
quiver_error_t quiver_database_free_scalar_metadata(quiver_scalar_metadata_t* metadata);
quiver_error_t quiver_database_free_group_metadata(quiver_group_metadata_t* metadata);

// Metadata list
quiver_error_t quiver_database_list_scalar_attributes(quiver_database_t* db,
    const char* collection,
    quiver_scalar_metadata_t** out_metadata, size_t* out_count);
quiver_error_t quiver_database_list_vector_groups(quiver_database_t* db,
    const char* collection,
    quiver_group_metadata_t** out_metadata, size_t* out_count);
quiver_error_t quiver_database_list_set_groups(quiver_database_t* db,
    const char* collection,
    quiver_group_metadata_t** out_metadata, size_t* out_count);
quiver_error_t quiver_database_list_time_series_groups(quiver_database_t* db,
    const char* collection,
    quiver_group_metadata_t** out_metadata, size_t* out_count);

// Metadata array free
quiver_error_t quiver_database_free_scalar_metadata_array(
    quiver_scalar_metadata_t* metadata, size_t count);
quiver_error_t quiver_database_free_group_metadata_array(
    quiver_group_metadata_t* metadata, size_t count);
```

### ScalarMetadata Dataclass
```python
from __future__ import annotations
from dataclasses import dataclass

@dataclass(frozen=True)
class ScalarMetadata:
    """Metadata for a scalar attribute in a collection."""
    name: str
    data_type: int
    not_null: bool
    primary_key: bool
    default_value: str | None
    is_foreign_key: bool
    references_collection: str | None
    references_column: str | None
```

### GroupMetadata Dataclass
```python
@dataclass(frozen=True)
class GroupMetadata:
    """Metadata for a vector, set, or time series group in a collection."""
    group_name: str
    dimension_column: str  # empty string for vector/set groups
    value_columns: list[ScalarMetadata]
```

### Metadata Parsing Helpers
```python
def _parse_scalar_metadata(meta) -> ScalarMetadata:
    """Parse a quiver_scalar_metadata_t struct into a ScalarMetadata dataclass."""
    return ScalarMetadata(
        name=decode_string(meta.name),
        data_type=int(meta.data_type),
        not_null=bool(meta.not_null),
        primary_key=bool(meta.primary_key),
        default_value=decode_string_or_none(meta.default_value),
        is_foreign_key=bool(meta.is_foreign_key),
        references_collection=decode_string_or_none(meta.references_collection),
        references_column=decode_string_or_none(meta.references_column),
    )

def _parse_group_metadata(meta) -> GroupMetadata:
    """Parse a quiver_group_metadata_t struct into a GroupMetadata dataclass."""
    value_columns = []
    for i in range(meta.value_column_count):
        value_columns.append(_parse_scalar_metadata(meta.value_columns[i]))
    return GroupMetadata(
        group_name=decode_string(meta.group_name),
        dimension_column="" if meta.dimension_column == ffi.NULL else decode_string(meta.dimension_column),
        value_columns=value_columns,
    )
```

### Test Schema Requirements

Tests need these schemas (all exist in `tests/schemas/valid/`):
- **basic.sql** -- Configuration with scalar integers/floats/strings/dates. Used for: scalar reads, scalar by-id, element IDs.
- **collections.sql** -- Collection with vectors (`value_int`, `value_float`) and sets (`tag`). Used for: vector reads, set reads, metadata queries.
- **relations.sql** -- Parent/Child with foreign keys. Used for: relation reads, FK metadata.

### Test Fixture Additions Needed
```python
# conftest.py -- add these fixtures
@pytest.fixture
def collections_schema_path(schemas_path: Path) -> Path:
    return schemas_path / "valid" / "collections.sql"

@pytest.fixture
def relations_schema_path(schemas_path: Path) -> Path:
    return schemas_path / "valid" / "relations.sql"
```

### Note on create_element Dependency
Phase 2 read tests need data in the database. The `create_element` method is not yet implemented on the Python `Database` class (it is Phase 3 / WRITE-01). However, Phase 1 already declared `quiver_database_create_element` in the CFFI cdef, and the method could be added as a minimal helper in Phase 2 to unblock tests -- or tests could use a raw C API call via `ffi`/`get_lib()` to insert test data.

**Recommended approach:** Add a minimal `create_element()` method to `Database` in Phase 2 since it is already declared in the cdef. This keeps tests clean and avoids raw FFI calls in test code. The Phase 3 WRITE-01 implementation can then enhance or validate this method. Alternatively, a test-only helper function in conftest.py could wrap the raw C API call.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| CFFI API mode (compile C extension) | CFFI ABI mode (dlopen) | Project design decision | No compiler needed at install time; slightly slower but simpler |
| Properties as @property | Properties as methods | Phase 1 decision | `db.path()` not `db.path` -- consistent across all methods |

## Open Questions

1. **create_element for test data**
   - What we know: The cdef already declares `quiver_database_create_element`. Tests need data.
   - What's unclear: Should Phase 2 add a minimal `create_element()` to Database, or use a test helper?
   - Recommendation: Add `create_element()` to Database in Phase 2 since the cdef already supports it. Mark it as "minimal implementation, enhanced in Phase 3." This keeps test code clean and validates the create path early.

2. **data_type field: raw int vs Python enum**
   - What we know: The C API uses `quiver_data_type_t` enum (INTEGER=0, FLOAT=1, STRING=2, DATE_TIME=3). Julia stores the raw C enum value. Dart stores raw int.
   - What's unclear: Should Python expose a Python IntEnum or raw int?
   - Recommendation: Store as `int` in the dataclass (matching Julia/Dart). Optionally define constants `QUIVER_DATA_TYPE_INTEGER = 0` etc. in a constants module or on the metadata module for user convenience. But this is not required for Phase 2 -- the raw int is sufficient and matches other bindings.

## Sources

### Primary (HIGH confidence)
- `include/quiver/c/database.h` -- C API function signatures, struct definitions, and free functions. All 30+ read functions and 10+ metadata functions verified.
- `include/quiver/c/common.h` -- Error codes and utility function declarations.
- `src/c/database_read.cpp` -- C API read implementation, confirming memory allocation patterns (new[]/delete[]).
- `src/c/database_metadata.cpp` -- C API metadata implementation, confirming struct allocation and free patterns.
- `src/c/database_helpers.h` -- Template helpers showing `read_scalars_impl`, `read_vectors_impl`, `copy_strings_to_c`, metadata converters.
- `bindings/julia/src/database_read.jl` -- Julia reference implementation for all read functions.
- `bindings/julia/src/database_metadata.jl` -- Julia reference implementation for all metadata functions and struct definitions.
- `bindings/julia/test/test_database_read.jl` -- Julia test patterns for reads.
- `bindings/julia/test/test_database_metadata.jl` -- Julia test patterns for metadata.
- `bindings/julia/test/test_database_relations.jl` -- Julia test patterns for relations.
- `bindings/dart/lib/src/database_metadata.dart` -- Dart reference implementation for metadata.
- `bindings/python/src/quiver/_c_api.py` -- Current Phase 1 CFFI declarations (extend this).
- `bindings/python/src/quiver/_helpers.py` -- Current helper functions (reuse these).
- `bindings/python/src/quiver/database.py` -- Current Database class (add methods here).
- `bindings/python/src/quiver/_declarations.py` -- Full generated declarations (reference for correct signatures).
- `tests/schemas/valid/basic.sql`, `collections.sql`, `relations.sql` -- Test schemas.

### Secondary (MEDIUM confidence)
- None needed -- all information sourced from codebase.

### Tertiary (LOW confidence)
- None.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- using only stdlib `dataclasses` and existing `cffi` dependency
- Architecture: HIGH -- direct codebase analysis of 3 existing bindings (Julia, Dart, C API implementation)
- Pitfalls: HIGH -- identified from actual C API memory patterns and CFFI behavior with complex pointer types

**Research date:** 2026-02-23
**Valid until:** Indefinite (project-internal codebase analysis, no external dependencies)
