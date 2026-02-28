# Phase 3: Python DataType Constants - Research

**Researched:** 2026-02-28
**Domain:** Python IntEnum for C API data type constants
**Confidence:** HIGH

## Summary

This phase replaces all hardcoded magic integer literals (0, 1, 2, 3, 4) used for data type dispatch in the Python binding with a `DataType` IntEnum class. The change is entirely internal to the Python binding layer -- no C/C++ changes, no other binding changes.

The implementation is straightforward: define a `DataType(IntEnum)` in `metadata.py`, update `ScalarMetadata.data_type` to use it, replace all bare integer comparisons in `database.py` with `DataType` members, export it from `__init__.py`, and add a validation test. Because `IntEnum` is-a `int`, all existing CFFI interop (passing values to C arrays, comparing with C struct fields) works unchanged.

**Primary recommendation:** Define `DataType` as a standard `IntEnum` in `metadata.py` with values matching the C API `quiver_data_type_t` enum. This follows the same pattern Julia uses (referencing `C.QUIVER_DATA_TYPE_INTEGER` etc.) and Dart uses (referencing `quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER` etc.), adapted to Python idioms.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- DataType IntEnum defined in `metadata.py` alongside ScalarMetadata (which uses it as its `data_type` field)
- `database.py` imports DataType directly: `from quiverdb.metadata import DataType`
- No new files -- follows existing module structure
- DataType exported in `__init__.py` so users can write `from quiverdb import DataType`
- Added to `__all__` list
- Query parameter building stays as-is (auto-detects type from Python values, no DataType hints)
- Replace magic integers in ALL three contexts in `database.py`:
  1. ScalarMetadata.data_type field type (int -> DataType)
  2. Type dispatch in composite reads (read_scalars_by_id, read_vectors_by_id, read_sets_by_id, read_vector_group_by_id, read_set_group_by_id)
  3. Time series columnar reads and updates (ctype comparisons)
- Also replace magic integers in query param building (c_types[i] = DataType.NULL instead of 4, etc.)
- IntEnum is-a int, so all C interop works unchanged -- values pass directly to C arrays

### Claude's Discretion
- Exact IntEnum class definition style (class body formatting)
- Whether to add a `__str__` override or rely on default IntEnum behavior
- Test structure for validating DataType values match C API constants

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PY-02 | Python `DataType` IntEnum replaces all hardcoded `0/1/2/3/4` magic numbers in `database.py` | Full inventory of 30+ magic integer sites identified in database.py across 7 distinct contexts; IntEnum pattern verified as zero-risk for CFFI interop |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `enum.IntEnum` | stdlib (Python 3.13+) | Named integer constants that remain `int`-compatible | Part of Python standard library; idiomatic way to replace magic integers while preserving int compatibility |
| `cffi` | >=2.0.0 | C FFI interface (already in use) | Already the project's FFI layer; IntEnum values pass through unchanged |

### Supporting
No additional libraries needed. This is a pure refactoring using only stdlib `enum`.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `IntEnum` | `IntFlag` | IntFlag is for bitmask values; DataType values are ordinal, not combinable -- IntEnum is correct |
| `IntEnum` | Plain `Enum` | Plain Enum is not int-compatible; would break CFFI array assignments like `c_types[i] = DataType.NULL` |
| `IntEnum` | Module-level constants (`INTEGER = 0`) | No type safety, no membership validation, no repr benefits |

## Architecture Patterns

### Module Structure (No Changes)
```
src/quiverdb/
├── __init__.py           # Add DataType to imports and __all__
├── metadata.py           # Add DataType IntEnum here (before ScalarMetadata)
├── database.py           # Replace all magic integers with DataType members
└── (everything else unchanged)
```

### Pattern 1: IntEnum Definition
**What:** Standard Python IntEnum with values matching C API `quiver_data_type_t`
**When to use:** When wrapping C enum constants in Python
**Example:**
```python
# Source: Python stdlib enum documentation
from enum import IntEnum

class DataType(IntEnum):
    INTEGER = 0
    FLOAT = 1
    STRING = 2
    DATE_TIME = 3
    NULL = 4
```

**Key property:** `DataType.INTEGER == 0` is `True`, `isinstance(DataType.INTEGER, int)` is `True`. This means every place that currently uses `0` can use `DataType.INTEGER` with zero behavioral change.

### Pattern 2: ScalarMetadata field type annotation
**What:** Change `data_type: int` to `data_type: DataType` in the frozen dataclass
**When to use:** When the metadata parsing function constructs ScalarMetadata
**Example:**
```python
@dataclass(frozen=True)
class ScalarMetadata:
    name: str
    data_type: DataType  # was: int
    not_null: bool
    # ...
```

The parsing function `_parse_scalar_metadata` in `database.py` currently does `data_type=int(meta.data_type)`. Change to `data_type=DataType(int(meta.data_type))` to construct the enum member from the C struct's integer value.

### Pattern 3: Magic integer replacement in dispatch
**What:** Replace `if attr.data_type == 0:` with `if attr.data_type == DataType.INTEGER:`
**When to use:** All type dispatch blocks in database.py
**Example:**
```python
# Before:
if attr.data_type == 0:  # INTEGER
    result[name] = self.read_scalar_integer_by_id(collection, name, id)
elif attr.data_type == 1:  # FLOAT
    result[name] = self.read_scalar_float_by_id(collection, name, id)

# After:
if attr.data_type == DataType.INTEGER:
    result[name] = self.read_scalar_integer_by_id(collection, name, id)
elif attr.data_type == DataType.FLOAT:
    result[name] = self.read_scalar_float_by_id(collection, name, id)
```

### Pattern 4: C array assignment replacement
**What:** Replace `c_types[i] = 4` with `c_types[i] = DataType.NULL` in param marshaling
**When to use:** `_marshal_params()` and `_marshal_time_series_columns()` functions
**Example:**
```python
# Before:
c_types[i] = 4  # QUIVER_DATA_TYPE_NULL

# After:
c_types[i] = DataType.NULL
```

This works because IntEnum is-a int, so CFFI `int[]` arrays accept IntEnum values directly.

### Anti-Patterns to Avoid
- **Using plain Enum instead of IntEnum:** Would break CFFI interop since `Enum` members are not `int`-compatible
- **Converting IntEnum back to int before assignment:** Unnecessary; `IntEnum` IS an `int`, no conversion needed
- **Adding `auto()` values:** Values MUST match the C API exactly; explicit assignment is required

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Named integer constants | Dict mapping or module-level vars | `enum.IntEnum` | IntEnum provides type checking, repr, iteration, and int compatibility out of the box |

**Key insight:** IntEnum is purpose-built for exactly this use case -- wrapping C enum constants in Python with full int compatibility.

## Common Pitfalls

### Pitfall 1: Forgetting IntEnum is-a int
**What goes wrong:** Developers add unnecessary `int()` casts around DataType members when passing to CFFI
**Why it happens:** Not understanding that IntEnum inherits from int
**How to avoid:** Trust that `DataType.INTEGER` IS an int; no cast needed for CFFI array assignment, comparison, or any int context
**Warning signs:** Seeing `int(DataType.INTEGER)` anywhere except the `_parse_scalar_metadata` construction site

### Pitfall 2: Bool subclass ordering in isinstance checks
**What goes wrong:** N/A for this phase, but worth noting: `_marshal_params` checks `isinstance(p, int)` which catches `bool` (since `bool` is a subclass of `int`). The comment already documents this. DataType replacement doesn't affect this logic since `isinstance` checks remain on the parameter values, not the type constants.
**How to avoid:** Leave the isinstance checks in `_marshal_params` exactly as-is; only replace the `c_types[i] = N` assignments

### Pitfall 3: Existing tests using bare integers for data_type assertions
**What goes wrong:** Tests like `assert meta.data_type == 0` will still pass (IntEnum == int comparison works), but they won't validate the enum type
**Why it happens:** Tests were written before DataType existed
**How to avoid:** Existing tests can optionally be updated to use `DataType.INTEGER` but this is cosmetic -- the existing integer comparisons will continue to work correctly because `DataType.INTEGER == 0` is `True`
**Warning signs:** N/A -- this is a non-breaking change

### Pitfall 4: Forgetting the dimension column type in time series marshaling
**What goes wrong:** `_marshal_time_series_columns` hardcodes `2` for the dimension column type: `columns.append((metadata.dimension_column, 2))`
**Why it happens:** Dimension column is always STRING (TEXT in SQL), and this is a hardcoded constant
**How to avoid:** Replace with `DataType.STRING` -- this is one of the magic integers that must be replaced
**Warning signs:** Any remaining bare `2` in the time series marshaling code

## Code Examples

### Complete DataType definition (recommended)
```python
from enum import IntEnum

class DataType(IntEnum):
    """Data type constants matching C API quiver_data_type_t."""
    INTEGER = 0
    FLOAT = 1
    STRING = 2
    DATE_TIME = 3
    NULL = 4
```

No `__str__` override needed. Default IntEnum behavior produces `DataType.INTEGER` for repr and `0` for int contexts, which is clear and useful.

### Updated _parse_scalar_metadata
```python
def _parse_scalar_metadata(meta) -> ScalarMetadata:
    return ScalarMetadata(
        name=decode_string(meta.name),
        data_type=DataType(int(meta.data_type)),  # was: int(meta.data_type)
        # ... rest unchanged
    )
```

### Test validating DataType matches C API constants
```python
from quiverdb import DataType
from quiverdb._c_api import ffi, get_lib

def test_datatype_values_match_c_api():
    """Verify DataType enum values match CFFI quiver_data_type_t constants."""
    lib = get_lib()
    assert DataType.INTEGER == lib.QUIVER_DATA_TYPE_INTEGER
    assert DataType.FLOAT == lib.QUIVER_DATA_TYPE_FLOAT
    assert DataType.STRING == lib.QUIVER_DATA_TYPE_STRING
    assert DataType.DATE_TIME == lib.QUIVER_DATA_TYPE_DATE_TIME
    assert DataType.NULL == lib.QUIVER_DATA_TYPE_NULL
```

Note: CFFI ABI-mode exposes C enum constants as attributes on the `lib` object. The `quiver_data_type_t` enum is declared in `_c_api.py` lines 93-99, so `lib.QUIVER_DATA_TYPE_INTEGER` etc. are available.

### Updated __init__.py
```python
from quiverdb.database import Database
from quiverdb.database_csv_export import DatabaseCSVExport
from quiverdb.database_csv_import import DatabaseCSVImport
from quiverdb.exceptions import QuiverError
from quiverdb.metadata import CSVOptions, DataType, GroupMetadata, ScalarMetadata

__all__ = [
    "CSVOptions",
    "DataType",
    "Database",
    "DatabaseCSVExport",
    "DatabaseCSVImport",
    "GroupMetadata",
    "QuiverError",
    "ScalarMetadata",
    "version",
]
```

## Inventory of Magic Integer Sites

Complete enumeration of all magic integer locations that must be replaced in `database.py`:

### Context 1: ScalarMetadata construction (1 site)
- Line 1541: `data_type=int(meta.data_type)` -> `data_type=DataType(int(meta.data_type))`

### Context 2: Composite read dispatch (5 methods, 20 comparisons)
- `read_scalars_by_id` (lines 1273-1280): `attr.data_type == 0/1/3` and else for STRING(2)
- `read_vectors_by_id` (lines 1294-1302): `dt == 0/1/3` and else for STRING(2)
- `read_sets_by_id` (lines 1316-1324): `dt == 0/1/3` and else for STRING(2)
- `read_vector_group_by_id` (lines 1360-1367): `col.data_type == 0/1/3` and else for STRING(2)
- `read_set_group_by_id` (lines 1394-1401): `col.data_type == 0/1/3` and else for STRING(2)

### Context 3: Time series read dispatch (1 method, 2 comparisons)
- `read_time_series_group` (lines 1129-1134): `ctype == 0/1` and else for STRING/DATE_TIME

### Context 4: Param marshaling (1 function, 4 assignments)
- `_marshal_params` (lines 1443-1456): `c_types[i] = 4/0/1/2`

### Context 5: Time series write marshaling (1 function, multiple sites)
- `_marshal_time_series_columns` (line 1478): `columns.append((metadata.dimension_column, 2))` (hardcoded STRING)
- `_marshal_time_series_columns` validation (lines 1492-1500): `col_type == 0/1` and else
- `_marshal_time_series_columns` data build (lines 1515-1529): `col_type == 0/1` and else

### Import addition needed
- `database.py` line 13: Add `DataType` to the metadata import

**Total: ~30 individual integer literal replacements across ~10 code locations.**

## Cross-Binding Comparison

| Binding | How data types are referenced | Defined where |
|---------|------------------------------|---------------|
| Julia | `C.QUIVER_DATA_TYPE_INTEGER` (generated C enum) | Auto-generated `c_api.jl` via Clang.jl |
| Dart | `quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER` (generated C enum) | Auto-generated `bindings.dart` via ffigen |
| Python (current) | Bare integers: `0`, `1`, `2`, `3`, `4` | Nowhere -- magic numbers inline |
| Python (after this phase) | `DataType.INTEGER`, `DataType.FLOAT`, etc. | `metadata.py` as IntEnum |

Julia and Dart benefit from auto-generated bindings that bring C enum constants into the binding language. Python's CFFI ABI-mode doesn't auto-generate a Python enum from the C typedef -- hence the need for this manual DataType definition.

## Open Questions

None. The implementation path is fully determined by the locked decisions in CONTEXT.md and the code inventory above.

## Sources

### Primary (HIGH confidence)
- Direct code inspection of `bindings/python/src/quiverdb/database.py` (1561 lines) -- all magic integer sites enumerated
- Direct code inspection of `bindings/python/src/quiverdb/metadata.py` -- current ScalarMetadata definition
- Direct code inspection of `bindings/python/src/quiverdb/__init__.py` -- current exports
- Direct code inspection of `bindings/python/src/quiverdb/_c_api.py` -- CFFI cdef with `quiver_data_type_t` enum (lines 92-99)
- Direct code inspection of `include/quiver/c/database.h` -- canonical C API `quiver_data_type_t` values
- Python stdlib `enum.IntEnum` -- built-in, well-documented, stable API

### Secondary (MEDIUM confidence)
- Cross-binding comparison with Julia (`c_api.jl`) and Dart (`bindings.dart`) data type enum usage patterns

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - using Python stdlib IntEnum, zero external dependencies
- Architecture: HIGH - direct code inspection, all sites enumerated, pattern is mechanical replacement
- Pitfalls: HIGH - IntEnum/int compatibility is well-established Python behavior

**Research date:** 2026-02-28
**Valid until:** indefinite (stdlib IntEnum is stable; code sites are project-specific)
