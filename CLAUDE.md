# Project: PSR Database

SQLite wrapper library with C++ core, C API for FFI, and language bindings (Julia, Dart).

## Architecture

```
include/psr/           # C++ public headers
  database.h           # Database class - main API
  element.h            # Element builder for create operations
  lua_runner.h         # Lua scripting support
include/psr/c/         # C API headers (for FFI)
  database.h
  element.h
  lua_runner.h
src/                   # Implementation
bindings/julia/        # Julia bindings (PSRDatabase.jl)
bindings/dart/         # Dart bindings (psr_database)
tests/                 # C++ tests and shared SQL schemas
```

## Principles

- WIP project - no backwards compatibility required
- Be critical, not passive
- Breaking changes are acceptable
- Clean code over defensive code
- Simple solutions over complex abstractions
- Delete unused code, do not deprecate
- Intelligence in C++ layer, bindings are thin wrappers
- All public C++ methods should be binded to C API, then to Julia/Dart/Lua
- All *.sql test schemas in `tests/schemas/`, bindings reference from there

## Build

```bash
cmake --build build --config Debug
```

## Build and Test All

```bash
build-all.bat            # Build everything + run all tests (Debug)
build-all.bat --release  # Build in Release mode
test-all.bat             # Run all tests (assumes already built)
```

## Tests

C++ tests:
```bash
./build/bin/psr_database_tests.exe      # Core library tests
./build/bin/psr_database_c_tests.exe    # C API tests
```

Test files are organized by functionality:
- `test_database_lifecycle.cpp` - open, close, move semantics, options
- `test_database_create.cpp` - create element operations
- `test_database_read.cpp` - read scalar/vector/set operations
- `test_database_update.cpp` - update scalar/vector/set operations
- `test_database_delete.cpp` - delete element operations
- `test_database_relations.cpp` - relation operations

C API tests follow same pattern with `test_c_api_database_*.cpp` prefix.

Julia tests:
```bash
bindings/julia/test/test.bat
```

Dart tests:
```bash
bindings/dart/test/test.bat
```

## Core API

### Database Class
- Factory methods: `from_schema()`, `from_migrations()`
- CRUD: `create_element(collection, element)`
- Scalar readers: `read_scalar_integers/floats/strings(collection, attribute)`
- Vector readers: `read_vector_integers/floats/strings(collection, attribute)`
- Set readers: `read_set_integers/floats/strings(collection, attribute)`
- Relations: `set_scalar_relation()`, `read_scalar_relation()`

### Element Class
Builder for element creation with fluent API:
```cpp
Element().set("label", "Item 1").set("value", 42).set("tags", {"a", "b"})
```

### LuaRunner Class
Executes Lua scripts with database access:
```cpp
LuaRunner lua(db);
lua.run(R"(
    db:create_element("Collection", { label = "Item", value = 42 })
    local values = db:read_scalar_integers("Collection", "value")
)");
```

Available Lua methods:
- `db:create_element(collection, table)`
- `db:read_scalar_strings/integers/floats(collection, attribute)`
- `db:read_vector_strings/integers/floats(collection, attribute)`

## Bindings

### Regenerating FFI Bindings

When C API changes, regenerate:
```bash
bindings/julia/generator/generator.bat   # Julia
bindings/dart/generator/generator.bat    # Dart
```

### Julia Notes
- Delete `bindings/julia/Manifest.toml` if version conflicts, then:
  ```bash
  julia --project=bindings/julia -e "using Pkg; Pkg.instantiate()"
  ```

### Dart Notes
- `libpsr_database_c.dll` depends on `libpsr_database.dll` - both must be in PATH
- test.bat handles PATH setup automatically

## Schema Conventions

Every schema must have a Configuration table:
```sql
CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;
```

Collections follow the pattern:
```sql
CREATE TABLE MyCollection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    -- scalar attributes
) STRICT;
```

Vector tables: `{Collection}_vector_{name}` with `(id, vector_index)` PK
Set tables: `{Collection}_set_{name}` with UNIQUE constraint

## Patterns

- Pimpl for ABI stability
- Factory methods for database creation
- RAII for resource management
- Move semantics, no copying for Database/LuaRunner
- Fluent API for Element builder
