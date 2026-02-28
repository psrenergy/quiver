# Feature Landscape

**Domain:** C FFI library code quality -- free function naming, Python constant patterns, Python LuaRunner binding, cross-binding test coverage
**Researched:** 2026-02-27

## Table Stakes

Features users (and maintainers) expect. Missing = library feels inconsistent or amateurish.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Consistent free function naming across entity scopes | Every mature C library (libgit2, SQLite, curl) uses a predictable `prefix_entity_verb` pattern for deallocation. Quiver's current `quiver_element_free_string` is used to free strings returned by `quiver_database_query_string` and `quiver_database_read_scalar_string_by_id`, which is semantically wrong -- callers must use an "element" function to free "database" results. | Low | Add `quiver_database_free_string`; update all 4 bindings |
| Python named constants for C data type enums | The Python binding uses bare integers (`0`, `1`, `2`, `3`, `4`) for `QUIVER_DATA_TYPE_*` in 20+ locations across `database.py`. Every Python CFFI library of quality defines `IntEnum` or module-level constants for C enums rather than scattering magic numbers. | Low | Define `class DataType(IntEnum)` in a constants module, replace all 20+ usages |
| Python LuaRunner binding | Julia, Dart, and Lua all expose `LuaRunner`. Python is the only binding missing this feature. The C API already exists (`quiver_lua_runner_new/run/get_error/free`). Incomplete cross-language parity is a red flag. | Med | Wrap 4 C API functions; manage lifetime; add CFFI cdef declarations |
| Test coverage for `is_healthy()` and `path()` across all bindings | Python has tests; Julia and Dart do not have explicit tests for these methods. Cross-layer testing consistency is a baseline expectation. | Low | Add 2-4 tests per binding |
| C++ ownership of `DatabaseOptions` and `CSVOptions` types | The C++ layer currently imports `quiver_database_options_t` from the C API header via `using DatabaseOptions = quiver_database_options_t`. This is a layer inversion -- the C++ core should define its own types, and the C API should convert to/from them. | Med | New C++ `LogLevel` enum, restructure headers, update C API converters |
| Python convenience helper tests | PROJECT.md lists this as active. Tests exist for major helpers but edge cases (empty collections, type boundary conditions) are untested. | Low | Audit existing tests; fill gaps |

## Differentiators

Features that improve quality beyond minimum expectations. Not strictly required, but valued by maintainers and contributors.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Python `DataType` enum used in `ScalarMetadata.data_type` field | Replace `int` with `DataType` in the `ScalarMetadata` dataclass so callers get IDE autocompletion and type checking on metadata results, not just raw ints. `IntEnum` is drop-in compatible so existing comparisons still work. | Low | Depends on DataType constant definition |
| C++ `LogLevel` as proper enum class | Type safety, prevents invalid values. Natural outcome of fixing the layer inversion. Use `enum class LogLevel { Debug, Info, Warn, Error, Off }` instead of the C-style `quiver_log_level_t`. | Low | Part of the DatabaseOptions restructuring |
| `DatabaseOptions` with member initializers | `bool read_only = false; LogLevel console_level = LogLevel::Info;` -- default-constructed options just work without factory functions. | Low | Natural improvement during restructuring |
| Context manager support for Python LuaRunner | Pythonic resource management via `with` statement, matching the existing `Database` pattern. | Low | `__enter__`/`__exit__` on top of explicit `close()` |

## Anti-Features

Features to explicitly NOT build as part of this quality milestone.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Generic `quiver_free` catch-all function | SQLite uses `sqlite3_free` for all `sqlite3_malloc`-allocated memory, but this works because SQLite has a unified allocator. Quiver uses `new[]`/`new` with type-specific deallocation (strings need per-element cleanup, integer arrays are a single `delete[]`). A generic free would require a type tag or allocator registry, adding complexity for no benefit. | Keep entity-scoped `quiver_database_free_*` and `quiver_element_free_*` patterns |
| Python `@property` for database methods | CLAUDE.md explicitly states "Properties are regular methods, not `@property`". This is a design decision, not a gap. | Maintain method-based API |
| Replacing CFFI ABI mode with API mode | API mode requires a C compiler at install time. ABI mode is simpler for distribution. The hand-maintained `_c_api.py` is a known tradeoff. | Keep ABI mode; improve sync tooling if needed |
| Exposing configurable `DatabaseOptions` in Python binding | Out of scope for v0.5. Options are defaulted internally. | Defer to future milestone |
| `ffi.gc()` for Python LuaRunner cleanup | Cannot check error codes from destructor; inconsistent with Database pattern which uses explicit `close()`. | Use explicit `close()` + context manager |
| Backwards-compatible shims for renamed C API functions | Project is WIP; CLAUDE.md states "breaking changes acceptable, no backwards compatibility required". | Make clean renames, regenerate all FFI bindings |

## Feature Dependencies

```
DataType IntEnum constants --> ScalarMetadata.data_type typed field
DataType IntEnum constants --> Replace magic numbers in database.py convenience helpers
DataType IntEnum constants --> Replace magic numbers in _marshal_params
DataType IntEnum constants --> Replace magic numbers in _marshal_time_series_columns

Fix DatabaseOptions layer inversion
  --> New C++ LogLevel enum class in include/quiver/options.h
  --> New C++ DatabaseOptions struct (no longer a typedef of C API type)
  --> C API converter: quiver_database_options_t <-> quiver::DatabaseOptions
  --> Update all C API factory functions (database.cpp)
  --> No binding-level changes (C API struct layout stays the same)

quiver_database_free_string addition --> Update Julia/Dart/Python bindings to use new function
quiver_database_free_string addition --> Regenerate Julia/Dart FFI bindings
quiver_database_free_string addition --> Keep quiver_element_free_string for element.h usage only

Python LuaRunner binding --> CFFI cdef additions to _c_api.py (4 function declarations)
Python LuaRunner binding --> lua_runner.py wrapper class
Python LuaRunner binding --> Python LuaRunner tests
```

## Detailed Analysis

### 1. Free Function Naming: Entity-Scoped Convention

**Confidence: HIGH** (verified against multiple C library conventions)

The core problem: `quiver_element_free_string` lives in `element.h`/`element.cpp` but is called from 7 locations across bindings to free strings returned by database operations (`query_string`, `query_string_params`, `read_scalar_string_by_id`). Callers of database operations must reach into the element module to free results.

**How mature C libraries handle deallocation naming:**

| Library | Pattern | Principle |
|---------|---------|-----------|
| **libgit2** | `git_{entity}_free()` per opaque type | "Do not mix allocators. Use the free functions provided for each object type." Entity-scoped: `git_repository_free()`, `git_commit_free()` |
| **SQLite** | `sqlite3_free()` for all `sqlite3_malloc` memory; `sqlite3_finalize()` for statements | Unified allocator enables single free. Type-specific cleanup for complex types. |
| **libcurl** | `curl_{module}_cleanup()` per handle type | Module-scoped: `curl_easy_cleanup()`, `curl_url_cleanup()` |
| **SDL2** | `SDL_Destroy{Resource}()` for handles; `SDL_free()` for raw allocations | Verb differentiates: "Destroy" for complex resources, "free" for raw memory |

**The pattern that fits Quiver:** Entity-scoped free functions where the entity matches the **allocating module**. Quiver already follows this for arrays:
- `quiver_database_free_integer_array` -- allocated by `quiver_database_read_scalar_integers`
- `quiver_database_free_string_array` -- allocated by `quiver_database_read_scalar_strings`
- `quiver_database_free_integer_vectors` -- allocated by `quiver_database_read_vector_integers`

The fix is making string deallocation consistent: `quiver_database_free_string` for strings allocated by database operations.

**Recommended approach:** Add `quiver_database_free_string(char* str)` in `database_read.cpp` (co-located with other database free functions per the alloc/free co-location convention). It internally does `delete[] str` -- identical to `quiver_element_free_string`. Keep `quiver_element_free_string` for `quiver_element_to_string` only.

**Cross-binding update scope:**

| Binding | Files to update | Call sites |
|---------|----------------|------------|
| Julia | `database_read.jl`, `database_query.jl` | 3 calls |
| Dart | `database_read.dart`, `database_query.dart` | 3 calls |
| Python | `database.py` | 2 calls |
| Lua (internal) | None -- Lua uses C++ directly via sol2 | 0 calls |
| C API generated bindings | Julia `c_api.jl`, Dart `bindings.dart` | Regenerate |

### 2. Python Data Type Constants

**Confidence: HIGH** (verified against Python stdlib and CFFI documentation)

The current state: `database.py` contains 20+ instances of bare integer comparisons:
```python
if attr.data_type == 0:  # INTEGER
    ...
elif attr.data_type == 1:  # FLOAT
    ...
elif attr.data_type == 3:  # DATE_TIME
    ...
else:  # STRING (2)
```

And parameter marshaling with magic numbers:
```python
c_types[i] = 4  # QUIVER_DATA_TYPE_NULL
c_types[i] = 0  # QUIVER_DATA_TYPE_INTEGER
c_types[i] = 1  # QUIVER_DATA_TYPE_FLOAT
c_types[i] = 2  # QUIVER_DATA_TYPE_STRING
```

**Why `IntEnum` is the right pattern:**

1. **Drop-in compatible with integers** -- `DataType.INTEGER == 0` is `True`. All existing comparison logic works unchanged.
2. **IDE autocompletion** -- callers see `DataType.INTEGER` in completions.
3. **Readable repr** -- `DataType(0)` displays `<DataType.INTEGER: 0>`, aiding debugging.
4. **No runtime dependency on C library handle** -- unlike `lib.QUIVER_DATA_TYPE_INTEGER`, `IntEnum` constants are available at import time without loading the DLL.
5. **Python stdlib-endorsed** -- the `enum` docs explicitly state IntEnum is for "interoperability with other systems" and "when integer constants are replaced with enumerations."

**Why not CFFI `lib.*` enum access:** CFFI ABI mode does expose enum constants via the lib handle, but Quiver lazy-loads the library via `get_lib()`. Accessing constants before any database operation would force premature DLL loading, which is undesirable.

**Recommended implementation:**

```python
# quiverdb/constants.py
from enum import IntEnum

class DataType(IntEnum):
    """Quiver data types, mirroring quiver_data_type_t from the C API."""
    INTEGER = 0
    FLOAT = 1
    STRING = 2
    DATE_TIME = 3
    NULL = 4
```

**Scope of changes:** ~25 bare integer occurrences in `database.py` across 7 functions: `read_scalars_by_id`, `read_vectors_by_id`, `read_sets_by_id`, `read_vector_group_by_id`, `read_set_group_by_id`, `_marshal_params`, `_marshal_time_series_columns`. Plus the `ScalarMetadata.data_type` type annotation upgrade from `int` to `DataType`.

### 3. Python LuaRunner Binding

**Confidence: HIGH** (C API well-defined; Dart binding serves as reference implementation)

The C API surface for LuaRunner is minimal (4 functions in `include/quiver/c/lua_runner.h`):

```c
quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner);
quiver_error_t quiver_lua_runner_free(quiver_lua_runner_t* runner);
quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script);
quiver_error_t quiver_lua_runner_get_error(quiver_lua_runner_t* runner, const char** out_error);
```

**Implementation requirements:**

1. **CFFI cdef additions** to `_c_api.py`: Declare the opaque type and 4 functions.
2. **`lua_runner.py` module**: Wrap the opaque handle. Follow existing `Database` class patterns (explicit `close()`, `__enter__`/`__exit__`, `_ensure_open()` guard).
3. **Lifetime management**: `LuaRunner` borrows the `Database` handle. The Python wrapper must hold a reference to the `Database` object to prevent GC from collecting it while the runner is alive.
4. **Error retrieval**: `quiver_lua_runner_get_error` returns a pointer valid only until the next `run()` call. The Python wrapper must decode it immediately into a Python string.
5. **Export from `__init__.py`**: Add `LuaRunner` to the package's public API.

**Complexity drivers:**
- Lifetime coupling (Database must outlive LuaRunner) -- solved by storing `self._db = db` reference
- Error pointer semantics (valid until next `run()`) -- solved by immediate `ffi.string()` decode
- Testing requires Lua-enabled C library build -- already the default

**Expected Python API:**
```python
with LuaRunner(db) as lua:
    lua.run('db:create_element("Items", { label = "Test", value = 42 })')
    error = lua.last_error()  # or None
```

### 4. C++ Options Type Ownership (Layer Inversion Fix)

**Confidence: HIGH** (architectural issue clearly visible in code)

Current state in `include/quiver/options.h`:
```cpp
#include "quiver/c/options.h"  // C++ header imports C API header!
using DatabaseOptions = quiver_database_options_t;  // C++ type IS the C type
```

The C++ core library depends on the C API layer for its own type definitions. The correct flow: C++ defines types --> C API defines C-compatible equivalents --> converter functions bridge them.

**What needs to change:**

1. **C++ `LogLevel` enum class** in `include/quiver/options.h`:
   ```cpp
   enum class LogLevel { Debug, Info, Warn, Error, Off };
   ```

2. **C++ `DatabaseOptions` struct** with member initializers:
   ```cpp
   struct DatabaseOptions {
       bool read_only = false;
       LogLevel console_level = LogLevel::Info;
   };
   ```

3. **Remove** the `#include "quiver/c/options.h"` from the C++ options header.

4. **C API conversion** in `src/c/database.cpp` or a new `database_options.h` helper:
   ```cpp
   quiver::DatabaseOptions convert_csv_options(const quiver_database_options_t* opts);
   ```

5. **C API factory functions** (`quiver_database_open`, `quiver_database_from_schema`, `quiver_database_from_migrations`) convert the C struct to C++ type before passing to the Database constructor.

**Impact radius:** The C API struct layout does not change (`quiver_database_options_t` keeps its fields), so no binding-level changes are needed. Only the C++ internal representation changes.

### 5. Cross-Binding Test Coverage

**Confidence: HIGH** (direct code inspection)

| Binding | `is_healthy()` tested | `path()` tested | Action needed |
|---------|----------------------|-----------------|---------------|
| Python | Yes | Yes | None |
| Julia | No dedicated test | No dedicated test | Add tests |
| Dart | No dedicated test | No dedicated test | Add tests |
| C++ / C API | Yes (lifecycle tests) | Yes (lifecycle tests) | None |

Tests are trivial (call method, assert return type and value). 2-3 tests per binding, each under 10 lines.

## MVP Recommendation

Prioritize for v0.5 quality milestone:

1. **DataType IntEnum constants** -- Lowest effort (new file + search-replace), highest readability impact. Eliminates 20+ magic numbers. No cross-binding impact.
2. **Fix DatabaseOptions layer inversion** -- Medium effort, fixes architectural principle violation. Must happen before any further options-related work.
3. **Free function naming fix** -- Low effort at C level, moderate effort across bindings due to regeneration. Establishes correct naming for all future additions.
4. **Python LuaRunner binding** -- Medium effort, closes the last cross-language feature gap. Depends on CFFI cdef additions.
5. **Cross-binding `is_healthy`/`path` tests** -- Lowest effort, incremental coverage. Can happen in parallel with any other work.
6. **Python convenience helper test audit** -- Low effort. Review existing tests for completeness and fill gaps.

## Sources

- [libgit2 conventions](https://github.com/libgit2/libgit2/blob/main/docs/conventions.md) -- Entity-scoped free function naming, memory ownership rules
- [SQLite C/C++ Interface](https://sqlite.org/capi3ref.html) -- `sqlite3_free` unified allocator pattern, function family naming
- [libcurl API overview](https://curl.se/libcurl/c/libcurl.html) -- `curl_*_cleanup` module-scoped naming convention
- [Python Enum HOWTO](https://docs.python.org/3/howto/enum.html) -- IntEnum for FFI constant interoperability
- [Python enum module](https://docs.python.org/3/library/enum.html) -- IntEnum design rationale, drop-in integer replacement
- [CFFI documentation: Using the ffi/lib objects](https://cffi.readthedocs.io/en/latest/using.html) -- Enum value access in ABI mode
- [CFFI documentation: Preparing and Distributing modules](https://cffi.readthedocs.io/en/latest/cdef.html) -- Ellipsis pattern for enum declarations
- [Fuchsia C Library Readability Rubric](https://fuchsia.dev/fuchsia-src/development/api/c) -- FFI-friendly C API design, pointer type conventions
- Direct codebase inspection of all relevant source files (see Detailed Analysis sections for specific file references)
