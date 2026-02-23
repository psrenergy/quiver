# Architecture Patterns: Python CFFI Bindings

**Domain:** Python FFI bindings for a C++ library exposed through a C API
**Researched:** 2026-02-23
**Confidence:** HIGH — existing Julia and Dart bindings provide direct structural precedent

---

## Existing Architecture Context

The project uses a proven layered architecture:

```
C++ Core (database.h / Database class)
    ↓ wraps via Pimpl
C API (include/quiver/c/*.h + src/c/*.cpp)
    ↓ consumed by
Language Bindings (julia/, dart/, python/)
    ↓ each binding:
      - loads libquiver_c.{dll,so,dylib} at runtime
      - declares/generates FFI type definitions
      - wraps each C function in an idiomatic language function
      - raises language-native exceptions from quiver_get_last_error()
```

Python joins this chain identically. No C++ changes, no C API changes. The binding is a pure consumer.

---

## Recommended Architecture

### CFFI Mode: ABI Mode

Use CFFI **ABI mode** (`cffi.dlopen()`), not API mode (`cffi.cdef()` + `ffibuilder`).

**Why ABI mode:**

| Criterion | ABI Mode | API Mode |
|-----------|----------|----------|
| Build step at install | None — just `pip install cffi` | Requires C compiler at install time (`ffibuilder.compile()`) |
| Distribution complexity | Simple — load DLL at import | Requires `setup.py` / `build` hook generating `.so` |
| Consistency with other bindings | Julia uses `Libdl.dlopen()` runtime load; same concept | Would diverge from the pattern |
| C declaration source | Hand-written `.h` fragment (or preprocessed from headers) | Same, but compiled into a shared extension |
| Stability requirement | No compiled extension → no ABI mismatch | Compiled extension must match Python version |
| Project constraint | "build from source for now" — no PyPI packaging | Would add unnecessary toolchain requirement |

ABI mode requires one `ffi.cdef(...)` call with the C declarations (stripped of macros), then `lib = ffi.dlopen(path_to_dll)`. All function calls go through `lib.quiver_*`. This is the correct fit for this project.

### DLL Discovery Strategy

Mirror Julia's approach exactly: resolve relative to the binding's own installed location.

```python
# bindings/python/src/quiver/_lib.py

import os
import sys
import cffi

ffi = cffi.FFI()

def _find_library() -> str:
    # Walk up from this file to the repo root's build/ directory.
    # File location: bindings/python/src/quiver/_lib.py
    # Build location: build/bin/libquiver_c.dll  (Windows)
    #                 build/lib/libquiver_c.so    (Linux)
    #                 build/lib/libquiver_c.dylib (macOS)
    here = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.join(here, "..", "..", "..", "..", "..")
    repo_root = os.path.normpath(repo_root)

    if sys.platform == "win32":
        name = "libquiver_c.dll"
        subdir = "bin"
    elif sys.platform == "darwin":
        name = "libquiver_c.dylib"
        subdir = "lib"
    else:
        name = "libquiver_c.so"
        subdir = "lib"

    path = os.path.join(repo_root, "build", subdir, name)
    if not os.path.exists(path):
        raise OSError(
            f"quiver: Cannot find {name} at {path}. "
            "Run cmake --build build --config Debug first."
        )
    return path

_lib = ffi.dlopen(_find_library())
```

On Windows, `libquiver_c.dll` depends on `libquiver.dll`. Both must be in PATH or the same directory. Julia's test.bat handles this — Python's test.bat must do the same (add `build/bin` to PATH before running pytest).

### Header Declarations: Manual `ffi.cdef()` Fragment

Do not auto-generate with an external tool. Use a hand-maintained `_declarations.py` module containing the `ffi.cdef(...)` string.

**Why manual over auto-generation:**

- Julia uses Clang.jl generator → produces `c_api.jl` (auto). Dart uses `ffigen` → produces `bindings.dart` (auto).
- Python has no standard equivalent tool that integrates cleanly without a build step (pcpp/cffi-pycparser is fragile with `#ifdef`, platform macros, and `__declspec`).
- The C API surface is stable and bounded. The entire API fits in ~200 lines of stripped declarations.
- Manual declarations are readable, version-controlled, and do not require Clang/libclang at install time.
- Changes to the C API require updating declarations in exactly one file anyway (same for auto-generation).

The declaration file strips all macros (`QUIVER_C_API`, `__declspec`, `__attribute__`, `#ifdef`, `extern "C"`) — CFFI requires clean C declarations without preprocessor directives.

```python
# bindings/python/src/quiver/_declarations.py

CDEF = """
    typedef enum { QUIVER_OK = 0, QUIVER_ERROR = 1 } quiver_error_t;
    typedef enum { QUIVER_LOG_DEBUG = 0, QUIVER_LOG_INFO = 1,
                   QUIVER_LOG_WARN = 2, QUIVER_LOG_ERROR = 3,
                   QUIVER_LOG_OFF = 4 } quiver_log_level_t;
    typedef enum { QUIVER_DATA_STRUCTURE_SCALAR = 0,
                   QUIVER_DATA_STRUCTURE_VECTOR = 1,
                   QUIVER_DATA_STRUCTURE_SET = 2 } quiver_data_structure_t;
    typedef enum { QUIVER_DATA_TYPE_INTEGER = 0, QUIVER_DATA_TYPE_FLOAT = 1,
                   QUIVER_DATA_TYPE_STRING = 2, QUIVER_DATA_TYPE_DATE_TIME = 3,
                   QUIVER_DATA_TYPE_NULL = 4 } quiver_data_type_t;

    typedef struct quiver_database quiver_database_t;
    typedef struct quiver_element quiver_element_t;

    typedef struct {
        int read_only;
        quiver_log_level_t console_level;
    } quiver_database_options_t;

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

    typedef struct {
        const char* date_time_format;
        const char* const* enum_attribute_names;
        const size_t* enum_entry_counts;
        const int64_t* enum_values;
        const char* const* enum_labels;
        size_t enum_attribute_count;
    } quiver_csv_export_options_t;

    // ... all function declarations from include/quiver/c/*.h
    // stripped of QUIVER_C_API, extern "C", #ifdef guards
"""
```

### Component Boundaries

| Component | File(s) | Responsibility |
|-----------|---------|---------------|
| FFI loader | `src/quiver/_lib.py` | Locate DLL, call `ffi.dlopen()`, expose `ffi` + `lib` |
| C declarations | `src/quiver/_declarations.py` | All `ffi.cdef(...)` content — single source of truth |
| Error handling | `src/quiver/_error.py` | `QuiverError(Exception)`, `check()` helper |
| Database class | `src/quiver/database.py` | `Database` class, factory methods, `__del__` finalizer |
| Element class | `src/quiver/element.py` | `Element` class, context manager, `__setitem__` |
| CRUD wrappers | `src/quiver/database_create.py` | `create_element`, `delete_element`, `update_element` |
| Read wrappers | `src/quiver/database_read.py` | All `read_scalar_*`, `read_vector_*`, `read_set_*` |
| Update wrappers | `src/quiver/database_update.py` | All `update_scalar_*`, `update_vector_*`, `update_set_*` |
| Metadata wrappers | `src/quiver/database_metadata.py` | `get_*_metadata`, `list_*_groups`, Python dataclasses |
| Query wrappers | `src/quiver/database_query.py` | `query_string`, `query_integer`, `query_float` (+ params) |
| Transaction wrappers | `src/quiver/database_transaction.py` | `begin_transaction`, `commit`, `rollback`, `transaction()` ctx mgr |
| Time series wrappers | `src/quiver/database_time_series.py` | `read_time_series_group`, `update_time_series_group`, files ops |
| CSV wrappers | `src/quiver/database_csv.py` | `export_csv`, CSV options construction |
| Public namespace | `src/quiver/__init__.py` | Re-exports `Database`, `Element`, `QuiverError`, constants |

### Data Flow: C → Python

```
lib.quiver_database_read_scalar_integers(db_ptr, col, attr, out_vals, out_count)
    ↓  returns QUIVER_OK / QUIVER_ERROR
check(ret)  →  raises QuiverError(quiver_get_last_error()) on error
    ↓
out_vals[0]  →  ffi pointer to int64_t[]
ffi.unpack(out_vals[0], out_count[0])  →  Python list[int]
lib.quiver_database_free_integer_array(out_vals[0])
    ↓
return list[int]  to caller
```

---

## Patterns to Follow

### Pattern 1: Error Check Helper

Every C API call goes through `check()`. Never check return codes inline.

```python
# src/quiver/_error.py

from ._lib import ffi, lib

class QuiverError(Exception):
    pass

def check(err) -> None:
    if err != lib.QUIVER_OK:
        msg = ffi.string(lib.quiver_get_last_error()).decode("utf-8")
        if not msg:
            raise QuiverError("Unknown error (quiver_get_last_error returned empty)")
        raise QuiverError(msg)
```

### Pattern 2: Database Finalizer via `__del__`

CFFI ABI mode has no automatic GC integration. Use `__del__` as the finalizer, same as Julia's `finalizer()`.

```python
# src/quiver/database.py

class Database:
    def __init__(self, ptr):
        self._ptr = ptr  # ffi pointer to quiver_database_t

    def __del__(self):
        if self._ptr is not None:
            lib.quiver_database_close(self._ptr)
            self._ptr = None
```

`__del__` is sufficient here. The database is not a public context manager — `close()` is an explicit method, and `__del__` ensures cleanup on GC. This matches Julia's finalizer pattern precisely.

### Pattern 3: String Marshaling

C-owned strings come back as `char*`. Python must copy before freeing.

```python
# Pattern for char* out-params (scalar string by id)
out_value = ffi.new("char**")
out_has = ffi.new("int*")
check(lib.quiver_database_read_scalar_string_by_id(
    db._ptr, collection.encode(), attribute.encode(), id, out_value, out_has
))
if out_has[0] == 0 or out_value[0] == ffi.NULL:
    return None
result = ffi.string(out_value[0]).decode("utf-8")
lib.quiver_element_free_string(out_value[0])
return result
```

All `str → char*` conversions: `.encode("utf-8")`. All `char* → str` conversions: `ffi.string(ptr).decode("utf-8")`.

### Pattern 4: Integer/Float Array Marshaling

```python
# Pattern for int64_t** out arrays (read_scalar_integers)
out_values = ffi.new("int64_t**")
out_count = ffi.new("size_t*")
check(lib.quiver_database_read_scalar_integers(
    db._ptr, collection.encode(), attribute.encode(), out_values, out_count
))
count = out_count[0]
if count == 0 or out_values[0] == ffi.NULL:
    return []
result = list(ffi.unpack(out_values[0], count))
lib.quiver_database_free_integer_array(out_values[0])
return result
```

`ffi.unpack(ptr, count)` returns a Python list directly for numeric types. For `double**` use `ffi.unpack(out_values[0], count)` — same pattern.

### Pattern 5: String Array Marshaling

```python
# Pattern for char*** out arrays (read_scalar_strings)
out_values = ffi.new("char***")
out_count = ffi.new("size_t*")
check(lib.quiver_database_read_scalar_strings(
    db._ptr, collection.encode(), attribute.encode(), out_values, out_count
))
count = out_count[0]
if count == 0 or out_values[0] == ffi.NULL:
    return []
ptrs = ffi.unpack(out_values[0], count)
result = [ffi.string(p).decode("utf-8") for p in ptrs]
lib.quiver_database_free_string_array(out_values[0], count)
return result
```

### Pattern 6: Element as Context Manager

`Element` wraps a `quiver_element_t*`. Expose as context manager to guarantee `quiver_element_destroy()` is called.

```python
# src/quiver/element.py

class Element:
    def __init__(self):
        out = ffi.new("quiver_element_t**")
        check(lib.quiver_element_create(out))
        self._ptr = out[0]

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self._destroy()

    def _destroy(self):
        if self._ptr is not None:
            lib.quiver_element_destroy(self._ptr)
            self._ptr = None

    def __setitem__(self, name: str, value):
        # dispatch on type: int → set_integer, float → set_float, etc.
        ...
```

Julia uses `try/finally` to destroy the element; Python uses the context manager for the same guarantee.

### Pattern 7: Transaction Context Manager

```python
# src/quiver/database_transaction.py

from contextlib import contextmanager

@contextmanager
def transaction(db):
    db.begin_transaction()
    try:
        yield db
        db.commit()
    except Exception:
        try:
            db.rollback()
        except Exception:
            pass
        raise
```

Matches Julia's `transaction(fn, db)` block and Dart's `db.transaction((db) {...})` semantically.

### Pattern 8: Metadata as Dataclasses

Return structured metadata as Python dataclasses, not raw CFFI structs. Convert immediately after the C call.

```python
from dataclasses import dataclass
from typing import Optional

@dataclass
class ScalarMetadata:
    name: str
    data_type: int
    not_null: bool
    primary_key: bool
    default_value: Optional[str]
    is_foreign_key: bool
    references_collection: Optional[str]
    references_column: Optional[str]

@dataclass
class GroupMetadata:
    group_name: str
    dimension_column: Optional[str]
    value_columns: list[ScalarMetadata]
```

### Pattern 9: Parameterized Queries

The `query_*_params` functions use parallel `int[]` type arrays and `void*[]` value arrays. CFFI requires explicit pointer casting:

```python
def query_string(db, sql: str, params: list = None) -> Optional[str]:
    if not params:
        # use quiver_database_query_string
        ...
    else:
        # build param_types and param_values arrays for quiver_database_query_string_params
        n = len(params)
        type_arr = ffi.new("int[]", n)
        val_ptrs = ffi.new("void*[]", n)
        # keep Python objects alive for the duration of the call
        keepalive = []
        for i, p in enumerate(params):
            if isinstance(p, int):
                type_arr[i] = lib.QUIVER_DATA_TYPE_INTEGER
                buf = ffi.new("int64_t*", p)
                val_ptrs[i] = buf
                keepalive.append(buf)
            elif isinstance(p, float):
                type_arr[i] = lib.QUIVER_DATA_TYPE_FLOAT
                buf = ffi.new("double*", p)
                val_ptrs[i] = buf
                keepalive.append(buf)
            elif isinstance(p, str):
                type_arr[i] = lib.QUIVER_DATA_TYPE_STRING
                buf = ffi.new("char[]", p.encode("utf-8"))
                val_ptrs[i] = buf
                keepalive.append(buf)
            elif p is None:
                type_arr[i] = lib.QUIVER_DATA_TYPE_NULL
                val_ptrs[i] = ffi.NULL
        # call quiver_database_query_string_params ...
```

The `keepalive` list is critical — CFFI does not keep intermediate `ffi.new()` results alive past the Python expression boundary without an explicit reference.

---

## Anti-Patterns to Avoid

### Anti-Pattern 1: CFFI API Mode (Build at Install)

**What:** Using `ffibuilder.set_source()` and compiling a C extension at `pip install` time.

**Why bad:** Requires a C compiler on the user's machine, produces a `.so`/`.pyd` that must match the Python version, adds a build step inconsistent with the "build from source" model where the user already ran CMake. The project explicitly excludes PyPI packaging.

**Instead:** ABI mode with `ffi.dlopen()` — same approach Julia takes with `Libdl.dlopen()`.

### Anti-Pattern 2: Forgetting `keepalive` for `void*[]` params

**What:** Creating CFFI buffers for query parameters inline without retaining references.

**Why bad:** CFFI's garbage collector may reclaim `ffi.new()` allocations before the C call executes when they're not bound to a live Python variable.

**Instead:** Collect all parameter buffers in a `keepalive` list for the duration of the C call.

### Anti-Pattern 3: Decoding Strings Eagerly in the Low-Level Layer

**What:** Calling `.decode("utf-8")` inside `_lib.py` or at the CFFI call site.

**Why bad:** Mixes marshaling responsibility with transport. Makes the pattern inconsistent when callers need raw bytes.

**Instead:** All decoding happens in the wrapper functions (`database_read.py`, etc.), never in `_lib.py`. `_lib.py` exposes raw CFFI objects only.

### Anti-Pattern 4: Crafting Error Messages in Python

**What:** Writing Python code like `raise QuiverError(f"Collection {name} not found")`.

**Why bad:** Violates the project constraint: "Error messages are defined in the C++/C API layer. Bindings retrieve and surface them — they never craft their own."

**Instead:** Always use `quiver_get_last_error()` as the message source. `check()` handles this uniformly.

### Anti-Pattern 5: Putting Logic in the Python Layer

**What:** Any non-trivial data transformation, validation, or computation in Python wrappers beyond type conversion and memory management.

**Why bad:** Violates "intelligence resides in C++ layer" principle. Creates divergence between bindings.

**Instead:** If a feature requires logic, implement it in C++ and expose through C API. Python just marshals.

---

## Project Layout

```
bindings/python/
  pyproject.toml                  # MODIFY: name="quiver", add cffi dependency
  .python-version                 # EXISTS: 3.13
  Makefile                        # MODIFY: add test.bat equivalent, PATH setup
  ruff.toml                       # EXISTS: keep as-is
  test/
    test.bat                      # NEW: sets PATH to build/bin, runs pytest
  src/
    quiver/                       # NEW: rename from src/ stub, restructure
      __init__.py                 # NEW: public API surface
      py.typed                    # MOVE from src/
      _lib.py                     # NEW: ffi instance, dlopen, DLL path resolution
      _declarations.py            # NEW: ffi.cdef() content (all C headers stripped)
      _error.py                   # NEW: QuiverError, check()
      element.py                  # NEW: Element class (context manager)
      database.py                 # NEW: Database class, from_schema, from_migrations
      database_create.py          # NEW: create_element, delete_element, update_element
      database_read.py            # NEW: read_scalar_*, read_vector_*, read_set_*, by_id variants
      database_update.py          # NEW: update_scalar_*, update_vector_*, update_set_*
      database_metadata.py        # NEW: ScalarMetadata, GroupMetadata dataclasses + getters
      database_query.py           # NEW: query_string/integer/float + _params variants
      database_transaction.py     # NEW: begin_transaction, commit, rollback, transaction()
      database_time_series.py     # NEW: read/update_time_series_group, files ops
      database_csv.py             # NEW: export_csv, CSVExportOptions
  tests/
    conftest.py                   # MODIFY: add shared fixture (path to test schemas)
    unit/
      __init__.py                 # EXISTS: keep
      test_database.py            # REPLACE: placeholder → actual lifecycle tests
      test_database_create.py     # NEW
      test_database_read.py       # NEW
      test_database_update.py     # NEW
      test_database_delete.py     # NEW
      test_database_query.py      # NEW
      test_database_metadata.py   # NEW
      test_database_transaction.py # NEW
      test_database_time_series.py # NEW
      test_database_csv.py        # NEW
```

---

## Integration Points: New vs Modified

### New Components (Python only)

| Component | Description |
|-----------|-------------|
| `src/quiver/` package | The actual package (current stub has wrong name "margaux", wrong structure) |
| `src/quiver/_lib.py` | CFFI ffi instance + DLL loader |
| `src/quiver/_declarations.py` | `ffi.cdef()` string — all C types and function signatures |
| `src/quiver/_error.py` | `QuiverError`, `check()` |
| `src/quiver/element.py` | `Element` context manager |
| `src/quiver/database*.py` | One module per functional area, matching Julia layout |
| `test/test.bat` | Windows test runner (sets PATH, calls pytest) |

### Modified Components

| Component | Change |
|-----------|--------|
| `pyproject.toml` | Rename project to "quiver", add `cffi>=1.17` dependency |
| `src/__init__.py` | Replace placeholder with proper package re-exports |
| `tests/conftest.py` | Add fixture providing path to `tests/schemas/valid/` |
| `Makefile` | Update `test` target to account for PATH setup on Windows |

### Unchanged Components (Existing Architecture)

| Component | Why Unchanged |
|-----------|---------------|
| `include/quiver/c/*.h` | Python consumes as-is, no new C API needed |
| `src/c/*.cpp` | No new C API functions required |
| CMakeLists.txt | `libquiver_c` already built; Python finds it via path resolution |
| `tests/schemas/` | Shared; Python tests reference them directly |
| Julia / Dart bindings | No changes required |

---

## Build Order and Dependencies

```
1. cmake --build build --config Debug
   → produces build/bin/libquiver_c.dll (Windows)
   → produces build/bin/libquiver.dll (Windows, libquiver_c dependency)

2. cd bindings/python
   uv sync  (installs cffi + dev deps)
   → ffi.dlopen() resolves DLL at runtime, no compilation step

3. test/test.bat  (or: PATH=$REPO/build/bin:$PATH uv run pytest)
   → pytest discovers tests/unit/test_*.py
   → each test calls from_schema(":memory:", schema_path)
```

On Windows, step 3 must prepend `build/bin` to PATH so `libquiver_c.dll` can find `libquiver.dll`. This mirrors what Dart's `test.bat` does.

---

## Scalability Considerations

| Concern | Impact | Approach |
|---------|--------|----------|
| C API surface grows | `_declarations.py` must be updated | Single file, trivial to add declarations |
| New data types in C API | Python marshaling code expands | Follow existing type-dispatch pattern |
| Multi-column time series (N columns, void* arrays) | Most complex marshaling in the binding | Direct port of Julia's `read_time_series_group` logic |
| Thread safety | `quiver_get_last_error()` is thread-local | Safe; no extra Python locking needed |
| Memory leaks | Every C allocation has a matching free call | Enforce: every `ffi.new("T**")` output must be freed before return |

---

## Sources

- Codebase direct inspection: `include/quiver/c/*.h` (HIGH confidence — source of truth for C API)
- `bindings/julia/src/c_api.jl` — DLL path resolution pattern, function-by-function mapping (HIGH)
- `bindings/julia/src/database.jl`, `element.jl`, `exceptions.jl` — finalizer and error check patterns (HIGH)
- `bindings/dart/ffigen.yaml` — auto-generation approach for comparison (HIGH)
- CFFI documentation (ABI vs API mode): https://cffi.readthedocs.io/en/stable/overview.html (HIGH)
- `.planning/PROJECT.md` — constraints, decisions, existing stub location (HIGH)
