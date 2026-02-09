# Phase 1: C++ Impl Header Extraction - Research

**Researched:** 2026-02-09
**Domain:** C++ Pimpl pattern, internal header extraction, CMake private includes
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- The `Database::Impl` struct definition moves to `src/database_impl.h`
- Anonymous namespace helper templates (read_grouped_values_all, get_row_value, etc.) stay local -- they belong in their future split files, not in a shared header
- All decisions about TransactionGuard placement, with_transaction() approach, and helper organization are at Claude's discretion

### Claude's Discretion
- Whether TransactionGuard moves with Impl or stays in database.cpp (pick the cleanest approach based on actual dependencies)
- Whether with_transaction is an Impl method or standalone function (pick idiomatic C++)
- Header location and naming convention (src/database_impl.h is the default expectation)
- Include guard strategy (pragma once vs ifdef)
- Forward declarations vs direct includes in the internal header
- CMake include path approach (simplest correct method)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

## Summary

This phase is a purely mechanical extraction of the `Database::Impl` struct definition from `src/database.cpp` (lines 166-258) into a new private internal header `src/database_impl.h`. The goal is to make `Database::Impl` includable from multiple `.cpp` files (Phase 2) without exposing `sqlite3` or `spdlog` headers through the public API.

The extraction is straightforward because the CMake build system already has `src/` as a `PRIVATE` include directory for the `quiver` target (line 30 of `src/CMakeLists.txt`). This means no CMake changes are required -- any header placed in `src/` is automatically includable by all `src/*.cpp` files via `#include "database_impl.h"`, and it is guaranteed not to be part of the public include path (`include/`).

**Primary recommendation:** Move the `Database::Impl` struct (with its nested `TransactionGuard` class) into `src/database_impl.h`, include it from `src/database.cpp`, verify the build and all tests pass, and confirm no public headers changed.

## Standard Stack

Not applicable -- this phase uses no new libraries. It is a structural refactoring within the existing C++20 codebase using existing dependencies (sqlite3, spdlog).

## Architecture Patterns

### Current File Structure
```
include/quiver/database.h        # Public header: forward-declares struct Impl
src/database.cpp                  # 1934 lines: Impl definition + all method implementations
src/CMakeLists.txt                # PRIVATE include of ${CMAKE_CURRENT_SOURCE_DIR} (src/)
```

### Target File Structure
```
include/quiver/database.h        # UNCHANGED -- still forward-declares struct Impl
src/database_impl.h              # NEW -- Database::Impl struct definition + TransactionGuard
src/database.cpp                  # MODIFIED -- includes database_impl.h, Impl definition removed
```

### Pattern: Pimpl with Internal Header

The existing codebase already uses the Pimpl pattern for `Database` (hiding `sqlite3*` and `spdlog::logger` from public headers) and `LuaRunner` (hiding `sol::state`). The public header forward-declares the nested struct:

```cpp
// include/quiver/database.h (public)
class Database {
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

Currently, `Impl` is defined in-line within `database.cpp`. The extraction places the definition in a private header that is:
1. In `src/` (not `include/quiver/`), making it invisible to consumers
2. Includable by any `src/*.cpp` file via the existing PRIVATE include directory

### What the Impl Struct Contains (lines 166-258 of database.cpp)

**Data members:**
- `sqlite3* db = nullptr` -- raw sqlite3 connection (requires `<sqlite3.h>`)
- `std::string path` -- database file path
- `std::shared_ptr<spdlog::logger> logger` -- logging (requires spdlog headers)
- `std::unique_ptr<Schema> schema` -- parsed schema (uses `quiver/schema.h`)
- `std::unique_ptr<TypeValidator> type_validator` -- type validation (uses `quiver/type_validator.h`)

**Methods:**
- `require_schema(const char*)` -- throws if no schema loaded
- `require_collection(const std::string&, const char*)` -- throws if collection not found
- `load_schema_metadata()` -- populates schema + type_validator from database
- `~Impl()` -- closes sqlite3 connection via `sqlite3_close_v2`
- `begin_transaction()` -- executes `BEGIN TRANSACTION`
- `commit()` -- executes `COMMIT`
- `rollback()` -- executes `ROLLBACK`

**Nested class:**
- `TransactionGuard` -- RAII transaction wrapper (calls `begin_transaction()` on construction, `rollback()` on destruction unless `commit()` called)

### Recommendation: TransactionGuard Moves WITH Impl

TransactionGuard is defined inside `Impl` (as `Impl::TransactionGuard`) and directly accesses `Impl`'s members (`impl_.begin_transaction()`, `impl_.commit()`, `impl_.rollback()`). It is used in 10+ places throughout `database.cpp` as `Impl::TransactionGuard txn(*impl_)`. Moving it with Impl is the only clean approach because:

1. It takes an `Impl&` constructor argument
2. It is already scoped within `Impl`
3. Splitting it out would create an unnecessary coupling seam
4. Future split files (Phase 2) will need `TransactionGuard` for update/create/delete operations

**There is no `with_transaction` method currently.** The codebase uses `TransactionGuard` RAII directly. No change needed here -- the existing pattern is idiomatic and clean.

### Recommendation: Include Guard Strategy

Use `#pragma once`. Rationale:
- The project already uses `#ifndef` guards in all public headers (following a `QUIVER_*_H` convention)
- However, `src/database_impl.h` is a private, non-installable header
- `#pragma once` is simpler, avoids macro name collision risk, and is supported by every compiler this project targets (MSVC, GCC, Clang)
- The existing private header `src/c/internal.h` uses `#ifndef` guards, so either approach has precedent in the codebase

Alternatively, to maintain consistency with all existing headers: use `#ifndef QUIVER_DATABASE_IMPL_H` / `#define QUIVER_DATABASE_IMPL_H`. This is the safer choice for stylistic consistency.

**Final call: Use `#ifndef QUIVER_DATABASE_IMPL_H` to maintain full consistency with every other header in the project.**

### Recommendation: Direct Includes (not forward declarations)

The internal header must include the headers for types it uses directly as members:

```cpp
// Required includes for Impl member types
#include "quiver/database.h"           // Database class (Impl is Database::Impl)
#include "quiver/schema.h"             // Schema (used as unique_ptr<Schema>)
#include "quiver/type_validator.h"     // TypeValidator (used as unique_ptr<TypeValidator>)

#include <memory>                      // unique_ptr, shared_ptr
#include <string>                      // string

// Private dependency includes
#include <spdlog/spdlog.h>             // shared_ptr<spdlog::logger>
#include <sqlite3.h>                   // sqlite3*
```

Forward declarations are insufficient because:
- `spdlog::logger` is used as `shared_ptr<spdlog::logger>` (needs full type for shared_ptr)
- `sqlite3` can technically be forward-declared (it is a C struct), but `sqlite3.h` is needed for `sqlite3_close_v2()` in the destructor and `sqlite3_exec()` in transaction methods
- `Schema` and `TypeValidator` are used through `unique_ptr` (deletion requires complete type), and their methods are called directly in Impl methods

### CMake: No Changes Required

The critical discovery from `src/CMakeLists.txt` line 29-31:
```cmake
target_include_directories(quiver
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}   # This is src/
)
```

This means `src/` is already a PRIVATE include path. Any header in `src/` can be included by `src/*.cpp` files with `#include "database_impl.h"`. The PUBLIC include path is `include/`, which does NOT include `src/`. Therefore:
- No CMake changes needed
- `database_impl.h` is automatically invisible to consumers
- Success criterion 5 ("not reachable from any public include path") is satisfied by the existing CMake configuration

### What Stays in database.cpp

Per the locked decision, anonymous namespace helpers stay in `database.cpp`:
- `g_logger_counter` (atomic counter for logger names)
- `sqlite3_init_flag` (once_flag for sqlite3_initialize)
- `get_row_value()` overloads (type-specific row extractors)
- `read_grouped_values_all()` template
- `read_grouped_values_by_id()` template
- `ensure_sqlite3_initialized()` function
- `to_spdlog_level()` function
- `create_database_logger()` function
- `find_dimension_column()` function
- `scalar_metadata_from_column()` function (in `quiver` namespace, file-static)

All 1676 lines of Database method implementations also stay in `database.cpp`.

### Anti-Patterns to Avoid
- **DO NOT place database_impl.h in include/quiver/**: This would expose private dependencies (sqlite3, spdlog) to all consumers
- **DO NOT add database_impl.h to the install rules**: The CMake `install(DIRECTORY include/quiver ...)` already only installs public headers
- **DO NOT change the Database public header**: The forward declaration `struct Impl;` must remain unchanged
- **DO NOT move anonymous namespace helpers to the impl header**: They are local to database.cpp and will be distributed to their respective split files in Phase 2

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Include guard | Custom mutex-based guard | Standard `#ifndef`/`#define`/`#endif` | Compiler support is universal |
| Private include path | Manual `-I` flags | Existing CMake `PRIVATE` include dir | Already configured in src/CMakeLists.txt |

**Key insight:** This phase requires zero new infrastructure. Everything needed (private include paths, Pimpl pattern, include guards) already exists in the project.

## Common Pitfalls

### Pitfall 1: Including database_impl.h in the wrong order
**What goes wrong:** Compilation errors from missing forward declarations or circular includes
**Why it happens:** `database_impl.h` defines `Database::Impl`, which requires the `Database` class definition from `database.h`
**How to avoid:** `database_impl.h` must include `quiver/database.h` as its first project include. This is already the include pattern in `database.cpp` line 1.
**Warning signs:** "incomplete type" compiler errors

### Pitfall 2: Missing spdlog/sqlite3 includes in the impl header
**What goes wrong:** Compilation errors when Impl's destructor or transaction methods reference sqlite3/spdlog symbols
**Why it happens:** The Impl struct's methods call `sqlite3_close_v2`, `sqlite3_exec`, `sqlite3_free`, and `logger->debug/error`
**How to avoid:** Include both `<sqlite3.h>` and `<spdlog/spdlog.h>` in `database_impl.h`
**Warning signs:** Undefined symbol errors for sqlite3 or spdlog functions

### Pitfall 3: Forgetting to remove the Impl definition from database.cpp
**What goes wrong:** "redefinition of struct Database::Impl" linker/compiler error
**Why it happens:** If the Impl struct is defined in both the header and the .cpp file
**How to avoid:** After creating database_impl.h, delete lines 166-258 from database.cpp and replace with `#include "database_impl.h"`
**Warning signs:** Duplicate symbol errors

### Pitfall 4: Accidentally changing public header
**What goes wrong:** Violates success criterion 3 ("no public header changed")
**Why it happens:** Temptation to add includes or modify the forward declaration
**How to avoid:** The public header `include/quiver/database.h` must not be touched at all. Verify with `git diff include/` after changes.
**Warning signs:** Any modification to files in `include/quiver/`

### Pitfall 5: Breaking the include chain for database.cpp's other needs
**What goes wrong:** database.cpp stops compiling because includes were moved to the impl header but some are still needed locally
**Why it happens:** Moving all includes to the impl header when some are only needed by method implementations
**How to avoid:** Only move includes that are required by the Impl struct definition itself. Leave database.cpp's own includes (migrations.h, result.h, filesystem, fstream, iostream, etc.) in database.cpp.
**Warning signs:** Missing include errors in database.cpp after extraction

## Code Examples

### database_impl.h (Target Content)

```cpp
#ifndef QUIVER_DATABASE_IMPL_H
#define QUIVER_DATABASE_IMPL_H

#include "quiver/database.h"
#include "quiver/schema.h"
#include "quiver/type_validator.h"

#include <memory>
#include <string>

#include <spdlog/spdlog.h>
#include <sqlite3.h>

namespace quiver {

struct Database::Impl {
    sqlite3* db = nullptr;
    std::string path;
    std::shared_ptr<spdlog::logger> logger;
    std::unique_ptr<Schema> schema;
    std::unique_ptr<TypeValidator> type_validator;

    void require_schema(const char* operation) const {
        if (!schema) {
            throw std::runtime_error(std::string("Cannot ") + operation + ": no schema loaded");
        }
    }

    void require_collection(const std::string& collection, const char* operation) const {
        require_schema(operation);
        if (!schema->has_table(collection)) {
            throw std::runtime_error("Collection not found in schema: " + collection);
        }
    }

    void load_schema_metadata() {
        schema = std::make_unique<Schema>(Schema::from_database(db));
        SchemaValidator validator(*schema);
        validator.validate();
        type_validator = std::make_unique<TypeValidator>(*schema);
    }

    ~Impl() {
        if (db) {
            logger->debug("Closing database: {}", path);
            sqlite3_close_v2(db);
            db = nullptr;
            logger->info("Database closed");
        }
    }

    void begin_transaction() {
        char* err_msg = nullptr;
        const auto rc = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::string error = err_msg ? err_msg : "Unknown error";
            sqlite3_free(err_msg);
            throw std::runtime_error("Failed to begin transaction: " + error);
        }
        logger->debug("Transaction started");
    }

    void commit() {
        char* err_msg = nullptr;
        const auto rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::string error = err_msg ? err_msg : "Unknown error";
            sqlite3_free(err_msg);
            throw std::runtime_error("Failed to commit transaction: " + error);
        }
        logger->debug("Transaction committed");
    }

    void rollback() {
        char* err_msg = nullptr;
        const auto rc = sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::string error = err_msg ? err_msg : "Unknown error";
            sqlite3_free(err_msg);
            logger->error("Failed to rollback transaction: {}", error);
        } else {
            logger->debug("Transaction rolled back");
        }
    }

    class TransactionGuard {
        Impl& impl_;
        bool committed_ = false;

    public:
        explicit TransactionGuard(Impl& impl) : impl_(impl) { impl_.begin_transaction(); }

        void commit() {
            impl_.commit();
            committed_ = true;
        }

        ~TransactionGuard() {
            if (!committed_) {
                impl_.rollback();
            }
        }

        TransactionGuard(const TransactionGuard&) = delete;
        TransactionGuard& operator=(const TransactionGuard&) = delete;
    };
};

}  // namespace quiver

#endif  // QUIVER_DATABASE_IMPL_H
```

### database.cpp Changes (Top of File)

Before:
```cpp
#include "quiver/database.h"

#include "quiver/migrations.h"
#include "quiver/result.h"
#include "quiver/schema.h"
#include "quiver/schema_validator.h"
#include "quiver/type_validator.h"

#include <atomic>
#include <filesystem>
// ... more includes ...
// ... anonymous namespace ...

namespace quiver {

static ScalarMetadata scalar_metadata_from_column(const ColumnDefinition& col) { ... }

struct Database::Impl {
    // ... 93 lines of Impl definition ...
};

Database::Database(...) { ... }
```

After:
```cpp
#include "database_impl.h"

#include "quiver/migrations.h"
#include "quiver/result.h"
#include "quiver/schema_validator.h"

#include <atomic>
#include <filesystem>
// ... more includes (unchanged) ...
// ... anonymous namespace (unchanged) ...

namespace quiver {

static ScalarMetadata scalar_metadata_from_column(const ColumnDefinition& col) { ... }

// Impl definition is now in database_impl.h

Database::Database(...) { ... }
```

Note that after including `database_impl.h`, we no longer need explicit includes for `quiver/database.h`, `quiver/schema.h`, or `quiver/type_validator.h` in `database.cpp` because `database_impl.h` transitively provides them. However, `quiver/schema_validator.h` is still needed because it is used in `Impl::load_schema_metadata()` -- wait, actually `load_schema_metadata()` is IN the Impl struct which is now in the header. So `database_impl.h` needs `quiver/schema_validator.h` too.

**Correction:** The `load_schema_metadata()` method calls `SchemaValidator validator(*schema); validator.validate();`, which means `database_impl.h` must also include `quiver/schema_validator.h`. Updated include list for database_impl.h:

```cpp
#include "quiver/database.h"
#include "quiver/schema.h"
#include "quiver/schema_validator.h"
#include "quiver/type_validator.h"
```

And `database.cpp` can then drop `schema.h`, `schema_validator.h`, and `type_validator.h` from its includes (they come transitively through `database_impl.h`).

## State of the Art

Not applicable -- this is a standard C++ refactoring pattern (Pimpl with internal headers). No evolving standards or tool changes affect this work.

## Open Questions

1. **load_schema_metadata() inline in header vs declaration-only**
   - What we know: Currently `load_schema_metadata()` is defined inline in the Impl struct. It calls `SchemaValidator`, which means the impl header must include `schema_validator.h`.
   - What's unclear: Should `load_schema_metadata()` be moved out of the struct definition (declared in header, defined in database.cpp) to reduce the header's include footprint?
   - Recommendation: Keep it inline in the struct for Phase 1 (minimal change principle). Phase 2 can optimize the include chain if needed. The goal of Phase 1 is extraction, not restructuring.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection: `src/database.cpp` (1934 lines, Impl at lines 166-258)
- Direct codebase inspection: `src/CMakeLists.txt` (PRIVATE include at line 30)
- Direct codebase inspection: `include/quiver/database.h` (forward declaration at line 184)
- Direct codebase inspection: `src/c/internal.h` (existing internal header pattern precedent)

### Secondary (MEDIUM confidence)
- None needed -- all findings are verified from codebase inspection

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new libraries needed, pure refactoring
- Architecture: HIGH -- pattern is well-established (Pimpl internal header), CMake already configured
- Pitfalls: HIGH -- all pitfalls identified from actual code inspection, not hypothetical

**Research date:** 2026-02-09
**Valid until:** No expiration -- structural facts about the codebase do not change unless the codebase does
