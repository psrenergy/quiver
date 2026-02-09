# Architecture Research

**Domain:** C++20 library refactoring with C FFI and multi-language bindings
**Researched:** 2026-02-09
**Confidence:** HIGH (based on direct codebase analysis; decomposition patterns are well-established in C++ practice)

## Current State Analysis

### Line Counts (Monoliths)

| File | Lines | Responsibility |
|------|-------|----------------|
| `src/database.cpp` | 1934 | Everything: lifecycle, CRUD, reads, updates, metadata, queries, describe, CSV, time series, time series files, relations |
| `src/c/database.cpp` | 1612 | 1:1 C wrapper for every C++ Database method, plus memory helpers, type converters, metadata converters |

Both files are monoliths that mix unrelated concerns. Every new feature added to Database grows both files linearly.

### Functional Regions in `src/database.cpp`

Careful analysis of the file reveals these distinct functional regions:

| Region | Lines (approx) | Responsibility |
|--------|----------------|----------------|
| Anonymous namespace helpers | 1-158 | Logger creation, row extractors, template helpers, sqlite init |
| `Database::Impl` struct | 166-258 | sqlite3 handle, schema, type_validator, transaction guard, transaction ops |
| Lifecycle | 260-296 | Constructor, destructor, move ops, is_healthy |
| SQL execution core | 297-456 | `execute()`, `execute_raw()`, `current_version()`, `set_version()`, `begin_transaction()`, `commit()`, `rollback()` |
| Schema/migration application | 458-532 | `migrate_up()`, `apply_schema()`, factory methods (`from_schema`, `from_migrations`) |
| Element CRUD | 534-903 | `create_element()` (207 lines), `update_element()` (152 lines), `delete_element_by_id()` |
| Relations | 906-986 | `set_scalar_relation()`, `read_scalar_relation()` |
| Scalar reads | 988-1070 | `read_scalar_integers/floats/strings`, by-id variants |
| Vector reads | 1072-1118 | `read_vector_integers/floats/strings`, by-id variants |
| Set reads | 1120-1172 | `read_set_integers/floats/strings`, by-id variants, `read_element_ids()` |
| Scalar updates | 1174-1214 | `update_scalar_integer/float/string` |
| Vector updates | 1216-1286 | `update_vector_integers/floats/strings` |
| Set updates | 1288-1355 | `update_set_integers/floats/strings` |
| Metadata | 1357-1567 | `get_scalar_metadata`, `get_vector_metadata`, `get_set_metadata`, `get_time_series_metadata`, `list_*` methods |
| Time series | 1569-1699 | `read_time_series_group_by_id`, `update_time_series_group` |
| Time series files | 1701-1816 | `has_time_series_files`, `list_time_series_files_columns`, `read_time_series_files`, `update_time_series_files` |
| Query methods | 1826-1848 | `query_string/integer/float` |
| Describe | 1850-1934 | `describe()` (schema inspection output) |
| CSV stubs | 1818-1824 | `export_to_csv`, `import_from_csv` (empty stubs) |

### Functional Regions in `src/c/database.cpp`

| Region | Lines (approx) | Responsibility |
|--------|----------------|----------------|
| Template helpers | 1-78 | `read_scalars_impl`, `read_vectors_impl`, `free_vectors_impl`, `copy_strings_to_c` |
| Lifecycle | 80-162 | open, close, from_schema, from_migrations, is_healthy, path, version |
| Element CRUD | 164-262 | create, update, delete, relations |
| Scalar reads | 264-331 | all + by-id, free functions |
| Vector reads | 333-429 | all + by-id, free functions |
| Set reads | 431-501 | all + by-id (reuses vector helpers) |
| Scalar by-id reads | 503-679 | integer/float/string by-id, vector by-id, set by-id |
| Updates (scalar) | 695-743 | scalar integer/float/string |
| Updates (vector) | 745-822 | vector integer/float/string |
| Updates (set) | 824-901 | set integer/float/string |
| Metadata helpers + API | 903-1128 | `to_c_data_type`, `strdup_safe`, `convert_scalar_to_c`, `convert_group_to_c`, free helpers, all metadata/list APIs |
| CSV | 1130-1154 | export/import stubs |
| Query | 1156-1336 | query_string/integer/float, parameterized variants, `convert_params` |
| Describe | 1338-1348 | describe wrapper |
| Time series metadata + ops | 1350-1496 | time series metadata, read, update, free |
| Time series files | 1498-1612 | has, list_columns, read, update, free |

## Recommended Architecture

### System Overview

```
                         Bindings (Julia, Dart, Lua)
                                   |
                            [Generated FFI]
                                   |
                    include/quiver/c/*.h  (C headers)
                                   |
                         src/c/*.cpp  (C wrappers)
                                   |
                    include/quiver/*.h  (C++ headers)
                                   |
                         src/*.cpp  (C++ implementation)
                                   |
                   [sqlite3, spdlog, sol2, toml++]
```

The layering is clean and correct. The problem is not the layer architecture -- it is that `database.cpp` and `src/c/database.cpp` are monoliths within their respective layers.

### Component Boundaries (Proposed Decomposition)

Split `Database::Impl` method bodies across multiple `.cpp` files that all share the same `Impl` struct definition. The public header (`database.h`) stays unchanged. A single private header declares the `Impl` struct.

| Component File | Responsibility | Estimated Lines | Communicates With |
|----------------|---------------|-----------------|-------------------|
| `src/database_impl.h` (NEW, private) | `Database::Impl` struct definition, `TransactionGuard` | ~100 | All database_*.cpp files |
| `src/database.cpp` (slimmed) | Constructor, destructor, move ops, `is_healthy()`, `path()`, `execute()`, `execute_raw()`, `current_version()`, transaction delegates, factory methods, `apply_schema()`, `migrate_up()` | ~300 | database_impl.h |
| `src/database_create.cpp` (NEW) | `create_element()`, `update_element()`, `delete_element_by_id()` | ~400 | database_impl.h |
| `src/database_read.cpp` (NEW) | All `read_scalar_*`, `read_vector_*`, `read_set_*`, `read_element_ids()` | ~200 | database_impl.h |
| `src/database_update.cpp` (NEW) | All `update_scalar_*`, `update_vector_*`, `update_set_*` | ~200 | database_impl.h |
| `src/database_metadata.cpp` (NEW) | `get_scalar_metadata`, `get_vector_metadata`, `get_set_metadata`, `get_time_series_metadata`, all `list_*` methods, `describe()` | ~250 | database_impl.h |
| `src/database_time_series.cpp` (NEW) | `read_time_series_group_by_id`, `update_time_series_group`, time series files operations | ~200 | database_impl.h |
| `src/database_relations.cpp` (NEW) | `set_scalar_relation()`, `read_scalar_relation()` | ~80 | database_impl.h |
| `src/database_query.cpp` (NEW) | `query_string/integer/float` | ~30 | database_impl.h |
| `src/database_csv.cpp` (NEW) | `export_to_csv`, `import_from_csv` (stubs now, future growth) | ~10 | database_impl.h |

Similarly for the C API:

| Component File | Responsibility | Estimated Lines |
|----------------|---------------|-----------------|
| `src/c/database_helpers.h` (NEW, private) | Template helpers (`read_scalars_impl`, `read_vectors_impl`, `free_vectors_impl`, `copy_strings_to_c`), `strdup_safe`, metadata converters | ~200 |
| `src/c/database.cpp` (slimmed) | Lifecycle, element CRUD, relations | ~250 |
| `src/c/database_read.cpp` (NEW) | All scalar/vector/set read functions + free functions | ~350 |
| `src/c/database_update.cpp` (NEW) | All scalar/vector/set update functions | ~200 |
| `src/c/database_metadata.cpp` (NEW) | Metadata get/list/free functions | ~200 |
| `src/c/database_time_series.cpp` (NEW) | Time series read/update/free + files operations | ~200 |
| `src/c/database_query.cpp` (NEW) | Query functions, `convert_params`, parameterized variants | ~200 |

### Data Flow

```
[Binding Call]
    |
    v
[C API function] -- validates pointers (QUIVER_REQUIRE) -- try/catch wrapper
    |
    v
[Database public method] -- validates schema/collection -- delegates to Impl
    |
    v
[Database::Impl] -- owns sqlite3*, Schema*, TypeValidator*, logger
    |                  provides TransactionGuard, execute(), require_*()
    v
[sqlite3 API]
```

### Key Data Flows

1. **Read path:** C API -> Database method -> `impl_->require_collection()` -> build SQL -> `execute()` -> extract from `Result` -> return to C API -> marshal to C types via template helpers -> return to binding
2. **Write path:** C API -> Database method -> `impl_->type_validator->validate_*()` -> `TransactionGuard` -> build SQL -> `execute()` -> `txn.commit()` -> return
3. **Metadata path:** C API -> Database method -> `impl_->schema->get_table()` -> build metadata struct -> C API marshals with `convert_scalar_to_c`/`convert_group_to_c` -> return

## Architectural Patterns

### Pattern 1: Split Pimpl Implementation Across Files

**What:** A single class with Pimpl (`Database`) has its method bodies distributed across multiple `.cpp` files. All files include the same private header that defines the `Impl` struct.

**When to use:** When a Pimpl class grows beyond ~500 lines and its methods partition into coherent functional groups.

**Trade-offs:**
- Pro: Each file has a single responsibility; easier to navigate, review, and modify
- Pro: Compile-time improvement (change to read logic only recompiles `database_read.cpp`)
- Pro: Git history becomes clearer (diffs scoped to functional area)
- Con: One more private header to maintain
- Con: Must keep the Impl struct definition in sync

**Example:**

```cpp
// src/database_impl.h (private, not installed)
#ifndef QUIVER_DATABASE_IMPL_H
#define QUIVER_DATABASE_IMPL_H

#include "quiver/database.h"
#include "quiver/schema.h"
#include "quiver/type_validator.h"

#include <spdlog/spdlog.h>
#include <sqlite3.h>

namespace quiver {

struct Database::Impl {
    sqlite3* db = nullptr;
    std::string path;
    std::shared_ptr<spdlog::logger> logger;
    std::unique_ptr<Schema> schema;
    std::unique_ptr<TypeValidator> type_validator;

    void require_schema(const char* operation) const;
    void require_collection(const std::string& collection, const char* operation) const;
    void load_schema_metadata();
    void begin_transaction();
    void commit();
    void rollback();

    ~Impl();

    class TransactionGuard {
        Impl& impl_;
        bool committed_ = false;
    public:
        explicit TransactionGuard(Impl& impl);
        void commit();
        ~TransactionGuard();
        TransactionGuard(const TransactionGuard&) = delete;
        TransactionGuard& operator=(const TransactionGuard&) = delete;
    };
};

}  // namespace quiver

#endif
```

```cpp
// src/database_read.cpp
#include "database_impl.h"

namespace {
// Template helpers stay local to the translation unit that needs them
template <typename T>
std::vector<std::vector<T>> read_grouped_values_all(const quiver::Result& result) { ... }
}

namespace quiver {

std::vector<int64_t> Database::read_scalar_integers(const std::string& collection,
                                                      const std::string& attribute) {
    impl_->require_collection(collection, "read scalar");
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);
    // ...
}

// ... all other read methods

}  // namespace quiver
```

### Pattern 2: C API Helper Header

**What:** Extract reusable template helpers and conversion functions from the C API monolith into a private header.

**When to use:** When multiple C API source files need the same marshaling templates (read_scalars_impl, copy_strings_to_c, etc.).

**Trade-offs:**
- Pro: Eliminates duplication as the C API splits across files
- Pro: Each C API file focuses on its functional domain
- Con: Header-only templates increase include cost (marginal for internal headers)

**Example:**

```cpp
// src/c/database_helpers.h (private)
#ifndef QUIVER_C_DATABASE_HELPERS_H
#define QUIVER_C_DATABASE_HELPERS_H

#include "internal.h"
#include <string>
#include <vector>

// Reusable marshaling templates
template <typename T>
quiver_error_t read_scalars_impl(const std::vector<T>& values, T** out, size_t* count) { ... }

template <typename T>
quiver_error_t read_vectors_impl(const std::vector<std::vector<T>>& vecs, T*** out, size_t** sizes, size_t* count) { ... }

char* strdup_safe(const std::string& str);
void convert_scalar_to_c(const quiver::ScalarMetadata& src, quiver_scalar_metadata_t& dst);
void convert_group_to_c(const quiver::GroupMetadata& src, quiver_group_metadata_t& dst);
void free_scalar_fields(quiver_scalar_metadata_t& m);
void free_group_fields(quiver_group_metadata_t& m);
quiver_data_type_t to_c_data_type(quiver::DataType type);
quiver_error_t copy_strings_to_c(const std::vector<std::string>& values, char*** out, size_t* count);

#endif
```

### Pattern 3: Mirror C++ File Organization in C API

**What:** For every `src/database_X.cpp`, create a corresponding `src/c/database_X.cpp`. This makes it trivial to find the C wrapper for any C++ method.

**When to use:** Always, in this codebase. The 1:1 mapping between C++ methods and C functions is a fundamental architectural invariant.

**Trade-offs:**
- Pro: Predictable navigation -- developers always know where to find the C wrapper
- Pro: Parallel file names reinforce the layer separation
- Con: More files (but each is small and focused)

## Recommended Project Structure

```
include/quiver/
    database.h              # Public C++ header (UNCHANGED)
    attribute_metadata.h    # ScalarMetadata, GroupMetadata (UNCHANGED)
    element.h               # Element builder (UNCHANGED)
    lua_runner.h            # Lua scripting (UNCHANGED)
    ...                     # Other public headers (UNCHANGED)

include/quiver/c/
    database.h              # Public C header (UNCHANGED)
    element.h               # (UNCHANGED)
    lua_runner.h            # (UNCHANGED)
    ...

src/
    database_impl.h         # (NEW) Private Impl struct definition
    database.cpp            # Lifecycle, execute(), factories, schema/migration
    database_create.cpp     # (NEW) create_element, update_element, delete_element_by_id
    database_read.cpp       # (NEW) All read_scalar_*, read_vector_*, read_set_*, read_element_ids
    database_update.cpp     # (NEW) All update_scalar_*, update_vector_*, update_set_*
    database_metadata.cpp   # (NEW) get_*_metadata, list_*, describe()
    database_time_series.cpp# (NEW) Time series + time series files operations
    database_relations.cpp  # (NEW) set_scalar_relation, read_scalar_relation
    database_query.cpp      # (NEW) query_string/integer/float
    database_csv.cpp        # (NEW) export/import CSV stubs
    element.cpp             # (UNCHANGED)
    lua_runner.cpp          # (UNCHANGED)
    schema.cpp              # (UNCHANGED)
    schema_validator.cpp    # (UNCHANGED)
    type_validator.cpp      # (UNCHANGED)
    migration.cpp           # (UNCHANGED)
    migrations.cpp          # (UNCHANGED)
    result.cpp              # (UNCHANGED)
    row.cpp                 # (UNCHANGED)

src/c/
    internal.h              # (UNCHANGED) quiver_database struct, QUIVER_REQUIRE macros
    database_helpers.h      # (NEW) Template helpers, strdup_safe, metadata converters
    common.cpp              # (UNCHANGED)
    database.cpp            # Lifecycle, element CRUD, relations
    database_read.cpp       # (NEW) All read C wrappers + free functions
    database_update.cpp     # (NEW) All update C wrappers
    database_metadata.cpp   # (NEW) Metadata get/list/free C wrappers
    database_time_series.cpp# (NEW) Time series C wrappers
    database_query.cpp      # (NEW) Query C wrappers + convert_params
    element.cpp             # (UNCHANGED)
    lua_runner.cpp          # (UNCHANGED)
```

### Structure Rationale

- **`database_impl.h`:** The single private header that all `database_*.cpp` files include. Contains the `Impl` struct with `sqlite3*`, `Schema*`, `TypeValidator*`, `logger`, `TransactionGuard`. This is the only file that `#include <sqlite3.h>` and `#include <spdlog/spdlog.h>` for the Database implementation, preserving the Pimpl encapsulation.
- **Functional file split:** Each `database_*.cpp` maps to a logical API surface (read, update, create, metadata, etc.). This mirrors how the test files are already organized (`test_database_read.cpp`, `test_database_create.cpp`, etc.), making it easy to find the implementation for any tested behavior.
- **C API mirrors C++:** `src/c/database_read.cpp` wraps the functions implemented in `src/database_read.cpp`. Navigation is predictable.
- **Helper headers are private:** `database_impl.h` and `database_helpers.h` are never installed. They are implementation details.

## Build Order for Refactoring

This decomposition should be done in a specific order to maintain a green test suite at every step.

### Phase 1: Extract `database_impl.h` (Foundation)

Extract the `Database::Impl` struct and its inline method bodies into `src/database_impl.h`. Move the anonymous namespace helpers (logger creation, row extractors) there too since they are shared. Update `src/database.cpp` to `#include "database_impl.h"`. **Tests pass with zero behavior change.**

This is the critical enabling step. Everything else depends on it.

### Phase 2: Extract C++ Read Operations

Move all `read_scalar_*`, `read_vector_*`, `read_set_*`, and `read_element_ids` into `src/database_read.cpp`. Move the associated anonymous namespace template helpers (`read_grouped_values_all`, `read_grouped_values_by_id`, `get_row_value`) into the new file's anonymous namespace (they are only used by read methods).

Update `src/CMakeLists.txt` to add the new source file. Tests pass.

### Phase 3: Extract C++ Update Operations

Move all `update_scalar_*`, `update_vector_*`, `update_set_*` into `src/database_update.cpp`. Update CMakeLists. Tests pass.

### Phase 4: Extract C++ Create/Delete Operations

Move `create_element`, `update_element`, `delete_element_by_id` into `src/database_create.cpp`. These are the largest individual methods (~200 lines for create, ~150 for update). Update CMakeLists. Tests pass.

### Phase 5: Extract C++ Metadata, Time Series, Relations, Query, CSV

Move remaining functional groups into their respective files. This can be done in one batch or incrementally:
- `database_metadata.cpp`: metadata + describe
- `database_time_series.cpp`: time series + files
- `database_relations.cpp`: relation operations
- `database_query.cpp`: query methods
- `database_csv.cpp`: CSV stubs

After this phase, `src/database.cpp` contains only lifecycle, execute core, and factory methods (~300 lines).

### Phase 6: Extract C API Helpers

Create `src/c/database_helpers.h` with the template helpers and conversion functions. Update `src/c/database.cpp` to include it.

### Phase 7: Split C API Files

Mirror the C++ split: create `src/c/database_read.cpp`, `src/c/database_update.cpp`, `src/c/database_metadata.cpp`, `src/c/database_time_series.cpp`, `src/c/database_query.cpp`. Update CMakeLists.

### Phase 8: Verify Full Stack

Run `scripts/build-all.bat` to confirm all C++ tests, C API tests, Julia tests, and Dart tests pass. The bindings are unaffected because the C header (`include/quiver/c/database.h`) does not change.

## Anti-Patterns

### Anti-Pattern 1: Splitting the Public Header

**What people do:** Try to decompose `include/quiver/database.h` into multiple public headers (one per functional group).

**Why it is wrong:** The `Database` class is a single type. Splitting its declaration across headers breaks the type system and makes the API harder to consume. Bindings generators would need to discover and combine multiple headers. The header is 200 lines -- it is not the problem.

**Do this instead:** Keep the single public header. Split only the `.cpp` implementation files.

### Anti-Pattern 2: Breaking the Pimpl by Exposing Impl Details

**What people do:** Make `database_impl.h` a public header, or give split files direct access to `sqlite3*` bypassing the Impl struct.

**Why it is wrong:** The entire point of Pimpl is that consumers of `database.h` never see sqlite3 or spdlog headers. If split implementation files access `sqlite3*` directly without going through `Impl`, the encapsulation boundary becomes inconsistent.

**Do this instead:** All split files include the private `database_impl.h` and access sqlite3 through `impl_->db`. The Impl struct is the single source of truth for the database connection state.

### Anti-Pattern 3: Creating Intermediate Classes

**What people do:** Create `DatabaseReader`, `DatabaseWriter`, `DatabaseMetadata` classes to decompose the monolith, then compose them inside `Database`.

**Why it is wrong in this codebase:** All operations share the same `sqlite3*` connection, the same `Schema*`, the same `TransactionGuard`. Splitting into classes creates artificial ownership boundaries and makes transactions that span operations (like `create_element` which writes to scalar table + vector tables + set tables + time series tables) harder to implement. The API contract is that `Database` is the single entry point.

**Do this instead:** Split implementation files, not the class. One class, many source files, one Impl struct.

### Anti-Pattern 4: Splitting C API Without Splitting C++ First

**What people do:** Start by splitting `src/c/database.cpp` because it has "simpler" code (just wrappers).

**Why it is wrong:** The C API file structure should mirror the C++ file structure. If you split the C API first, you create a file organization that has no correspondence to the C++ layer. Then when you split the C++ layer later, you have to re-reorganize the C API to match.

**Do this instead:** C++ first, C API second. The C API mirrors the C++ structure.

## Integration Points

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| `database_*.cpp` <-> `database_impl.h` | Direct member access via `impl_->` | All files share the same Impl struct; no message passing |
| `src/c/database_*.cpp` <-> `src/c/database_helpers.h` | Template function calls | Helpers are header-only templates, no link-time dependency |
| `src/c/database_*.cpp` <-> `src/c/internal.h` | `quiver_database` struct, `QUIVER_REQUIRE` macro | Unchanged from current architecture |
| C++ layer <-> C layer | `db->db.method_name(...)` pattern | C API holds `quiver::Database` by value inside opaque `quiver_database` struct |

### CMake Integration

Adding new source files requires updating `src/CMakeLists.txt`:

```cmake
# Core library sources
set(QUIVER_SOURCES
    database.cpp
    database_create.cpp      # NEW
    database_read.cpp        # NEW
    database_update.cpp      # NEW
    database_metadata.cpp    # NEW
    database_time_series.cpp # NEW
    database_relations.cpp   # NEW
    database_query.cpp       # NEW
    database_csv.cpp         # NEW
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

And for the C API:

```cmake
if(QUIVER_BUILD_C_API)
    add_library(quiver_c SHARED
        c/common.cpp
        c/database.cpp
        c/database_read.cpp        # NEW
        c/database_update.cpp      # NEW
        c/database_metadata.cpp    # NEW
        c/database_time_series.cpp # NEW
        c/database_query.cpp       # NEW
        c/element.cpp
        c/lua_runner.cpp
    )
```

### Binding Impact

**None.** The bindings (Julia, Dart) depend only on the installed C headers (`include/quiver/c/*.h`). Since no headers change, no binding regeneration is needed. This is the primary reason to decompose at the `.cpp` level only.

## Scaling Considerations

| Concern | Current (1934 lines) | After Split (~300 per file) | Future Growth |
|---------|---------------------|----------------------------|---------------|
| Developer navigation | Requires search to find method in monolith | Open file matching test file name | Each new feature group gets its own file |
| Compile time impact | Any change recompiles entire 1934-line TU | Only affected TU recompiles | Incremental build stays fast |
| Merge conflicts | High probability on concurrent changes | Low -- changes to reads don't conflict with changes to writes | Feature branches touch different files |
| Code review clarity | Reviewers see 1934 lines of context | PRs scoped to specific functional area | Self-documenting structure |

### Scaling Priority

1. **First bottleneck (current):** `src/database.cpp` at 1934 lines. Every change requires navigating the entire file. Concurrent work causes merge conflicts.
2. **Second bottleneck (current):** `src/c/database.cpp` at 1612 lines. Same problem, compounded by the mechanical wrapper pattern making it hard to find the right function.
3. **Non-bottleneck:** All other files are appropriately sized. `schema.cpp`, `schema_validator.cpp`, `type_validator.cpp`, `element.cpp`, `lua_runner.cpp` are each focused on a single responsibility and do not need splitting.

## Sources

- Direct codebase analysis of Quiver repository (all confidence levels HIGH)
- C++ Pimpl idiom: Standard practice documented in Herb Sutter's GotW #100, Scott Meyers' Effective Modern C++ Item 22
- Split implementation pattern: Used by major C++ projects (LLVM splits `Sema` into `SemaDecl.cpp`, `SemaExpr.cpp`, `SemaType.cpp` etc. -- same class, multiple source files)
- CMake multi-file builds: Standard CMake documentation for `add_library` with multiple sources

---
*Architecture research for: Quiver C++20 library refactoring*
*Researched: 2026-02-09*
