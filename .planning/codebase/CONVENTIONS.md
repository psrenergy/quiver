# Conventions

## Code Style

### C++
- **Standard:** C++20, modern features used where they simplify logic
- **Naming:** `snake_case` for functions/variables, `PascalCase` for types, `UPPER_CASE` for macros
- **Namespaces:** `quiver` (core), `quiver::string` (utilities)
- **Braces:** Allman style (opening brace on new line for functions/classes)
- **Includes:** System headers in `<>`, project headers in `""`, grouped by category

### Method Naming Convention
Public Database methods follow `verb_[category_]type[_by_id]`:
- **Verbs:** `create`, `read`, `update`, `delete`, `get`, `list`, `has`, `query`, `describe`, `export`, `import`
- **`_by_id` suffix:** Only when both all-elements and single-element variants exist
- **Singular vs plural:** Matches return cardinality

### C API
- All functions prefixed `quiver_database_` or `quiver_binary_`
- Binary return codes: `QUIVER_OK (0)` / `QUIVER_ERROR (1)`
- Values via out-parameters
- Matching `_free_*` for every allocation, co-located in same translation unit

## Error Handling

### C++ Layer — Three Patterns Only

1. **Precondition:** `"Cannot {operation}: {reason}"`
2. **Not found:** `"{Entity} not found: {identifier}"`
3. **Operation failure:** `"Failed to {operation}: {reason}"`

No ad-hoc error formats. All `throw std::runtime_error(...)`.

### C API Layer
```cpp
try {
    // C++ call
    return QUIVER_OK;
} catch (const std::exception& e) {
    quiver_set_last_error(e.what());
    return QUIVER_ERROR;
}
```

### Bindings
Bindings never craft error messages. They retrieve from `quiver_get_last_error()` and wrap in language-appropriate exception types.

## Memory Management

### C++ Layer
- RAII throughout
- Pimpl for classes hiding private dependencies (`Database`, `LuaRunner`, `BinaryFile`, `CSVConverter`)
- Rule of Zero for value types (`Element`, `BinaryMetadata`, `Dimension`, etc.)
- Non-copyable, movable for resource types

### C API Layer
- `new`/`delete` for opaque handles
- `quiver::string::new_c_str()` for all string allocations
- Every alloc has matching `_free_*` in same `.cpp` file
- Thread-local error storage via `quiver_set_last_error()`

### Null Checking
- `QUIVER_REQUIRE(a, b, c)` macro — variadic (1-9 args), auto-generates error messages
- C++ core trusts callers (clean code philosophy), C API validates at boundary

## Patterns

### Pimpl
Used only when hiding private dependencies:
```cpp
// Header
class Database {
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Source
struct Database::Impl {
    sqlite3* db;  // hidden from consumers
};
```

### Factory Methods
Static methods returning constructed objects:
```cpp
static Database from_schema(path, schema_path, options);
static Database from_migrations(path, migrations_path, options);
```

### Transaction Guard (RAII, nest-aware)
```cpp
TransactionGuard guard(impl);
// operations...
guard.commit();  // no-op if explicit transaction already active
```

### Element Builder (fluent API)
```cpp
Element().set("label", "Item 1").set("value", 42).set("tags", {"a", "b"})
```

### Columnar Typed Arrays (C API time series)
Parallel arrays: `column_names[]`, `column_types[]`, `column_data[]` with `column_count`, `row_count`.

### Parameterized Queries
Parallel arrays: `param_types[]`, `param_values[]`, `param_count` for type-safe FFI.

## Logging

spdlog with debug/info/warn/error levels:
```cpp
spdlog::debug("Opening database: {}", path);
spdlog::error("Failed to execute query: {}", sqlite3_errmsg(db));
```

## Schema Conventions

- Every schema has a `Configuration` table with `id` + `label`
- Collections: `{Name}` with `id INTEGER PRIMARY KEY AUTOINCREMENT, label TEXT UNIQUE NOT NULL`
- Vectors: `{Collection}_vector_{name}` with composite PK `(id, vector_index)`
- Sets: `{Collection}_set_{name}` with `UNIQUE (id, value)`
- Time series: `{Collection}_time_series_{name}` with dimension column starting with `date_`
- All FKs use `ON DELETE CASCADE ON UPDATE CASCADE`
- All tables use `STRICT` mode

## Cross-Layer Naming

| C++ | C API | Julia | Dart | Python | Lua |
|-----|-------|-------|------|--------|-----|
| `method_name` | `quiver_database_method_name` | `method_name` (+`!` if mutating) | `methodName` | `method_name` | `method_name` |

Transformation is mechanical — no lookup table needed.
