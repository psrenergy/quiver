# Phase 1: Foundation - Research

**Researched:** 2026-02-23
**Domain:** Python binding infrastructure -- CFFI ABI-mode FFI, package setup, DLL loading, error handling, Database/Element lifecycle
**Confidence:** HIGH

## Summary

Phase 1 establishes the Python binding foundation: a `quiver` package using CFFI ABI mode to load the existing C API DLLs, with error handling, Database lifecycle (factory methods, context manager, close), Element builder, and a test runner. The domain is well understood because the project already has two mature reference bindings (Julia and Dart) that solve identical problems. The C API surface is stable and fully documented in headers.

CFFI 2.0.0 (latest stable) supports Python 3.13 and provides ABI-mode DLL loading with automatic Windows dependency chain resolution when using full paths with `ffi.dlopen()`. The `cdef()` system accepts C-like declarations for structs, enums, typedefs, and function signatures. All patterns needed for this phase (opaque pointers, out-parameters, struct marshaling, string handling) are well-supported and verified in CFFI documentation.

**Primary recommendation:** Follow the Julia binding structure closely for module layout and the Dart binding for lifecycle patterns (closed-state guarding, idempotent close). Use CFFI ABI inline mode with `ffi.cdef()` + `ffi.dlopen()`. Declare all C API functions needed for Phase 1 in a single `_c_api.py` module, loading both `libquiver.dll` and `libquiver_c.dll` with full paths.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Modular structure mirroring Julia: `src/quiver/` with separate files per domain (database.py, element.py, exceptions.py, c_api.py, etc.)
- Build system: hatchling as build backend, uv as package manager for dependency management and running tests
- Test files mirror Julia/Dart pattern: one test file per functional area (test_database_lifecycle.py, etc.)
- Full type hints on every public method parameter and return type (Python 3.13+, py.typed marker)
- `__repr__` on all public types (Database, Element, ScalarMetadata, GroupMetadata, etc.)
- Element builder: fluent `.set()` only -- `Element().set('label', 'x').set('value', 42)` -- consistent across all bindings
- Database properties (path, current_version, is_healthy): regular methods, not @property -- matches Julia/Dart pattern
- Single `QuiverError` exception inheriting from Exception -- no hierarchy
- Message only (no structured error_code or operation attribute) -- `str(e)` gives the C++ message
- Verbatim passthrough of C++ error messages -- bindings never craft their own
- `check()` helper reads C API thread-local error and raises, does not call `quiver_clear_last_error()` -- matches Julia/Dart behavior
- DLL loading: Python-conventional -- use `ctypes.util.find_library()` first, fall back to known paths
- Generator script for updating declarations when C API changes

### Claude's Discretion
- Top-level `__init__.py` re-export strategy (everything vs minimal)
- CFFI declaration file organization (single vs split)
- CFFI declaration format (inline strings vs .h file)
- Internal helper structure and private module organization

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope

</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| INFRA-01 | Python package renamed from "margaux" to "quiver" with CFFI dependency, uv as package manager, and correct project metadata | Existing `pyproject.toml` needs name change, add `cffi>=2.0.0` dependency, restructure `src/` to `src/quiver/`. Hatchling build backend already configured. |
| INFRA-02 | CFFI ABI-mode declarations covering the full C API surface (hand-written from Julia c_api.jl reference) | Julia `c_api.jl` has ~520 lines of declarations; Phase 1 needs only lifecycle subset (~30 functions). CFFI `ffi.cdef()` accepts C-like syntax for structs, enums, typedefs, opaque types, function pointers. |
| INFRA-03 | DLL/SO loader that resolves libquiver and libquiver_c at runtime with Windows pre-load for dependency chain | CFFI 1.17+ auto-applies `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` when path contains slashes. Load `libquiver.dll` first (via `ffi.dlopen()`), then `libquiver_c.dll`. Build output is `build/bin/` on Windows. |
| INFRA-04 | Error handling via check() function that reads thread-local C API errors and raises Python exceptions | Julia and Dart patterns verified: check `QUIVER_OK`, call `quiver_get_last_error()`, raise `QuiverError`. Do not call `quiver_clear_last_error()`. |
| INFRA-05 | String encoding helpers for UTF-8 conversion at the FFI boundary (including nullable strings) | CFFI handles UTF-8 via `ffi.new("char[]", s.encode("utf-8"))` and `ffi.string(ptr).decode("utf-8")`. Need helper for nullable strings (check `ffi.NULL`). |
| INFRA-06 | Test runner script (test.bat) matching Julia/Dart pattern with PATH setup | Julia test.bat calls `julia --project=...`, Dart test.bat calls `dart test`. Python equivalent: `uv run pytest`. PATH must include `build/bin/` for DLL resolution. |
| LIFE-01 | Database.from_schema() and Database.from_migrations() factory methods wrapping C API | Julia pattern: allocate out-pointer, call C function, check error, wrap handle. Python: `ffi.new("quiver_database_t**")`, call, check, store handle. |
| LIFE-02 | Database.close() with pointer nullification and context manager support (with statement) | Dart pattern: `_isClosed` flag, idempotent close, `_ensureNotClosed()` guard. Python: `__enter__`/`__exit__` for context manager. |
| LIFE-03 | Database.path, Database.current_version, Database.is_healthy properties | These are regular methods (not @property per user decision). `path()` returns string from `const char*` (non-owned pointer). `current_version()` returns int. `is_healthy()` returns bool. |
| LIFE-04 | Database.describe() for schema inspection | Trivial wrapper: call C function, check error. Prints to stdout (C++ side). |
| LIFE-05 | Element builder with set() fluent API for integer, float, string, null, and array types | Dart pattern: type-switch on value, delegate to typed setters. Python: use `isinstance()` checks. Must return `self` for fluent chaining. Arrays need `ffi.new("int64_t[]", values)` etc. |
| LIFE-06 | quiver.version() returning library version string | Module-level function calling `quiver_version()` -> `ffi.string().decode("utf-8")`. |

</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| cffi | 2.0.0 | C FFI binding via ABI mode (ffi.cdef + ffi.dlopen) | Project decision; no compiler at install time, direct C API access |
| hatchling | latest | Build backend for pyproject.toml | Already configured in existing pyproject.toml |
| uv | latest | Package manager, dependency resolver, test runner | User decision; modern Python tooling |
| pytest | >=8.4 | Test framework | Already in dev dependencies |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| ruff | >=0.12 | Linting and formatting | Already configured with isort checks |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| cffi ABI mode | ctypes | ctypes is stdlib but less ergonomic for struct declarations; cffi cdef() mirrors C headers directly |
| cffi ABI mode | cffi API mode | API mode requires C compiler at install time; ABI mode loads DLLs directly |
| hatchling | setuptools | hatchling already configured, no reason to switch |

**Installation:**
```bash
uv add cffi
uv add --dev pytest ruff
```

## Architecture Patterns

### Recommended Project Structure
```
bindings/python/
  pyproject.toml             # Package metadata (name="quiver", cffi dep)
  .python-version            # 3.13
  ruff.toml                  # Linting config
  src/quiver/                # Package root
    __init__.py              # Public API re-exports
    py.typed                 # PEP 561 marker for type hints
    _c_api.py                # CFFI declarations + DLL loading (private)
    _loader.py               # DLL path resolution (private)
    exceptions.py            # QuiverError class
    database.py              # Database class (lifecycle + describe)
    element.py               # Element builder class
  tests/
    conftest.py              # Fixtures (tests_path, schema_path, db fixture)
    test_database_lifecycle.py
    test_element.py
  test/
    test.bat                 # Test runner with PATH setup
  generator/
    generator.py             # CFFI declaration generator (reads C headers)
    generator.bat            # Runner script
```

### Pattern 1: CFFI ABI Mode Setup
**What:** Single FFI instance with centralized declarations and library loading
**When to use:** At module import time (lazy-loaded)
**Example:**
```python
# _c_api.py
from cffi import FFI

ffi = FFI()
ffi.cdef("""
    // Error codes
    typedef enum { QUIVER_OK = 0, QUIVER_ERROR = 1 } quiver_error_t;

    // Utility functions
    const char* quiver_version(void);
    const char* quiver_get_last_error(void);
    void quiver_clear_last_error(void);

    // Opaque handles
    typedef struct quiver_database quiver_database_t;
    typedef struct quiver_element quiver_element_t;

    // Options
    typedef enum { QUIVER_LOG_DEBUG=0, QUIVER_LOG_INFO=1, QUIVER_LOG_WARN=2, QUIVER_LOG_ERROR=3, QUIVER_LOG_OFF=4 } quiver_log_level_t;
    typedef struct { int read_only; quiver_log_level_t console_level; } quiver_database_options_t;
    quiver_database_options_t quiver_database_options_default(void);

    // Database lifecycle
    quiver_error_t quiver_database_from_schema(const char*, const char*, const quiver_database_options_t*, quiver_database_t**);
    quiver_error_t quiver_database_from_migrations(const char*, const char*, const quiver_database_options_t*, quiver_database_t**);
    quiver_error_t quiver_database_close(quiver_database_t*);
    quiver_error_t quiver_database_is_healthy(quiver_database_t*, int*);
    quiver_error_t quiver_database_path(quiver_database_t*, const char**);
    quiver_error_t quiver_database_current_version(quiver_database_t*, int64_t*);
    quiver_error_t quiver_database_describe(quiver_database_t*);

    // Element lifecycle
    quiver_error_t quiver_element_create(quiver_element_t**);
    quiver_error_t quiver_element_destroy(quiver_element_t*);
    quiver_error_t quiver_element_clear(quiver_element_t*);
    quiver_error_t quiver_element_set_integer(quiver_element_t*, const char*, int64_t);
    quiver_error_t quiver_element_set_float(quiver_element_t*, const char*, double);
    quiver_error_t quiver_element_set_string(quiver_element_t*, const char*, const char*);
    quiver_error_t quiver_element_set_null(quiver_element_t*, const char*);
    quiver_error_t quiver_element_set_array_integer(quiver_element_t*, const char*, const int64_t*, int32_t);
    quiver_error_t quiver_element_set_array_float(quiver_element_t*, const char*, const double*, int32_t);
    quiver_error_t quiver_element_set_array_string(quiver_element_t*, const char*, const char* const*, int32_t);
    quiver_error_t quiver_element_to_string(quiver_element_t*, char**);
    quiver_error_t quiver_element_free_string(char*);
""")

# Load library (lazy, cached)
_lib = None
def get_lib():
    global _lib
    if _lib is None:
        _lib = _load_library()
    return _lib
```
Source: [CFFI ABI mode documentation](https://cffi.readthedocs.io/en/stable/overview.html)

### Pattern 2: DLL Loading with Dependency Chain (Windows)
**What:** Load core DLL before C API DLL so Windows resolves the dependency
**When to use:** At library initialization
**Example:**
```python
# _loader.py
import os
import sys
from pathlib import Path

def _find_library_dir() -> Path | None:
    """Find the build output directory containing DLLs."""
    # Walk up from this file to find the project root (contains CMakeLists.txt)
    current = Path(__file__).resolve()
    for parent in current.parents:
        build_bin = parent / "build" / "bin"
        if build_bin.exists():
            return build_bin
    return None

def load_library(ffi):
    """Load libquiver_c, handling Windows dependency chain."""
    lib_dir = _find_library_dir()
    if lib_dir is None:
        # Fallback: try system PATH
        return ffi.dlopen("libquiver_c.dll" if sys.platform == "win32" else "libquiver_c.so")

    if sys.platform == "win32":
        # Pre-load core library so C API library can find it
        # CFFI 1.17+ auto-applies LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR when path has slashes
        core_path = str(lib_dir / "quiver.dll")
        ffi.dlopen(core_path)
        return ffi.dlopen(str(lib_dir / "quiver_c.dll"))
    elif sys.platform == "darwin":
        return ffi.dlopen(str(lib_dir / "libquiver_c.dylib"))
    else:
        return ffi.dlopen(str(lib_dir / "libquiver_c.so"))
```
Source: [CFFI Windows DLL loading behavior](https://cffi.readthedocs.io/en/latest/cdef.html)

### Pattern 3: Error Handling (check function)
**What:** Translate C API error codes to Python exceptions
**When to use:** After every C API call that returns `quiver_error_t`
**Example:**
```python
# exceptions.py
class QuiverError(Exception):
    """Error from the Quiver C API."""
    pass

# _c_api.py (or helpers module)
def check(err: int) -> None:
    """Check C API return code and raise QuiverError if non-zero."""
    if err != 0:  # QUIVER_ERROR
        detail = ffi.string(lib.quiver_get_last_error()).decode("utf-8")
        if not detail:
            raise QuiverError("Unknown error")
        raise QuiverError(detail)
```
Source: Julia `exceptions.jl` and Dart `exceptions.dart` patterns in this codebase

### Pattern 4: Database Lifecycle with Context Manager
**What:** Wrap opaque C handle with Python class, support `with` statement
**When to use:** Database class
**Example:**
```python
class Database:
    def __init__(self, ptr) -> None:
        self._ptr = ptr
        self._closed = False

    @staticmethod
    def from_schema(db_path: str, schema_path: str) -> "Database":
        options = ffi.new("quiver_database_options_t*", lib.quiver_database_options_default())
        out_db = ffi.new("quiver_database_t**")
        check(lib.quiver_database_from_schema(
            db_path.encode("utf-8"), schema_path.encode("utf-8"), options, out_db
        ))
        return Database(out_db[0])

    def close(self) -> None:
        if self._closed:
            return
        lib.quiver_database_close(self._ptr)
        self._ptr = ffi.NULL
        self._closed = True

    def _ensure_open(self) -> None:
        if self._closed:
            raise QuiverError("Database has been closed")

    def __enter__(self) -> "Database":
        return self

    def __exit__(self, *args) -> None:
        self.close()

    def __repr__(self) -> str:
        if self._closed:
            return "Database(closed)"
        return f"Database(path={self.path()!r})"
```
Source: Dart `database.dart` and Julia `database.jl` patterns in this codebase

### Pattern 5: Element Builder with Fluent API
**What:** Type-dispatching `.set()` method that returns self for chaining
**When to use:** Element class
**Example:**
```python
class Element:
    def __init__(self) -> None:
        out_el = ffi.new("quiver_element_t**")
        check(lib.quiver_element_create(out_el))
        self._ptr = out_el[0]
        self._destroyed = False

    def set(self, name: str, value) -> "Element":
        self._ensure_valid()
        if value is None:
            self._set_null(name)
        elif isinstance(value, bool):
            # Must check bool before int (bool is subclass of int)
            self._set_integer(name, int(value))
        elif isinstance(value, int):
            self._set_integer(name, value)
        elif isinstance(value, float):
            self._set_float(name, value)
        elif isinstance(value, str):
            self._set_string(name, value)
        elif isinstance(value, list):
            self._set_array(name, value)
        else:
            raise TypeError(f"Unsupported type {type(value).__name__} for '{name}'")
        return self  # fluent chaining

    def destroy(self) -> None:
        if self._destroyed:
            return
        check(lib.quiver_element_destroy(self._ptr))
        self._ptr = ffi.NULL
        self._destroyed = True
```
Source: Dart `element.dart` pattern in this codebase

### Anti-Patterns to Avoid
- **Declaring cdef with `...` placeholders in ABI mode:** The `...` (ellipsis) syntax in cdef works only in API mode where a C compiler fills in the gaps. In ABI mode, all struct fields, enum values, and sizes must be explicitly specified. Misdeclaring a struct layout will cause crashes, not compile errors.
- **Using `@property` for Database accessors:** User explicitly decided regular methods (`db.path()` not `db.path`), matching Julia/Dart patterns.
- **Crafting error messages in Python:** Bindings must passthrough C++ error messages verbatim. Never format or wrap error strings.
- **Using `ffi.gc()` for Database handle:** The Database class manages its own lifecycle via `close()` and context manager. Using `ffi.gc()` would make cleanup non-deterministic and conflict with explicit `close()`. Element similarly uses explicit `destroy()`.
- **Forgetting to encode strings:** All strings passed to C API must be `.encode("utf-8")`. CFFI does not auto-convert Python str to `const char*`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| C type declarations | Parse C headers manually | CFFI `ffi.cdef()` with C-like syntax | CFFI parser handles typedefs, structs, enums natively |
| DLL loading | Custom LoadLibrary/dlopen wrappers | CFFI `ffi.dlopen()` | Handles platform differences, Windows DLL search flags automatically |
| Memory allocation for out-params | Manual ctypes pointer allocation | CFFI `ffi.new()` | Auto-manages GC, correct alignment, type-safe |
| String conversion at FFI boundary | Custom char*-to-str conversion | `ffi.string()` + `.decode("utf-8")` and `s.encode("utf-8")` | Standard CFFI idiom, handles null termination |
| CFFI declaration generation | Hand-maintain declarations forever | Generator script that reads C headers | User explicitly requested this; C API has ~200 declarations that change over time |

**Key insight:** CFFI ABI mode is purpose-built for this exact use case -- loading a pre-built DLL and calling its functions. The entire Phase 1 is translating patterns already proven in Julia and Dart into CFFI idioms.

## Common Pitfalls

### Pitfall 1: ABI Mode Struct Layout Mismatch
**What goes wrong:** CFFI ABI mode trusts your `cdef()` declarations blindly. If a struct field order, size, or padding differs from the actual C struct, you get silent memory corruption or crashes.
**Why it happens:** C struct layout depends on field order, types, and platform padding rules. Python-side declarations must match exactly.
**How to avoid:** Copy struct declarations directly from the C headers. For this project, the C API structs (`quiver_database_options_t`, `quiver_scalar_metadata_t`, `quiver_group_metadata_t`) are all defined in `include/quiver/c/options.h` and `include/quiver/c/database.h`. The generator script should produce these mechanically.
**Warning signs:** Segfaults when accessing struct fields, garbage values in out-parameters, crashes on function calls with struct arguments.

### Pitfall 2: Windows DLL Dependency Chain
**What goes wrong:** `libquiver_c.dll` depends on `libquiver.dll`. If `libquiver.dll` isn't loaded first or isn't discoverable, loading `libquiver_c.dll` fails with an unhelpful "DLL not found" error.
**Why it happens:** Windows DLL search order changed in Python 3.8+. The current directory and PATH are no longer searched for load-time dependencies of DLLs.
**How to avoid:** Two strategies work: (1) Load `libquiver.dll` explicitly before `libquiver_c.dll`, or (2) use `ffi.dlopen()` with a full path containing slashes, which triggers `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` (CFFI 1.17+), telling Windows to search the DLL's directory for dependencies.
**Warning signs:** `OSError: cannot load library` on Windows only, works fine on Linux/macOS.

### Pitfall 3: Python bool is a subclass of int
**What goes wrong:** `isinstance(True, int)` returns `True` in Python. If the Element `set()` method checks `isinstance(value, int)` before checking for `bool`, boolean values silently become integers.
**Why it happens:** Python's type hierarchy has `bool` inheriting from `int`.
**How to avoid:** Check `isinstance(value, bool)` before `isinstance(value, int)` in the type dispatch chain, or use explicit type matching order.
**Warning signs:** `element.set("flag", True)` stores `1` instead of raising a type error (if booleans should be disallowed) or stores correctly if booleans are acceptable as integers.

### Pitfall 4: String Lifetime with ffi.new
**What goes wrong:** `ffi.new("char[]", s.encode("utf-8"))` creates a temporary that can be garbage collected before the C function reads it.
**Why it happens:** CFFI's GC collects `cdata` objects when no Python reference exists. If you pass a temporary directly to a C function, the GC may collect it during the call.
**How to avoid:** Always keep a Python reference alive for the duration of the C call. The simplest pattern: assign to a local variable before passing to the C function.
**Warning signs:** Intermittent crashes or garbled strings, especially under memory pressure.

### Pitfall 5: Forgetting try/finally for Element destroy
**What goes wrong:** If an exception occurs between `Element()` creation and `destroy()`, the C-side memory leaks.
**Why it happens:** Python doesn't have Dart's `finally` pattern built into the calling convention.
**How to avoid:** Document that Element should be used with try/finally, or implement `__enter__`/`__exit__` for context manager usage. Note: the Dart binding has this exact pattern (try/finally around every element usage).
**Warning signs:** Memory leaks visible under valgrind or address sanitizer, growing memory usage in long-running tests.

### Pitfall 6: Package Layout with hatchling
**What goes wrong:** After renaming from `margaux` to `quiver` and restructuring `src/` to `src/quiver/`, the hatchling build backend may not find the package.
**Why it happens:** The current `pyproject.toml` has `packages = ["src"]` which maps the `src` directory as the package. With the rename, it needs to point to `src/quiver` or use the default discovery.
**How to avoid:** Update `[tool.hatch.build.targets.wheel]` to `packages = ["src/quiver"]` so that `import quiver` works correctly. Verify with `uv pip install -e .` and `python -c "import quiver"`.
**Warning signs:** `ModuleNotFoundError: No module named 'quiver'` after install.

## Code Examples

Verified patterns from the existing codebase (Julia and Dart reference implementations):

### Database Factory Method (from Julia database.jl)
```python
# Python translation of Julia's from_schema pattern
@staticmethod
def from_schema(db_path: str, schema_path: str) -> "Database":
    lib = get_lib()
    options = ffi.new("quiver_database_options_t*")
    options[0] = lib.quiver_database_options_default()
    out_db = ffi.new("quiver_database_t**")
    check(lib.quiver_database_from_schema(
        db_path.encode("utf-8"),
        schema_path.encode("utf-8"),
        options,
        out_db,
    ))
    return Database(out_db[0])
```

### Error Check (from Julia exceptions.jl, Dart exceptions.dart)
```python
# Direct translation of Julia's check() and Dart's check()
def check(err: int) -> None:
    if err != 0:
        msg_ptr = lib.quiver_get_last_error()
        detail = ffi.string(msg_ptr).decode("utf-8") if msg_ptr != ffi.NULL else ""
        if not detail:
            raise QuiverError("Unknown error")
        raise QuiverError(detail)
```

### Element Array Setter (from Dart element.dart setArrayString)
```python
def _set_array_string(self, name: str, values: list[str]) -> None:
    lib = get_lib()
    c_name = name.encode("utf-8")
    # Encode all strings and keep references alive
    encoded = [v.encode("utf-8") for v in values]
    c_strings = [ffi.new("char[]", e) for e in encoded]
    c_array = ffi.new("const char*[]", c_strings)
    check(lib.quiver_element_set_array_string(
        self._ptr, c_name, c_array, len(values)
    ))
```

### Context Manager Usage (target API)
```python
# Success criterion 2: "with Database.from_schema(...) as db:" closes automatically
with Database.from_schema(":memory:", schema_path) as db:
    version = db.current_version()
    db.describe()
# db.close() called automatically via __exit__
```

### Version Function (target API)
```python
# Success criterion 1: "import quiver; quiver.version()" returns version string
def version() -> str:
    lib = get_lib()
    return ffi.string(lib.quiver_version()).decode("utf-8")
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| ctypes for Python FFI | cffi ABI mode | Project decision (user chose CFFI) | Cleaner C-like declarations, better struct handling |
| PATH-based DLL search on Windows | `os.add_dll_directory()` + explicit pre-loading | Python 3.8 | Must explicitly manage DLL directories on Windows |
| `cffi 1.x` with `ffi.verify()` | `cffi 2.0.0` with ABI-only inline mode | 2025-09 (cffi 2.0.0 release) | Verify removed; ABI inline mode is the primary pattern for DLL loading without compilation |
| `pip` package management | `uv` package management | ~2024 | Faster, integrated resolver, `uv run pytest` replaces `pip install` + `python -m pytest` |

**Deprecated/outdated:**
- `ffi.verify()`: Removed in CFFI 2.0. Use `ffi.cdef()` + `ffi.dlopen()` for ABI mode.
- Manual `LOAD_LIBRARY_SEARCH_*` flags: CFFI 1.17+ handles this automatically when path contains slashes.
- `ctypes.util.find_library()` on Windows: Has known issues with `os.add_dll_directory()` paths (CPython issue #132328). Using explicit full paths with `ffi.dlopen()` is more reliable.

## Open Questions

1. **DLL naming on Windows: `libquiver_c.dll` vs `quiver_c.dll`**
   - What we know: Julia's `c_api.jl` references `libquiver_c.dll`, but the Dart binding references `quiver_c.dll`. CMake on Windows may or may not add the `lib` prefix depending on configuration.
   - What's unclear: The exact filename produced by the CMake build.
   - Recommendation: Check the actual build output in `build/bin/`. The loader should try both variants (`libquiver_c.dll` and `quiver_c.dll`). Dart's `library_loader.dart` uses `quiver_c.dll` (no lib prefix), while Julia uses `libquiver_c.dll` (with prefix). The loader should handle both.

2. **Generator script scope for Phase 1**
   - What we know: User explicitly wants a generator script for CFFI declarations, overriding the "hand-write" suggestion from out-of-scope notes.
   - What's unclear: Whether the generator should be built in Phase 1 or deferred. Phase 1 only needs ~30 C API functions (lifecycle subset), but the user's CONTEXT.md lists it as an implementation decision.
   - Recommendation: Build a basic generator in Phase 1 that reads the C headers and produces the `ffi.cdef()` string. Even if Phase 1 only uses a subset, having the generator early prevents drift. Julia's generator uses Clang.jl; Python could use regex parsing of the C headers (the C API headers are very regular and don't use complex macros).

3. **`__del__` vs explicit `close()` for Database**
   - What we know: Julia uses finalizer (GC-triggered). Dart uses explicit `close()` with `_isClosed` flag.
   - What's unclear: Whether Python's `__del__` should be a safety net.
   - Recommendation: Implement explicit `close()` and `__enter__`/`__exit__` as primary. Add `__del__` as a safety net that warns (via `warnings.warn`) if the database wasn't closed explicitly, then closes it. This matches Python conventions (e.g., file objects warn about unclosed handles).

## Sources

### Primary (HIGH confidence)
- CFFI 2.0.0 stable documentation: [Overview](https://cffi.readthedocs.io/en/stable/overview.html), [cdef reference](https://cffi.readthedocs.io/en/stable/cdef.html), [API reference](https://cffi.readthedocs.io/en/latest/ref.html)
- [CFFI 2.0.0 changelog](https://cffi.readthedocs.io/en/stable/whatsnew.html) - version 2.0.0, Python 3.13 support confirmed
- [CFFI PyPI page](https://pypi.org/project/cffi/) - latest stable is 2.0.0 (2025-09)
- Codebase: Julia `bindings/julia/src/c_api.jl` (520 lines of C API declarations - complete reference)
- Codebase: Julia `bindings/julia/src/database.jl`, `element.jl`, `exceptions.jl` (lifecycle patterns)
- Codebase: Dart `bindings/dart/lib/src/database.dart`, `element.dart`, `exceptions.dart` (lifecycle patterns)
- Codebase: C API headers `include/quiver/c/common.h`, `database.h`, `element.h`, `options.h` (canonical signatures)

### Secondary (MEDIUM confidence)
- [CFFI Windows DLL loading with LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR](https://cffi.readthedocs.io/en/latest/cdef.html) - CFFI 1.17+ behavior for paths with slashes
- [CPython issue #132328](https://github.com/python/cpython/issues/132328) - `find_library()` limitation with `os.add_dll_directory()`
- [Python Developer Tooling Handbook](https://pydevtools.com/handbook/tutorial/setting-up-testing-with-pytest-and-uv/) - uv + pytest setup patterns
- [uv dependency groups documentation](https://docs.astral.sh/uv/concepts/projects/dependencies/) - `[dependency-groups]` in pyproject.toml

### Tertiary (LOW confidence)
- None. All findings verified with primary or secondary sources.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - CFFI 2.0.0 verified on PyPI, ABI mode documented extensively, Python 3.13 support confirmed
- Architecture: HIGH - Two reference implementations (Julia, Dart) provide exact patterns to follow; C API surface is stable and documented in headers
- Pitfalls: HIGH - Windows DLL loading behavior verified in CFFI docs; struct layout risk is well-known CFFI ABI-mode limitation; bool/int subclass is Python fundamentals

**Research date:** 2026-02-23
**Valid until:** 2026-03-23 (stable domain, CFFI and C API unlikely to change)
