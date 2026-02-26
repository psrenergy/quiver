# Phase 1: Python kwargs API - Research

**Researched:** 2026-02-26
**Domain:** Python FFI binding API design (kwargs pattern)
**Confidence:** HIGH

## Summary

This phase replaces Python's `Element` builder pattern with native kwargs-based `create_element` and `update_element` methods, aligning the Python binding with the Julia and Dart patterns. The Julia binding is the direct reference implementation: a kwargs overload creates an `Element` internally, iterates over kwargs to set attributes, calls the Element-based function, and destroys the Element in a `finally` block. The Dart binding uses the same pattern but with a `Map<String, Object?>` parameter.

The change is well-scoped: modify two methods in `database.py`, remove `Element` from `__init__.py` exports, and mechanically update ~249 `Element()` occurrences across 12 test files. No C API, C++ core, or other binding changes are required. The `element.py` module is retained as an internal implementation detail.

**Primary recommendation:** Follow Julia's pattern exactly -- add kwargs overloads to `create_element` and `update_element` that build an `Element` internally, keep the old Element-based path as the internal implementation, and remove `Element` from public exports.

<user_constraints>

## User Constraints (from CONTEXT.md)

### Locked Decisions

**Kwargs call signature:**
- `db.create_element("Collection", label="Item1", some_integer=42)` -- collection is positional, all attributes as kwargs
- `db.update_element("Collection", id, label="Updated")` -- collection and id positional, updated attributes as kwargs
- Lists for vectors/sets: `tags=["a", "b"]`, `values=[1, 2, 3]` -- type inferred from first element, same as Element.set() today
- Nulls via `value=None` -- natural Python pattern
- Time series columns as parallel kwargs arrays: `date_time=["2024-01-01"], value=[1.0]` -- same as current Element behavior
- kwargs-only, no dict positional arg -- users can do `**my_dict` unpacking if needed

**Element class fate:**
- Remove from `__init__.py` exports and `__all__` -- Element is no longer part of the public API
- Keep `element.py` as internal implementation detail -- `create_element`/`update_element` build an Element under the hood (same pattern as Julia)
- Keep `test_element.py` -- update imports from `from quiverdb import Element` to `from quiverdb.element import Element`

**Test migration:**
- Mechanical 1:1 swap: `Element().set("label", "x").set("value", 42)` becomes `label="x", value=42`
- Same test names, same structure, same assertions -- no reorganization
- Add explicit test that `from quiverdb import Element` raises `ImportError` (success criterion #3)
- Add one test verifying `**dict` unpacking works with `create_element`

### Claude's Discretion

- Whether to trim Element's public API (remove `__repr__`, `clear`, etc.) since it's internal-only now
- Internal implementation details of the kwargs-to-Element conversion
- Any type hint annotations on the kwargs parameters

### Deferred Ideas (OUT OF SCOPE)

None -- discussion stayed within phase scope

</user_constraints>

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PYAPI-01 | User can create an element with kwargs: `db.create_element("Collection", label="x", value=42)` | Julia reference pattern in `database_create.jl` lines 7-17 shows exact implementation. Python `**kwargs` maps directly to Julia's `kwargs...`. Element is built internally and destroyed in `finally`. |
| PYAPI-02 | User can update an element with kwargs: `db.update_element("Collection", id, label="y")` | Julia reference pattern in `database_update.jl` lines 6-17 shows exact implementation. Same kwargs-to-Element pattern as create. |
| PYAPI-03 | Element class is removed from public API and `__init__.py` exports | Current `__init__.py` exports Element in both import and `__all__`. Remove both. `element.py` stays as internal module. Test imports change to `from quiverdb.element import Element`. |
| PYAPI-04 | All Python tests pass with the new kwargs API | 249 `Element()` occurrences across 12 test files require mechanical conversion. `test_element.py` (15 occurrences) keeps direct Element usage via internal import. |
| PYAPI-05 | CLAUDE.md cross-layer naming table updated with new Python pattern | Lines 395-427 of CLAUDE.md contain the transformation rules and cross-layer table. Python column needs updating to reflect kwargs pattern for create/update. |

</phase_requirements>

## Standard Stack

### Core

No new libraries needed. This phase modifies existing Python code only.

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| cffi | >=2.0.0 | ABI-mode FFI bindings | Already in use, no change |
| pytest | >=8.4.1 | Test framework | Already in use, no change |
| ruff | >=0.12.2 | Linter/formatter | Already in use, no change |

### Supporting

None required -- no new dependencies for this phase.

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Internal Element builder | Direct C API calls from kwargs loop | Would bypass Element's type dispatch, duplicate logic, and lose the centralized type handling in `element.py`. Julia uses Element internally for the same reason. |
| `**kwargs` parameter | `dict` positional parameter (Dart pattern) | User decision locked: kwargs-only. Users can `**my_dict` for dict use case. |

## Architecture Patterns

### Recommended Changes

```
bindings/python/src/quiverdb/
  __init__.py              # MODIFY: remove Element from imports and __all__
  database.py              # MODIFY: add kwargs overloads for create_element, update_element
  element.py               # NO CHANGE: retained as internal implementation

bindings/python/tests/
  conftest.py              # MODIFY: remove Element import (not used as fixture)
  test_element.py          # MODIFY: change import to from quiverdb.element import Element
  test_database_create.py  # MODIFY: convert Element() to kwargs
  test_database_update.py  # MODIFY: convert Element() to kwargs
  test_database_delete.py  # MODIFY: convert Element() to kwargs
  ... (9 more test files)  # MODIFY: convert Element() to kwargs

CLAUDE.md                  # MODIFY: update Python column in cross-layer tables
```

### Pattern 1: kwargs-to-Element Internal Conversion (Julia Reference)

**What:** The public `create_element`/`update_element` methods accept `**kwargs`, build an `Element` internally, set each kwarg as an attribute, call the Element-based C API function, and destroy the Element in a `finally` block.

**When to use:** For both `create_element` and `update_element`.

**Reference implementation (Julia `database_create.jl` lines 7-17):**
```julia
function create_element!(db::Database, collection::String; kwargs...)
    e = Element()
    for (k, v) in kwargs
        e[String(k)] = v
    end
    try
        return create_element!(db, collection, e)
    finally
        destroy!(e)
    end
end
```

**Python equivalent:**
```python
def create_element(self, collection: str, **kwargs: object) -> int:
    """Create a new element. Returns the new element ID."""
    self._ensure_open()
    elem = Element()
    try:
        for name, value in kwargs.items():
            elem.set(name, value)
        lib = get_lib()
        out_id = ffi.new("int64_t*")
        check(
            lib.quiver_database_create_element(
                self._ptr,
                collection.encode("utf-8"),
                elem._ptr,
                out_id,
            )
        )
        return out_id[0]
    finally:
        elem.destroy()
```

**Key details:**
- `**kwargs: object` type hint -- per discretion, simple and accurate
- Element import is internal: `from quiverdb.element import Element`
- `finally` block ensures Element is always destroyed (RAII equivalent)
- No need to check for empty kwargs -- the C API handles that (it will raise "element must have at least one scalar attribute")

### Pattern 2: Dart Reference (Map-based)

**What:** Dart uses `Map<String, Object?>` as the public API, iterates entries to build Element.

**Reference (Dart `database_create.dart` lines 6-18):**
```dart
int createElement(String collection, Map<String, Object?> values) {
    _ensureNotClosed();
    final element = Element();
    try {
      for (final entry in values.entries) {
        element.set(entry.key, entry.value);
      }
      return createElementFromBuilder(collection, element);
    } finally {
      element.dispose();
    }
}
```

**Relevance:** Confirms the pattern. Dart keeps the Element-based method as `createElementFromBuilder` (internal). Python's old `create_element(collection, element)` signature becomes the internal path, and the new `create_element(collection, **kwargs)` is the public API.

### Pattern 3: Test Migration

**What:** Mechanical conversion from Element builder to kwargs.

**Before:**
```python
db.create_element("Collection", Element().set("label", "Item1").set("some_integer", 42))
```

**After:**
```python
db.create_element("Collection", label="Item1", some_integer=42)
```

**Multi-line before:**
```python
db.create_element(
    "Child",
    Element()
    .set("label", "Child 1")
    .set("parent_id", "Parent 1")
    .set("mentor_id", ["Parent 2"])
    .set("date_time", ["2024-01-01"])
    .set("sponsor_id", ["Parent 2"]),
)
```

**Multi-line after:**
```python
db.create_element(
    "Child",
    label="Child 1",
    parent_id="Parent 1",
    mentor_id=["Parent 2"],
    date_time=["2024-01-01"],
    sponsor_id=["Parent 2"],
)
```

**Update element before:**
```python
db.update_element("Collection", elem_id, Element().set("some_integer", 99))
```

**Update element after:**
```python
db.update_element("Collection", elem_id, some_integer=99)
```

### Anti-Patterns to Avoid

- **Removing element.py entirely:** The Element class still handles type dispatch (int, float, str, bool, None, lists) and C API marshaling. Duplicating this in database.py would violate DRY and the project's "Intelligence resides in C++ layer, bindings remain thin" principle.
- **Accepting both Element and kwargs:** This would create signature ambiguity. The old Element-based path becomes internal only (no `element` positional parameter on the public method).
- **Adding a `values: dict` positional parameter:** User decision locked this out. kwargs-only, users can `**my_dict`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Type dispatch for attribute values | Custom type checking in database.py | `Element.set()` method | Already handles int, float, str, bool->int, None, list type inference from first element. Tested in `test_element.py`. |
| C FFI string encoding | Manual encode in kwargs loop | `Element._set_string()` etc. | Element already handles `encode("utf-8")`, array marshaling, etc. |

**Key insight:** The Element class already has all the type dispatch logic. The kwargs layer is just a thin loop: `for name, value in kwargs.items(): elem.set(name, value)`. Do not replicate Element's internals.

## Common Pitfalls

### Pitfall 1: Breaking the Element positional parameter signature

**What goes wrong:** If the new kwargs-based signature still accepts an Element as a positional argument, users could accidentally pass a dict or other object as the second argument and get confusing errors.

**Why it happens:** The old signature is `create_element(self, collection: str, element)` -- if you just add `**kwargs` you get `create_element(self, collection: str, element=None, **kwargs)` which is ambiguous.

**How to avoid:** Replace the signature entirely. The new `create_element` accepts only `collection` as positional, everything else is kwargs. The internal Element-based call is done inside the method body, not exposed in the signature.

**Warning signs:** Tests still passing `Element()` as a positional arg to `create_element`.

### Pitfall 2: Forgetting to destroy Element on exception

**What goes wrong:** If Element creation succeeds but the C API call fails, the Element leaks.

**Why it happens:** Not using `try/finally` pattern.

**How to avoid:** Always use `try/finally` with `elem.destroy()` in the finally block. This is exactly what Julia does.

**Warning signs:** Element objects surviving beyond the method call (detectable by `__del__` warnings in tests).

### Pitfall 3: conftest.py still importing Element

**What goes wrong:** `conftest.py` currently imports `from quiverdb import Database, Element`. After removing Element from `__init__.py`, this will raise `ImportError` and ALL tests will fail.

**Why it happens:** conftest.py imports Element even though it doesn't use it in fixtures.

**How to avoid:** Remove the `Element` import from conftest.py as part of the migration.

**Warning signs:** All tests failing with ImportError before any individual test file is touched.

### Pitfall 4: test_database_time_series.py helper functions

**What goes wrong:** `_create_sensor` and `_create_collection_element` helpers in test_database_time_series.py use Element directly. These need to switch to kwargs too.

**Why it happens:** Helper functions are easy to miss during mechanical search-and-replace.

**How to avoid:** Grep for ALL `Element()` occurrences including in helper functions, not just in test methods.

**Warning signs:** ImportError in time_series tests.

### Pitfall 5: CSV test files using non-chained Element

**What goes wrong:** `test_database_csv_export.py` and `test_database_csv_import.py` use a non-chained pattern (`e1 = Element(); e1.set("label", "Item1"); e1.set("name", "Alpha"); ...`) rather than the chained pattern. The conversion is different: multi-line `e.set()` calls become kwargs on a single `create_element` call.

**Why it happens:** These tests were written before the fluent API was standard.

**How to avoid:** Recognize both patterns during conversion: chained (`Element().set().set()`) and sequential (`e = Element(); e.set(); e.set()`).

**Warning signs:** Leftover `Element()` constructor calls or `e.set()` calls in CSV test files.

## Code Examples

### Example 1: create_element with kwargs (target implementation)

```python
# In database.py
from quiverdb.element import Element  # internal import

def create_element(self, collection: str, **kwargs: object) -> int:
    """Create a new element. Returns the new element ID."""
    self._ensure_open()
    elem = Element()
    try:
        for name, value in kwargs.items():
            elem.set(name, value)
        lib = get_lib()
        out_id = ffi.new("int64_t*")
        check(
            lib.quiver_database_create_element(
                self._ptr,
                collection.encode("utf-8"),
                elem._ptr,
                out_id,
            )
        )
        return out_id[0]
    finally:
        elem.destroy()
```

### Example 2: update_element with kwargs (target implementation)

```python
def update_element(self, collection: str, id: int, **kwargs: object) -> None:
    """Update an existing element's attributes."""
    self._ensure_open()
    elem = Element()
    try:
        for name, value in kwargs.items():
            elem.set(name, value)
        lib = get_lib()
        check(
            lib.quiver_database_update_element(
                self._ptr,
                collection.encode("utf-8"),
                id,
                elem._ptr,
            )
        )
    finally:
        elem.destroy()
```

### Example 3: __init__.py after change

```python
from quiverdb.database import Database
from quiverdb.database_csv_export import DatabaseCSVExport
from quiverdb.database_csv_import import DatabaseCSVImport
from quiverdb.exceptions import QuiverError
from quiverdb.metadata import CSVOptions, GroupMetadata, ScalarMetadata

__all__ = [
    "CSVOptions",
    "Database",
    "DatabaseCSVExport",
    "DatabaseCSVImport",
    "QuiverError",
    "ScalarMetadata",
    "version",
]
```

### Example 4: dict unpacking test

```python
def test_create_element_with_dict_unpacking(self, collections_db: Database) -> None:
    collections_db.create_element("Configuration", label="cfg")
    attrs = {"label": "Item1", "some_integer": 42}
    elem_id = collections_db.create_element("Collection", **attrs)
    value = collections_db.read_scalar_integer_by_id("Collection", "some_integer", elem_id)
    assert value == 42
```

### Example 5: ImportError test

```python
def test_element_not_in_public_api() -> None:
    """Element is not importable from quiverdb public API."""
    with pytest.raises(ImportError):
        from quiverdb import Element  # noqa: F401
```

### Example 6: test_element.py import change

```python
# Before:
from quiverdb import Element

# After:
from quiverdb.element import Element
```

## Inventory of Changes

### Test file change counts (Element() occurrences to convert)

| File | Element() Count | Notes |
|------|----------------|-------|
| `test_database_create.py` | 48 | Largest file, FK resolution tests |
| `test_database_update.py` | 59 | Update + FK resolution tests |
| `test_database_read_scalar.py` | 25 | Setup elements for read tests |
| `test_database_read_set.py` | 25 | Setup elements for read tests |
| `test_database_read_vector.py` | 22 | Setup elements for read tests |
| `test_database_query.py` | 15 | Setup elements for query tests |
| `test_database_transaction.py` | 11 | Transaction control tests |
| `test_database_csv_export.py` | 11 | Non-chained Element pattern |
| `test_database_csv_import.py` | 8 | Non-chained Element pattern |
| `test_database_delete.py` | 8 | Delete cascade tests |
| `test_database_time_series.py` | 2 | Helper functions only |
| `test_element.py` | 15 | KEEP Element usage, change import only |
| `conftest.py` | 0 | Remove unused Element import |
| **Total** | **249** | 234 to convert to kwargs, 15 to keep |

### Source file changes

| File | Change |
|------|--------|
| `__init__.py` | Remove `Element` import and `__all__` entry |
| `database.py` | Replace `create_element(collection, element)` with `create_element(collection, **kwargs)`, same for `update_element`. Add `from quiverdb.element import Element` internal import. |

### Documentation changes

| File | Change |
|------|--------|
| `CLAUDE.md` | Update Python column in cross-layer naming tables. Add note that Python uses kwargs like Julia. Update Python Core API example. |

## Open Questions

1. **Element's `__repr__`, `clear`, `__del__` methods**
   - What we know: These are only useful for debugging and were part of the public API. Now Element is internal-only.
   - What's unclear: Whether to trim these methods.
   - Recommendation: Keep them. `__del__` is essential for cleanup safety. `__repr__` is useful for debugging. `clear` costs nothing. Trimming would be unnecessary churn with no benefit. (Claude's discretion area.)

2. **Type hints on kwargs**
   - What we know: `**kwargs: object` is the simplest accurate type hint. More precise would be `Union[int, float, str, None, bool, list[int], list[float], list[str]]` but Python's `**kwargs` typing doesn't support per-key types.
   - What's unclear: Whether a TypeAlias for the value type is worth it.
   - Recommendation: Use `**kwargs: object`. Simple, accurate, consistent with the thin-binding philosophy. Element.set() already does runtime type checking.

3. **`database.py` import of Element**
   - What we know: Currently `database.py` does not import Element. It receives Element as a parameter. After the change, it needs to import and instantiate Element internally.
   - What's unclear: Nothing -- this is straightforward.
   - Recommendation: Add `from quiverdb.element import Element` at the top of `database.py`.

## Sources

### Primary (HIGH confidence)

- **Julia binding source** (`bindings/julia/src/database_create.jl`, `database_update.jl`) -- Reference implementation for kwargs pattern
- **Dart binding source** (`bindings/dart/lib/src/database_create.dart`, `database_update.dart`) -- Confirms the Element-internal pattern
- **Python binding source** (`bindings/python/src/quiverdb/`) -- Current implementation analyzed directly
- **Python test files** (`bindings/python/tests/`) -- All 13 test files analyzed for Element usage patterns
- **CLAUDE.md** -- Project architecture and cross-layer naming conventions

### Secondary (MEDIUM confidence)

None required -- all information sourced from codebase.

### Tertiary (LOW confidence)

None.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new libraries, purely internal refactoring
- Architecture: HIGH -- Julia and Dart reference implementations exist and are verified in source
- Pitfalls: HIGH -- identified from direct analysis of all affected files

**Research date:** 2026-02-26
**Valid until:** 2026-03-26 (stable domain, no external dependencies)
