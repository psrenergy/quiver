# Phase 9: CSV Import and Options - Research

**Researched:** 2026-02-24
**Domain:** Python CFFI bindings for CSV import/export with structured options marshaling
**Confidence:** HIGH

## Summary

This phase brings the Python binding's CSV support to parity with Julia and Dart. The C++ implementation and C API are fully complete -- both `quiver_database_export_csv` and `quiver_database_import_csv` are implemented and working. The CFFI declarations in `_c_api.py` already include both functions and the `quiver_csv_options_t` struct. The work is purely in the Python binding layer: rename `CSVExportOptions` to `CSVOptions`, restructure `enum_labels` from `dict[str, dict[int, str]]` to `dict[str, dict[str, dict[str, int]]]`, rewrite the marshaler for 3-level dict flattening, implement `import_csv` by calling the real C API, and add round-trip tests.

The Julia (`database_csv.jl`) and Dart (`database_csv.dart`) implementations serve as verified reference implementations. Both use identical patterns: build a `quiver_csv_options_t` from nested dicts, flatten attribute -> locale -> entries into grouped parallel arrays, and call `quiver_database_import_csv` / `quiver_database_export_csv`. The Python implementation should produce identical C API calls.

**Primary recommendation:** Follow the Julia/Dart reference implementations exactly for the marshaling pattern (3-level dict flattening into grouped parallel arrays), reuse the existing CFFI allocation style (keepalive list), and model tests after the Julia/Dart CSV test files which cover scalar round-trip, vector group round-trip, and enum resolution.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Rename `CSVExportOptions` to `CSVOptions` -- remove old name entirely, no alias (WIP project, breaking changes acceptable)
- Frozen dataclass, same as current pattern
- `enum_labels` structure: `dict[str, dict[str, dict[str, int]]]` -- attribute -> locale -> label -> value -- matches C++ exactly
- Value direction inverted from current Python: was `int -> str`, now `str -> int` matching C++/Julia/Dart
- Update existing export tests to use new dict direction (don't rewrite from scratch)
- Signature: `import_csv(self, collection: str, group: str, path: str, *, options: CSVOptions | None = None) -> None`
- `collection`, `group`, `path` are required positional parameters
- `group` is required (pass `''` for scalar-only imports) -- matches Julia/Dart
- `options` is keyword-only with `None` default
- Returns `None` (void equivalent) -- errors raise exceptions
- Align `export_csv` to match same parameter pattern: `(collection, group, path, *, options=None)`
- Tests live in existing CSV test file alongside export tests (same file)
- Use existing schemas from `tests/schemas/valid/` -- ensures cross-binding parity
- Cover all three paths: scalar import (group=''), group import (vector/set data), enum-labeled import
- `date_time_format` testing as part of broader round-trip test (not separate)
- Round-trip pattern: export -> import -> read back -> verify
- Rewrite `_marshal_csv_export_options` as new `_marshal_csv_options` -- clean slate for 3-level dict flattening
- Single shared marshaler used by both `export_csv` and `import_csv`
- Match current CFFI allocation pattern (no context manager abstraction)
- No Python-side validation of enum_labels structure -- let C API handle type mismatches per project principles
- Julia and Dart bindings serve as reference implementations -- Python should produce identical C API calls for the same logical operations

### Claude's Discretion
- Exact CFFI array allocation details
- How to organize the marshaling code within the file
- Which specific test schemas best cover scalar and group import paths
- Generator updates needed (if any) for CFFI declarations

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CSV-01 | Implement import_csv in Python by calling the real C API function (collection, group, path, options) | C API declaration already in `_c_api.py` (line 345-347). Julia/Dart reference implementations show exact call pattern. New `import_csv` method replaces NotImplementedError stub. |
| CSV-02 | Rename CSVExportOptions -> CSVOptions and fix enum_labels structure to match C++ (attribute -> locale -> label -> value, with full locale support) | Current `CSVExportOptions` in `metadata.py` has `dict[str, dict[int, str]]`. Must become `CSVOptions` with `dict[str, dict[str, dict[str, int]]]`. Affects: metadata.py, database.py, __init__.py, test_database_csv.py. |
| CSV-03 | Add import_csv tests covering scalar and group import round-trips | Julia test file (`test_database_csv.jl` lines 192-417) and Dart test file (`database_csv_test.dart` lines 226-478) provide verified test patterns. Schema `csv_export.sql` covers all needed table types. |
| CSV-04 | Update CLAUDE.md references from CSVExportOptions to CSVOptions | CLAUDE.md already uses `CSVOptions` throughout (C++ naming). Only Python-specific references need checking -- CLAUDE.md has no Python-specific `CSVExportOptions` references currently. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| cffi | (already installed) | ABI-mode FFI for calling C API | Project's established FFI approach for Python binding |
| pytest | 9.0.2 | Test framework | Already in use, all Python tests use pytest |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| dataclasses | stdlib | Frozen CSVOptions dataclass | Type for options (existing pattern) |

### Alternatives Considered
None -- all decisions are locked. No library choices to make.

## Architecture Patterns

### Files That Change

```
bindings/python/src/quiverdb/
  metadata.py              # CSVExportOptions -> CSVOptions, new enum_labels type
  database.py              # import_csv implementation, export_csv signature, new marshaler
  __init__.py              # CSVExportOptions -> CSVOptions in exports
bindings/python/tests/
  test_database_csv.py     # Update existing tests + add import tests
CLAUDE.md                  # Verify no stale CSVExportOptions references
```

### Pattern 1: Three-Level Dict Flattening for Enum Labels
**What:** Convert Python `dict[str, dict[str, dict[str, int]]]` into C API's grouped parallel arrays.
**When to use:** Both `export_csv` and `import_csv` calls.
**Example:**
```python
# Source: Julia database_csv.jl lines 44-78, Dart database_csv.dart lines 14-57
# Python input:
# {"status": {"en": {"Active": 1, "Inactive": 2}, "pt": {"Ativo": 1}}}
#
# C API output:
#   enum_attribute_names = ["status", "status"]   (one per (attr, locale) group)
#   enum_locale_names    = ["en", "pt"]
#   enum_entry_counts    = [2, 1]
#   enum_labels          = ["Active", "Inactive", "Ativo"]  (concatenated)
#   enum_values          = [1, 2, 1]                        (concatenated)
#   enum_group_count     = 2
```

### Pattern 2: Keepalive List for CFFI Memory
**What:** All `ffi.new()` allocations stored in a list that stays referenced during the C API call.
**When to use:** Every marshaling function in the Python binding.
**Example:**
```python
# Source: existing _marshal_csv_export_options in database.py lines 1416-1477
keepalive: list = []
c_opts = ffi.new("quiver_csv_options_t*")
dtf_buf = ffi.new("char[]", options.date_time_format.encode("utf-8"))
keepalive.append(dtf_buf)
c_opts.date_time_format = dtf_buf
# ... keepalive must survive until after check() returns
```

### Pattern 3: import_csv Calling Convention
**What:** Matches Julia/Dart exactly -- `(collection, group, path, opts)` positional to C API.
**When to use:** The new `import_csv` method.
**Example:**
```python
# Source: Julia database_csv.jl lines 104-110
def import_csv(
    self, collection: str, group: str, path: str, *,
    options: CSVOptions | None = None,
) -> None:
    """Import CSV data into a collection."""
    self._ensure_open()
    lib = get_lib()
    if options is None:
        options = CSVOptions()
    keepalive, c_opts = _marshal_csv_options(options)
    check(lib.quiver_database_import_csv(
        self._ptr, collection.encode("utf-8"), group.encode("utf-8"),
        path.encode("utf-8"), c_opts))
```

### Anti-Patterns to Avoid
- **Keeping old CSVExportOptions as alias:** WIP project, no backwards compatibility. Remove entirely.
- **Separate marshalers for export and import:** Both use identical `quiver_csv_options_t`. One shared function.
- **Python-side validation of enum_labels nesting:** Per project principles ("Clean code over defensive code"), let C API handle type mismatches.
- **Two-level enum dict (old pattern):** The current `dict[str, dict[int, str]]` pattern must not persist anywhere. All code paths use 3-level dict.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| CSV parsing/import logic | Python CSV reader | C API `quiver_database_import_csv` | All logic lives in C++ layer per project principle |
| Options validation | Python type checks on enum_labels | C API error handling | "Intelligence resides in C++ layer. Bindings remain thin." |
| Enum label resolution | Python reverse-lookup code | C API's built-in resolution | C++ handles case-insensitive matching across locales |

**Key insight:** The Python binding is a thin wrapper. `import_csv` is literally: marshal options -> call C API -> check error. No business logic in Python.

## Common Pitfalls

### Pitfall 1: Wrong Dict Direction in Existing Export Tests
**What goes wrong:** Current tests use `{1: "Active", 2: "Inactive"}` (int -> str). New structure is `{"Active": 1, "Inactive": 2}` (str -> int) wrapped in locale layer.
**Why it happens:** The direction was inverted for Python convenience in the original implementation, now aligning with C++.
**How to avoid:** Update ALL existing test enum_labels to 3-level format: `{"status": {"en": {"Active": 1, "Inactive": 2}}}`.
**Warning signs:** Tests constructing `CSVOptions` with `dict[str, dict[int, str]]` instead of `dict[str, dict[str, dict[str, int]]]`.

### Pitfall 2: Existing export_csv Positional Options Argument
**What goes wrong:** Current `export_csv` accepts `options` as 4th positional arg. After making it keyword-only (`*`), existing test calls `csv_db.export_csv("Items", "", out, opts)` will fail with TypeError.
**Why it happens:** The `*` separator changes call conventions.
**How to avoid:** Update all test calls to use `options=opts` keyword syntax when passing options.
**Warning signs:** `TypeError: export_csv() takes X positional arguments but Y were given`.

### Pitfall 3: CFFI Array Sizing for 3-Level Flattening
**What goes wrong:** With 2-level dict, `attr_count == len(enum_labels)`. With 3-level dict, `group_count == sum(len(locales) for locales in enum_labels.values())` because each (attribute, locale) pair is a separate group.
**Why it happens:** The grouping is per (attribute, locale) pair, not per attribute.
**How to avoid:** Iterate attribute -> locale -> entries, counting groups as (attribute, locale) pairs. See Julia lines 44-56.
**Warning signs:** Array index out of bounds, wrong `enum_group_count`, labels/values not matching.

### Pitfall 4: Forgetting to Update __init__.py Exports
**What goes wrong:** Module exports `CSVExportOptions` but class is now `CSVOptions`. Import fails at test time.
**Why it happens:** `__init__.py` has explicit `__all__` list and import statement.
**How to avoid:** Update both the import line and `__all__` in `__init__.py`.
**Warning signs:** `ImportError: cannot import name 'CSVOptions' from 'quiverdb'`.

### Pitfall 5: Forgetting to Remove NotImplementedError Stub Test
**What goes wrong:** `TestImportCSVStub` test class still expects `NotImplementedError`, but `import_csv` now works.
**Why it happens:** The stub test was written when import was not implemented.
**How to avoid:** Remove or replace `TestImportCSVStub` class with real import tests.
**Warning signs:** Test failure on `pytest.raises(NotImplementedError)`.

### Pitfall 6: CFFI const char* const* vs const char** Mismatch
**What goes wrong:** The C header declares `const char* const*` but CFFI `_c_api.py` uses `const char**` for the options struct. This works in ABI mode but could cause confusion.
**Why it happens:** CFFI ABI mode is more lenient about const qualifiers.
**How to avoid:** Use `ffi.new("const char*[]", count)` for arrays of string pointers -- this is the established pattern in the codebase (see existing marshaler line 1440).
**Warning signs:** None expected -- existing pattern works.

## Code Examples

### New CSVOptions Dataclass
```python
# Source: C++ options.h line 19-23, matches Julia/Dart structure
@dataclass(frozen=True)
class CSVOptions:
    """Options for CSV import and export operations."""
    date_time_format: str = ""
    enum_labels: dict[str, dict[str, dict[str, int]]] = field(default_factory=dict)
```

### New _marshal_csv_options Function (3-Level Flattening)
```python
# Source: Julia database_csv.jl lines 13-93, Dart database_csv.dart lines 4-66
def _marshal_csv_options(options: CSVOptions) -> tuple:
    """Marshal CSVOptions into C API struct pointer.

    Returns (keepalive, c_opts) where keepalive must stay referenced
    during the C API call.
    """
    keepalive: list = []
    c_opts = ffi.new("quiver_csv_options_t*")

    # date_time_format
    dtf_buf = ffi.new("char[]", options.date_time_format.encode("utf-8"))
    keepalive.append(dtf_buf)
    c_opts.date_time_format = dtf_buf

    if not options.enum_labels:
        c_opts.enum_attribute_names = ffi.NULL
        c_opts.enum_locale_names = ffi.NULL
        c_opts.enum_entry_counts = ffi.NULL
        c_opts.enum_labels = ffi.NULL
        c_opts.enum_values = ffi.NULL
        c_opts.enum_group_count = 0
        return keepalive, c_opts

    # Flatten attribute -> locale -> entries into grouped parallel arrays
    # Each (attribute, locale) pair is one group
    group_attr_names: list[str] = []
    group_locale_names: list[str] = []
    group_entry_counts: list[int] = []
    all_labels: list[str] = []
    all_values: list[int] = []

    for attr_name, locales in options.enum_labels.items():
        for locale_name, entries in locales.items():
            group_attr_names.append(attr_name)
            group_locale_names.append(locale_name)
            group_entry_counts.append(len(entries))
            for label, value in entries.items():
                all_labels.append(label)
                all_values.append(value)

    group_count = len(group_attr_names)
    total_entries = len(all_labels)

    c_attr_names = ffi.new("const char*[]", group_count)
    keepalive.append(c_attr_names)
    c_locale_names = ffi.new("const char*[]", group_count)
    keepalive.append(c_locale_names)
    c_entry_counts = ffi.new("size_t[]", group_count)
    keepalive.append(c_entry_counts)
    c_labels = ffi.new("const char*[]", total_entries)
    keepalive.append(c_labels)
    c_values = ffi.new("int64_t[]", total_entries)
    keepalive.append(c_values)

    for i in range(group_count):
        name_buf = ffi.new("char[]", group_attr_names[i].encode("utf-8"))
        keepalive.append(name_buf)
        c_attr_names[i] = name_buf
        locale_buf = ffi.new("char[]", group_locale_names[i].encode("utf-8"))
        keepalive.append(locale_buf)
        c_locale_names[i] = locale_buf
        c_entry_counts[i] = group_entry_counts[i]

    for i in range(total_entries):
        label_buf = ffi.new("char[]", all_labels[i].encode("utf-8"))
        keepalive.append(label_buf)
        c_labels[i] = label_buf
        c_values[i] = all_values[i]

    c_opts.enum_attribute_names = c_attr_names
    c_opts.enum_locale_names = c_locale_names
    c_opts.enum_entry_counts = c_entry_counts
    c_opts.enum_labels = c_labels
    c_opts.enum_values = c_values
    c_opts.enum_group_count = group_count

    return keepalive, c_opts
```

### Updated export_csv (keyword-only options)
```python
# Source: existing database.py line 1124, modified per CONTEXT.md decision
def export_csv(
    self, collection: str, group: str, path: str, *,
    options: CSVOptions | None = None,
) -> None:
    """Export a collection or group to CSV file."""
    self._ensure_open()
    lib = get_lib()
    if options is None:
        options = CSVOptions()
    keepalive, c_opts = _marshal_csv_options(options)
    check(lib.quiver_database_export_csv(
        self._ptr, collection.encode("utf-8"), group.encode("utf-8"),
        path.encode("utf-8"), c_opts))
```

### New import_csv Implementation
```python
# Source: Julia database_csv.jl lines 104-110, Dart database_csv.dart lines 117-142
def import_csv(
    self, collection: str, group: str, path: str, *,
    options: CSVOptions | None = None,
) -> None:
    """Import CSV data into a collection."""
    self._ensure_open()
    lib = get_lib()
    if options is None:
        options = CSVOptions()
    keepalive, c_opts = _marshal_csv_options(options)
    check(lib.quiver_database_import_csv(
        self._ptr, collection.encode("utf-8"), group.encode("utf-8"),
        path.encode("utf-8"), c_opts))
```

### Test: Scalar Round-Trip
```python
# Source: Julia test_database_csv.jl lines 195-233, Dart database_csv_test.dart lines 227-267
class TestImportCSVScalarRoundTrip:
    def test_scalar_round_trip(self, csv_db, tmp_path, csv_export_schema_path):
        e1 = Element()
        e1.set("label", "Item1")
        e1.set("name", "Alpha")
        e1.set("status", 1)
        e1.set("price", 9.99)
        e1.set("date_created", "2024-01-15T10:30:00")
        e1.set("notes", "first")
        csv_db.create_element("Items", e1)
        # ... create Item2 ...

        csv_path = str(tmp_path / "scalars.csv")
        csv_db.export_csv("Items", "", csv_path)

        # Import into fresh DB
        db2 = Database.from_schema(str(tmp_path / "csv2.db"), str(csv_export_schema_path))
        try:
            db2.import_csv("Items", "", csv_path)
            names = db2.read_scalar_strings("Items", "name")
            assert len(names) == 2
            assert names[0] == "Alpha"
            assert names[1] == "Beta"
        finally:
            db2.close()
```

### Test: Enum Resolution Import
```python
# Source: Julia test_database_csv.jl lines 287-305, Dart database_csv_test.dart lines 320-345
class TestImportCSVEnumResolution:
    def test_enum_resolution(self, csv_db, tmp_path):
        csv_path = str(tmp_path / "enum.csv")
        with open(csv_path, "wb") as f:
            f.write(b"sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,Active,,,\n")

        opts = CSVOptions(
            enum_labels={"status": {"en": {"Active": 1, "Inactive": 2}}},
        )
        csv_db.import_csv("Items", "", csv_path, options=opts)

        status = csv_db.read_scalar_integer_by_id("Items", "status", 1)
        assert status == 1
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `CSVExportOptions` with `dict[str, dict[int, str]]` | `CSVOptions` with `dict[str, dict[str, dict[str, int]]]` | This phase | Breaking change to Python API surface, aligns with C++/Julia/Dart |
| `import_csv` stub raising NotImplementedError | Real C API call via `quiver_database_import_csv` | This phase | Python gains CSV import capability |
| `options` as positional arg in `export_csv` | `options` as keyword-only arg | This phase | Minor breaking change, improves API clarity |

## Open Questions

1. **CLAUDE.md CSVExportOptions references**
   - What we know: CLAUDE.md already uses `CSVOptions` throughout (it follows C++ naming). No Python-specific `CSVExportOptions` references found.
   - What's unclear: Whether any other documentation files reference the old name.
   - Recommendation: Search entire repo for `CSVExportOptions` after rename to catch any stragglers.

2. **Test file needs access to schema path for fresh DB creation**
   - What we know: Round-trip tests need to create a second DB from the same schema. The `csv_db` fixture creates one DB but the schema path is encapsulated in `csv_export_schema_path` fixture.
   - What's unclear: Whether to pass the fixture directly to tests or create a new fixture.
   - Recommendation: Tests that need a fresh DB should accept `csv_export_schema_path` fixture directly (already defined in conftest.py line 72-73). This is how the conftest is designed.

## Sources

### Primary (HIGH confidence)
- **C API header** `include/quiver/c/options.h` - `quiver_csv_options_t` struct layout with grouped parallel arrays, full documentation
- **C++ implementation** `src/database_csv.cpp` - Complete import_csv implementation (scalar + group paths, enum resolution, FK handling)
- **C API implementation** `src/c/database_csv.cpp` - `convert_options()` showing C struct to C++ mapping
- **C++ CSVOptions** `include/quiver/options.h` - `dict[str, dict[str, dict[str, int64_t]]]` structure
- **Julia reference** `bindings/julia/src/database_csv.jl` - `build_quiver_csv_options` with 3-level flattening
- **Dart reference** `bindings/dart/lib/src/database_csv.dart` - `_fillCSVOptions` with identical pattern
- **Julia tests** `bindings/julia/test/test_database_csv.jl` - 15 test cases covering export + import
- **Dart tests** `bindings/dart/test/database_csv_test.dart` - 15 test cases covering export + import
- **Python current** `bindings/python/src/quiverdb/database.py` - Existing `export_csv`, stub `import_csv`, old marshaler
- **Python CFFI** `bindings/python/src/quiverdb/_c_api.py` - Already declares `quiver_database_import_csv` (line 345-347)
- **Python declarations** `bindings/python/src/quiverdb/_declarations.py` - Already has `quiver_database_import_csv` (line 478-482)

### Secondary (MEDIUM confidence)
None needed -- all information is from primary codebase sources.

### Tertiary (LOW confidence)
None.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - No new dependencies, using established project patterns
- Architecture: HIGH - Direct port from working Julia/Dart implementations with exact C API match
- Pitfalls: HIGH - All identified from actual code inspection (existing tests, current marshaler, __init__.py exports)

**Research date:** 2026-02-24
**Valid until:** Indefinite (stable codebase, no external dependencies changing)
