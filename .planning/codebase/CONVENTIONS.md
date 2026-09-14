# Coding Conventions

**Analysis Date:** 2026-09-14

## Naming Patterns

### Cross-Layer Transformation Rules

**All public methods follow `verb_[category_]type[_by_id]` pattern:**

| Aspect | Rule | Example |
|--------|------|---------|
| **C++** | `snake_case` methods | `create_element`, `read_scalar_integers`, `update_element_by_label` |
| **C API** | Prefix `quiver_database_` | `quiver_database_create_element`, `quiver_database_read_scalar_integers` |
| **Julia** | Same `snake_case` + `!` suffix for mutators | `create_element!`, `update_element!`, `delete_element!` |
| **Dart** | Convert to `camelCase` + named constructors | `createElement()`, `readScalarIntegers()`, `Database.fromSchema()` |
| **Python** | Same `snake_case` as C++ + `@staticmethod` for factories | `create_element()`, `read_scalar_integers()`, `Database.from_schema()` |
| **JS** | Convert to `camelCase` | `createElement()`, `readScalarIntegers()`, `Database.fromSchema()` |
| **Lua** | Exact `snake_case` match (1:1 with C++) | `create_element()`, `read_scalar_integers()`, `db:update_element_by_label()` |

**Verb categories:**
- `create` — write operations that produce new rows
- `read` — read all elements (no filter)
- `get` — retrieve metadata or single values
- `list` — enumerate a collection
- `update` / `upsert` — modify existing rows
- `delete` — remove rows
- `describe` / `summarize` — text-format introspection
- `query` — parameterized SQL
- `export` / `import` — CSV I/O

**Type suffixes:**
- Singular when return is single value: `read_scalar_integer_by_id` returns `optional<int64>`
- Plural when return is array/collection: `read_scalar_integers` returns `vector<optional<int64>>`
- `_by_id` only when both id-addressed and bulk-addressed variants exist
- `_by_label` for label-addressed form of an id operation

### Files and Directories

**C++ source files:**
- Header: `include/quiver/` — names match class/namespace scope
- Implementation: `src/` — one `.cpp` per major concern (`database_create.cpp`, `database_read.cpp`, etc.)
- Pattern: `database_*.cpp` for `Database` class methods grouped by concern
- Inline headers: `src/database_impl.h`, `src/database_internal.h` for Pimpl + internal helpers

**Bindings:**
- `bindings/{julia,dart,python,js}/` — one subdir per language
- File naming mirrors C++ but adapted to language convention: `database_read_scalar.jl`, `database_read_scalar_test.dart`, `test_database_read_scalar.py`, `database-read-scalar.test.ts`

### Variables and Functions

**C++:**
- Functions: `snake_case`
- Variables: `snake_case`
- Class members: `snake_case_` (private members end with `_`)
- Class names: `PascalCase`
- Enum values: `PascalCase` (e.g., `LogLevel::Debug`, `DataType::Integer`)
- Namespace names: `snake_case` (or `quiver::` for public)
- Templated functions: Function name is lowercase, template parameter names `PascalCase`

**Example from `database.cpp`:**
```cpp
namespace quiver {
  class Database {
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    void create_database_logger(const std::string& db_path) { ... }
  };
}
```

**Julia, Python, Lua:**
- Functions: `snake_case` (matching C++)
- Variables: `snake_case`
- Classes: `PascalCase` (where applicable)

**Dart, JS:**
- Functions: `camelCase`
- Variables: `camelCase`
- Classes: `PascalCase`

## Code Style

### Formatting

**Tool:** `clang-format` (C/C++), language-specific formatters per binding

**C/C++ settings** (`.clang-format`):
- **Column limit:** 120 characters
- **Indentation:** 4 spaces, never tabs
- **Line ending:** LF only (enforced by `.gitattributes`)
- **Brace style:** Attach (opening brace on same line)
- **Pointer alignment:** Left (`int* ptr`)
- **Access modifiers:** Dedent by 4 spaces
- **Template declarations:** Always break (`AlwaysBreakTemplateDeclarations: Yes`)
- **Function parameters:** Never bin-pack, one per line when multiple

**Per-binding formatters:**
- Julia: `JuliaFormatter` (run via `bindings/julia/format/format.bat`)
- Dart: `dart format` (run via `bindings/dart/format.bat`)
- Python: `ruff format` + `ruff check --fix` (run via `bindings/python/format.bat`)
- JavaScript: `biome check --write` (run via `bindings/js/format.bat`)

**Master formatter command:**
```bash
scripts/format.bat                    # All languages
```

### Linting

**C/C++ linting:** `clang-tidy` (`.clang-tidy`)

**Active checks:**
- `bugprone-*` — bug patterns
- `modernize-*` — C++20 best practices (disabled: `use-trailing-return-type`, `use-ranges`, etc.)
- `performance-*` — efficiency issues
- `readability-identifier-naming` — enforces naming conventions above

**Disabled high-noise checks:**
- `bugprone-easily-swappable-parameters`
- `performance-inefficient-string-concatenation` (strings built intentionally)
- `modernize-use-nodiscard` (too many false positives)

**Run tidy:**
```bash
scripts/tidy.bat                      # Run all checks
```

**Pre-commit hooks** (`.pre-commit-config.yaml`):
- `clang-format` — auto-format C/C++
- `cppcheck` — static analysis
- `trailing-whitespace` — LF only, no CRLF
- `end-of-file-fixer` — single trailing newline
- `mixed-line-ending --fix=lf` — enforce LF

## Import Organization

### C++ Includes

**Order (strictly enforced by `clang-format`):**
1. Local project headers (`#include "database_impl.h"`)
2. Public project headers (`#include <quiver/database.h>`)
3. External library headers (`#include <sqlite3.h>`, `#include <spdlog/spdlog.h>`)
4. Standard library headers (`#include <string>`, `#include <vector>`)

**Within each group, alphabetically sorted by `SortIncludes: true`**

**Example from `src/database.cpp`:**
```cpp
#include "database_impl.h"
#include "quiver/migrations.h"
#include "quiver/result.h"
#include "utils/string.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>
```

### Python Imports

**Order (via `ruff isort`):**
1. Future imports (`from __future__ import annotations`)
2. Standard library
3. Third-party (cffi, typing, etc.)
4. Project-local imports

### Julia Imports

**Convention:** Imports at top of file, organized by origin (stdlib, Quiver C API bindings, local)

### JavaScript/TypeScript Imports

**Convention:** Module imports first, then internal imports, organized alphabetically within groups

## Error Handling

### Error Message Patterns

**Three documented patterns used throughout C++ core** (`src/type_validator.cpp`, `src/database_impl.h`):

**Pattern 1 — Precondition failure:**
```cpp
throw std::runtime_error("Cannot {operation}: {reason}");
```
- Used by: validation failures (type mismatch, schema errors, invalid input)
- Operation: the public method the user called (threaded through validators)
- Example: `"Cannot create_element: element must have at least one scalar attribute"`
- Example: `"Cannot update_element: type mismatch for column 'value': expected REAL, got INTEGER"`

**Pattern 2 — Not found:**
```cpp
throw std::runtime_error("{Entity} not found: {identifier}");
```
- Used by: lookups that miss (element by id/label, attribute in collection, migration version)
- No "Cannot" prefix; just the entity type and identifier
- Example: `"Element not found: <id> in collection '<collection>'"`
- Example: `"Scalar attribute not found: 'value' in collection 'Items'"`
- Example: `"Time series attribute not found: 'data' in group 'readings'"`

**Pattern 3 — Operation failure:**
```cpp
throw std::runtime_error("Failed to {operation}: {reason}");
```
- Used by: system failures (file I/O, database operations, parsing)
- Details come from underlying system (SQLite error message, file error, parse failure)
- Example: `"Failed to open database: unable to open database file"`
- Example: `"Failed to execute statement: " + std::string(sqlite3_errmsg(db))`
- Example: `"Failed to parse initial_datetime: 2024-02-31 (invalid date)"`

**Documented exceptions to "messages come from C++":**

1. **Pre-FFI type marshalling errors** in bindings (when a value never reaches the C API because it's incompatible with the binding's FFI layer) — for example, Python's `_parse_datetime` and `_integer_to_boolean` for datetime and boolean convenience wrappers. Messages must name `collection.attribute`.

2. **Lua validators** for unsupported value types during encoding, group column marshalling (jagged columns), and type conversions — always thread the calling operation name.

3. **Binary/expression subsystem exceptions** predate the three patterns and use descriptive messages like `"Number of labels must be positive, got 0"` (not changed retroactively).

### Throws vs Returns

- **All error conditions throw `std::runtime_error`** (C++) or the binding's native exception type (Python `ValueError` / `QuiverError`, Dart/Java exceptions, JS `QuiverError`)
- **No silent failures** — missing elements are never a no-op; they always throw Pattern 2
- **No null returns for errors** — use `std::optional<T>` only for legitimate NULL values (e.g., a nullable database column read), not for error cases
- **Transactions are atomic** — if a write fails mid-operation, it rolls back automatically via `TransactionGuard` RAII

### Validation

- **Scalar type validation** in `TypeValidator::validate_value()` runs before any writes
- **Array length validation** in group insert helpers runs before the DELETE (so failures don't leave partially cleared groups)
- **DATE_TIME validation** via `datetime::is_valid_iso8601()` runs at write time, not just when reading
- **Foreign key validation** is implicit (SQLite STRICT tables + ON DELETE CASCADE)

## Logging

**Framework:** spdlog (C++) — **per-database logger instance, never global spdlog functions**

**Levels:** `Debug`, `Info`, `Warn`, `Error`, `Off`

**Usage pattern in C++:**
```cpp
impl_->logger->debug("Opening database: {}", path);
impl_->logger->warn("Database is in-memory; no file logging will be performed");
impl_->logger->error("Failed to create file sink: {}", error_msg);
```

**Location:** Each `Database` instance creates a unique logger (`quiver_database_<id>`) with:
- Stderr sink (colored output, respects `console_level` option)
- File sink to `<db_dir>/quiver_database.log` (always debug level, created by `create_database_logger()`)

**Configuration:** Via `DatabaseOptions.console_level` (only affects stderr, not file)

## Comments

### When to Comment

- **Public API headers:** Use docstring comments on all public methods describing inputs/outputs/behavior
- **Complex algorithms:** Explain the "why" not the "what" (code reads the "what")
- **Non-obvious design decisions:** Thread-safety notes, transaction nesting notes, lazy loading rationale
- **Workarounds:** Mark with rationale if code exists to handle a quirk or limitation
- **TODO/FIXME:** Include issue numbers when possible; never leave `FIXME` without context

**Examples:**
```cpp
// Lazy loading: schema metadata is read here, not in the constructor, so the handle
// can be passed through to operations before migrations have run. See design decision.
Impl::require_schema();

// Write registry prevents reading files currently open for writing.
// Not thread-safe or multi-process safe.
static std::unordered_set<std::string> open_write_paths;
```

### JSDoc/Javadoc/Doc Comments

**C++:** Public headers use inline comments, no formal doc format (users read `.md` files)

**Python:** Docstrings on public classes/methods (Google style):
```python
def create_element(self, collection: str, **kwargs) -> int:
    """Create an element in a collection.
    
    Args:
        collection: Collection name
        **kwargs: Element attributes
    
    Returns:
        Element id
        
    Raises:
        QuiverError: If validation fails
    """
```

**Dart/Julia/JS:** Follow language conventions (dartdoc, docstrings, JSDoc)

## Function Design

### Size Guidelines

- **Prefer functions < 30 lines** (except I/O-heavy operations like `export_csv`)
- **Extract complex validation** into named helper functions (e.g., `TypeValidator::validate_value`)
- **Use Pimpl only when hiding private dependencies** (e.g., Database hides sqlite3, BinaryFile hides file I/O; Element, Row, Expression do NOT use Pimpl)

### Parameters

- **Grouped logically:** Positional params for core inputs, options last
- **Name threading:** Calling operation name passed to validators (produces accurate "Cannot X" messages)
- **Const correctness:** Public API promises const where possible; `const` methods can trigger lazy loads (schema, metadata) via `mutable` members

### Return Values

- **Use `std::optional<T>`** for NULL values coming from the database (scalar nullable reads)
- **Use `std::vector<T>`** for collections (always; never raw pointers)
- **Use `std::string`** for text results (`describe()`, `describe_collection()` return text reports)
- **Use value types** for simple data (GroupMetadata, ScalarMetadata, Element — all Rule of Zero)

## Module Design

### Exports

**C++ public API:**
- All `QUIVER_API` decorated classes/functions are exported from `include/quiver/`
- No implementation details leak into headers (Pimpl used for complex internals)

**Binding exports:**
- Julia: Top-level module exports only public API (Database, LuaRunner, metadata types)
- Dart: Export from `lib/quiverdb.dart` 
- Python: Export from `src/quiverdb/__init__.py`
- JS: Export from `src/index.ts`

### Barrel Files

**C++:** `include/quiver/quiver.h` umbrella header includes all public types

**Julia:** Module `Quiver` in `bindings/julia/src/Quiver.jl` re-exports C API bindings

**JS:** `src/index.ts` re-exports from submodules (`./database.ts`, `./errors.ts`, etc.)

**Python:** `src/quiverdb/__init__.py` re-exports Database, QuiverError, metadata types

### Scope of Internal Headers

- `database_impl.h` — Pimpl definition, shared internal helpers (label resolution, group inserts)
- `database_internal.h` — Template read helpers, type validators shared across methods
- `expression_helpers.h` — Aggregation accumulators, broadcast metadata builders shared by multiple expression nodes
- Binary/expression subsystem: thin public headers wrapping Pimpl or value types

## Database Schema Conventions

### Required Configuration Table

Every schema must define a Configuration table:
```sql
CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;
```

### Collection Tables

```sql
CREATE TABLE {CollectionName} (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    -- scalar attributes here
) STRICT;
```

### Vector and Set Group Tables

Named `{Collection}_{vector,set}_{name}`:
```sql
CREATE TABLE Items_vector_values (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    value REAL NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;

CREATE TABLE Items_set_tags (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    value TEXT NOT NULL,
    UNIQUE (id, value)
) STRICT;
```

### Time Series Group Tables

Named `{Collection}_time_series_{name}`, with a dimension column starting with `date_`:
```sql
CREATE TABLE Items_time_series_data (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    value REAL NOT NULL,
    PRIMARY KEY (id, date_time)
) STRICT;
```

All foreign keys use `ON DELETE CASCADE ON UPDATE CASCADE`.

---

*Convention analysis: 2026-09-14*
