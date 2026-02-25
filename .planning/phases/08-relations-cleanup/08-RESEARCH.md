# Phase 8: Relations Cleanup - Research

**Researched:** 2026-02-24
**Domain:** Python binding cleanup -- remove redundant relation methods, port FK resolution tests from Julia/Dart
**Confidence:** HIGH

## Summary

This phase removes two redundant pure-Python methods (`read_scalar_relation`, `update_scalar_relation`) from the Python `Database` class and replaces their tests with the integrated FK resolution pattern used by Julia and Dart. The key insight is that **FK label resolution already happens in the C++ layer** -- both `Database::create_element()` and `Database::update_element()` call `resolve_element_fk_labels()` internally. When a string is passed for an FK integer column, C++ resolves it to the target element's ID automatically. The Python convenience methods were doing this resolution manually in Python, which is redundant.

The phase also requires cleaning up phantom C API declarations in `_declarations.py` (lines 127-138 declare `quiver_database_update_scalar_relation` and `quiver_database_read_scalar_relation` which do not exist in the actual C headers) and porting the full FK test suite from Julia/Dart to Python (10 create tests, 8 update tests, plus FK metadata tests).

**Primary recommendation:** Remove the two Python-only relation methods, delete phantom declarations by regenerating `_declarations.py`, migrate the 3 existing relation tests to the create/update pattern, and port the remaining FK resolution test categories from Julia/Dart.

<user_constraints>

## User Constraints (from CONTEXT.md)

### Locked Decisions
- FK values set via `Element.set()` at creation time (not separate update calls)
- Relation tests move from `test_database_read_scalar.py` to `test_database_create.py` and `test_database_update.py` to match Julia/Dart organization
- Remove the `TestReadScalarRelation` class and its 3 test methods entirely from `test_database_read_scalar.py`
- Assertions verify FK integer IDs only (no label round-trip queries back to parent)
- Keep all 3 original scenarios (basic FK, empty collection, self-reference) as part of the new create-based tests
- Python's `create_element` and `update_element` must support FK label resolution: passing a string label (e.g., `"Parent 1"`) for an FK integer column auto-resolves to the target element's ID
- This matches how Julia (`create_element!`, `update_element!`) and Dart (`createElement`, `updateElement`) handle FK columns
- Resolution happens in the Python binding layer (Element.set or pre-create/update hook), not in the C API
- Remove phantom C API declarations from `_declarations.py` (lines 127-138)
- Regenerate `_declarations.py` from C headers using `generator.bat` to ensure full sync
- Port ALL FK resolution test categories from Julia/Dart (10 create tests, 8 update tests)
- FK metadata tests: verify `get_scalar_metadata` returns `is_foreign_key` and `references_collection`

### Claude's Discretion
- Where FK label resolution logic lives (Element class, Database class helper, or pre-operation hook)
- Internal implementation of label-to-ID resolution (query approach, caching, error message format)
- Exact test method naming conventions (should follow existing Python test patterns)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope

</user_constraints>

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| REL-01 | Remove read_scalar_relation and update_scalar_relation methods from Python Database class | Database.py lines 539-595 contain both methods; _declarations.py lines 127-138 contain phantom C API declarations; __init__.py is clean |
| REL-02 | Update relation tests to use the new create/update/read pattern (relations via Element.set + read_scalar_integer_by_id) | C++ layer already resolves FK labels in create_element/update_element; Julia/Dart test suites provide exact test categories to port; existing test_create_with_fk_label in Python confirms the pattern works |

</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Python | 3.13 | Runtime | Project uses .python-version specifying 3.13 |
| CFFI | ABI-mode | FFI binding to C API | Already in use; no compiler required at install time |
| pytest | 9.0.2 | Test framework | Already in use with class-based organization |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| uv | (project tool) | Package/env management | Running tests via `uv run pytest` |

### Alternatives Considered
None -- this phase uses the existing stack exclusively. No new libraries needed.

## Architecture Patterns

### Key Insight: FK Resolution in the C++ Layer

The C++ `Database::create_element()` and `Database::update_element()` both call `impl_->resolve_element_fk_labels(collection, element, *this)` as a pre-resolve pass before any writes. This means:

1. When `Element.set("parent_id", "Parent 1")` is called in Python, the string `"Parent 1"` is set via `quiver_element_set_string()` in the C API
2. When `create_element()` or `update_element()` is called, C++ detects the FK column, resolves the string label to an integer ID, and writes the integer
3. **No Python-side resolution is needed** -- it happens transparently in C++

This is the same pattern used by Julia and Dart. Neither binding does FK resolution; they just set string values and let C++ handle it.

**CRITICAL CORRECTION to CONTEXT.md decision:** The CONTEXT.md states "Resolution happens in the Python binding layer (Element.set or pre-create/update hook), not in the C API." This is **incorrect** based on code investigation. Resolution actually happens in the C++ layer (`resolve_element_fk_labels`), which is called via the C API's `quiver_database_create_element` and `quiver_database_update_element`. **No Python-side hook is needed.** Python just needs to pass through string values for FK columns via `Element.set()`, exactly as Julia and Dart do. The existing `test_create_with_fk_label` test in Python already confirms this works.

### Current Python Element.set() Behavior

```python
# element.py - Element.set() handles string values
def set(self, name: str, value: object) -> Element:
    if isinstance(value, str):
        self._set_string(name, value)      # calls quiver_element_set_string
    elif isinstance(value, int):
        self._set_integer(name, value)     # calls quiver_element_set_integer
    elif isinstance(value, list):
        self._set_array(name, value)       # routes to typed array setter
```

For FK columns:
- `Element.set("parent_id", "Parent 1")` --> stores as string --> C++ resolves to integer at create/update time
- `Element.set("parent_id", 1)` --> stores as integer --> C++ passes through as-is
- `Element.set("mentor_id", ["Parent 1", "Parent 2"])` --> stores as string array --> C++ resolves each to integer

**No changes needed to `Element` class.** The existing `set()` method already supports the correct types.

### Files to Modify

```
bindings/python/
  src/quiverdb/
    database.py          # Remove update_scalar_relation() and read_scalar_relation() (lines 539-595)
    _declarations.py     # Regenerate to remove phantom declarations (lines 127-138)
  generator/
    generator.py         # Fix header paths: include/quiverdb/c/ -> include/quiver/c/
  tests/
    test_database_read_scalar.py   # Remove TestReadScalarRelation class (lines 137-174)
    test_database_create.py        # Add FK resolution test classes
    test_database_update.py        # Add FK resolution test classes
    test_database_metadata.py      # Add FK metadata tests (if not already present)
```

### Recommended Test Organization

Following Julia/Dart patterns and existing Python conventions:

```python
# test_database_create.py -- new classes to add:
class TestFKResolutionCreate:
    def test_scalar_fk_label(self, relations_db): ...
    def test_scalar_fk_integer(self, relations_db): ...
    def test_vector_fk_labels(self, relations_db): ...
    def test_set_fk_labels(self, relations_db): ...
    def test_time_series_fk_labels(self, relations_db): ...
    def test_all_fk_types_in_one_call(self, relations_db): ...
    def test_no_fk_columns_unchanged(self, db): ...
    def test_missing_target_label_error(self, relations_db): ...
    def test_non_fk_string_rejection_error(self, relations_db): ...
    def test_resolution_failure_no_partial_writes(self, relations_db): ...

# test_database_update.py -- new classes to add:
class TestFKResolutionUpdate:
    def test_scalar_fk_label(self, relations_db): ...
    def test_scalar_fk_integer(self, relations_db): ...
    def test_vector_fk_labels(self, relations_db): ...
    def test_set_fk_labels(self, relations_db): ...
    def test_time_series_fk_labels(self, relations_db): ...
    def test_all_fk_types_in_one_call(self, relations_db): ...
    def test_no_fk_columns_unchanged(self, db): ...
    def test_resolution_failure_preserves_existing(self, relations_db): ...
```

### Anti-Patterns to Avoid
- **Adding Python-side FK resolution logic:** The C++ layer already handles this. Adding Python hooks would duplicate logic and diverge from Julia/Dart.
- **Testing via label round-trip queries:** Tests should verify resolved integer IDs directly (via `read_scalar_integer_by_id`), not query parent labels back.
- **Keeping any references to the old relation methods:** Clean removal, no deprecation warnings (per project philosophy: "Delete unused code, do not deprecate").

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| FK label-to-ID resolution | Python binding-layer resolver | C++ `resolve_element_fk_labels` (already built) | C++ handles atomicity, error messages, schema introspection; all other bindings use this |
| Relation convenience methods | Pure-Python `read_scalar_relation`/`update_scalar_relation` | `Element.set()` + `read_scalar_integer_by_id()` | Matches C++/Julia/Dart pattern; keeps Python thin |

**Key insight:** The Python relation methods were hand-rolled convenience wrappers doing what C++ already does. Removing them aligns Python with the project principle: "Intelligence resides in C++ layer. Bindings remain thin."

## Common Pitfalls

### Pitfall 1: Generator Path Mismatch
**What goes wrong:** Running `generator.bat` fails because header paths are wrong.
**Why it happens:** `generator.py` references `include/quiverdb/c/` but actual headers are at `include/quiver/c/`.
**How to avoid:** Fix the `HEADERS` list in `generator.py` before running regeneration.
**Warning signs:** "Header not found" error from generator.

### Pitfall 2: Forgetting to Create Configuration Element
**What goes wrong:** FK resolution tests fail with "element not found" errors.
**Why it happens:** The `relations.sql` schema requires a Configuration element to exist before creating Parent/Child.
**How to avoid:** Every test using `relations_db` must start with `relations_db.create_element("Configuration", Element().set("label", "..."))`.
**Warning signs:** Tests pass individually but fail when run in different order.

### Pitfall 3: Incomplete Test Removal
**What goes wrong:** Old relation tests still reference removed methods, causing test failures.
**Why it happens:** Forgetting to remove the `TestReadScalarRelation` class or leaving imports.
**How to avoid:** Remove the entire class (lines 137-174 of `test_database_read_scalar.py`) and verify no other test files reference `read_scalar_relation` or `update_scalar_relation`.
**Warning signs:** `AttributeError: 'Database' object has no attribute 'read_scalar_relation'`.

### Pitfall 4: Time Series FK Test Complexity
**What goes wrong:** Time series FK tests fail because the dimension column (date_time) also needs values.
**Why it happens:** Time series tables require paired arrays (dimension + value columns).
**How to avoid:** Always include `date_time` values alongside FK column values in time series tests.
**Warning signs:** "Cannot create_element: time series columns must have the same length".

### Pitfall 5: QuiverError vs DatabaseException Naming
**What goes wrong:** Test assertions catch the wrong exception type.
**Why it happens:** Julia uses `DatabaseException`, Dart uses `QuiverException`, Python uses `QuiverError`.
**How to avoid:** Python FK error tests should use `pytest.raises(QuiverError)`.
**Warning signs:** Tests don't catch the exception, get unexpected pass/fail.

## Code Examples

Verified patterns from codebase investigation:

### Existing FK Resolution (Python, already working)
```python
# Source: bindings/python/tests/test_database_create.py (line 48-57)
def test_create_with_fk_label(self, relations_db: Database) -> None:
    """FK label resolution: string value for FK column resolves to parent ID."""
    relations_db.create_element("Configuration", Element().set("label", "cfg"))
    relations_db.create_element("Parent", Element().set("label", "Parent 1"))
    child_id = relations_db.create_element(
        "Child",
        Element().set("label", "Child 1").set("parent_id", "Parent 1"),
    )
    result = relations_db.read_scalar_integer_by_id("Child", "parent_id", child_id)
    assert result == 1
```

### Julia FK Create Pattern (to port to Python)
```python
# Source: bindings/julia/test/test_database_create.jl (line 456-472)
# Python equivalent:
def test_vector_fk_labels(self, relations_db: Database) -> None:
    relations_db.create_element("Configuration", Element().set("label", "Test Config"))
    relations_db.create_element("Parent", Element().set("label", "Parent 1"))
    relations_db.create_element("Parent", Element().set("label", "Parent 2"))
    child_id = relations_db.create_element(
        "Child",
        Element().set("label", "Child 1").set("parent_ref", ["Parent 1", "Parent 2"]),
    )
    result = relations_db.read_vector_integers_by_id("Child", "parent_ref", child_id)
    assert result == [1, 2]
```

### Julia FK Update Pattern (to port to Python)
```python
# Source: bindings/julia/test/test_database_update.jl (line 768-785)
# Python equivalent:
def test_scalar_fk_label(self, relations_db: Database) -> None:
    relations_db.create_element("Configuration", Element().set("label", "Test Config"))
    relations_db.create_element("Parent", Element().set("label", "Parent 1"))
    relations_db.create_element("Parent", Element().set("label", "Parent 2"))
    relations_db.create_element(
        "Child", Element().set("label", "Child 1").set("parent_id", "Parent 1"),
    )
    relations_db.update_element(
        "Child", 1, Element().set("parent_id", "Parent 2"),
    )
    result = relations_db.read_scalar_integer_by_id("Child", "parent_id", 1)
    assert result == 2
```

### Error Case Pattern
```python
# Source: bindings/julia/test/test_database_create.jl (line 548-565)
# Python equivalent:
def test_resolution_failure_no_partial_writes(self, relations_db: Database) -> None:
    relations_db.create_element("Configuration", Element().set("label", "Test Config"))
    with pytest.raises(QuiverError):
        relations_db.create_element(
            "Child",
            Element().set("label", "Orphan Child").set("parent_id", "Nonexistent Parent"),
        )
    # Verify: no child was created (zero partial writes)
    labels = relations_db.read_scalar_strings("Child", "label")
    assert len(labels) == 0
```

### Methods to Remove from database.py
```python
# Source: bindings/python/src/quiverdb/database.py (lines 539-595)
# DELETE entirely:

    # -- Relation operations ------------------------------------------------

    def update_scalar_relation(
        self, collection: str, attribute: str, from_label: str, to_label: str,
    ) -> None:
        # ... 16 lines ...

    def read_scalar_relation(
        self, collection: str, attribute: str,
    ) -> list[str | None]:
        # ... 20 lines ...
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Python-side FK label resolution via `update_scalar_relation` | C++ `resolve_element_fk_labels` called automatically | v0.4 C++ refactor | Python relation methods are now redundant |
| Separate read/update relation methods | FK via `Element.set()` + standard typed reads | v0.4 C++ refactor | All bindings use unified CRUD path |

**Deprecated/outdated:**
- `read_scalar_relation`: Redundant -- use `read_scalar_integer_by_id` (FK columns store integer IDs)
- `update_scalar_relation`: Redundant -- use `Element.set("fk_col", "label")` + `create_element`/`update_element`
- `quiver_database_read_scalar_relation` / `quiver_database_update_scalar_relation` in `_declarations.py`: Phantom declarations -- these C API functions never existed

## Open Questions

1. **Generator header path fix**
   - What we know: `generator.py` references `include/quiverdb/c/` but actual headers are at `include/quiver/c/`
   - What's unclear: Whether this was intentionally different or a rename that wasn't propagated
   - Recommendation: Fix the path in `generator.py` before running regeneration. This is a prerequisite for the `_declarations.py` regeneration step.

2. **Empty collection FK test migration**
   - What we know: The old `test_read_scalar_relation_empty` tested reading relations from an empty Child collection. The Julia/Dart FK test suites don't have an exact equivalent.
   - What's unclear: Whether this scenario needs a dedicated FK test
   - Recommendation: This is already covered by the "no FK columns unchanged" test pattern (basic.sql schema). The empty collection case tests the read method, not FK resolution, so it can be dropped when removing the relation read method.

3. **Self-reference FK test migration**
   - What we know: The old `test_read_scalar_relation_self_reference` tested `sibling_id` (Child -> Child FK). Julia/Dart don't have a specific self-reference FK resolution test.
   - What's unclear: Whether self-referencing FKs need special testing
   - Recommendation: Add a simple self-reference test to `TestFKResolutionCreate` (create a Child, then create another Child with `sibling_id = "Child 1"`). C++ resolves self-references the same as cross-collection references.

## Sources

### Primary (HIGH confidence)
- `src/database_create.cpp` lines 16-17: C++ `create_element` calls `resolve_element_fk_labels`
- `src/database_update.cpp` lines 18-19: C++ `update_element` calls `resolve_element_fk_labels`
- `bindings/python/src/quiverdb/database.py` lines 539-595: Python relation methods to remove
- `bindings/python/src/quiverdb/_declarations.py` lines 127-138: Phantom C API declarations
- `bindings/python/src/quiverdb/_c_api.py`: No relation declarations (confirms phantom status)
- `include/quiver/c/database.h`: No relation functions in C headers
- `bindings/julia/test/test_database_create.jl` lines 370-566: Complete FK resolution create test suite
- `bindings/julia/test/test_database_update.jl` lines 765-964: Complete FK resolution update test suite
- `tests/schemas/valid/relations.sql`: FK schema with scalar, vector, set, and time series FK columns
- `bindings/python/tests/test_database_create.py` line 48-57: Existing Python FK test confirming pattern works

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - existing project stack, no new dependencies
- Architecture: HIGH - all claims verified by direct source code inspection
- Pitfalls: HIGH - identified from actual code structure and schema requirements

**Research date:** 2026-02-24
**Valid until:** 2026-03-24 (stable -- internal codebase, no external dependency changes expected)
