# Phase 4: Python LuaRunner Binding - Research

**Researched:** 2026-02-28
**Domain:** Python CFFI binding for Quiver LuaRunner C API
**Confidence:** HIGH

## Summary

This phase wraps the existing C API `quiver_lua_runner_new/run/get_error/free` functions into a Python `LuaRunner` class. The C API surface is small (4 functions, 1 opaque type) and the Python binding pattern is well-established from the existing `Database` class. All design decisions are locked by CONTEXT.md, so the implementation is purely mechanical: add CFFI declarations, create `lua_runner.py` following the `Database` lifecycle pattern, export from `__init__.py`, and write tests.

The C API's `quiver_lua_runner_run` stores its error in a runner-local `last_error` field (not the global `quiver_set_last_error`), which means error resolution must check `quiver_lua_runner_get_error` first, then fall back to `quiver_get_last_error` for `QUIVER_REQUIRE` null-argument failures. This two-source error pattern is the only non-trivial aspect of the implementation.

The existing Julia and Dart LuaRunner bindings provide direct reference implementations. Both follow the same pattern: create runner from database handle, store database reference to prevent GC, run scripts, check runner-specific error on failure.

**Primary recommendation:** Follow the Python `Database` lifecycle pattern exactly (`_closed`, `_ensure_open()`, `close()`, `__enter__`/`__exit__`, `__del__` with ResourceWarning), adding CFFI declarations for the 4 LuaRunner functions and the opaque struct type.

<user_constraints>

## User Constraints (from CONTEXT.md)

### Locked Decisions
- Constructor takes a `Database` instance: `LuaRunner(db)`
- Store database reference as private attribute `self._db = db` to prevent GC from collecting the database while runner is alive
- Follow the Python `Database` class lifecycle pattern exactly:
  - `close()` method for explicit cleanup (idempotent)
  - `__enter__`/`__exit__` for context manager support (`with LuaRunner(db) as lua:`)
  - `__del__` with `ResourceWarning` if not closed explicitly
  - `_ensure_open()` guard on `run()` calls
- Track closed state with `self._closed = False`
- Use `QuiverError` (matching the Python Database class), not a custom LuaError or RuntimeError
- Error resolution order: `quiver_lua_runner_get_error` first (Lua-specific), then `quiver_get_last_error` (global fallback for QUIVER_REQUIRE failures)
- Generic fallback message `"Lua script execution failed"` if both error sources return empty/null
- Post-close calls raise `QuiverError("LuaRunner is closed")` -- distinct from Lua execution errors
- Single method: `run(script: str) -> None`
- No `run_file(path)` convenience -- users read files themselves
- Strings only (no bytes)
- Returns None -- scripts operate via database side effects
- Multiple `run()` calls on the same runner explicitly supported (runner is long-lived)
- Own file: `lua_runner.py` in `src/quiverdb/` -- matches Dart (`lua_runner.dart`) and Julia (`lua_runner.jl`)
- Exported from `__init__.py`: `from quiverdb import LuaRunner`
- Added to `__all__` list
- CFFI declarations for `quiver_lua_runner_*` functions added to hand-written `_c_api.py`

### Claude's Discretion
- Exact CFFI struct/function signature formatting in `_c_api.py`
- Whether to also update `_declarations.py` (generator reference file)
- Test file organization and test case structure

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope

</user_constraints>

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PY-01 | Python LuaRunner binding wraps `quiver_lua_runner_new/run/get_error/free`; stores Database reference to prevent GC lifetime hazard | C API header defines exactly 4 functions + 1 opaque type; Database lifecycle pattern provides the template; Julia/Dart implementations provide cross-binding reference |

</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| cffi | (existing) | ABI-mode FFI for calling C API | Already used by the project; no compiler needed at install time |
| pytest | (existing) | Test framework | Already used by all Python binding tests |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| quiverdb._c_api | N/A | CFFI declarations and library loader | Adding LuaRunner function/type declarations |
| quiverdb._helpers | N/A | `check()`, `decode_string()` helpers | Error checking in LuaRunner (partially -- see error handling notes) |
| quiverdb.exceptions | N/A | `QuiverError` exception class | All LuaRunner errors |

### Alternatives Considered
None -- all technology choices are locked by the existing Python binding architecture.

**Installation:**
No new dependencies. All libraries already present in the project.

## Architecture Patterns

### Recommended File Structure
```
bindings/python/src/quiverdb/
  lua_runner.py            # NEW: LuaRunner class
  __init__.py              # MODIFY: add LuaRunner export
  _c_api.py                # MODIFY: add CFFI declarations
  database.py              # REFERENCE ONLY: lifecycle pattern template
  _helpers.py              # REFERENCE ONLY: check() and decode helpers
  exceptions.py            # REFERENCE ONLY: QuiverError
bindings/python/tests/
  test_lua_runner.py       # NEW: LuaRunner tests
```

### Pattern 1: CFFI Opaque Type Declaration
**What:** Declare the `quiver_lua_runner` opaque struct and its 4 functions in `_c_api.py`
**When to use:** When binding a new C API opaque handle type
**Example:**
```python
# Added to _c_api.py ffi.cdef() block
# Source: include/quiver/c/lua_runner.h

// lua_runner.h
typedef struct quiver_lua_runner quiver_lua_runner_t;

quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner);
quiver_error_t quiver_lua_runner_free(quiver_lua_runner_t* runner);
quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script);
quiver_error_t quiver_lua_runner_get_error(quiver_lua_runner_t* runner, const char** out_error);
```

### Pattern 2: Lifecycle Class (from Database)
**What:** Python class wrapping an opaque C handle with explicit resource management
**When to use:** Any class that holds a native resource requiring cleanup
**Example:**
```python
# Source: existing database.py pattern
class LuaRunner:
    def __init__(self, db: Database) -> None:
        self._db = db           # prevent GC of database
        self._closed = False
        # ... create native handle via C API ...

    def close(self) -> None:
        if self._closed:
            return
        # ... free native handle ...
        self._closed = True

    def _ensure_open(self) -> None:
        if self._closed:
            raise QuiverError("LuaRunner is closed")

    def __enter__(self) -> LuaRunner:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    def __del__(self) -> None:
        if not self._closed:
            warnings.warn(
                "LuaRunner was not closed explicitly",
                ResourceWarning,
                stacklevel=2,
            )
            self.close()
```

### Pattern 3: Two-Source Error Resolution
**What:** Check runner-local error first, then global error, then generic fallback
**When to use:** When the C API stores errors in a per-handle field rather than the global `quiver_set_last_error`
**Why needed:** `quiver_lua_runner_run` stores Lua execution errors in `runner->last_error` (not the global error). But `QUIVER_REQUIRE` null-argument failures go to the global error. Both paths must be checked.
**Example:**
```python
# Source: Dart lua_runner.dart error handling + Julia lua_runner.jl

def run(self, script: str) -> None:
    self._ensure_open()
    lib = get_lib()
    err = lib.quiver_lua_runner_run(self._ptr, script.encode("utf-8"))
    if err != 0:
        # 1. Try runner-specific error
        out_error = ffi.new("const char**")
        get_err = lib.quiver_lua_runner_get_error(self._ptr, out_error)
        if get_err == 0 and out_error[0] != ffi.NULL:
            msg = ffi.string(out_error[0]).decode("utf-8")
            if msg:
                raise QuiverError(msg)
        # 2. Fall back to global error (QUIVER_REQUIRE failures)
        ptr = lib.quiver_get_last_error()
        detail = ffi.string(ptr).decode("utf-8") if ptr != ffi.NULL else ""
        if detail:
            raise QuiverError(detail)
        # 3. Generic fallback
        raise QuiverError("Lua script execution failed")
```

**Critical note:** This error handling does NOT use `_helpers.check()` because `check()` only reads `quiver_get_last_error()`. The LuaRunner `run()` method needs the two-source resolution. However, the constructor (`quiver_lua_runner_new`) can use `check()` since its errors go through the standard global error path.

### Anti-Patterns to Avoid
- **Using `check()` for `run()` errors:** The `check()` helper only reads global errors. Lua script errors are stored in the runner-local `last_error` field and would be missed.
- **Not storing `self._db`:** Without the database reference, Python's GC could collect the Database object while the LuaRunner still holds a pointer to its native handle, causing a use-after-free crash.
- **Setting `self._ptr = ffi.NULL` in `close()`:** The `quiver_lua_runner_free` function returns `quiver_error_t`, but unlike `quiver_database_close`, there is no need to call `check()` on it since `delete runner` cannot fail. Still set `self._ptr` to `ffi.NULL` for safety after freeing.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Resource lifecycle | Custom ref-counting or weak-ref scheme | The `_closed` + `close()` + `__del__` + `__enter__`/`__exit__` pattern from Database | Proven pattern already in the codebase; consistency matters |
| Error string decoding | Manual pointer arithmetic | `ffi.string(ptr).decode("utf-8")` | CFFI handles null-termination correctly |
| Library loading | Custom `ctypes.CDLL` or separate loader | `from quiverdb._c_api import ffi, get_lib` | Existing loader handles bundled/dev modes and DLL dependencies |

**Key insight:** This phase requires zero new infrastructure. Every building block already exists in the Python binding codebase.

## Common Pitfalls

### Pitfall 1: Missing Database GC Reference
**What goes wrong:** If `LuaRunner.__init__` does not store `self._db = db`, Python's garbage collector may collect the Database object while the LuaRunner is still alive. The LuaRunner's native handle holds a raw pointer to the database's native handle, resulting in a use-after-free crash.
**Why it happens:** Python has no way to know that the C-level LuaRunner depends on the C-level Database.
**How to avoid:** Store `self._db = db` in `__init__`. This is a locked decision.
**Warning signs:** Sporadic segfaults when using LuaRunner after the Database variable goes out of scope.

### Pitfall 2: Using check() for run() Errors
**What goes wrong:** `check()` reads `quiver_get_last_error()` which returns the global error string. But `quiver_lua_runner_run` stores Lua script errors in `runner->last_error`, not the global error. Using `check()` would produce misleading error messages (possibly from a previous unrelated operation, or empty string).
**Why it happens:** The LuaRunner C API uses a different error storage mechanism than other C API functions.
**How to avoid:** Implement two-source error resolution in `run()`: check `quiver_lua_runner_get_error` first, then `quiver_get_last_error` as fallback.
**Warning signs:** Lua syntax errors produce "Unknown error" instead of the actual Lua error message.

### Pitfall 3: Encoding Script Strings
**What goes wrong:** Passing a Python `str` directly to CFFI where `const char*` is expected causes a TypeError in CFFI ABI mode.
**Why it happens:** CFFI ABI mode requires explicit `bytes` or `ffi.new("char[]", ...)` for C string parameters.
**How to avoid:** Always call `script.encode("utf-8")` before passing to `quiver_lua_runner_run`. This matches the pattern used throughout `database.py`.
**Warning signs:** `TypeError: initializer for ctype 'char *' must be a bytes or list or tuple object, not str`

### Pitfall 4: Forgetting to Add CFFI Declarations
**What goes wrong:** Calling `lib.quiver_lua_runner_new(...)` raises `AttributeError` because the function is not declared in `ffi.cdef()`.
**Why it happens:** CFFI ABI mode requires all function signatures to be declared before use.
**How to avoid:** Add all 4 function declarations and the opaque struct typedef to `_c_api.py`'s `ffi.cdef()` block before implementing `lua_runner.py`.
**Warning signs:** `AttributeError: ffi object has no attribute 'quiver_lua_runner_new'` on first test run.

### Pitfall 5: Double-Free on Close
**What goes wrong:** If `close()` is called, then `__del__` fires during GC, the native handle gets freed twice.
**Why it happens:** `__del__` runs automatically when the object is garbage collected.
**How to avoid:** The `_closed` flag prevents double-free. `close()` sets `self._closed = True` and `__del__` checks it. This is exactly how `Database.close()` works.
**Warning signs:** Crash on interpreter shutdown.

## Code Examples

Verified patterns from the existing codebase:

### Database Lifecycle Pattern (template for LuaRunner)
```python
# Source: bindings/python/src/quiverdb/database.py lines 16-83
class Database:
    def __init__(self, ptr) -> None:
        self._ptr = ptr
        self._closed = False

    def close(self) -> None:
        if self._closed:
            return
        lib = get_lib()
        check(lib.quiver_database_close(self._ptr))
        self._ptr = ffi.NULL
        self._closed = True

    def _ensure_open(self) -> None:
        if self._closed:
            raise QuiverError("Database has been closed")

    def __enter__(self) -> Database:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    def __del__(self) -> None:
        if not self._closed:
            warnings.warn(
                "Database was not closed explicitly",
                ResourceWarning,
                stacklevel=2,
            )
            self.close()
```

### C API Header (source of truth for declarations)
```c
// Source: include/quiver/c/lua_runner.h
typedef struct quiver_lua_runner quiver_lua_runner_t;

quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner);
quiver_error_t quiver_lua_runner_free(quiver_lua_runner_t* runner);
quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script);
quiver_error_t quiver_lua_runner_get_error(quiver_lua_runner_t* runner, const char** out_error);
```

### C Implementation Error Flow (understanding error storage)
```cpp
// Source: src/c/lua_runner.cpp lines 38-51
// quiver_lua_runner_run stores errors in runner->last_error (NOT global)
quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script) {
    QUIVER_REQUIRE(runner, script);  // <-- THIS goes to global error
    try {
        runner->last_error.clear();
        runner->runner.run(script);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        runner->last_error = e.what();  // <-- THIS goes to runner-local
        return QUIVER_ERROR;
    }
}
```

### Julia LuaRunner (cross-binding reference)
```julia
# Source: bindings/julia/src/lua_runner.jl
mutable struct LuaRunner
    ptr::Ptr{C.quiver_lua_runner}
    db::Database  # Keep reference to prevent GC
end

function LuaRunner(db::Database)
    out_runner = Ref{Ptr{C.quiver_lua_runner}}(C_NULL)
    check(C.quiver_lua_runner_new(db.ptr, out_runner))
    runner = LuaRunner(out_runner[], db)
    finalizer(r -> r.ptr != C_NULL && C.quiver_lua_runner_free(r.ptr), runner)
    return runner
end
```

### Dart LuaRunner Error Handling (cross-binding reference)
```dart
// Source: bindings/dart/lib/src/lua_runner.dart lines 55-80
void run(String script) {
    _ensureNotDisposed();
    final err = bindings.quiver_lua_runner_run(_ptr, script);
    if (err != quiver_error_t.QUIVER_OK) {
        // 1. Try runner-specific error
        final outErrorPtr = arena<Pointer<Char>>();
        final getErr = bindings.quiver_lua_runner_get_error(_ptr, outErrorPtr);
        if (getErr == quiver_error_t.QUIVER_OK && outErrorPtr.value != nullptr) {
            final errorMsg = outErrorPtr.value.cast<Utf8>().toDartString();
            if (errorMsg.isNotEmpty) {
                throw LuaException(errorMsg);
            }
        }
        // 2. Fallback: try global error (for QUIVER_REQUIRE failures)
        check(err);
    }
}
```

### Test Pattern (from conftest.py)
```python
# Source: bindings/python/tests/conftest.py
@pytest.fixture
def collections_db(collections_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    database = Database.from_schema(str(tmp_path / "collections.db"), str(collections_schema_path))
    yield database
    database.close()
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| N/A | N/A | N/A | First implementation of Python LuaRunner |

No prior Python LuaRunner implementation exists. Julia and Dart implementations are the reference.

**Deprecated/outdated:**
- None applicable.

## Open Questions

1. **Whether to update `_declarations.py`**
   - What we know: `_declarations.py` is a generator reference file. The hand-written `_c_api.py` is what the code actually uses.
   - What's unclear: Whether maintaining `_declarations.py` parity is important for future generator runs.
   - Recommendation: Skip updating `_declarations.py` -- it is explicitly noted as a reference file. The generator can be re-run later. This aligns with the "Python generator rework" being out of scope in REQUIREMENTS.md.

2. **`quiver_lua_runner_free` return value handling**
   - What we know: `quiver_lua_runner_free` returns `quiver_error_t` but the C implementation just calls `delete runner` which cannot fail. It always returns `QUIVER_OK`.
   - What's unclear: Whether to call `check()` on the return value for safety.
   - Recommendation: Do NOT call `check()` on `quiver_lua_runner_free` -- consistent with how `Database.close()` handles `quiver_database_close` (which does call check, but that function can legitimately fail). Since `delete` cannot fail, ignoring the return value is safe. However, for consistency with Database.close(), calling check() is also acceptable and harmless.

## Sources

### Primary (HIGH confidence)
- `include/quiver/c/lua_runner.h` -- C API header defining all 4 functions and opaque type
- `src/c/lua_runner.cpp` -- C implementation showing error storage in `runner->last_error`
- `src/c/internal.h` -- `QUIVER_REQUIRE` macro behavior (sets global error)
- `bindings/python/src/quiverdb/database.py` -- Python lifecycle pattern (lines 16-83)
- `bindings/python/src/quiverdb/_c_api.py` -- CFFI declaration pattern
- `bindings/python/src/quiverdb/_helpers.py` -- `check()` helper behavior
- `bindings/python/src/quiverdb/__init__.py` -- Export pattern
- `bindings/python/src/quiverdb/exceptions.py` -- `QuiverError` class
- `bindings/python/tests/conftest.py` -- Test fixture patterns
- `bindings/julia/src/lua_runner.jl` -- Julia LuaRunner reference implementation
- `bindings/dart/lib/src/lua_runner.dart` -- Dart LuaRunner reference implementation
- `bindings/dart/test/lua_runner_test.dart` -- Dart LuaRunner test cases (reference for Python tests)

### Secondary (MEDIUM confidence)
None needed -- all sources are primary project code.

### Tertiary (LOW confidence)
None.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new dependencies; all libraries already in the project
- Architecture: HIGH -- directly follows established Database lifecycle pattern; all C API signatures verified from headers
- Pitfalls: HIGH -- error storage mechanism verified from C source code; GC hazard verified from Julia/Dart patterns

**Research date:** 2026-02-28
**Valid until:** 2026-03-30 (stable -- C API and Python patterns are mature)
