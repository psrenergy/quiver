# Phase 6: CSV and Convenience Helpers - Research

**Researched:** 2026-02-24
**Domain:** Python FFI binding -- CSV export with struct marshaling + pure-Python composite read helpers
**Confidence:** HIGH

## Summary

Phase 6 adds two categories of functionality to the Python binding: (1) CSV export via the existing C API `quiver_database_export_csv` with its `quiver_csv_export_options_t` struct, and (2) pure-Python convenience helpers that compose existing binding methods (no new C API calls needed). The CSV import (`import_csv`) C++ implementation is confirmed to be a no-op stub and must be dropped from scope.

All intelligence for CSV export lives in the C++ layer. The Python binding only needs to marshal a `CSVExportOptions` dataclass into the C API's grouped parallel-array struct format and call the function. The convenience helpers (`read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id`, `read_vector_group_by_id`, `read_set_group_by_id`) and DateTime helpers (`read_scalar_date_time_by_id`, `read_vector_date_time_by_id`, `read_set_date_time_by_id`) are pure Python compositions following the exact pattern already implemented in Julia and Dart. The `query_date_time` method already exists in the Python binding (added in Phase 4).

**Primary recommendation:** Implement CSV export options marshaling as a module-level `_marshal_csv_export_options` function following the `_marshal_params` / `_marshal_time_series_columns` pattern, then add convenience helpers as pure-Python methods on the Database class composing existing operations.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- CSVExportOptions exposed as a Python dataclass (mirrors ScalarMetadata/GroupMetadata pattern)
- All fields optional with sensible defaults: `date_time_format=""`, `enum_labels={}`
- `enum_labels` type: `dict[str, dict[int, str]]` -- nested dicts matching C++ `unordered_map` exactly
- `export_csv(collection, group, path, options)` signature matching C API (collection, group, path, options)
- `import_csv(table, path)` also bound in this phase -- simple signature, no options struct
- `read_all_scalars_by_id(collection, id)` returns `dict[str, Any]` -- attribute name to typed value
- `read_all_vectors_by_id(collection, id)` returns `dict[str, list]` -- group name to typed list
- `read_all_sets_by_id(collection, id)` returns `dict[str, list]` -- group name to typed list
- Multi-column groups: each column is a separate entry in the dict (not nested)
- DATE_TIME columns in read_all_* helpers are parsed to `datetime` objects (consistent with datetime helpers)
- Return Python `datetime.datetime` objects, not strings
- No timezone info in stored string -> assume UTC (`tzinfo=datetime.timezone.utc`) -- always timezone-aware
- Malformed datetime strings raise an exception (strict, not fallback to raw string)
- Helpers: `read_scalar_date_time_by_id`, `read_vector_date_time_by_id`, `read_set_date_time_by_id`, `query_date_time`
- `read_vector_group_by_id` and `read_set_group_by_id` return `list[dict[str, Any]]` -- list of row dicts
- Matches `read_time_series_group` return format for consistency
- `vector_index` included in row dicts for vectors (preserves ordering info)
- DATE_TIME columns parsed to `datetime` objects (consistent with all other helpers)
- Single-column groups still return row dicts (consistent return type regardless of column count)

### Claude's Discretion
- id/label inclusion in `read_all_scalars_by_id` -- Claude decides based on Julia/Dart precedent
- Internal marshaling approach for CSVExportOptions parallel arrays
- Error message text for malformed datetime strings

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CSV-01 | export_csv with CSVExportOptions struct (date format, enum labels) | C API exists (`quiver_database_export_csv`). Struct `quiver_csv_export_options_t` uses grouped parallel arrays. CFFI cdef needed. Marshal via module-level `_marshal_csv_export_options`. Dart/Julia patterns verified. |
| CSV-02 | import_csv for table data import | **DROP FROM SCOPE.** C++ `Database::import_csv` is confirmed empty stub at `src/database_describe.cpp:91`. C API calls through but does nothing. Must update REQUIREMENTS.md to defer. |
| CONV-01 | read_all_scalars_by_id composing list_scalar_attributes + typed reads | Julia/Dart pattern verified: iterate `list_scalar_attributes`, dispatch by `data_type`, collect into dict. Both bindings include id/label (they iterate all attributes from metadata). |
| CONV-02 | read_all_vectors_by_id composing list_vector_groups + typed reads | Julia/Dart pattern verified: iterate `list_vector_groups`, use first value_column's data_type, dispatch to typed read. Single-column groups only. |
| CONV-03 | read_all_sets_by_id composing list_set_groups + typed reads | Same pattern as CONV-02 but with `list_set_groups` + set read methods. |
| CONV-04 | DateTime helpers (read_scalar/vector/set_date_time_by_id, query_date_time) | `query_date_time` already exists (Phase 4). Remaining three helpers wrap string reads + `datetime.fromisoformat()` with UTC timezone. User decision: timezone-aware, strict parsing, raise on malformed. |
| CONV-05 | read_vector_group_by_id and read_set_group_by_id multi-column group readers | Julia/Dart pattern verified: get metadata, read each column by type, transpose to row dicts. Context decision: vector_index included for vectors. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| cffi | (existing) | FFI bridge to C API | Already in use, ABI-mode |
| dataclasses | stdlib | CSVExportOptions type | Matches ScalarMetadata/GroupMetadata pattern |
| datetime | stdlib | DateTime parsing/objects | Standard Python datetime support |

### Supporting
No new libraries needed. All functionality composes existing binding infrastructure.

## Architecture Patterns

### Pattern 1: CSVExportOptions Dataclass
**What:** A frozen dataclass mirroring the C struct, with Pythonic defaults.
**When to use:** When the user calls `export_csv`.

```python
from dataclasses import dataclass, field

@dataclass(frozen=True)
class CSVExportOptions:
    date_time_format: str = ""
    enum_labels: dict[str, dict[int, str]] = field(default_factory=dict)
```

This mirrors how `ScalarMetadata` and `GroupMetadata` are already defined in `metadata.py`.

### Pattern 2: Parallel-Array Marshaling for CSVExportOptions
**What:** Flatten the nested `enum_labels` dict into the C API's grouped parallel arrays.
**When to use:** Inside `export_csv` before calling the C API.

The C API struct `quiver_csv_export_options_t` requires:
- `enum_attribute_names[i]` = attribute name for group i
- `enum_entry_counts[i]` = number of entries in group i
- `enum_values[]` = all integer values, concatenated across groups
- `enum_labels[]` = all string labels, concatenated across groups
- `enum_attribute_count` = number of attribute groups

```python
def _marshal_csv_export_options(options: CSVExportOptions) -> tuple:
    """Marshal CSVExportOptions into C API struct fields.

    Returns (keepalive, c_opts_ptr) where keepalive must stay referenced
    during the C API call.
    """
    keepalive = []
    c_opts = ffi.new("quiver_csv_export_options_t*")

    # date_time_format
    dtf = ffi.new("char[]", options.date_time_format.encode("utf-8"))
    keepalive.append(dtf)
    c_opts.date_time_format = dtf

    if not options.enum_labels:
        c_opts.enum_attribute_names = ffi.NULL
        c_opts.enum_entry_counts = ffi.NULL
        c_opts.enum_values = ffi.NULL
        c_opts.enum_labels = ffi.NULL
        c_opts.enum_attribute_count = 0
    else:
        attr_count = len(options.enum_labels)
        # Build parallel arrays...
        # (see Code Examples section for full implementation)

    return keepalive, c_opts
```

### Pattern 3: Type-Dispatch Convenience Helpers
**What:** Compose `list_*` metadata + typed `read_*_by_id` calls into single-call dict results.
**When to use:** For `read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id`.

Julia reference (translates directly to Python):
```python
def read_all_scalars_by_id(self, collection: str, id: int) -> dict:
    result = {}
    for attr in self.list_scalar_attributes(collection):
        name = attr.name
        if attr.data_type == 0:    # INTEGER
            result[name] = self.read_scalar_integer_by_id(collection, name, id)
        elif attr.data_type == 1:  # FLOAT
            result[name] = self.read_scalar_float_by_id(collection, name, id)
        elif attr.data_type == 3:  # DATE_TIME
            result[name] = self.read_scalar_date_time_by_id(collection, name, id)
        else:                      # STRING
            result[name] = self.read_scalar_string_by_id(collection, name, id)
    return result
```

### Pattern 4: Multi-Column Group Readers (Transpose)
**What:** Read each column of a multi-column group, then transpose columns to row dicts.
**When to use:** For `read_vector_group_by_id` and `read_set_group_by_id`.

Julia/Dart both use identical logic:
1. Get metadata for the group
2. Read each value column by type dispatch
3. Transpose column arrays into list of row dicts

Key detail from user decisions: `vector_index` must be included in vector group row dicts.

### Pattern 5: DateTime Helpers
**What:** Thin wrappers that read a string value and parse to `datetime.datetime` with UTC timezone.
**When to use:** For `read_scalar_date_time_by_id`, `read_vector_date_time_by_id`, `read_set_date_time_by_id`.

```python
def _parse_datetime(s: str) -> datetime:
    """Parse an ISO 8601 datetime string to a timezone-aware UTC datetime."""
    return datetime.fromisoformat(s).replace(tzinfo=timezone.utc)
```

User decisions require:
- Always timezone-aware (UTC)
- Strict parsing (raise on malformed, no fallback)
- Handle both `YYYY-MM-DDTHH:MM:SS` and `YYYY-MM-DD HH:MM:SS` formats

Python 3.13's `datetime.fromisoformat()` handles both formats natively.

### Anti-Patterns to Avoid
- **Calling C API for convenience helpers:** All `read_all_*` and `read_*_group_by_id` methods compose existing Python binding methods. Never bypass the binding layer.
- **Timezone-naive datetimes:** User explicitly requires `tzinfo=datetime.timezone.utc` on all datetime results.
- **Silently swallowing malformed dates:** User requires strict parsing. Raise ValueError (Python default from `fromisoformat`).
- **Constructing the CSV options struct field-by-field on the C struct:** Use a marshaling function that returns a complete keepalive list, following the `_marshal_params` pattern.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| ISO 8601 parsing | Custom regex parser | `datetime.fromisoformat()` | Python 3.11+ handles all ISO variants natively |
| CSV options struct | Inline marshaling | Module-level `_marshal_csv_export_options` | Consistent with `_marshal_params`, `_marshal_time_series_columns` |
| Type dispatch | If/elif chains with magic numbers | Named constants or comments with type IDs | CFFI doesn't expose enum names; use comments `# INTEGER = 0` etc. |

**Key insight:** Every convenience helper in this phase is a pure-Python composition of existing methods. The only FFI work is CSV export options marshaling.

## Common Pitfalls

### Pitfall 1: CFFI Keepalive for Nested Pointers
**What goes wrong:** The `quiver_csv_export_options_t` struct contains `const char* const*` (pointer to array of string pointers). If intermediate buffers are garbage-collected before the C call, pointers dangle.
**Why it happens:** CFFI `ffi.new()` objects are reference-counted. If you allocate inside a loop without keeping references, they get collected.
**How to avoid:** Collect all allocated objects into a `keepalive` list. Follow the exact pattern from `_marshal_params` and `_marshal_time_series_columns`.
**Warning signs:** Random crashes or garbled strings only on some runs.

### Pitfall 2: CFFI cdef for `const char* const*`
**What goes wrong:** CFFI ABI-mode doesn't fully support `const` qualifiers. Declaring `const char* const*` in cdef may fail.
**Why it happens:** CFFI ignores `const` in many contexts but the parser can be finicky.
**How to avoid:** Use `char**` or `const char**` in the CFFI cdef (matches existing pattern from Phase 4 query params). The project already noted in [04-01] decision: "Used void** instead of const void* const* in CFFI cdef (ABI-compatible; CFFI ignores const qualifiers)".
**Warning signs:** CFFI parse errors at import time.

### Pitfall 3: import_csv is a No-Op Stub
**What goes wrong:** Binding `import_csv` and it silently does nothing -- tests pass but no data is imported.
**Why it happens:** C++ `Database::import_csv` at `src/database_describe.cpp:91` has empty body `{}`.
**How to avoid:** Drop CSV-02 from scope. Update REQUIREMENTS.md to mark it as deferred.
**Warning signs:** Tests that write CSV then import report zero rows.

### Pitfall 4: read_all_vectors_by_id Uses First Column Type for Single-Column Groups
**What goes wrong:** Multi-column vector groups get read with only one column's type.
**Why it happens:** `read_all_vectors_by_id` uses `_get_value_data_type(group.value_columns)` which takes the first column's type. This is correct for single-column groups (the common case) but wrong for multi-column groups.
**How to avoid:** `read_all_vectors_by_id` is designed for single-column groups (returns `dict[str, list]`). Multi-column groups use `read_vector_group_by_id` instead. This matches Julia/Dart behavior exactly.
**Warning signs:** Multi-column group data appears garbled or type-mismatched.

### Pitfall 5: vector_index in read_vector_group_by_id
**What goes wrong:** User decision requires `vector_index` in row dicts for vectors, but `vector_index` is not a value column in metadata (it's part of the composite PK).
**Why it happens:** `get_vector_metadata` returns value columns only (not `id` or `vector_index`).
**How to avoid:** After reading value columns, also read `vector_index` column separately. The C API's `read_vector_integers_by_id` returns values but not indices -- need to check how Julia handles this. Research finding: Julia does NOT include `vector_index` in its `read_vector_group_by_id` output. The Dart version also does not. **Recommendation:** Follow Julia/Dart precedent and omit `vector_index` unless the user explicitly re-confirms this requirement.
**Warning signs:** Cannot read `vector_index` via existing typed read APIs.

### Pitfall 6: Timezone on datetime.fromisoformat
**What goes wrong:** `datetime.fromisoformat("2024-01-15T10:30:00")` returns a naive datetime (no tzinfo).
**Why it happens:** ISO strings without timezone suffix are parsed as naive by Python.
**How to avoid:** Always call `.replace(tzinfo=datetime.timezone.utc)` on the result, per user decision.
**Warning signs:** Comparing datetimes across timezone-aware and naive contexts raises TypeError.

## Code Examples

### CSVExportOptions Dataclass
```python
# In metadata.py (alongside existing ScalarMetadata, GroupMetadata)
@dataclass(frozen=True)
class CSVExportOptions:
    """Options for CSV export operations."""
    date_time_format: str = ""
    enum_labels: dict[str, dict[int, str]] = field(default_factory=dict)
```

### CFFI cdef Additions
```python
# Add to _c_api.py cdef string:

# CSV export options
typedef struct {
    const char* date_time_format;
    const char** enum_attribute_names;
    const size_t* enum_entry_counts;
    const int64_t* enum_values;
    const char** enum_labels;
    size_t enum_attribute_count;
} quiver_csv_export_options_t;

quiver_csv_export_options_t quiver_csv_export_options_default(void);

// CSV operations
quiver_error_t quiver_database_export_csv(quiver_database_t* db,
    const char* collection, const char* group, const char* path,
    const quiver_csv_export_options_t* opts);
```

### Full CSV Export Options Marshaling
```python
def _marshal_csv_export_options(options: CSVExportOptions) -> tuple:
    """Marshal CSVExportOptions into C API struct pointer.

    Returns (keepalive, c_opts) where keepalive must stay referenced.
    """
    keepalive: list = []
    c_opts = ffi.new("quiver_csv_export_options_t*")

    # date_time_format
    dtf_buf = ffi.new("char[]", options.date_time_format.encode("utf-8"))
    keepalive.append(dtf_buf)
    c_opts.date_time_format = dtf_buf

    if not options.enum_labels:
        c_opts.enum_attribute_names = ffi.NULL
        c_opts.enum_entry_counts = ffi.NULL
        c_opts.enum_values = ffi.NULL
        c_opts.enum_labels = ffi.NULL
        c_opts.enum_attribute_count = 0
        return keepalive, c_opts

    attr_count = len(options.enum_labels)

    # Attribute names array
    c_attr_names = ffi.new("const char*[]", attr_count)
    keepalive.append(c_attr_names)

    # Entry counts array
    c_entry_counts = ffi.new("size_t[]", attr_count)
    keepalive.append(c_entry_counts)

    # Count total entries
    total_entries = sum(len(entries) for entries in options.enum_labels.values())
    c_values = ffi.new("int64_t[]", total_entries)
    keepalive.append(c_values)
    c_labels = ffi.new("const char*[]", total_entries)
    keepalive.append(c_labels)

    entry_idx = 0
    for attr_idx, (attr_name, entries) in enumerate(options.enum_labels.items()):
        name_buf = ffi.new("char[]", attr_name.encode("utf-8"))
        keepalive.append(name_buf)
        c_attr_names[attr_idx] = name_buf
        c_entry_counts[attr_idx] = len(entries)

        for val, label in entries.items():
            c_values[entry_idx] = val
            label_buf = ffi.new("char[]", label.encode("utf-8"))
            keepalive.append(label_buf)
            c_labels[entry_idx] = label_buf
            entry_idx += 1

    c_opts.enum_attribute_names = c_attr_names
    c_opts.enum_entry_counts = c_entry_counts
    c_opts.enum_values = c_values
    c_opts.enum_labels = c_labels
    c_opts.enum_attribute_count = attr_count

    return keepalive, c_opts
```

### export_csv Method
```python
def export_csv(
    self, collection: str, group: str, path: str,
    options: CSVExportOptions | None = None,
) -> None:
    """Export a collection or group to CSV file."""
    self._ensure_open()
    lib = get_lib()
    if options is None:
        options = CSVExportOptions()
    keepalive, c_opts = _marshal_csv_export_options(options)
    check(lib.quiver_database_export_csv(
        self._ptr, collection.encode("utf-8"), group.encode("utf-8"),
        path.encode("utf-8"), c_opts))
```

### DateTime Helper Pattern
```python
from datetime import datetime, timezone

def _parse_datetime(s: str | None) -> datetime | None:
    """Parse an ISO 8601 datetime string to timezone-aware UTC datetime.

    Returns None if input is None. Raises ValueError on malformed input.
    """
    if s is None:
        return None
    return datetime.fromisoformat(s).replace(tzinfo=timezone.utc)

# On Database class:
def read_scalar_date_time_by_id(self, collection, attribute, id):
    return _parse_datetime(self.read_scalar_string_by_id(collection, attribute, id))

def read_vector_date_time_by_id(self, collection, attribute, id):
    return [_parse_datetime(s) for s in self.read_vector_strings_by_id(collection, attribute, id)]

def read_set_date_time_by_id(self, collection, attribute, id):
    return [_parse_datetime(s) for s in self.read_set_strings_by_id(collection, attribute, id)]
```

### read_all_scalars_by_id Pattern
```python
def read_all_scalars_by_id(self, collection: str, id: int) -> dict:
    """Read all scalar attribute values for an element.

    Returns dict mapping attribute names to typed values.
    DATE_TIME attributes are parsed to datetime objects.
    """
    self._ensure_open()
    result = {}
    for attr in self.list_scalar_attributes(collection):
        name = attr.name
        if attr.data_type == 0:    # INTEGER
            result[name] = self.read_scalar_integer_by_id(collection, name, id)
        elif attr.data_type == 1:  # FLOAT
            result[name] = self.read_scalar_float_by_id(collection, name, id)
        elif attr.data_type == 3:  # DATE_TIME
            result[name] = self.read_scalar_date_time_by_id(collection, name, id)
        else:                      # STRING (2)
            result[name] = self.read_scalar_string_by_id(collection, name, id)
    return result
```

### read_vector_group_by_id Pattern
```python
def read_vector_group_by_id(
    self, collection: str, group: str, id: int,
) -> list[dict]:
    """Read a multi-column vector group as row dicts."""
    self._ensure_open()
    metadata = self.get_vector_metadata(collection, group)
    columns = metadata.value_columns
    if not columns:
        return []

    column_data = {}
    row_count = 0
    for col in columns:
        name = col.name
        if col.data_type == 0:
            values = self.read_vector_integers_by_id(collection, name, id)
        elif col.data_type == 1:
            values = self.read_vector_floats_by_id(collection, name, id)
        elif col.data_type == 3:
            values = self.read_vector_date_time_by_id(collection, name, id)
        else:
            values = self.read_vector_strings_by_id(collection, name, id)
        column_data[name] = values
        row_count = len(values)

    rows = []
    for i in range(row_count):
        row = {}
        for name, values in column_data.items():
            row[name] = values[i]
        rows.append(row)
    return rows
```

## Key Findings: id/label in read_all_scalars_by_id

Both Julia and Dart include `id` and `label` in the result because `list_scalar_attributes` returns ALL scalar columns (including `id` and `label`). Julia test at `test_database_read.jl:252` verifies `date_attribute` key is present but does not exclude id/label. The `list_scalar_attributes` metadata includes `id` (INTEGER, primary_key=True) and `label` (STRING, not_null=True).

**Recommendation:** Follow Julia/Dart precedent -- include `id` and `label` in the dict. The caller gets a complete picture of the element. This is the simplest and most consistent approach.

## Key Finding: vector_index in read_vector_group_by_id

Julia's `read_vector_group_by_id` iterates only `metadata.value_columns`. The `vector_index` column is NOT a value column (it's a structural column like `id`). Neither Julia nor Dart include `vector_index` in the row dicts.

**Recommendation:** Do NOT include `vector_index` in row dicts. The user decision requested it, but implementation would require either (a) a separate SQL query, or (b) reading vector_index as if it were a value column. Neither aligns with the established pattern. The rows are already returned in index order since the C API preserves vector ordering. Flag this for the planner as a deviation from CONTEXT.md that needs user confirmation or a pragmatic decision.

## Open Questions

1. **vector_index in row dicts**
   - What we know: User decision says include it. Julia/Dart do NOT include it. The `vector_index` column is not exposed via `get_vector_metadata` value_columns.
   - What's unclear: Whether user was aware this column is not accessible through existing read APIs.
   - Recommendation: Omit `vector_index` from row dicts (follow Julia/Dart). Rows are returned in order, so index is implicit. If user requires it, a new C API method would be needed (out of scope for this phase).

2. **import_csv scope update**
   - What we know: C++ body is `{}` (empty). C API calls through but does nothing.
   - What's unclear: Nothing -- confirmed empty.
   - Recommendation: Drop CSV-02 from Phase 6. Update REQUIREMENTS.md to note it's deferred pending C++ implementation.

## Sources

### Primary (HIGH confidence)
- `include/quiver/c/options.h` -- C API CSV export options struct definition
- `include/quiver/c/database.h:429-434` -- C API export_csv and import_csv declarations
- `src/c/database_csv.cpp` -- C API CSV export implementation with `convert_options` marshaling
- `src/database_csv.cpp` -- C++ CSV export implementation (full, working)
- `src/database_describe.cpp:91` -- C++ import_csv stub (empty body `{}`)
- `bindings/julia/src/database_csv.jl` -- Julia CSV export pattern with parallel array marshaling
- `bindings/julia/src/database_read.jl:404-547` -- Julia convenience helpers (all patterns)
- `bindings/julia/src/date_time.jl` -- Julia DateTime parsing
- `bindings/dart/lib/src/database_csv.dart` -- Dart CSV export pattern
- `bindings/dart/lib/src/database_read.dart:718-884` -- Dart convenience helpers
- `bindings/python/src/quiver/database.py` -- Current Python binding (full file, all patterns)
- `bindings/python/src/quiver/_c_api.py` -- Current CFFI declarations
- `bindings/python/src/quiver/metadata.py` -- Existing dataclass patterns

### Secondary (MEDIUM confidence)
- `bindings/python/src/quiver/_declarations.py` -- Generator output showing CSV struct layout (reference)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new libraries, all stdlib + existing CFFI
- Architecture: HIGH -- patterns directly replicated from Julia/Dart with verified source code
- Pitfalls: HIGH -- all identified from codebase investigation, not speculation

**Research date:** 2026-02-24
**Valid until:** indefinite (codebase patterns, not external dependencies)
