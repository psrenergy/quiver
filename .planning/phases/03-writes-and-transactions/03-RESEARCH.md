# Phase 3: Writes and Transactions - Research

**Researched:** 2026-02-23
**Domain:** Python bindings for CRUD write operations and transaction control (CFFI ABI-mode wrapping C API)
**Confidence:** HIGH

## Summary

Phase 3 adds all write operations (create, update, delete elements; scalar/vector/set attribute updates; relation updates) and explicit transaction control with a context manager to the Python bindings. All operations are thin wrappers around existing C API functions that are already declared in `_declarations.py` (auto-generated) but not yet exposed in the hand-written `_c_api.py` cdef.

The implementation follows established Phase 1/2 patterns exactly: add CFFI declarations to `_c_api.py`, add methods to `Database` class in `database.py`, write pytest-based test files. The C API signatures are fully known from the headers and existing `_declarations.py`. The main technical considerations are: (1) marshaling Python lists to C arrays for vector/set updates, (2) handling `_Bool*` for `in_transaction`, (3) implementing the transaction context manager with `@contextmanager`, and (4) the `update_scalar_string` NULL limitation (C API requires non-NULL value parameter).

**Primary recommendation:** Follow the exact patterns from Phase 1/2 for CFFI wrapping. Use `_declarations.py` as the authoritative reference for function signatures. Implement `transaction()` as a `@contextmanager` that yields `db` for cross-binding consistency.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Transaction context manager uses `@contextmanager` from `contextlib`
- `with db.transaction() as db:` yields `db` back (mirrors Julia/Dart)
- Auto-commits on success, rolls back on exception (best-effort rollback, swallow rollback errors)
- No return value from context manager; use variables outside `with` block
- Vector/set updates accept plain Python lists
- Empty list clears vector/set
- `update_scalar_string` accepts `None` to set NULL
- `update_element` reuses existing `Element` class
- Plan 03-01: Write operations (CFFI declarations, update_element, delete_element, all scalar/vector/set updates, write tests)
- Plan 03-02: Transactions (CFFI declarations, begin_transaction/commit/rollback/in_transaction, context manager, transaction tests)
- One test file per functional area: `test_database_create.py`, `test_database_update.py`, `test_database_delete.py`, `test_database_transaction.py`

### Claude's Discretion
- Parameter order for update methods (follow C API or adjust for Python idiom)
- Error propagation on transaction rollback (propagate original exception vs wrap)
- Internal marshaling approach for list-to-C-array conversion

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| WRITE-01 | create_element wrapping C API with Element parameter and returned ID | Already implemented in `database.py` -- exists from Phase 1. Needs `test_database_create.py` test file. |
| WRITE-02 | update_element wrapping C API with Element parameter | C API signature: `quiver_database_update_element(db, collection, id, element)`. Requires CFFI declaration + Database method. |
| WRITE-03 | delete_element wrapping C API | C API signature: `quiver_database_delete_element(db, collection, id)`. Simple scalar call, no output params. |
| WRITE-04 | update_scalar_integer/float/string for individual scalar updates | Three C API functions with simple scalar params. String variant needs special NULL handling (see pitfalls). |
| WRITE-05 | update_vector_integers/floats/strings for vector replacement | C API takes `(db, collection, attribute, id, values_ptr, count)`. Requires list-to-C-array marshaling. |
| WRITE-06 | update_set_integers/floats/strings for set replacement | Identical signature pattern to vector updates. Same marshaling approach. |
| WRITE-07 | update_scalar_relation for relation updates | Already implemented in `database.py` from Phase 2 (added early for test setup). No additional work needed. |
| TXN-01 | begin_transaction, commit, rollback, in_transaction methods | Four C API functions. `in_transaction` uses `_Bool*` output param -- use CFFI `_Bool` type. |
| TXN-02 | transaction() context manager wrapping begin/commit/rollback | Pure Python using `@contextmanager`. Follows Julia `transaction(fn, db)` / Dart `db.transaction((db) {...})` pattern. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| cffi | (installed) | ABI-mode FFI binding to C API | Already chosen in Phase 1, project standard |
| contextlib | stdlib | `@contextmanager` decorator for transaction context | Python stdlib, cleanest way to implement context managers |
| pytest | (installed) | Test framework | Already established in Phase 1/2 tests |

### Supporting
No additional libraries needed. All marshaling uses CFFI's `ffi.new()` and `ffi.NULL`.

## Architecture Patterns

### Files Modified (Plan 03-01: Write Operations)
```
bindings/python/src/quiver/
    _c_api.py              # Add CFFI cdef declarations for write operations
    database.py            # Add update_element, delete_element, update_scalar_*, update_vector_*, update_set_* methods
bindings/python/tests/
    test_database_create.py   # NEW: tests for create_element (moved from read test setup usage)
    test_database_update.py   # NEW: tests for update_element and all scalar/vector/set update methods
    test_database_delete.py   # NEW: tests for delete_element
```

### Files Modified (Plan 03-02: Transactions)
```
bindings/python/src/quiver/
    _c_api.py              # Add CFFI cdef for transaction functions (if not already added in 03-01)
    database.py            # Add begin_transaction, commit, rollback, in_transaction, transaction() context manager
bindings/python/tests/
    test_database_transaction.py  # NEW: tests for explicit transactions and context manager
```

### Pattern 1: Simple Scalar Write (no output params)
**What:** C API functions that take scalar input parameters and return only an error code.
**When to use:** `update_scalar_integer`, `update_scalar_float`, `delete_element`
**Example:**
```python
# Source: Existing database.py pattern for update_scalar_relation
def update_scalar_integer(self, collection: str, attribute: str, id: int, value: int) -> None:
    """Update an integer scalar attribute by element ID."""
    self._ensure_open()
    lib = get_lib()
    check(lib.quiver_database_update_scalar_integer(
        self._ptr,
        collection.encode("utf-8"),
        attribute.encode("utf-8"),
        id,
        value,
    ))
```

### Pattern 2: List-to-C-Array Marshaling (vector/set updates)
**What:** Converting Python lists to C arrays for vector/set replacement operations.
**When to use:** `update_vector_integers`, `update_set_strings`, etc.
**Example:**
```python
# Source: Existing element.py _set_array_integer pattern
def update_vector_integers(self, collection: str, attribute: str, id: int, values: list[int]) -> None:
    """Replace the integer vector for an element."""
    self._ensure_open()
    lib = get_lib()
    if len(values) == 0:
        check(lib.quiver_database_update_vector_integers(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, ffi.NULL, 0,
        ))
    else:
        c_arr = ffi.new("int64_t[]", values)
        check(lib.quiver_database_update_vector_integers(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, c_arr, len(values),
        ))
```

### Pattern 3: String Array Marshaling
**What:** Converting Python string lists to `const char* const*` for C API.
**When to use:** `update_vector_strings`, `update_set_strings`
**Example:**
```python
# Source: Existing element.py _set_array_string pattern
def update_vector_strings(self, collection: str, attribute: str, id: int, values: list[str]) -> None:
    """Replace the string vector for an element."""
    self._ensure_open()
    lib = get_lib()
    if len(values) == 0:
        check(lib.quiver_database_update_vector_strings(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, ffi.NULL, 0,
        ))
    else:
        encoded = [v.encode("utf-8") for v in values]
        c_strings = [ffi.new("char[]", e) for e in encoded]
        c_arr = ffi.new("const char*[]", c_strings)
        check(lib.quiver_database_update_vector_strings(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, c_arr, len(values),
        ))
```

### Pattern 4: Transaction Context Manager
**What:** `@contextmanager` yielding `db` with auto-commit/rollback.
**When to use:** `db.transaction()` context manager
**Example:**
```python
# Source: Julia bindings/julia/src/database_transaction.jl + Dart database_transaction.dart
from contextlib import contextmanager

@contextmanager
def transaction(self):
    """Execute a block within a transaction. Auto-commits on success, rolls back on exception."""
    self._ensure_open()
    self.begin_transaction()
    try:
        yield self
        self.commit()
    except BaseException:
        try:
            self.rollback()
        except Exception:
            pass  # Best-effort rollback; swallow rollback errors
        raise
```

### Pattern 5: Bool Output Parameter (in_transaction)
**What:** C API returns `_Bool*` (from `stdbool.h`), needs CFFI `_Bool` type.
**When to use:** `in_transaction`
**Example:**
```python
# CFFI cdef uses _Bool (matching _declarations.py)
# quiver_error_t quiver_database_in_transaction(quiver_database_t* db, _Bool* out_active);

def in_transaction(self) -> bool:
    """Return True if an explicit transaction is active."""
    self._ensure_open()
    lib = get_lib()
    out = ffi.new("_Bool*")
    check(lib.quiver_database_in_transaction(self._ptr, out))
    return bool(out[0])
```

### Anti-Patterns to Avoid
- **Do not use `int*` for `in_transaction`:** The C API uses `bool*` (`_Bool*` in CFFI). Using `int*` would cause undefined behavior due to size mismatch.
- **Do not forget to keep string references alive:** When building `const char*[]` arrays, the encoded bytes and `ffi.new("char[]", ...)` objects must remain in scope until the C call completes. CFFI handles this for local variables, but beware of list comprehensions that might GC intermediates.
- **Do not catch `BaseException` in transaction commit path:** The `try: yield self; self.commit()` must allow the `commit()` exception to propagate. Only the rollback should swallow errors.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Context manager | Custom `__enter__`/`__exit__` class | `@contextmanager` from `contextlib` | Cleaner, less code, same functionality |
| Array type conversion | Manual memory allocation | `ffi.new("int64_t[]", values)` | CFFI handles allocation and GC correctly |
| String array building | Manual pointer arithmetic | `ffi.new("const char*[]", c_strings)` | Matches existing `element.py` pattern exactly |

## Common Pitfalls

### Pitfall 1: update_scalar_string with None (NULL)
**What goes wrong:** The C API `quiver_database_update_scalar_string` has `QUIVER_REQUIRE(db, collection, attribute, value)` -- passing NULL for `value` returns QUIVER_ERROR.
**Why it happens:** CONTEXT.md says `update_scalar_string` should accept `None` to set NULL, but the C API doesn't support it directly.
**How to avoid:** When `value is None`, use `update_element` with `Element().set(attribute, None)` instead of calling `update_scalar_string` with NULL. This routes through `quiver_element_set_null` which IS supported.
**Warning signs:** `QuiverError` when passing None to update_scalar_string.

### Pitfall 2: _Bool vs int for in_transaction
**What goes wrong:** Using `ffi.new("int*")` for a `_Bool*` parameter causes memory corruption or incorrect values because `_Bool` is 1 byte, `int` is 4 bytes.
**Why it happens:** CFFI is strict about type sizes. The C header uses `bool` from `<stdbool.h>`, which the generator correctly maps to `_Bool`.
**How to avoid:** Use `ffi.new("_Bool*")` exactly as shown in `_declarations.py`. The existing `_c_api.py` does not yet have this declaration, so it must be added.
**Warning signs:** Incorrect boolean return values, crashes on some platforms.

### Pitfall 3: Empty list handling for vector/set updates
**What goes wrong:** Passing `ffi.new("int64_t[]", [])` to CFFI creates a zero-length array, but the pointer may be invalid.
**Why it happens:** C API accepts `NULL` pointer with count=0 to clear a vector/set, but a zero-length CFFI array is not NULL.
**How to avoid:** Explicitly check `if len(values) == 0` and pass `ffi.NULL` with count `0`. The C API implementation validates `if (count > 0 && !values)` -- so NULL with count 0 is fine.
**Warning signs:** Mysterious errors or empty arrays not clearing properly.

### Pitfall 4: CFFI string lifetime in list comprehensions
**What goes wrong:** When building `const char*[]` from a list of strings, intermediate `ffi.new("char[]", ...)` objects can be garbage collected before the C call.
**Why it happens:** CFFI keeps references alive only if assigned to a variable that survives until the call. List comprehensions can create temporaries that get collected.
**How to avoid:** Store the intermediate list explicitly: `c_strings = [ffi.new("char[]", e) for e in encoded]` then `c_arr = ffi.new("const char*[]", c_strings)`. Both `c_strings` and `c_arr` must be in scope when the C function is called.
**Warning signs:** Segfaults or corrupted strings, especially with longer string lists.

### Pitfall 5: update_element signature difference from create_element
**What goes wrong:** `create_element` takes `(db, collection, element, out_id)` but `update_element` takes `(db, collection, id, element)` -- the `id` parameter position is between `collection` and `element`.
**Why it happens:** Different C API signature ordering.
**How to avoid:** Check the C header carefully. Python method should be `update_element(self, collection, id, element)` matching the C API parameter order.
**Warning signs:** Type errors or wrong element being updated.

### Pitfall 6: Transaction context manager must catch BaseException, not Exception
**What goes wrong:** Using `except Exception` misses `KeyboardInterrupt` and `SystemExit`, potentially leaving a transaction open.
**Why it happens:** Common Python mistake -- `Exception` doesn't catch all exit paths.
**How to avoid:** Use `except BaseException` in the context manager to ensure rollback on any exit. The re-raise ensures the original exception propagates.
**Warning signs:** Dangling transactions after Ctrl+C.

## Code Examples

### CFFI Declarations to Add (from _declarations.py reference)
```python
# Add to _c_api.py ffi.cdef() block:

    // Update element
    quiver_error_t quiver_database_update_element(quiver_database_t* db,
                                                   const char* collection,
                                                   int64_t id,
                                                   const quiver_element_t* element);
    quiver_error_t quiver_database_delete_element(quiver_database_t* db,
                                                   const char* collection,
                                                   int64_t id);

    // Update scalar attributes
    quiver_error_t quiver_database_update_scalar_integer(quiver_database_t* db,
                                                         const char* collection,
                                                         const char* attribute,
                                                         int64_t id,
                                                         int64_t value);
    quiver_error_t quiver_database_update_scalar_float(quiver_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       int64_t id,
                                                       double value);
    quiver_error_t quiver_database_update_scalar_string(quiver_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        int64_t id,
                                                        const char* value);

    // Update vector attributes
    quiver_error_t quiver_database_update_vector_integers(quiver_database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          int64_t id,
                                                          const int64_t* values,
                                                          size_t count);
    quiver_error_t quiver_database_update_vector_floats(quiver_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        int64_t id,
                                                        const double* values,
                                                        size_t count);
    quiver_error_t quiver_database_update_vector_strings(quiver_database_t* db,
                                                         const char* collection,
                                                         const char* attribute,
                                                         int64_t id,
                                                         const char* const* values,
                                                         size_t count);

    // Update set attributes
    quiver_error_t quiver_database_update_set_integers(quiver_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       int64_t id,
                                                       const int64_t* values,
                                                       size_t count);
    quiver_error_t quiver_database_update_set_floats(quiver_database_t* db,
                                                     const char* collection,
                                                     const char* attribute,
                                                     int64_t id,
                                                     const double* values,
                                                     size_t count);
    quiver_error_t quiver_database_update_set_strings(quiver_database_t* db,
                                                      const char* collection,
                                                      const char* attribute,
                                                      int64_t id,
                                                      const char* const* values,
                                                      size_t count);

    // Transaction control
    quiver_error_t quiver_database_begin_transaction(quiver_database_t* db);
    quiver_error_t quiver_database_commit(quiver_database_t* db);
    quiver_error_t quiver_database_rollback(quiver_database_t* db);
    quiver_error_t quiver_database_in_transaction(quiver_database_t* db, _Bool* out_active);
```

### update_element Method
```python
# Source: C API header include/quiver/c/database.h line 66-69
def update_element(self, collection: str, id: int, element) -> None:
    """Update an existing element's attributes by ID."""
    self._ensure_open()
    lib = get_lib()
    check(lib.quiver_database_update_element(
        self._ptr, collection.encode("utf-8"), id, element._ptr,
    ))
```

### delete_element Method
```python
# Source: C API header include/quiver/c/database.h line 70
def delete_element(self, collection: str, id: int) -> None:
    """Delete an element by ID."""
    self._ensure_open()
    lib = get_lib()
    check(lib.quiver_database_delete_element(
        self._ptr, collection.encode("utf-8"), id,
    ))
```

### update_scalar_string with None Support
```python
# Source: C API + CONTEXT.md decision
def update_scalar_string(self, collection: str, attribute: str, id: int, value: str | None) -> None:
    """Update a string scalar attribute. Pass None to set NULL."""
    self._ensure_open()
    lib = get_lib()
    if value is None:
        # C API requires non-NULL value; use update_element with null instead
        from quiver.element import Element
        e = Element()
        try:
            e.set(attribute, None)
            check(lib.quiver_database_update_element(
                self._ptr, collection.encode("utf-8"), id, e._ptr,
            ))
        finally:
            e.destroy()
    else:
        check(lib.quiver_database_update_scalar_string(
            self._ptr, collection.encode("utf-8"), attribute.encode("utf-8"),
            id, value.encode("utf-8"),
        ))
```

### Transaction Context Manager
```python
# Source: Julia database_transaction.jl + Dart database_transaction.dart + CONTEXT.md
from contextlib import contextmanager

@contextmanager
def transaction(self):
    """Execute operations within a transaction.

    Auto-commits on successful exit, rolls back on exception.
    Usage: `with db.transaction() as db: ...`
    """
    self._ensure_open()
    self.begin_transaction()
    try:
        yield self
        self.commit()
    except BaseException:
        try:
            self.rollback()
        except Exception:
            pass  # Best-effort rollback
        raise
```

### Test Structure Examples
```python
# test_database_create.py -- mirrors Julia test_database_create.jl
class TestCreateElement:
    def test_create_element_returns_id(self, db: Database) -> None:
        e = Element().set("label", "item1").set("integer_attribute", 42)
        id1 = db.create_element("Configuration", e)
        assert isinstance(id1, int)
        assert id1 > 0

    def test_create_multiple_elements(self, db: Database) -> None:
        id1 = db.create_element("Configuration", Element().set("label", "item1"))
        id2 = db.create_element("Configuration", Element().set("label", "item2"))
        assert id1 != id2

# test_database_update.py -- mirrors Julia test_database_update.jl
class TestUpdateElement:
    def test_update_single_scalar(self, db: Database) -> None:
        id1 = db.create_element("Configuration", Element().set("label", "Config 1").set("integer_attribute", 100))
        db.update_element("Configuration", id1, Element().set("integer_attribute", 999))
        assert db.read_scalar_integer_by_id("Configuration", "integer_attribute", id1) == 999

# test_database_delete.py -- mirrors Julia test_database_delete.jl
class TestDeleteElement:
    def test_delete_element(self, db: Database) -> None:
        id1 = db.create_element("Configuration", Element().set("label", "item1"))
        db.delete_element("Configuration", id1)
        assert db.read_element_ids("Configuration") == []

# test_database_transaction.py -- mirrors Julia test_database_transaction.jl
class TestTransaction:
    def test_begin_and_commit_persists(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "Config"))
        collections_db.begin_transaction()
        collections_db.create_element("Collection", Element().set("label", "Item 1").set("some_integer", 10))
        collections_db.commit()
        labels = collections_db.read_scalar_strings("Collection", "label")
        assert labels == ["Item 1"]

class TestTransactionContextManager:
    def test_auto_commits_on_success(self, collections_db: Database) -> None:
        collections_db.create_element("Configuration", Element().set("label", "Config"))
        with collections_db.transaction() as db:
            db.create_element("Collection", Element().set("label", "Item 1").set("some_integer", 42))
        labels = collections_db.read_scalar_strings("Collection", "label")
        assert labels == ["Item 1"]
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Custom `__enter__`/`__exit__` | `@contextmanager` | Python 3.2+ | Simpler, less boilerplate |

No deprecated approaches relevant to this phase.

## Parameter Order Recommendation (Claude's Discretion)

The C API parameter order for scalar updates is: `(db, collection, attribute, id, value)`. Julia follows this exactly. The Python binding should follow the same order for cross-binding consistency:

```python
# Recommended (matches C API and Julia):
db.update_scalar_integer("Configuration", "integer_attribute", 1, 999)

# NOT recommended (reordering for "Python idiom"):
db.update_scalar_integer("Configuration", 1, "integer_attribute", 999)
```

**Rationale:** The project principle "Homogeneity: Binding interfaces must be consistent and intuitive" means matching parameter order across bindings. Julia uses `(db, collection, attribute, id, value)` and the C API does too.

## Error Propagation Recommendation (Claude's Discretion)

For transaction rollback errors, propagate the original exception and suppress rollback failures:

```python
except BaseException:
    try:
        self.rollback()
    except Exception:
        pass  # Swallow rollback error
    raise  # Re-raise original exception
```

**Rationale:** This matches Julia (`rethrow()` after swallowing rollback error) and Dart (`rethrow` with `catch (_) {}` for rollback). The original exception is always more informative than a rollback failure.

## Existing Code That Partially Covers Phase 3

Two methods are already implemented in `database.py`:
1. **`create_element`** (WRITE-01): Fully implemented, used throughout Phase 2 tests
2. **`update_scalar_relation`** (WRITE-07): Fully implemented, added in Phase 2 for relation test setup

These need test files (`test_database_create.py`) but no additional implementation work.

## Open Questions

1. **update_scalar_string with None via update_element**
   - What we know: C API `update_scalar_string` requires non-NULL value. `update_element` with `Element.set(attr, None)` routes through `quiver_element_set_null` which works.
   - What's unclear: Whether this has any side effects (e.g., does `update_element` with a single null attribute also clear other attributes?). The C++ `update_element` should only update attributes present in the Element.
   - Recommendation: Test this specific case. If `update_element` with a single null attribute preserves other attributes, use it. Otherwise, document that `update_scalar_string(None)` is not supported and raise `TypeError`.

## Sources

### Primary (HIGH confidence)
- `include/quiver/c/database.h` -- All C API function signatures for write and transaction operations
- `src/c/database_update.cpp` -- C API implementation confirming parameter validation behavior
- `src/c/database_transaction.cpp` -- C API implementation confirming `_Bool*` for `in_transaction`
- `src/c/database_create.cpp` -- C API implementation for update_element and delete_element
- `bindings/python/src/quiver/_declarations.py` -- Generator output confirming CFFI type mappings (`_Bool*`, `const char* const*`)
- `bindings/python/src/quiver/database.py` -- Current Python binding implementation (Phase 2 state)
- `bindings/python/src/quiver/_c_api.py` -- Current CFFI cdef declarations
- `bindings/python/src/quiver/element.py` -- Array marshaling patterns for `ffi.new`

### Secondary (HIGH confidence)
- `bindings/julia/src/database_update.jl` -- Julia write patterns (parameter order reference)
- `bindings/julia/src/database_transaction.jl` -- Julia transaction block pattern (rollback behavior)
- `bindings/julia/test/test_database_update.jl` -- Julia update test structure (test case reference)
- `bindings/julia/test/test_database_transaction.jl` -- Julia transaction test structure
- `bindings/julia/test/test_database_delete.jl` -- Julia delete test structure
- `bindings/dart/lib/src/database_update.dart` -- Dart update patterns (cross-validation)
- `bindings/dart/lib/src/database_transaction.dart` -- Dart transaction pattern (cross-validation)

### Tertiary (MEDIUM confidence)
- CFFI documentation on `_Bool` type handling (based on training knowledge, verified by `_declarations.py` usage)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- Using identical patterns from completed Phase 1/2
- Architecture: HIGH -- All C API signatures verified from headers, patterns established
- Pitfalls: HIGH -- Identified from actual C API source code examination (`QUIVER_REQUIRE` macros, `_Bool` type)

**Research date:** 2026-02-23
**Valid until:** Indefinite (C API is stable, patterns are established from prior phases)
