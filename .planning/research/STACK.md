# Technology Stack

**Project:** Quiver v0.5 Code Quality Improvements
**Researched:** 2026-02-27
**Focus:** Options type ownership, layer inversion fixes, Python CFFI patterns for LuaRunner

## Current Layer Inversion (The Problem)

The C++ `options.h` currently contains:

```cpp
#include "quiver/c/options.h"  // C++ includes C API header

using DatabaseOptions = quiver_database_options_t;  // C++ type IS the C type
```

This is a **layer inversion**: the C++ core depends on its own C API layer. The dependency should flow `C API -> C++`, never `C++ -> C API`. The C API is a consumer of C++ types, not a provider.

### Why This Matters

1. **Build coupling**: C++ core cannot compile without C API headers present
2. **Semantic confusion**: `DatabaseOptions` is a C struct pretending to be a C++ type -- no constructors, no defaults, no validation
3. **Extension friction**: Adding a C++-only field (e.g., `std::function` callback, `std::filesystem::path`) is impossible because the type IS the C struct
4. **Precedent**: `CSVOptions` is already correctly defined as a C++ struct with `std::unordered_map` and `std::string`, with a separate C representation. `DatabaseOptions` should follow the same pattern

## Recommended Pattern: C++ Owns, C API Converts

### Pattern 1: C++ Value Types as Source of Truth

Define options as plain C++ value types in `include/quiver/options.h`. The C API defines its own flat struct and converts at the boundary.

**C++ layer (source of truth):**

```cpp
// include/quiver/options.h
#ifndef QUIVER_OPTIONS_H
#define QUIVER_OPTIONS_H

#include "export.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace quiver {

enum class LogLevel { Debug, Info, Warn, Error, Off };

struct QUIVER_API DatabaseOptions {
    bool read_only = false;
    LogLevel console_level = LogLevel::Info;
};

inline DatabaseOptions default_database_options() {
    return {};  // all defaults via member initializers
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

#endif
```

**C API layer (flat representation for FFI):**

```c
// include/quiver/c/options.h
typedef enum {
    QUIVER_LOG_DEBUG = 0,
    QUIVER_LOG_INFO = 1,
    QUIVER_LOG_WARN = 2,
    QUIVER_LOG_ERROR = 3,
    QUIVER_LOG_OFF = 4,
} quiver_log_level_t;

typedef struct {
    int read_only;
    quiver_log_level_t console_level;
} quiver_database_options_t;
```

**C API conversion (in implementation only):**

```cpp
// src/c/database_options.h (internal, not public)
#include "quiver/options.h"
#include "quiver/c/options.h"

inline quiver::DatabaseOptions convert_database_options(const quiver_database_options_t* opts) {
    quiver::DatabaseOptions result;
    result.read_only = opts->read_only != 0;
    result.console_level = static_cast<quiver::LogLevel>(opts->console_level);
    return result;
}

inline quiver_database_options_t convert_to_c_options(const quiver::DatabaseOptions& opts) {
    quiver_database_options_t result;
    result.read_only = opts.read_only ? 1 : 0;
    result.console_level = static_cast<quiver_log_level_t>(opts.console_level);
    return result;
}
```

### Pattern 2: Enum Mirroring with Static Assertions

Ensure C and C++ enum values stay in sync without coupling:

```cpp
// src/c/database_options.h (internal)
static_assert(static_cast<int>(quiver::LogLevel::Debug) == QUIVER_LOG_DEBUG);
static_assert(static_cast<int>(quiver::LogLevel::Info)  == QUIVER_LOG_INFO);
static_assert(static_cast<int>(quiver::LogLevel::Warn)  == QUIVER_LOG_WARN);
static_assert(static_cast<int>(quiver::LogLevel::Error) == QUIVER_LOG_ERROR);
static_assert(static_cast<int>(quiver::LogLevel::Off)   == QUIVER_LOG_OFF);
```

This catches mismatches at compile time without requiring `#include` in either direction.

### Pattern 3: Conversion at API Boundary (Existing CSVOptions Pattern)

The codebase already does this correctly for `CSVOptions`:

```cpp
// src/c/database_csv_export.cpp
auto cpp_options = convert_options(options);  // C -> C++ conversion
db->db.export_csv(collection, group, path, cpp_options);
```

`DatabaseOptions` should follow the identical pattern: convert C struct to C++ struct at every C API function entry point.

## How `quiver_database_options_default()` Changes

Currently returns `quiver::default_database_options()` which returns a C struct. After the fix:

```cpp
QUIVER_C_API quiver_database_options_t quiver_database_options_default(void) {
    auto cpp = quiver::default_database_options();
    return convert_to_c_options(cpp);
}
```

Or more directly, since the C defaults match:

```cpp
QUIVER_C_API quiver_database_options_t quiver_database_options_default(void) {
    return {0, QUIVER_LOG_INFO};
}
```

## Python CFFI Pattern for LuaRunner Binding

### Current C API Surface (Already Exists)

The C API for LuaRunner is complete and well-structured:

```c
quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner);
quiver_error_t quiver_lua_runner_free(quiver_lua_runner_t* runner);
quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script);
quiver_error_t quiver_lua_runner_get_error(quiver_lua_runner_t* runner, const char** out_error);
```

### CFFI cdef Declarations Needed

Add to `_c_api.py`:

```python
# lua_runner.h
typedef struct quiver_lua_runner quiver_lua_runner_t;

quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner);
quiver_error_t quiver_lua_runner_free(quiver_lua_runner_t* runner);
quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script);
quiver_error_t quiver_lua_runner_get_error(quiver_lua_runner_t* runner, const char** out_error);
```

### Python Wrapper Pattern

Use `ffi.gc()` for automatic cleanup, matching the existing `Database` close-on-del pattern but using CFFI's built-in destructor association:

```python
class LuaRunner:
    """Thin Python wrapper around the Quiver C API LuaRunner handle."""

    def __init__(self, db: Database) -> None:
        db._ensure_open()
        lib = get_lib()
        out_runner = ffi.new("quiver_lua_runner_t**")
        check(lib.quiver_lua_runner_new(db._ptr, out_runner))
        self._ptr = out_runner[0]
        self._closed = False

    def run(self, script: str) -> None:
        """Run a Lua script with access to the database as 'db'."""
        self._ensure_open()
        lib = get_lib()
        err = lib.quiver_lua_runner_run(self._ptr, script.encode("utf-8"))
        if err != 0:
            # LuaRunner has its own error storage
            out_error = ffi.new("const char**")
            lib.quiver_lua_runner_get_error(self._ptr, out_error)
            msg = ffi.string(out_error[0]).decode("utf-8") if out_error[0] != ffi.NULL else "Unknown Lua error"
            raise QuiverError(msg)

    def close(self) -> None:
        if not self._closed and self._ptr is not None:
            lib = get_lib()
            lib.quiver_lua_runner_free(self._ptr)
            self._closed = True

    def _ensure_open(self) -> None:
        if self._closed:
            raise QuiverError("LuaRunner is closed")

    def __del__(self) -> None:
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args) -> None:
        self.close()
```

### Key Design Decisions for Python LuaRunner

| Decision | Rationale |
|----------|-----------|
| Use `__del__` + explicit `close()` | Matches existing `Database` pattern in this codebase |
| Read error from `quiver_lua_runner_get_error` | LuaRunner has its own error storage, not `quiver_get_last_error` |
| Store `_closed` flag | Prevents double-free, matches Database pattern |
| Support context manager | Pythonic resource management |
| Accept `Database` instance (not raw ptr) | Thin binding principle -- validates db is open before passing |

### Alternative: `ffi.gc()` Pattern

CFFI supports automatic destructor association via `ffi.gc()`:

```python
self._ptr = ffi.gc(out_runner[0], lib.quiver_lua_runner_free)
```

**Do NOT use this.** Reason: LuaRunner has a non-trivial destructor that accesses `quiver_lua_runner_free`, which returns `quiver_error_t`. The `ffi.gc` destructor cannot check error codes. The explicit `close()` / `__del__` pattern gives control over error handling and matches the existing `Database` pattern in this codebase. Consistency trumps cleverness.

## Python Data Type Constants

### Current Problem

The Python binding uses hardcoded integers for data types in parameterized queries:

```python
# Somewhere in the Python code, callers must know:
# 0 = INTEGER, 1 = FLOAT, 2 = STRING, 4 = NULL
```

### Recommended Fix

The C API already exposes `quiver_data_type_t` via the cdef. Python should expose named constants:

```python
# In metadata.py or a new constants.py
class DataType:
    """Constants matching quiver_data_type_t enum values."""
    INTEGER = 0
    FLOAT = 1
    STRING = 2
    DATE_TIME = 3
    NULL = 4
```

These values mirror the C enum exactly. Since CFFI ABI-mode already has the enum declared, you could also access them via `lib.QUIVER_DATA_TYPE_INTEGER`, but named Python constants are more ergonomic and avoid requiring the lib to be loaded at import time.

## Existing Stack (No Changes Needed)

These are validated and not being re-researched:

| Technology | Version | Purpose | Status |
|------------|---------|---------|--------|
| C++20 | -- | Core library | Validated |
| SQLite | Bundled | Database engine | Validated |
| spdlog | Bundled | Logging | Validated |
| sol2 | Bundled | Lua binding for C++ | Validated |
| CFFI | ABI-mode | Python FFI | Validated |
| CMake | -- | Build system | Validated |

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| C++ LogLevel | `enum class LogLevel` | Reuse `quiver_log_level_t` via typedef | Layer inversion; C++ should not depend on C API |
| DatabaseOptions | C++ struct with defaults | Keep as C typedef alias | Prevents adding C++-only members; wrong dependency direction |
| Python LuaRunner cleanup | Explicit `close()` + `__del__` | `ffi.gc()` auto-destructor | Cannot check error codes; inconsistent with Database pattern |
| Python DataType constants | Python class with int constants | Access via `lib.QUIVER_DATA_TYPE_*` | Requires lib loaded at import time; less Pythonic |

## Impact Summary

### Files That Change

| File | Change | Risk |
|------|--------|------|
| `include/quiver/options.h` | Remove `#include "quiver/c/options.h"`, define C++ `LogLevel` enum, change `DatabaseOptions` from alias to struct | Medium -- touches every file that uses `DatabaseOptions` |
| `include/quiver/c/options.h` | No change needed -- already defines the C types independently | None |
| `src/c/database_options.h` | Add `convert_database_options()` function | Low |
| `src/c/database.cpp` | Call `convert_database_options()` before passing to C++ | Low |
| `src/c/internal.h` | Update `quiver_database` constructor to take `quiver::DatabaseOptions` (already does) | None |
| `bindings/python/src/quiverdb/_c_api.py` | Add LuaRunner cdef declarations | Low |
| `bindings/python/src/quiverdb/lua_runner.py` | New file -- Python LuaRunner wrapper | Low |
| `bindings/python/src/quiverdb/metadata.py` | Add `DataType` class | Low |

### Files That Do NOT Change

The C API header `include/quiver/c/options.h` keeps its current struct layout. All FFI bindings (Julia, Dart, Python) continue to use `quiver_database_options_t` and `quiver_log_level_t` unchanged. The change is purely in the C++ layer's type definitions and the conversion functions in `src/c/`.

## Sources

- [Hourglass Interfaces for C++ APIs -- CppCon 2014](https://isocpp.org/blog/2015/07/cppcon-2014-hourglass-interfaces-for-cpp-apis-stefanus-dutoit) -- Authoritative pattern for C++/C API layering
- [Hourglass C-API for C++ libraries (GitHub)](https://github.com/JarnoRalli/hourglass-c-api) -- Reference implementation
- [Hourglass Interfaces (Brent Huisman)](https://brent.huisman.pl/hourglass-interfaces/) -- Practical walkthrough
- [C Wrappers for C++ Libraries and Interoperability](https://caiorss.github.io/C-Cpp-Notes/CwrapperToQtLibrary.html) -- Opaque handle patterns
- [CFFI Reference -- ffi.gc()](https://cffi.readthedocs.io/en/latest/ref.html) -- Destructor association for opaque handles
- [CFFI Using ffi/lib objects](https://cffi.readthedocs.io/en/latest/using.html) -- ABI-mode patterns for opaque types
- [Python context managers for CFFI resource management](https://kurtraschke.com/2015/10/python-cffi-context-managers/) -- Lifecycle patterns
