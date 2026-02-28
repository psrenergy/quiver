# Architecture Patterns

**Domain:** v0.5 Code Quality Improvements for Quiver (C++ SQLite Wrapper Library)
**Researched:** 2026-02-27
**Confidence:** HIGH (based on direct codebase analysis; all claims verified against source files)

## Overview

This document analyzes three architectural changes and their integration points:

1. **Fix layer inversion:** Move `DatabaseOptions` and `CSVOptions` ownership from C API to C++
2. **Rename `quiver_element_free_string`:** Resolve semantic confusion in free function naming
3. **Add Python LuaRunner:** Integrate Lua scripting into Python CFFI binding

Each section maps the current state, the target state, exact files impacted, and the build order for changes.

---

## Current Architecture (Layer Diagram)

```
  Julia    Dart    Python    Lua (via LuaRunner)
    |        |       |            |
    v        v       v            v
  +---------------------------------+
  |        C API (extern "C")       |  <-- include/quiver/c/
  +---------------------------------+
               |
               v
  +---------------------------------+
  |      C++ Core Library           |  <-- include/quiver/, src/
  +---------------------------------+
               |
               v
  +---------------------------------+
  |      SQLite / spdlog / sol2     |  <-- third-party dependencies
  +---------------------------------+
```

## Correct Dependency Direction (After Fix)

```
  C++ layer (include/quiver/options.h)       [standalone, no C API dependency]
       ^
       |  src/c/*.cpp #include both headers
       |
  C API layer (include/quiver/c/options.h)   [standalone, no C++ dependency]
```

The C API implementation files (`src/c/`) are the only place where both C and C++ headers should be included together. Public headers must never cross the boundary.

---

## 1. DatabaseOptions / CSVOptions Layer Inversion Fix

### Current State (The Problem)

The type ownership is inverted. The C API layer defines the canonical types, and C++ aliases them:

```
include/quiver/c/options.h          -- DEFINES quiver_database_options_t, quiver_log_level_t, quiver_csv_options_t
include/quiver/options.h            -- ALIASES: using DatabaseOptions = quiver_database_options_t;
                                    -- DEFINES: CSVOptions (C++ struct with std::unordered_map)
```

**`DatabaseOptions` is a pure C struct aliased into C++.** The C++ `Database` constructor takes `const DatabaseOptions&` which is actually `const quiver_database_options_t&` -- a C struct containing a C enum (`quiver_log_level_t`). This means:

- `include/quiver/options.h` line 6 includes `include/quiver/c/options.h` (C++ public header depends on C API header)
- The C++ public API surface contains C-prefixed type names in its include chain
- Any C++ consumer that includes `database.h` transitively pulls in C API headers

**`CSVOptions` is already C++-owned** but uses a different pattern: it is a proper C++ struct with `std::unordered_map` and `std::string`, and `src/c/database_options.h` provides `convert_csv_options()` to translate from `quiver_csv_options_t` to `quiver::CSVOptions`. This is the correct direction.

### Target State

```
include/quiver/options.h            -- DEFINES: LogLevel enum class, DatabaseOptions struct, CSVOptions struct
include/quiver/c/options.h          -- DEFINES: quiver_log_level_t, quiver_database_options_t (C-compatible)
src/c/database_options.h            -- CONVERTS: quiver_database_options_t -> quiver::DatabaseOptions
                                    -- CONVERTS: quiver_csv_options_t -> quiver::CSVOptions (already exists)
```

### Recommended Implementation

**Step 1: Define C++ types in `include/quiver/options.h`**

```cpp
#ifndef QUIVER_OPTIONS_H
#define QUIVER_OPTIONS_H

#include "export.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace quiver {

enum class LogLevel : int {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
    Off = 4,
};

struct DatabaseOptions {
    bool read_only = false;
    LogLevel console_level = LogLevel::Info;
};

inline DatabaseOptions default_database_options() {
    return {};
}

struct QUIVER_API CSVOptions {
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, int64_t>>>
        enum_labels;
    std::string date_time_format;
};

inline CSVOptions default_csv_options() {
    return {};
}

}  // namespace quiver

#endif  // QUIVER_OPTIONS_H
```

Key decisions:
- `LogLevel` is an `enum class` (not a raw enum) because C++20 is the target standard and scoped enums are idiomatic
- `DatabaseOptions` uses `bool` instead of `int` for `read_only` because C++ consumers should use native types
- The `CSVOptions` struct is unchanged -- it was already correct
- **Remove** the `#include "quiver/c/options.h"` from this file entirely

**Step 2: Keep C API types in `include/quiver/c/options.h` (unchanged)**

The C API header remains exactly as-is. It defines `quiver_log_level_t`, `quiver_database_options_t`, and `quiver_csv_options_t` with C-compatible layout. No change needed here.

**Step 3: Add conversion in `src/c/database_options.h`**

Add `convert_database_options()` alongside the existing `convert_csv_options()`, with static asserts for enum correspondence:

```cpp
static_assert(static_cast<int>(quiver::LogLevel::Debug) == QUIVER_LOG_DEBUG);
static_assert(static_cast<int>(quiver::LogLevel::Info)  == QUIVER_LOG_INFO);
static_assert(static_cast<int>(quiver::LogLevel::Warn)  == QUIVER_LOG_WARN);
static_assert(static_cast<int>(quiver::LogLevel::Error) == QUIVER_LOG_ERROR);
static_assert(static_cast<int>(quiver::LogLevel::Off)   == QUIVER_LOG_OFF);

inline quiver::DatabaseOptions convert_database_options(const quiver_database_options_t* options) {
    quiver::DatabaseOptions cpp_options;
    cpp_options.read_only = options->read_only != 0;
    cpp_options.console_level = static_cast<quiver::LogLevel>(options->console_level);
    return cpp_options;
}
```

**Step 4: Update `src/c/database.cpp`**

The C API database lifecycle functions currently pass `quiver_database_options_t` directly. After the change, they must convert first:

```cpp
// Before:
*out_db = new quiver_database(path, *options);

// After:
auto cpp_options = options ? convert_database_options(options) : quiver::default_database_options();
*out_db = new quiver_database(path, cpp_options);
```

This pattern applies to `quiver_database_open`, `quiver_database_from_schema`, and `quiver_database_from_migrations`.

**Step 5: Update spdlog mapping in `src/database.cpp`**

The `Database` constructor currently maps `quiver_log_level_t` enum values to spdlog levels. After the change, it maps `quiver::LogLevel` values:

```cpp
// Before:
case QUIVER_LOG_DEBUG: return spdlog::level::debug;
// After:
case LogLevel::Debug: return spdlog::level::debug;
```

### Files Modified (DatabaseOptions Fix)

| File | Change Type | Description |
|------|-------------|-------------|
| `include/quiver/options.h` | **Modified** | Remove C API include, define C++ `LogLevel` enum class, define C++ `DatabaseOptions` struct |
| `src/c/database_options.h` | **Modified** | Add `convert_database_options()` function, add `static_assert`s |
| `src/c/database.cpp` | **Modified** | Call `convert_database_options()` before constructing `quiver_database` |
| `src/database.cpp` | **Modified** | Update spdlog level mapping from `quiver_log_level_t` to `quiver::LogLevel` |

### Files Unchanged

| File | Why Unchanged |
|------|---------------|
| `include/quiver/c/options.h` | C API types are correct as-is |
| `include/quiver/c/database.h` | Still accepts `quiver_database_options_t*` -- no change |
| `src/c/internal.h` | Constructor signature uses `quiver::DatabaseOptions` which is still the same name |
| `include/quiver/database.h` | Already uses `DatabaseOptions` which is still `quiver::DatabaseOptions` |
| All bindings | All bindings call C API functions with `quiver_database_options_t` -- C API signature unchanged |
| All generators | Generators read C API headers only -- C++ header changes are invisible to them |

### Impact on Binding Generators

**None.** All four binding generators (Julia Clang.jl, Dart ffigen, Python CFFI generator) consume only the `include/quiver/c/` headers. The C API header `include/quiver/c/options.h` is unchanged, so:
- Julia generator output (`bindings/julia/src/c_api.jl`) -- unchanged
- Dart generator output (`bindings/dart/lib/src/ffi/bindings.dart`) -- unchanged
- Python generator output (`bindings/python/src/quiverdb/_declarations.py`) -- unchanged
- Python hand-written `_c_api.py` -- unchanged

### Build Order

1. Modify `include/quiver/options.h` (removes C API dependency)
2. Modify `src/database.cpp` (update LogLevel mapping)
3. Modify `src/c/database_options.h` (add convert function + static asserts)
4. Modify `src/c/database.cpp` (use convert function)
5. Build C++ core (`libquiver`) -- must succeed before C API
6. Build C API (`libquiver_c`) -- depends on core
7. Run C++ tests, then C API tests
8. No binding changes needed; run binding tests to confirm no regressions

---

## 2. Rename `quiver_element_free_string`

### Current State (The Problem)

`quiver_element_free_string` is defined in `include/quiver/c/element.h` and implemented in `src/c/element.cpp`. Its implementation is trivial: `delete[] str;`.

The semantic confusion: this function is used to free strings returned by **database operations**, not just element operations:

| Function that allocates | Entity scope | Free function used | Correct? |
|------------------------|--------------|--------------------|----------|
| `quiver_element_to_string` | Element | `quiver_element_free_string` | Yes |
| `quiver_database_query_string` | Database | `quiver_element_free_string` | **No** |
| `quiver_database_query_string_params` | Database | `quiver_element_free_string` | **No** |
| `quiver_database_read_scalar_string_by_id` | Database | `quiver_element_free_string` | **No** |

The string from `quiver_database_query_string` is allocated in `src/c/database_query.cpp` via `strdup_safe()`, but callers free it with `quiver_element_free_string`. This violates the co-location principle: the alloc/free pair should live in the same entity scope.

### Usage Sites Across All Bindings

**Dart** (`bindings/dart/lib/src/`):
- `database_read.dart:485` -- read_scalar_string_by_id result
- `database_query.dart:29` -- query_string result
- `database_query.dart:131` -- query_string_params result

**Julia** (`bindings/julia/src/`):
- `database_read.jl:242` -- read_scalar_string_by_id result
- `database_query.jl:62` -- query_string result
- `database_query.jl:129` -- query_string_params result

**Python** (`bindings/python/src/quiverdb/`):
- `database.py:243` -- read_scalar_string_by_id result
- `database.py:503` -- query_string result

**Correctly scoped usages (keep as-is):**
- `bindings/python/src/quiverdb/element.py:120` -- element to_string
- `bindings/julia/src/element.jl:106` -- element to_string
- (Dart does not appear to have an element to_string wrapper)

### Recommended Approach: Add `quiver_database_free_string`

Given the project's WIP status and "breaking changes acceptable" policy, a clean fix is optimal. Add `quiver_database_free_string` for database-scoped strings. Keep `quiver_element_free_string` only for element-scoped strings.

### Implementation

**Step 1: Add `quiver_database_free_string` to C API**

In `include/quiver/c/database.h`, add alongside the other free functions:

```c
// Free a single string returned by query or read-by-id operations
QUIVER_C_API quiver_error_t quiver_database_free_string(char* str);
```

In `src/c/database_read.cpp` (co-located with `quiver_database_free_string_array`):

```cpp
QUIVER_C_API quiver_error_t quiver_database_free_string(char* str) {
    delete[] str;
    return QUIVER_OK;
}
```

**Step 2: Update all bindings to use `quiver_database_free_string` for database-scoped strings**

### Files Modified (Free Function Fix)

| File | Change Type | Lines | Description |
|------|-------------|-------|-------------|
| `include/quiver/c/database.h` | **Modified** | (add near existing free functions) | Add `quiver_database_free_string` declaration |
| `src/c/database_read.cpp` | **Modified** | (add near `free_string_array`) | Add `quiver_database_free_string` implementation |
| `bindings/dart/lib/src/database_read.dart` | **Modified** | 485 | `quiver_element_free_string` -> `quiver_database_free_string` |
| `bindings/dart/lib/src/database_query.dart` | **Modified** | 29, 131 | `quiver_element_free_string` -> `quiver_database_free_string` |
| `bindings/julia/src/database_read.jl` | **Modified** | 242 | `C.quiver_element_free_string` -> `C.quiver_database_free_string` |
| `bindings/julia/src/database_query.jl` | **Modified** | 62, 129 | `C.quiver_element_free_string` -> `C.quiver_database_free_string` |
| `bindings/python/src/quiverdb/database.py` | **Modified** | 243, 503 | `lib.quiver_element_free_string` -> `lib.quiver_database_free_string` |
| `bindings/python/src/quiverdb/_c_api.py` | **Modified** | (add to cdef) | Add `quiver_database_free_string` CFFI declaration |

### Files Unchanged

| File | Why |
|------|-----|
| `include/quiver/c/element.h` | `quiver_element_free_string` stays for `quiver_element_to_string` output |
| `src/c/element.cpp` | `quiver_element_free_string` implementation stays |
| `bindings/python/src/quiverdb/element.py` | Still uses `quiver_element_free_string` for `to_string()` (correct) |
| `bindings/julia/src/element.jl` | Still uses `C.quiver_element_free_string` for `to_string` (correct) |

### Impact on Binding Generators

**All generators must be re-run** after adding `quiver_database_free_string` to `include/quiver/c/database.h`:

| Generator | Input Header Changed | Output File | Action |
|-----------|---------------------|-------------|--------|
| Julia (`generator.bat`) | `include/quiver/c/database.h` | `bindings/julia/src/c_api.jl` | Re-run; auto-generates `quiver_database_free_string` wrapper |
| Dart (`generator.bat`) | `include/quiver/c/database.h` | `bindings/dart/lib/src/ffi/bindings.dart` | Re-run; auto-generates `quiver_database_free_string` binding |
| Python (`generator.bat`) | `include/quiver/c/database.h` | `bindings/python/src/quiverdb/_declarations.py` | Re-run; updates reference copy |

Note: The Dart and Julia generators auto-generate the FFI binding layer, so after re-running they will have the new function available. The Python generator updates `_declarations.py` (the reference copy), but `_c_api.py` (the hand-written file) must be updated **manually** to add the new declaration.

### Build Order

1. Add `quiver_database_free_string` to `include/quiver/c/database.h`
2. Add implementation to `src/c/database_read.cpp`
3. Build C++ core and C API
4. Add C API test for `quiver_database_free_string(nullptr)` -- should return QUIVER_OK
5. Run all generators (Julia, Dart, Python)
6. Update Julia binding: `database_read.jl`, `database_query.jl`
7. Update Dart binding: `database_read.dart`, `database_query.dart`
8. Update Python binding: `_c_api.py` (add declaration), `database.py` (change calls)
9. Run all test suites

---

## 3. Python LuaRunner Binding

### Current State

- C++ `LuaRunner` class exists (`include/quiver/lua_runner.h`, `src/lua_runner.cpp`)
- C API wrapper exists (`include/quiver/c/lua_runner.h`, `src/c/lua_runner.cpp`)
- Julia binding exists (`bindings/julia/src/lua_runner.jl`) -- uses finalizer for cleanup
- Dart binding exists (`bindings/dart/lib/src/lua_runner.dart`) -- uses dispose pattern
- Python binding **does not exist** -- no `lua_runner.py` in `bindings/python/src/quiverdb/`

### C API Surface to Wrap

The Python binding needs to wrap exactly four C API functions from `include/quiver/c/lua_runner.h`:

```c
quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner);
quiver_error_t quiver_lua_runner_free(quiver_lua_runner_t* runner);
quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script);
quiver_error_t quiver_lua_runner_get_error(quiver_lua_runner_t* runner, const char** out_error);
```

### Integration with Existing CFFI Infrastructure

**Step 1: Add CFFI declarations to `_c_api.py`**

Append to the hand-written cdef block:

```python
    // lua_runner.h
    typedef struct quiver_lua_runner quiver_lua_runner_t;
    quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner);
    quiver_error_t quiver_lua_runner_free(quiver_lua_runner_t* runner);
    quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script);
    quiver_error_t quiver_lua_runner_get_error(quiver_lua_runner_t* runner, const char** out_error);
```

**Step 2: Update the Python generator HEADERS list**

In `bindings/python/generator/generator.py`, add `lua_runner.h`:

```python
HEADERS = [
    "include/quiver/c/common.h",
    "include/quiver/c/options.h",
    "include/quiver/c/database.h",
    "include/quiver/c/element.h",
    "include/quiver/c/lua_runner.h",  # Add this
]
```

Then re-run the generator to update `_declarations.py` (the reference copy).

**Step 3: Create `bindings/python/src/quiverdb/lua_runner.py`**

Follow the Dart binding pattern adapted for CFFI:

```python
from __future__ import annotations

from quiverdb._c_api import ffi, get_lib
from quiverdb._helpers import check
from quiverdb.exceptions import QuiverError


class LuaException(QuiverError):
    """Raised when a Lua script fails to execute."""
    pass


class LuaRunner:
    """Execute Lua scripts with access to a Quiver database."""

    def __init__(self, db) -> None:
        lib = get_lib()
        out_runner = ffi.new("quiver_lua_runner_t**")
        check(lib.quiver_lua_runner_new(db._ptr, out_runner))
        self._ptr = out_runner[0]
        self._closed = False

    def run(self, script: str) -> None:
        if self._closed:
            raise QuiverError("LuaRunner has been closed")
        lib = get_lib()
        err = lib.quiver_lua_runner_run(self._ptr, script.encode("utf-8"))
        if err != 0:
            out_error = ffi.new("const char**")
            get_err = lib.quiver_lua_runner_get_error(self._ptr, out_error)
            if get_err == 0 and out_error[0] != ffi.NULL:
                error_msg = ffi.string(out_error[0]).decode("utf-8")
                if error_msg:
                    raise LuaException(error_msg)
            check(err)  # Fallback to global error

    def close(self) -> None:
        if self._closed:
            return
        get_lib().quiver_lua_runner_free(self._ptr)
        self._closed = True

    def __del__(self) -> None:
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()
```

Design decisions:
- **Context manager support** (`with LuaRunner(db) as lua:`) -- Pythonic resource management
- **`__del__` cleanup** -- Safety net for GC, same role as Julia's finalizer
- **`LuaException` inherits from `QuiverError`** -- Consistent with Dart's `LuaException`
- **Error retrieval follows Dart pattern** -- Check runner-specific error first, fall back to global error

**Step 4: Export from `__init__.py`**

Add `LuaRunner` and `LuaException` to `bindings/python/src/quiverdb/__init__.py`.

### CFFI ABI-Mode DLL Loading

No additional DLL loading is needed. The existing `_loader.py` handles the full dependency chain:

```
Python CFFI -> libquiver_c.dll -> libquiver.dll -> lua54.dll / spdlog / sqlite3
```

The LuaRunner functions are in `libquiver_c.dll`, which is already loaded by `get_lib()`.

### Files Modified / Created (Python LuaRunner)

| File | Change Type | Description |
|------|-------------|-------------|
| `bindings/python/src/quiverdb/lua_runner.py` | **New** | LuaRunner class with context manager |
| `bindings/python/src/quiverdb/_c_api.py` | **Modified** | Add LuaRunner CFFI declarations |
| `bindings/python/src/quiverdb/__init__.py` | **Modified** | Export `LuaRunner`, `LuaException` |
| `bindings/python/generator/generator.py` | **Modified** | Add `lua_runner.h` to HEADERS |
| `bindings/python/src/quiverdb/_declarations.py` | **Regenerated** | Re-run generator |
| `bindings/python/tests/test_lua_runner.py` | **New** | Tests for create, run, error handling, close |

### Files Unchanged

| File | Why |
|------|-----|
| `include/quiver/c/lua_runner.h` | C API already exists |
| `src/c/lua_runner.cpp` | C API implementation already exists |
| `bindings/python/src/quiverdb/_loader.py` | DLL loading already handles dependency chain |

### Impact on Other Binding Generators

**None.** This change only affects the Python binding. No C API header changes.

### Build Order

1. No C/C++ changes needed -- the C API already exists
2. Update `bindings/python/generator/generator.py` (add lua_runner.h)
3. Re-run Python generator
4. Add CFFI declarations to `_c_api.py`
5. Create `lua_runner.py`
6. Update `__init__.py` exports
7. Write tests in `test_lua_runner.py`
8. Run Python test suite

---

## Component Boundaries

### Before v0.5 Changes

```
include/quiver/options.h  --includes-->  include/quiver/c/options.h   [LAYER INVERSION]
include/quiver/database.h --includes-->  include/quiver/options.h     [transitively pulls C API]
```

### After v0.5 Changes

```
include/quiver/options.h                 [standalone C++ types, no C API dependency]
include/quiver/c/options.h               [standalone C types, no C++ dependency]
src/c/database_options.h  --includes-->  both  [conversion layer, internal only]
```

### Full Component Map

| Component | Responsibility | Modified in v0.5 | Communicates With |
|-----------|---------------|-------------------|-------------------|
| `include/quiver/options.h` | C++ option types | Yes (own types) | `database.h`, `database.cpp` |
| `include/quiver/c/options.h` | C option types | No | `database.h` (C API), all bindings |
| `include/quiver/c/database.h` | C API declarations | Yes (add free_string) | All bindings, generators |
| `include/quiver/c/element.h` | Element C API | No | Julia, Dart, Python element wrappers |
| `include/quiver/c/lua_runner.h` | LuaRunner C API | No | Julia, Dart, Python (new) wrappers |
| `src/c/database_options.h` | C-to-C++ conversion | Yes (add convert_database_options) | `database.cpp`, CSV files |
| `src/c/database.cpp` | Lifecycle C API | Yes (use conversion) | `internal.h`, `database_options.h` |
| `src/c/database_read.cpp` | Read + free C API | Yes (add free_string) | All bindings read/query |
| `src/c/database_query.cpp` | Query C API | No (alloc unchanged) | Bindings query wrappers |
| `src/database.cpp` | C++ Database impl | Yes (LogLevel mapping) | All C++ internals |
| `bindings/python/src/quiverdb/_c_api.py` | CFFI declarations | Yes (add free_string + LuaRunner) | All Python wrapper files |
| `bindings/python/src/quiverdb/lua_runner.py` | Python LuaRunner | **New file** | `_c_api.py`, `database.py` |
| `bindings/python/src/quiverdb/database.py` | Python Database | Yes (free_string calls) | `_c_api.py` |
| `bindings/julia/src/database_read.jl` | Julia read ops | Yes (free_string calls) | `c_api.jl` |
| `bindings/julia/src/database_query.jl` | Julia query ops | Yes (free_string calls) | `c_api.jl` |
| `bindings/dart/lib/src/database_read.dart` | Dart read ops | Yes (free_string calls) | `bindings.dart` |
| `bindings/dart/lib/src/database_query.dart` | Dart query ops | Yes (free_string calls) | `bindings.dart` |

---

## Patterns to Follow

### Pattern 1: Parallel Type Definitions with Boundary Conversion

**What:** Define equivalent types in both C++ and C layers independently, convert at the C API implementation boundary.

**When:** Any type that crosses the C++/C boundary.

**Already used for:** `CSVOptions`, `ScalarMetadata`, `GroupMetadata`, `DataType`.

**Must fix for:** `DatabaseOptions` (currently bypasses this pattern via typedef alias).

**Example conversion site:**
```cpp
// src/c/database.cpp
auto cpp_opts = options ? convert_database_options(options)
                        : quiver::default_database_options();
*out_db = new quiver_database(path, cpp_opts);
```

### Pattern 2: Static Assert Enum Correspondence

**What:** Use `static_assert` to ensure C and C++ enum values stay synchronized without compile-time coupling.

**When:** Parallel enums exist in both layers.

**Example:**
```cpp
// In src/c/database_options.h (implementation-only header)
static_assert(static_cast<int>(quiver::LogLevel::Debug) == QUIVER_LOG_DEBUG);
static_assert(static_cast<int>(quiver::LogLevel::Off)   == QUIVER_LOG_OFF);
```

### Pattern 3: Co-located Alloc/Free

**What:** Every C API function that allocates memory has its matching free function in the same `.cpp` file.

**When:** Adding any new C API function that returns heap-allocated data.

**Applied to v0.5:** `quiver_database_free_string` goes in `src/c/database_read.cpp` alongside `quiver_database_free_string_array` and other read-related free functions.

### Pattern 4: Entity-Scoped Free Functions

**What:** Free functions are named and scoped by the entity that logically owns the allocation.

**When:** Any heap-allocated data returned through the C API.

**Convention:** `quiver_database_free_*` for database-allocated data, `quiver_element_free_*` for element-allocated data.

---

## Anti-Patterns to Avoid

### Anti-Pattern 1: Type Aliasing Across Layers

**What:** Using `using CppType = CApiType;` to make a C struct serve as the C++ type.

**Why bad:** Creates an upward dependency (C++ depends on C API). Prevents adding C++-only features. Breaks the layered architecture.

**Instead:** Define independent types in each layer, convert at the boundary.

### Anti-Pattern 2: Cross-Entity Free Functions

**What:** Using `quiver_element_free_string` to free strings allocated by database operations.

**Why bad:** Violates ownership semantics. The function name implies element-scope allocation, but the string was allocated in `database_query.cpp` or `database_read.cpp`. Makes it unclear which entity owns the allocation.

**Instead:** Each entity has its own free functions. `quiver_database_free_string` for database scope, `quiver_element_free_string` for element scope.

### Anti-Pattern 3: Shared Headers Between Layers

**What:** Having C++ headers `#include` C API headers (or vice versa in public headers).

**Why bad:** Couples layers that should be independent.

**Instead:** Only `src/c/` implementation files include from both `include/quiver/` and `include/quiver/c/`.

---

## Global Build Order for All v0.5 Changes

The three changes have no dependencies on each other. If done sequentially, optimal order:

### Phase 1: DatabaseOptions Layer Fix (C++ only, no binding changes)

1. `include/quiver/options.h` -- define C++ types, remove C API include
2. `src/database.cpp` -- update LogLevel mapping
3. `src/c/database_options.h` -- add `convert_database_options()` + static asserts
4. `src/c/database.cpp` -- use conversion
5. Build and test C++ + C API
6. Run all binding tests (no binding changes, verify no regressions)

### Phase 2: Free Function Fix (C API + all bindings)

1. `include/quiver/c/database.h` -- add `quiver_database_free_string`
2. `src/c/database_read.cpp` -- add implementation
3. Build C++ + C API, add C API test
4. Run all generators (Julia, Dart, Python)
5. Update all bindings (Julia, Dart, Python hand-written files)
6. Run all test suites

### Phase 3: Python LuaRunner (Python only)

1. `bindings/python/generator/generator.py` -- add `lua_runner.h`
2. Re-run Python generator
3. `bindings/python/src/quiverdb/_c_api.py` -- add LuaRunner declarations
4. Create `bindings/python/src/quiverdb/lua_runner.py`
5. Update `__init__.py` exports
6. Write and run Python LuaRunner tests

### Phase ordering rationale

- **Phase 1 first:** C++-internal with zero binding impact. Low risk, high structural value.
- **Phase 2 second:** Modifies C API header, triggering generator re-runs. Generators should run once after this phase.
- **Phase 3 last:** Purely additive Python code with no risk to existing functionality.

**Optimization:** Phases 2 and 3 generator runs can be combined. Add both `quiver_database_free_string` to `database.h` and `lua_runner.h` to the Python generator HEADERS list, then run all generators once.

---

## Scalability Considerations

These are code quality changes with no performance implications. The architectural improvements (correct layering, consistent naming) improve maintainability for future additions but do not affect runtime behavior.

| Concern | Before v0.5 | After v0.5 |
|---------|-------------|------------|
| Adding new C++ option fields | Must update C struct too (layer inversion) | Add to C++ struct; optionally add to C struct; add conversion line |
| Adding new string-returning C API function | Unclear which free function to document | Always use `quiver_database_free_string` for database scope |
| Adding new Python binding features | Must hand-maintain CFFI declarations | Same (known concern, documented in CONCERNS.md) |

## Sources

All findings based on direct source file analysis of the Quiver codebase at commit `6dae8d1` on branch `rs/more4`. No external sources needed.

- `include/quiver/options.h` -- layer inversion visible at line 6 (`#include "quiver/c/options.h"`)
- `include/quiver/c/options.h` -- C API type definitions
- `src/c/database_options.h` -- existing `convert_csv_options()` pattern
- `src/c/database.cpp` -- C API lifecycle using `quiver_database_options_t` directly
- `src/c/element.cpp:157` -- `quiver_element_free_string` implementation
- `bindings/dart/lib/src/database_query.dart:29,131` -- Dart using element free for query results
- `bindings/julia/src/database_query.jl:62,129` -- Julia using element free for query results
- `bindings/python/src/quiverdb/database.py:243,503` -- Python using element free for results
- `bindings/python/generator/generator.py` -- HEADERS list missing `lua_runner.h`
- `.planning/codebase/CONCERNS.md` -- documents Python LuaRunner gap
