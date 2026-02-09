# Phase 2: C++ Core File Decomposition - Research

**Researched:** 2026-02-09
**Domain:** C++ translation unit decomposition / Pimpl pattern multi-file split
**Confidence:** HIGH

## Summary

Phase 2 decomposes `src/database.cpp` (1836 lines) into focused modules by operation type. The foundation is already in place: Phase 1 extracted `Database::Impl` into `src/database_impl.h`, which means any new `.cpp` file in `src/` can `#include "database_impl.h"` and implement `Database::` member functions with full access to `impl_->db`, `impl_->schema`, `impl_->logger`, and `impl_->type_validator`.

The C++ standard explicitly allows member function definitions to be split across multiple translation units, so this is a well-understood, zero-risk refactoring pattern. The only technical concerns are: (1) ensuring the anonymous namespace helpers and file-static functions used by multiple operation categories are properly shared, (2) updating `src/CMakeLists.txt` to register new source files, and (3) keeping private method declarations in the public header accessible from all split files (which they already are via `database_impl.h` including `quiver/database.h`).

**Primary recommendation:** Split `database.cpp` into 9 files by operation category, extract shared anonymous-namespace helpers into `database_internal.h`, update CMakeLists.txt, verify all 385 C++ tests pass after each split step.

## Standard Stack

### Core

No new libraries or dependencies are introduced. This is purely a file reorganization of existing C++ code.

| Component | Current | Purpose | Impact of Phase 2 |
|-----------|---------|---------|-------------------|
| CMake 3.21+ | Already configured | Build system | Must update `QUIVER_SOURCES` list in `src/CMakeLists.txt` |
| C++20 | Already configured | Language standard | No change |
| spdlog | Already linked | Logging | Some split files need spdlog headers for sink creation |
| SQLite3 | Already linked | Database backend | All split files access via `impl_->db` from `database_impl.h` |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Multiple `.cpp` files | Single file with `#pragma region` | Regions don't enable separate compilation; doesn't achieve the goal |
| Shared internal header for helpers | Duplicate helpers in each file | Duplication violates DRY, risks divergence |
| Keep private methods in impl | Move them to free functions | Would change the class interface; out of scope |

## Architecture Patterns

### Recommended File Structure After Decomposition

```
src/
  database_impl.h              # Database::Impl struct (Phase 1, exists)
  database_internal.h          # NEW: shared anonymous-namespace helpers and file-static functions
  database.cpp                 # Lifecycle only: constructor, destructor, move, factory methods, is_healthy, path, execute, execute_raw, transaction delegates, schema/migration application
  database_create.cpp          # create_element
  database_read.cpp            # read_scalar_*, read_vector_*, read_set_*, read_element_ids
  database_update.cpp          # update_element, update_scalar_*, update_vector_*, update_set_*
  database_delete.cpp          # delete_element_by_id
  database_metadata.cpp        # get_scalar_metadata, get_vector_metadata, get_set_metadata, list_scalar_attributes, list_vector_groups, list_set_groups + scalar_metadata_from_column helper
  database_time_series.cpp     # Time series: get_time_series_metadata, list_time_series_groups, read_time_series_group_by_id, update_time_series_group, has_time_series_files, list_time_series_files_columns, read_time_series_files, update_time_series_files
  database_query.cpp           # query_string, query_integer, query_float
  database_relations.cpp       # set_scalar_relation, read_scalar_relation
  database_describe.cpp        # describe, export_to_csv, import_from_csv
```

### Pattern 1: Multi-File Class Implementation

**What:** C++ allows a class's member function definitions to be distributed across multiple translation units, provided each TU includes the class definition.

**When to use:** When a single implementation file grows beyond ~500 lines and functions group naturally by domain.

**Key constraint:** Each `.cpp` file must include `database_impl.h` (which transitively includes `quiver/database.h`), giving it access to both the public class definition and the private Impl struct.

**Example:**
```cpp
// database_read.cpp
#include "database_impl.h"

namespace quiver {

std::vector<int64_t> Database::read_scalar_integers(const std::string& collection, const std::string& attribute) {
    impl_->require_collection(collection, "read scalar");
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);
    // ...
}

}  // namespace quiver
```

### Pattern 2: Shared Internal Helpers via Header

**What:** Anonymous-namespace template functions and file-static helpers that are used by multiple split files must be extracted to a shared internal header.

**When to use:** When helper functions in the anonymous namespace of the original monolithic file are needed by 2+ split files.

**Current anonymous-namespace helpers in database.cpp:**
1. `get_row_value(Row&, size_t, int64_t*)` -- used by read operations (template overloads for int64_t, double, string)
2. `read_grouped_values_all<T>(Result&)` -- used by read_vector_* and read_set_*
3. `read_grouped_values_by_id<T>(Result&)` -- used by read_vector_*_by_id, read_set_*_by_id, read_element_ids
4. `find_dimension_column(TableDefinition&)` -- used by time series read/update and metadata
5. `ensure_sqlite3_initialized()` -- used only by constructor (stays in database.cpp)
6. `to_spdlog_level(quiver_log_level_t)` -- used only by constructor (stays in database.cpp)
7. `create_database_logger(string&, quiver_log_level_t)` -- used only by constructor (stays in database.cpp)

**Current quiver-namespace file-static helpers:**
1. `scalar_metadata_from_column(ColumnDefinition&)` -- used by metadata operations and describe

**Decision matrix for each helper:**

| Helper | Used By | Action |
|--------|---------|--------|
| `get_row_value` (3 overloads) | read | Move to `database_internal.h` |
| `read_grouped_values_all<T>` | read | Move to `database_internal.h` |
| `read_grouped_values_by_id<T>` | read | Move to `database_internal.h` |
| `find_dimension_column` | time_series, metadata | Move to `database_internal.h` |
| `ensure_sqlite3_initialized` | lifecycle only | Keep in `database.cpp` |
| `to_spdlog_level` | lifecycle only | Keep in `database.cpp` |
| `create_database_logger` | lifecycle only | Keep in `database.cpp` |
| `scalar_metadata_from_column` | metadata, describe | Move to `database_internal.h` |

**Example:**
```cpp
// database_internal.h
#ifndef QUIVER_DATABASE_INTERNAL_H
#define QUIVER_DATABASE_INTERNAL_H

#include "quiver/result.h"
#include "quiver/attribute_metadata.h"
#include "quiver/schema.h"

namespace quiver {
namespace internal {

// Row value extractors
inline std::optional<int64_t> get_row_value(const Row& row, size_t index, int64_t*) {
    return row.get_integer(index);
}
// ... more overloads

template <typename T>
std::vector<std::vector<T>> read_grouped_values_all(const Result& result) { ... }

template <typename T>
std::vector<T> read_grouped_values_by_id(const Result& result) { ... }

inline std::string find_dimension_column(const TableDefinition& table_def) { ... }

inline ScalarMetadata scalar_metadata_from_column(const ColumnDefinition& col) { ... }

}  // namespace internal
}  // namespace quiver

#endif
```

**Note:** Functions moved to the internal header should use `inline` (since they are in a header included by multiple TUs) or be placed in a `namespace quiver::internal` to avoid ODR violations. Template functions are inherently safe. Non-template functions need `inline`.

### Pattern 3: Private Method Accessibility

**What:** The `Database` class declares private methods (`execute`, `execute_raw`, `begin_transaction`, `commit`, `rollback`, `set_version`, `migrate_up`, `apply_schema`) in `include/quiver/database.h`. These are accessible from any `.cpp` file that includes the header.

**Key insight:** Since `database_impl.h` already includes `quiver/database.h`, all split files automatically have access to these private methods. No changes to the public header are required.

**Methods that call private methods:**

| Private Method | Called By |
|----------------|-----------|
| `execute(sql, params)` | create, read, update, delete, time_series, query, relations, describe |
| `execute_raw(sql)` | lifecycle (migrate_up, apply_schema) only |
| `begin_transaction()` | lifecycle (migrate_up, apply_schema) only |
| `commit()` | lifecycle only |
| `rollback()` | lifecycle only |
| `set_version(version)` | lifecycle only |
| `migrate_up(path)` | lifecycle only |
| `apply_schema(path)` | lifecycle only |

**Critical observation:** `execute()` is the most widely used private method -- called from almost every split file. Since it is declared in the public header as `private:`, it is accessible to any implementation file of the Database class. This is standard C++ -- private access control governs usage by external code, not by member function implementations.

### Anti-Patterns to Avoid

- **Circular includes between split files:** Split `.cpp` files should NEVER include each other. They all include `database_impl.h` and optionally `database_internal.h`, nothing else from `src/`.
- **Duplicating helpers:** Do not copy-paste `read_grouped_values_all` or `find_dimension_column` into multiple files. Extract once to the shared header.
- **Changing function signatures:** The decomposition is purely a file-level reorganization. No method signature, return type, or behavior should change.
- **Moving private method implementations to Impl:** Private methods like `execute()` are part of `Database`, not `Database::Impl`. Moving them would require changing the public header, violating success criterion 5.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| ODR violations for shared helpers | Separate .cpp with shared helpers | `inline` functions in internal header | Inline avoids linker errors from multiple definitions; templates are inherently ODR-safe |
| CMake source file list | Manual glob or wildcard | Explicit file list in `QUIVER_SOURCES` | CMake best practice: explicit is better than implicit; avoids stale-build issues |

## Common Pitfalls

### Pitfall 1: ODR Violations from Non-Inline Helpers in Header

**What goes wrong:** Non-template, non-inline function defined in a header included by multiple `.cpp` files causes "multiple definition" linker errors.
**Why it happens:** Each translation unit gets its own copy of the function. Without `inline`, the linker sees duplicates.
**How to avoid:** Mark all non-template functions in `database_internal.h` as `inline`. Template functions are inherently safe.
**Warning signs:** Linker errors mentioning "multiple definition of `quiver::internal::find_dimension_column`" or similar.

### Pitfall 2: Forgetting to Update CMakeLists.txt

**What goes wrong:** New `.cpp` file exists but is not added to `QUIVER_SOURCES` in `src/CMakeLists.txt`. The functions defined in the new file have no object code, causing "undefined reference" linker errors.
**Why it happens:** CMake uses explicit source lists. New files are not auto-discovered.
**How to avoid:** Add every new `.cpp` file to the `QUIVER_SOURCES` list immediately when creating it.
**Warning signs:** Linker errors about undefined `Database::read_scalar_integers` etc., even though the code compiles fine.

### Pitfall 3: Anonymous Namespace vs Named Namespace for Shared Code

**What goes wrong:** Helpers put in anonymous namespace in the internal header are duplicated per TU (each file gets its own copy at link time). This wastes binary size and defeats the purpose of sharing.
**Why it happens:** Anonymous namespaces create internal linkage -- the opposite of what we want for shared code.
**How to avoid:** Use a named namespace (e.g., `quiver::internal`) with `inline` for the shared helper header. Only use anonymous namespace for truly file-local helpers that are specific to a single `.cpp` file.
**Warning signs:** Binary size increases, or subtle bugs if a helper in anonymous namespace has state.

### Pitfall 4: Include Order Breaking Compilation

**What goes wrong:** A split file includes `database_internal.h` but not `database_impl.h`, losing access to `impl_->` members.
**Why it happens:** Developer assumes one header is enough.
**How to avoid:** Every split file that implements `Database::` methods MUST include `database_impl.h` (for Impl access) as the first include. `database_internal.h` is additional, for shared helpers.
**Warning signs:** Compiler errors about `impl_` being incomplete type or unknown member.

### Pitfall 5: Splitting Mid-Transaction Logic Across Files

**What goes wrong:** A method uses `Impl::TransactionGuard` but the split file doesn't have the right includes for it.
**Why it happens:** `TransactionGuard` is nested inside `Database::Impl`, defined in `database_impl.h`.
**How to avoid:** All files that use `Impl::TransactionGuard` include `database_impl.h` (which they should already do).
**Warning signs:** Compiler error about `TransactionGuard` not being declared.

## Code Examples

### Example 1: Split File Structure (database_read.cpp)

```cpp
// src/database_read.cpp
#include "database_impl.h"
#include "database_internal.h"

namespace quiver {

std::vector<int64_t> Database::read_scalar_integers(const std::string& collection, const std::string& attribute) {
    impl_->require_collection(collection, "read scalar");
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);

    std::vector<int64_t> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_integer(0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

// ... all other read_scalar_*, read_vector_*, read_set_*, read_element_ids methods

}  // namespace quiver
```

### Example 2: Internal Helpers Header (database_internal.h)

```cpp
// src/database_internal.h
#ifndef QUIVER_DATABASE_INTERNAL_H
#define QUIVER_DATABASE_INTERNAL_H

#include "quiver/attribute_metadata.h"
#include "quiver/result.h"
#include "quiver/schema.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace quiver::internal {

// Type-specific Row value extractors
inline std::optional<int64_t> get_row_value(const Row& row, size_t index, int64_t*) {
    return row.get_integer(index);
}

inline std::optional<double> get_row_value(const Row& row, size_t index, double*) {
    return row.get_float(index);
}

inline std::optional<std::string> get_row_value(const Row& row, size_t index, std::string*) {
    return row.get_string(index);
}

// Template for reading grouped values (vectors or sets) for all elements
template <typename T>
std::vector<std::vector<T>> read_grouped_values_all(const Result& result) {
    std::vector<std::vector<T>> groups;
    int64_t current_id = -1;

    for (size_t i = 0; i < result.row_count(); ++i) {
        auto id = result[i].get_integer(0);
        auto val = get_row_value(result[i], 1, static_cast<T*>(nullptr));

        if (!id)
            continue;

        if (*id != current_id) {
            groups.emplace_back();
            current_id = *id;
        }

        if (val) {
            groups.back().push_back(*val);
        }
    }
    return groups;
}

// Template for reading grouped values for a single element by ID
template <typename T>
std::vector<T> read_grouped_values_by_id(const Result& result) {
    std::vector<T> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = get_row_value(result[i], 0, static_cast<T*>(nullptr));
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

// Find the dimension (ordering) column in a time series table
inline std::string find_dimension_column(const TableDefinition& table_def) {
    for (const auto& [col_name, col] : table_def.columns) {
        if (col_name == "id")
            continue;
        if (col.type == DataType::DateTime || is_date_time_column(col_name)) {
            return col_name;
        }
    }
    throw std::runtime_error("No dimension column found in time series table");
}

// Convert a column definition to ScalarMetadata
inline ScalarMetadata scalar_metadata_from_column(const ColumnDefinition& col) {
    return {col.name, col.type, col.not_null, col.primary_key, col.default_value};
}

}  // namespace quiver::internal

#endif  // QUIVER_DATABASE_INTERNAL_H
```

### Example 3: Updated CMakeLists.txt

```cmake
set(QUIVER_SOURCES
    database.cpp
    database_create.cpp
    database_read.cpp
    database_update.cpp
    database_delete.cpp
    database_metadata.cpp
    database_time_series.cpp
    database_query.cpp
    database_relations.cpp
    database_describe.cpp
    element.cpp
    lua_runner.cpp
    migration.cpp
    migrations.cpp
    result.cpp
    row.cpp
    schema.cpp
    schema_validator.cpp
    type_validator.cpp
)
```

## Detailed Method-to-File Mapping

### database.cpp (lifecycle) -- Target: ~430 lines

**Anonymous namespace (stays here):**
- `g_logger_counter` (line 20)
- `sqlite3_init_flag` (line 21)
- `ensure_sqlite3_initialized()` (lines 75-77)
- `to_spdlog_level()` (lines 79-94)
- `create_database_logger()` (lines 96-141)

**Database methods (stays here):**
- `Database::Database()` constructor (lines 162-188)
- `Database::~Database()` (line 190)
- `Database::Database(Database&&)` move constructor (line 192)
- `Database::operator=(Database&&)` move assignment (line 193)
- `Database::is_healthy()` (lines 195-197)
- `Database::execute()` (lines 199-275)
- `Database::current_version()` (lines 277-296)
- `Database::path()` (lines 298-300)
- `Database::from_migrations()` (lines 302-315)
- `Database::from_schema()` (lines 317-326)
- `Database::set_version()` (lines 328-338)
- `Database::begin_transaction()` (lines 340-342)
- `Database::commit()` (lines 343-345)
- `Database::rollback()` (lines 346-348)
- `Database::execute_raw()` (lines 350-358)
- `Database::migrate_up()` (lines 360-404)
- `Database::apply_schema()` (lines 406-434)

### database_create.cpp -- ~207 lines

- `Database::create_element()` (lines 436-642)

### database_read.cpp -- ~185 lines

Uses: `read_grouped_values_all<T>`, `read_grouped_values_by_id<T>` from internal header.

- `Database::read_scalar_integers()` (lines 890-904)
- `Database::read_scalar_floats()` (lines 906-920)
- `Database::read_scalar_strings()` (lines 922-936)
- `Database::read_scalar_integer_by_id()` (lines 938-948)
- `Database::read_scalar_float_by_id()` (lines 950-960)
- `Database::read_scalar_string_by_id()` (lines 962-972)
- `Database::read_vector_integers()` (lines 974-980)
- `Database::read_vector_floats()` (lines 982-988)
- `Database::read_vector_strings()` (lines 990-996)
- `Database::read_vector_integers_by_id()` (lines 998-1004)
- `Database::read_vector_floats_by_id()` (lines 1006-1012)
- `Database::read_vector_strings_by_id()` (lines 1014-1020)
- `Database::read_set_integers()` (lines 1022-1028)
- `Database::read_set_floats()` (lines 1030-1036)
- `Database::read_set_strings()` (lines 1038-1044)
- `Database::read_set_integers_by_id()` (lines 1046-1052)
- `Database::read_set_floats_by_id()` (lines 1054-1060)
- `Database::read_set_strings_by_id()` (lines 1062-1068)
- `Database::read_element_ids()` (lines 1070-1074)

### database_update.cpp -- ~260 lines

- `Database::update_element()` (lines 644-796)
- `Database::update_scalar_integer()` (lines 1076-1088)
- `Database::update_scalar_float()` (lines 1090-1102)
- `Database::update_scalar_string()` (lines 1104-1116)
- `Database::update_vector_integers()` (lines 1118-1140)
- `Database::update_vector_floats()` (lines 1142-1164)
- `Database::update_vector_strings()` (lines 1166-1188)
- `Database::update_set_integers()` (lines 1190-1211)
- `Database::update_set_floats()` (lines 1213-1234)
- `Database::update_set_strings()` (lines 1236-1257)

### database_delete.cpp -- ~12 lines

- `Database::delete_element_by_id()` (lines 798-806)

### database_metadata.cpp -- ~130 lines

Uses: `scalar_metadata_from_column` from internal header.

- `Database::get_scalar_metadata()` (lines 1259-1286)
- `Database::get_vector_metadata()` (lines 1288-1314)
- `Database::get_set_metadata()` (lines 1316-1342)
- `Database::list_scalar_attributes()` (lines 1344-1370)
- `Database::list_vector_groups()` (lines 1372-1393)
- `Database::list_set_groups()` (lines 1395-1416)

### database_time_series.cpp -- ~210 lines

Uses: `find_dimension_column`, `scalar_metadata_from_column` from internal header.

- `Database::list_time_series_groups()` (lines 1418-1439)
- `Database::get_time_series_metadata()` (lines 1441-1469)
- `Database::read_time_series_group_by_id()` (lines 1471-1528)
- `Database::update_time_series_group()` (lines 1530-1601)
- `Database::has_time_series_files()` (lines 1603-1607)
- `Database::list_time_series_files_columns()` (lines 1609-1622)
- `Database::read_time_series_files()` (lines 1624-1669)
- `Database::update_time_series_files()` (lines 1671-1718)

### database_query.cpp -- ~24 lines

- `Database::query_string()` (lines 1728-1734)
- `Database::query_integer()` (lines 1736-1742)
- `Database::query_float()` (lines 1744-1750)

### database_relations.cpp -- ~85 lines

- `Database::set_scalar_relation()` (lines 808-851)
- `Database::read_scalar_relation()` (lines 853-888)

### database_describe.cpp -- ~90 lines

Uses: `scalar_metadata_from_column` would NOT actually be used here. Actually, `describe()` uses `data_type_to_string()` which is in the public header `data_type.h`. The stub CSV methods also live here.

- `Database::describe()` (lines 1752-1834)
- `Database::export_to_csv()` (lines 1720-1722)
- `Database::import_from_csv()` (lines 1724-1726)

## Line Count Analysis

| File | Estimated Lines | Content |
|------|----------------|---------|
| `database.cpp` | ~430 | Lifecycle, factory methods, execute, transactions, migration, schema |
| `database_create.cpp` | ~210 | create_element |
| `database_read.cpp` | ~190 | All read_scalar/vector/set/element_ids |
| `database_update.cpp` | ~260 | update_element + all update_scalar/vector/set |
| `database_delete.cpp` | ~15 | delete_element_by_id |
| `database_metadata.cpp` | ~130 | get_*_metadata, list_*_groups/attributes |
| `database_time_series.cpp` | ~215 | All time series operations including files |
| `database_query.cpp` | ~30 | query_string/integer/float |
| `database_relations.cpp` | ~90 | set_scalar_relation, read_scalar_relation |
| `database_describe.cpp` | ~90 | describe, csv stubs |
| `database_internal.h` | ~75 | Shared helpers |
| **Total** | **~1735** | (vs 1836 original, slight reduction from removed duplication) |

**Success criterion check:** `database.cpp` at ~430 lines is well under the 500-line limit.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Monolithic database.cpp | Split by operation type | Phase 2 (this phase) | Developers navigate to specific file for any operation type |

## Open Questions

1. **Should `database_delete.cpp` be merged with another file?**
   - What we know: `delete_element_by_id` is only ~12 lines. A file this small could be considered overhead.
   - What's unclear: Whether the project prefers consistency (one file per operation type) or pragmatism (merge tiny files).
   - Recommendation: Keep it separate for consistency with the naming convention and the test file pattern (`test_database_delete.cpp` already exists). The overhead of a 15-line `.cpp` file is negligible, and it maintains the 1:1 mapping between operation categories and files.

2. **Should `database_query.cpp` be merged with another file?**
   - What we know: Query methods are also very short (~24 lines total). They just delegate to `execute()`.
   - Recommendation: Keep separate. Same reasoning as delete -- matches the test file pattern (`test_database_query.cpp` exists) and maintains consistent decomposition.

3. **Namespace for internal helpers: `quiver::internal` or anonymous?**
   - What we know: Using `quiver::internal` with `inline` is the correct approach for shared headers. Anonymous namespace causes per-TU duplication.
   - Recommendation: Use `quiver::internal` namespace. This is clean, explicit, and avoids ODR issues.

## Sources

### Primary (HIGH confidence)

- Direct source code analysis of `src/database.cpp` (1836 lines), `src/database_impl.h` (112 lines), `include/quiver/database.h` (204 lines), `src/CMakeLists.txt` (119 lines)
- Phase 1 verification report confirming `database_impl.h` is correctly extracted and all tests pass
- C++ standard: member function definitions may appear in any translation unit that includes the class definition (ISO C++ [class.mfct]/1)

### Secondary (MEDIUM confidence)

- CMake documentation: explicit source file lists are recommended over GLOB for reliability
- spdlog sink headers are only needed by the logger factory function in database.cpp lifecycle code

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - no new dependencies, purely file reorganization
- Architecture: HIGH - based on direct analysis of 1836 lines with exact line-by-line mapping
- Pitfalls: HIGH - ODR rules, CMake source lists, and include patterns are well-understood C++ fundamentals

**Research date:** 2026-02-09
**Valid until:** Indefinite (C++ file decomposition patterns are stable)
